/***************************************************************************************
  * 模块名称：MyI2C 软件模拟I2C总线驱动（用普通GPIO引脚模拟）
  * 功能简介：不使用STM32硬件I2C外设，仅靠两个GPIO引脚的置高/置低，手动模拟出I2C
  *           （Inter-Integrated Circuit，内置集成电路总线，读作“I方C”）通信所需的
  *           6个基本动作：起始、终止、发送一个字节、接收一个字节、发送应答、接收应答，
  *           供上层MPU6050姿态传感器驱动调用。
  * 硬件接线（以本工程代码为准，具体PCB请以原理图为准）：
  *           PB10 -> SCL（串行时钟线，始终由主机输出时钟脉冲，从机踩着节拍收发）
  *           PB11 -> SDA（串行数据线，双向，主机和从机都可以把它拉低）
  *           SCL、SDA两条线都需要上拉电阻（多数模块板载已有，接到VCC即可）
  * I2C协议入门要点：
  *           1. 总线只有SCL、SDA两根线。空闲时两线均为高电平；
  *           2. 起始信号：SCL为高电平期间，SDA由高变低（下降沿）；
  *              终止信号：SCL为高电平期间，SDA由低变高（上升沿）；
  *           3. 数据位规则：SCL高电平期间SDA必须保持稳定（此刻数据被采样），
  *              只允许在SCL低电平期间改变SDA；每8位组成1字节，高位(MSB)先行；
  *           4. 应答ACK：每发完8位，接收方在第9个SCL脉冲把SDA拉低表示“收到了(ACK=0)”，
  *              若SDA为高则表示“非应答(NACK=1)”，通常意味着后面不再继续收发；
  *           5. 推挽与开漏：推挽输出能主动输出高/低两种强电平；开漏输出只能主动拉低，
  *              输出1时引脚呈高阻态，电平靠外部上拉电阻拉高。I2C选择开漏+上拉，
  *              是为了让主机、从机都能安全地“拉低线”而不会出现两个输出短路对顶。
  *           6. 小技巧：STM32开漏输出模式下，向输出锁存器写1即“释放”引脚，此时直接
  *              读端口输入数据寄存器，就能得到SDA引脚的真实电平，故SDA一根脚既可
  *              发送又可接收，不必在输出/输入模式之间来回切换。
  * 在平衡车中的作用：
  *           是MPU6050六轴传感器的底层通信通道。主控制循环每隔10ms通过本模块
  *           读取加速度计和陀螺仪的原始数据，供姿态解算（互补滤波）得到车身倾角，
  *           再交给角度环/速度环PID，最终驱动电机保持平衡。
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header
#include "Delay.h"

/**
  * 函    数：MyI2C_W_SCL
  * 功    能：写SCL时钟线电平（I2C的“节拍器”）
  * 参    数：BitValue 要输出的电平，0=拉低SCL；1=释放SCL（开漏+外部上拉后呈高电平）
  * 返 回 值：无
  * 说    明：SCL由主机（STM32）单方驱动，从机不能控制它；
  *           拉高/拉低一次SCL构成一个时钟脉冲，1个脉冲传1位数据
  */
void MyI2C_W_SCL(uint8_t BitValue)
{
	/*向PB10输出电平，BitAction是枚举类型，强转后0对应Bit_RESET、1对应Bit_SET*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_10, (BitAction)BitValue);
//	Delay_us(10);
}

/**
  * 函    数：MyI2C_W_SDA
  * 功    能：写SDA数据线电平（发送数据位/起始/终止/应答时使用）
  * 参    数：BitValue 0=主动拉低SDA；1=释放SDA（交给上拉电阻或从机控制）
  * 返 回 值：无
  * 说    明：因为引脚是开漏模式，写1并不强制输出高电平，只是“松手”，
  *           这样在接收字节/应答阶段，从机才能把SDA拉低把数据送回主机
  */
void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)BitValue);
//	Delay_us(10);
}

/**
  * 函    数：MyI2C_R_SDA
  * 功    能：读取SDA数据线上的实际电平（接收数据位/应答位时使用）
  * 参    数：无
  * 返 回 值：SDA引脚电平，0=低电平，1=高电平
  * 说    明：读取前调用者应先MyI2C_W_SDA(1)释放SDA；
  *           开漏输出模式下读输入寄存器即可得到线上真实电平，无需切换GPIO模式
  */
uint8_t MyI2C_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11);	//读PB11输入电平
//	Delay_us(10);
	return BitValue;
}

/**
  * 函    数：MyI2C_Init
  * 功    能：软件I2C引脚初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：1. 开启GPIOB时钟；
  *           2. PB10(SCL)、PB11(SDA)均配置为“开漏输出50MHz”；
  *           3. 两个引脚先置1（释放总线），让SCL/SDA处于空闲高电平。
  *           注意：使用任何STM32外设/GPIO前都必须先开启对应总线的时钟，否则不工作
  */
void MyI2C_Init(void)
{
	/*开启GPIOB端口时钟（挂载在APB2总线上）*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/*定义GPIO初始化结构体并填充参数*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;					//开漏输出，I2C标准要求
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;		//同时选择PB10、PB11
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;				//输出速率等级（驱动能力参数）
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*释放SCL、SDA，两线由外部上拉电阻拉为高电平，进入I2C空闲状态*/
	GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
}

/**
  * 函    数：MyI2C_Start
  * 功    能：产生I2C起始信号
  * 参    数：无
  * 返 回 值：无
  * 说    明：起始信号定义——SCL为高电平期间，SDA出现下降沿（高->低）。
  *           时序分解：先确保SDA、SCL都为高（空闲态），再在SCL高时拉低SDA，
  *           最后拉低SCL，既“占用”总线，又方便紧接着在SCL低电平期间放数据位
  */
void MyI2C_Start(void)
{
	MyI2C_W_SDA(1);		//先释放SDA，确保它是高电平
	MyI2C_W_SCL(1);		//再释放SCL，确保它是高电平（此刻两线皆高=空闲）
	MyI2C_W_SDA(0);		//SCL高电平期间把SDA拉低：高->低的下降沿，即起始信号S
	MyI2C_W_SCL(0);		//拉低SCL，占用总线并准备输出第一个数据位
}

/**
  * 函    数：MyI2C_Stop
  * 功    能：产生I2C终止信号
  * 参    数：无
  * 返 回 值：无
  * 说    明：终止信号定义——SCL为高电平期间，SDA出现上升沿（低->高）。
  *           时序分解：先拉低SDA，再释放SCL为高，最后释放SDA，
  *           SDA在SCL高时由低变高，即终止信号P，总线恢复空闲
  */
void MyI2C_Stop(void)
{
	MyI2C_W_SDA(0);		//先拉低SDA，确保起始前SDA为低
	MyI2C_W_SCL(1);		//释放SCL为高电平
	MyI2C_W_SDA(1);		//SCL高电平期间释放SDA：低->高的上升沿，即终止信号P
}

/**
  * 函    数：MyI2C_SendByte
  * 功    能：在I2C总线上发送一个字节（8个数据位，高位先行）
  * 参    数：Byte 要发送的字节，范围0x00~0xFF（可以是从机地址、寄存器地址或数据）
  * 返 回 值：无
  * 说    明：每一位都遵循“先放数据到SDA（SCL低电平时），再拉高SCL让从机采样，
  *           然后拉低SCL”的节拍；8位发完后应答位由接收方负责，本函数不读取应答
  */
void MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i ++)		//循环8次，每次发送1位，从最高位bit7到最低位bit0
	{
		/*0x80>>i 依次取0x80/0x40/.../0x01作为掩码；与Byte按位与后，
		  用两个逻辑非!!把任意非零值规整成1，零仍为0，从而在SDA输出0或1*/
		MyI2C_W_SDA(!!(Byte & (0x80 >> i)));
		MyI2C_W_SCL(1);			//SCL产生上升沿并保持高电平，从机在此期间读取SDA
		MyI2C_W_SCL(0);			//拉低SCL，本位移送完成，准备下一位
	}
}

/**
  * 函    数：MyI2C_ReceiveByte
  * 功    能：从I2C总线接收一个字节（8个数据位，高位先行）
  * 参    数：无
  * 返 回 值：接收到的字节，范围0x00~0xFF
  * 说    明：接收前必须先MyI2C_W_SDA(1)释放SDA，把数据线“让给”从机驱动；
  *           主机每拉高一次SCL就读取一位SDA，按位拼接到Byte中；
  *           读完8位后是否给应答，由调用者另行调用MyI2C_SendAck决定
  */
uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i, Byte = 0x00;
	MyI2C_W_SDA(1);				//主机释放SDA，从机才能输出数据（开漏“松手”）
	for (i = 0; i < 8; i ++)	//循环8次，每次接收1位，先收最高位
	{
		MyI2C_W_SCL(1);						//拉高SCL，此刻SDA上的数据有效
		if (MyI2C_R_SDA()){Byte |= (0x80 >> i);}	//若读到1，就把Byte对应位置1；读到0则保持0
		MyI2C_W_SCL(0);						//拉低SCL，从机准备输出下一位
	}
	return Byte;				//返回拼接好的8位数据
}

/**
  * 函    数：MyI2C_SendAck
  * 功    能：主机向从机发送应答位（8位数据之后的第9个时钟）
  * 参    数：AckBit 应答电平，0=应答ACK（告诉从机：我还要继续收，请继续发）；
  *                          1=非应答NACK（告诉从机：就收到这里，准备结束通信）
  * 返 回 值：无
  * 说    明：本工程读连续寄存器时，中间字节回ACK(0)，最后一个字节回NACK(1)
  */
void MyI2C_SendAck(uint8_t AckBit)
{
	MyI2C_W_SDA(AckBit);		//SCL低电平期间先把应答位放到SDA上
	MyI2C_W_SCL(1);				//拉高SCL，从机在高电平期间采样应答位
	MyI2C_W_SCL(0);				//拉低SCL，应答时钟结束
}

/**
  * 函    数：MyI2C_ReceiveAck
  * 功    能：主机接收从机在第9个时钟发回的应答位
  * 参    数：无
  * 返 回 值：AckBit 应答电平，0=从机应答ACK（设备存在且接收正常）；
  *                        1=非应答NACK（地址不对、设备不在或通信异常）
  * 说    明：主机先释放SDA，再产生一个SCL脉冲并读取SDA；
  *           MPU6050驱动中每发完一个字节都会调用本函数（返回值未做判断）
  */
uint8_t MyI2C_ReceiveAck(void)
{
	uint8_t AckBit;
	MyI2C_W_SDA(1);				//主机释放SDA，交给从机拉低/释放来表达应答
	MyI2C_W_SCL(1);				//拉高SCL，应答位在此刻有效
	AckBit = MyI2C_R_SDA();		//读取SDA：0=ACK，1=NACK
	MyI2C_W_SCL(0);				//拉低SCL，应答时钟结束
	return AckBit;
}
