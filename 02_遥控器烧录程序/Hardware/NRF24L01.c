
/***************************************************************************************
  * 本程序由江协科技创建并免费开源共享
  * 你可以任意查看、使用和修改，并应用到自己的项目之中
  * 程序版权归江协科技所有，任何人或组织不得将其据为己有
  * 
  * 程序名称：				NRF24L01无线通信模块驱动程序
  * 程序创建时间：			2025.6.9
  * 当前程序版本：			V1.0
  * 当前版本发布时间：		2024.6.9
  * 
  * 江协科技官方网站：		jiangxiekeji.com
  * 江协科技官方淘宝店：	jiangxiekeji.taobao.com
  * 程序介绍及更新动态：	jiangxiekeji.com/tutorial/nrf24l01.html
  * 
  * 如果你发现程序中的漏洞或者笔误，可通过邮件向我们反馈：feedback@jiangxiekeji.com
  * 发送邮件之前，你可以先到更新动态页面查看最新程序，如果此问题已经修改，则无需再发邮件
  ***************************************************************************************
  */

/***************************************************************************************
  * 【遥控器工程教学补充说明】
  * 模块功能：			驱动NRF24L01 2.4GHz无线收发芯片，是遥控器与小车之间唯一的数据桥梁
  * 硬件接线（软件SPI）：	CE(片内收发模式控制) → PB11	CSN(SPI片选，低有效) → PB12
  * 						SCK(SPI时钟)        → PB13	MISO(主机输入)       → PB14
  * 						MOSI(主机输出)      → PB15	IRQ(中断输出)        → 不接（本驱动用查询方式）
  * 数据包布局：			发送固定32字节包，遥控器实际使用前6字节（main.c填充）：
  * 						[0]Mode 模式(0普通/1需要回传)  [1]LH  [2]LV  [3]RH  [4]RV(摇杆-100~100)  [5]KEY 键码
  * 						当Mode=1时，主循环调用NRF24L01_Receive接收小车回传的32字节包，前部16字节为：
  * 						[0]=0x02包标识  [1]PWML  [2]PWMR  [3]保留
  * 						[4..7]Angle(float4字节)  [8..11]SpeedL  [12..15]SpeedR
  * 收发状态切换：		初始化后芯片默认处于接收(RX)模式；每100ms发送时NRF24L01_Send内部先切到
  * 						发送(TX)模式，发完并等待结果后自动切回RX模式，因此平时也能收到小车的回传数据
  * 可靠性机制：			ACK自动应答（接收方收到正确数据后自动回一个"我收到了"的小包，发送方据此判定成功，
  * 						无需双方编程处理）+ CRC校验 + 失败自动重传，Send的返回值即发送成功/失败状态码
  ***************************************************************************************
  */

#include "stm32f10x.h"
#include "NRF24L01_Define.h"

/*全局变量*********************/

/*发送部分*/
uint8_t NRF24L01_TxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};		//发送地址，固定5字节
#define NRF24L01_TX_PACKET_WIDTH		32							//发送数据包宽度，范围：1~32字节
uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH];				//发送数据包

/*接收部分*/
uint8_t NRF24L01_RxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};		//接收通道0地址，固定5字节
#define NRF24L01_RX_PACKET_WIDTH		32							//接收通道0数据包宽度，范围：1~32字节
uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH];				//接收数据包

/**
  * 提示：设备A和设备B进行通信
  * A发B收时，A的发送地址、发送数据包宽度要与B的接收地址、接收数据包宽度对应相同
  * B发A收时，B的发送地址、发送数据包宽度要与A的接收地址、接收数据包宽度对应相同
  * 通常情况下，可以将A和B的发送地址、接收地址全设置一样，A和B的发送数据包宽度、接收数据包宽度也全设置一样
  * 这样A和B可以使用完全一样的模块程序，操作更加方便，也不容易搞混
  * 
  */

/*********************全局变量*/


/*引脚配置*********************/

/**
  * 函    数：NRF24L01写CE高低电平
  * 参    数：要写入CE的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写CE时，此函数会被调用
  *           用户需要根据参数传入的值，将CE置为高电平或者低电平
  *           当参数传入0时，置CE为低电平，当参数传入1时，置CE为高电平
  */
void NRF24L01_W_CE(uint8_t BitValue)
{
	/*根据BitValue的值，将CE置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写CSN高低电平
  * 参    数：要写入CSN的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写CSN时，此函数会被调用
  *           用户需要根据参数传入的值，将CSN置为高电平或者低电平
  *           当参数传入0时，置CSN为低电平，当参数传入1时，置CSN为高电平
  */
void NRF24L01_W_CSN(uint8_t BitValue)
{
	/*根据BitValue的值，将CSN置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_12, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写SCK高低电平
  * 参    数：要写入SCK的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写SCK时，此函数会被调用
  *           用户需要根据参数传入的值，将SCK置为高电平或者低电平
  *           当参数传入0时，置SCK为低电平，当参数传入1时，置SCK为高电平
  */
void NRF24L01_W_SCK(uint8_t BitValue)
{
	/*根据BitValue的值，将SCK置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_13, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写MOSI高低电平
  * 参    数：要写入MOSI的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写MOSI时，此函数会被调用
  *           用户需要根据参数传入的值，将MOSI置为高电平或者低电平
  *           当参数传入0时，置MOSI为低电平，当参数传入1时，置MOSI为高电平
  */
void NRF24L01_W_MOSI(uint8_t BitValue)
{
	/*根据BitValue的值，将MOSI置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_15, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01读MISO高低电平
  * 参    数：无
  * 返 回 值：读取得到MISO的电平值，范围：0/1
  * 说    明：当上层函数需要读MISO时，此函数会被调用
  *           用户需要读取MISO引脚，返回此引脚的高低电平状态
  *           当MISO为高电平时，返回1，当MISO为低电平时，返回0
  */
uint8_t NRF24L01_R_MISO(void)
{
	/*取MISO引脚的高低电平并返回*/
	return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14);
}

/**
  * 本代码使用查询的方式获取设备状态，因此不需要使用IRQ引脚
  */

/**
  * 函    数：NRF24L01引脚初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：当上层函数需要初始化时，此函数会被调用
  *           用户需要将CSN、CE、MISO、SCK引脚初始化为推挽输出模式，MISO引脚初始化为上拉输入模式
  */
void NRF24L01_GPIO_Init(void)
{
	/*开启GPIO时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	/*将CE、CSN、SCK、MOSI引脚初始化为推挽输出模式*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/*将MISO引脚初始化为上拉输入模式*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/*置引脚初始化后的默认电平*/
	NRF24L01_W_CE(0);		//CE默认为0，退出收发模式
	NRF24L01_W_CSN(1);		//CSN默认为1，不选中从机
	NRF24L01_W_SCK(0);		//SCK默认为0，对应SPI模式0
	NRF24L01_W_MOSI(0);		//MOSI默认电平随意，1和0均可
}

/*********************引脚配置*/


/*通信协议*********************/

/**
  * 函    数：SPI交换一个字节
  * 参    数：Byte 要发送的一个字节数据，范围：0x00~0xFF
  * 返 回 值：接收得到的一个字节数据，范围：0x00~0xFF
  */
uint8_t NRF24L01_SPI_SwapByte(uint8_t Byte)
{
	uint8_t i;
	
	/*【知识点·软件模拟SPI（模式0）】SPI模式0的约定：SCK空闲为低电平，在SCK上升沿采样数据、下降沿切换数据。*/
	/*数据高位先行（MSB First）：每一位先在MOSI上准备好，再拉高SCK让从机读走；与此同时从机也会在MISO上*/
	/*给出它要发的那一位，主机在高电平稳住期间读入——SPI的"交换"含义就在于此：发1位的同时必定收1位。*/
	/*此处使用SPI模式0进行通信*/
	/*循环8次，主机依次移出和移入数据的每一位*/
	for (i = 0; i < 8; i ++)
	{
		/*SPI为高位先行，因此移出高位至MOSI引脚*/
		if (Byte & 0x80)			//判断Byte的最高位
		{
			NRF24L01_W_MOSI(1);		//如果为1，则给MOSI输出1
		}
		else
		{
			NRF24L01_W_MOSI(0);		//如果为0，则给MOSI输出0
		}
		Byte <<= 1;					//Byte左移一位，最低位空出来用于接收数据位
		
		/*产生SCK上升沿*/
		NRF24L01_W_SCK(1);
		
		/*从MISO引脚移入数据，存入Byte的最低位*/
		if (NRF24L01_R_MISO())		//读取MISO引脚
		{
			Byte |= 0x01;			//如果为1，则给Byte最低位置1
		}							//如果为0，则不做任何操作，因为左移后低位默认补0
		
		/*产生SCK下降沿*/
		NRF24L01_W_SCK(0);
	}
	
	/*返回Byte数据，此时的Byte为SPI交换接收得到的一个字节数据*/
	return Byte;
}

/*********************通信协议*/


/*指令实现*********************/

/**
  * 函    数：NRF24L01读取寄存器（一个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 返 回 值：指定寄存器的数据，范围：0x00~0xFF
  */
uint8_t NRF24L01_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
	
	/*发送读寄存器指令后，开始交换接收，得到指定地址的数据*/
	Data = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
	
	/*返回读到的一个字节数据*/
	return Data;
}

/**
  * 函    数：NRF24L01读取寄存器（多个字节）
  * 参    数：RegAddress 指定寄存器的地址，范围：0x00~0x1F
  * 参    数：DataArray 读取得到的数据数组，输出参数
  * 参    数：Count 指定读取的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
	
	/*发送读寄存器指令后，开始交换接收，循环接收多次，得到指定地址下的多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将接收到的数据写入到输出参数DataArray中*/
		DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入寄存器（一个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 参    数：Data 要写入的一个字节数据，范围：0x00~0xFF
  * 返 回 值：无
  */
void NRF24L01_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
	
	/*发送写寄存器指令后，开始交换发送，在指定地址下写入数据*/
	NRF24L01_SPI_SwapByte(Data);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入寄存器（多个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 参    数：DataArray 要写入的数据数组，输入参数
  * 参    数：Count 指定写入的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_WriteRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
	
	/*发送写寄存器指令后，开始交换发送，循环发送多次，在指定地址下写入多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将输入参数DataArray的数据写入到指定地址中*/
		NRF24L01_SPI_SwapByte(DataArray[i]);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01读取Rx有效载荷
  * 参    数：DataArray 读取得到的数据数组，输出参数
  * 参    数：Count 指定读取的数量，范围：0~32
  * 返 回 值：无
  */
void NRF24L01_ReadRxPayload(uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读取Rx有效载荷*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_RX_PAYLOAD);
	
	/*发送读取Rx有效载荷指令后，开始交换接收，循环接收多次，得到多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将读取的数据写入到输出参数DataArray中*/
		DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入Tx有效载荷
  * 参    数：DataArray 要写入的数据数组，输入参数
  * 参    数：Count 指定写入的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_WriteTxPayload(uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写入Tx有效载荷*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_TX_PAYLOAD);
	
	/*发送写入Tx有效载荷指令后，开始交换发送，循环发送多次，写入多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将输入参数DataArray的数据写入到Tx有效载荷中*/
		NRF24L01_SPI_SwapByte(DataArray[i]);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01清空Tx FIFO的所有数据
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_FlushTx(void)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，清空Tx FIFO*/
	NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_TX);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01清空Rx FIFO的所有数据
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_FlushRx(void)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，清空Rx FIFO*/
	NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_RX);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01读取状态寄存器
  * 参    数：无
  * 返 回 值：状态寄存器的值，范围：0x00~0xFF
  */
uint8_t NRF24L01_ReadStatus(void)
{
	uint8_t Status;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，空指令*/
	/*第一个字节发送任意指令，都可以交换得到状态寄存器的值*/
	Status = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
	
	/*返回状态寄存器的值*/
	return Status;
}

/*********************指令实现*/


/*功能函数*********************/

/*【知识点·NRF24L01的状态机】芯片主要有4种工作状态，由CONFIG寄存器的两位和CE引脚电平共同决定：*/
/*  掉电PowerDown(PWR_UP=0)：最省电，寄存器内容保留，但不能收发；*/
/*  待机Standby(PWR_UP=1,CE=0)：已上电但不主动收发，可随时通过拉高CE进入TX/RX；*/
/*  发送TX(CE=1,PRIM_RX=0)：把发送FIFO中的数据包发射出去；接收RX(CE=1,PRIM_RX=1)：持续监听空中数据包。*/
/*下面每个模式切换函数都遵循"先CE=0退出当前收发 → 读-改-写CONFIG → 需要时再CE=1"的稳妥流程，*/
/*可避免在收发中途修改配置产生错误动作。另外：SPI通信正常时读到的寄存器不可能长期是0xFF，*/
/*若读出0xFF（MISO一直为高）通常意味着模块没接好/没供电，故驱动用它做故障判断。*/

/**
  * 函    数：NRF24L01进入掉电模式（CE = 0，PWR_UP = 0）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_PowerDown(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config &= ~0x02;								//配置寄存器位1（PWR_UP）置0
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
}

/**
  * 函    数：NRF24L01进入待机模式1（CE = 0，PWR_UP = 1）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_StandbyI(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x02;									//配置寄存器位1（PWR_UP）置1
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
}

/**
  * 函    数：NRF24L01进入接收模式（CE = 1，PWR_UP = 1，PRIM_RX = 1）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_Rx(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x03;									//配置寄存器位1（PWR_UP）和位0（PRIM_RX）都置1
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
	
	/*CE置1，进入收发模式，因为PRIM_RX为1，所以进入接收模式*/
	NRF24L01_W_CE(1);
}

/**
  * 函    数：NRF24L01进入发送模式（CE = 1，PWR_UP = 1，PRIM_RX = 0）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_Tx(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x02;									//配置寄存器位1（PWR_UP）置1
	Config &= ~0x01;								//配置寄存器位0（PRIM_RX）置0
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
	
	/*CE置1，进入收发模式，因为PRIM_RX为0，所以进入发送模式*/
	NRF24L01_W_CE(1);
}

/**
  * 函    数：NRF24L01初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用前，需要调用此初始化函数
  */
void NRF24L01_Init(void)
{
	/*先调用底层的端口初始化*/
	NRF24L01_GPIO_Init();
	
	/*初始化配置一系列寄存器，寄存器值的意义需参考手册中的寄存器描述*/
	/*以下配置通信双方必须保持一致，否则无法进行通信*/
	/*【知识点·无线通信可靠性"三件套"】①CRC校验：每包附带校验码，收方算出的校验码不一致就丢弃错包；*/
	/*②EN_AA自动应答：接收方收到正确包后，硬件自动回发一个ACK（Acknowledgement，应答）小包，*/
	/*  全程不需要用户写一行代码；③SETUP_RETR自动重传：发送方在设定时间内没收到ACK就自动重发，*/
	/*  直到收到ACK或达到最大重发次数。配合相同的频点、地址、地址宽度、速率和包长，两块模块才能可靠互通。*/
	/*0x08 = 0b00001000：仅EN_CRC位为1（使能1字节CRC），PWR_UP=0先不上电、PRIM_RX=0，后面NRF24L01_Rx()再统一切到接收*/
	NRF24L01_WriteReg(NRF24L01_CONFIG, 0x08);		//配置寄存器，不屏蔽中断，使能CRC，CRC为1字节，PWR_UP = 0，PRIM_RX = 0
	NRF24L01_WriteReg(NRF24L01_EN_AA, 0x3F);		//使能自动应答，开启接收通道0~通道5的自动应答
	NRF24L01_WriteReg(NRF24L01_EN_RXADDR, 0x01);	//使能接收通道，只开启接收通道0
	NRF24L01_WriteReg(NRF24L01_SETUP_AW, 0x03);		//设置地址宽度，地址宽度为5字节
	NRF24L01_WriteReg(NRF24L01_SETUP_RETR, 0x03);	//设置自动重传，间隔250us，重传3次
	NRF24L01_WriteReg(NRF24L01_RF_CH, 0x02);		//射频通道，频率为(2400 + 2)MHz = 2.402GHz
	NRF24L01_WriteReg(NRF24L01_RF_SETUP, 0x0E);		//射频设置，通信速率为2Mbps，发射功率为0dBm
	
	/*接收通道0的数据包宽度，设置为宏定义NRF24L01_RX_PACKET_WIDTH指定的值*/
	NRF24L01_WriteReg(NRF24L01_RX_PW_P0, NRF24L01_RX_PACKET_WIDTH);
	
	/*接收通道0地址，设置为全局数组NRF24L01_RxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
	
	/*清空Tx FIFO的所有数据*/
	NRF24L01_FlushTx();
	
	/*清空Rx FIFO的所有数据*/
	NRF24L01_FlushRx();
	
	/*给状态寄存器的位4（MAX_RT）、位5（TX_DS）和位6（RX_DR）写1，清标志位*/
	NRF24L01_WriteReg(NRF24L01_STATUS, 0x70);
	
	/*初始化配置完成，芯片默认进入接收模式*/
	NRF24L01_Rx();
}

/**
  * 函    数：NRF24L01发送数据包
  * 参    数：无
  * 返 回 值：发送标志位，方便用户了解发送状态
  * 			1：发送成功，无错误
  * 			2：达到了最大重发次数仍未收到应答，可能是收发双方配置不一致、接收方不存在、接收FIFO已满或者多个发送数据包碰撞
  * 			3：状态寄存器的值不合法，可能是设备不存在、断路、短路或者引脚配置不正确
  * 			4：发送超时，可能是设备未初始化、断路、短路或者引脚配置不正确
  * 说    明：调用此函数前，直接修改全局数组NRF24L01_TxAddress和NRF24L01_TxPacket来设置发送的地址和数据
  */
uint8_t NRF24L01_Send(void)
{
	uint8_t Status;
	uint8_t SendFlag;
	uint32_t Timeout;
	
	/*【发送完整流程】①写发送目标地址 → ②把接收通道0地址临时改成同一地址（用来接收对方回的ACK）*/
	/*→ ③把NRF24L01_TxPacket的32字节写入发送FIFO → ④切换到TX模式，CE拉高启动发射 */
	/*→ ⑤不断查询STATUS寄存器：出现TX_DS=成功，MAX_RT=重发耗尽，都没有则等到超时 */
	/*→ ⑥清标志、冲刷TX FIFO、恢复RX通道0原地址、切回RX模式。发送期间是"独占"的，主循环每100ms才调一次。*/
	/*遥控器实际只关心32字节中的前6字节[Mode,LH,LV,RH,RV,KEY]，其余字节内容不影响通信（包长固定32，双方一致）。*/
	/*发送地址，设置为全局数组NRF24L01_TxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_TX_ADDR, NRF24L01_TxAddress, 5);
	
	/*接收通道0地址，此处必须也设置为发送地址，用于接收应答*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_TxAddress, 5);
	
	/*写发送有效载荷，写入全局数组NRF24L01_TxPacket指定的数据，数据宽度为NRF24L01_TX_PACKET_WIDTH*/
	NRF24L01_WriteTxPayload(NRF24L01_TxPacket, NRF24L01_TX_PACKET_WIDTH);
	
	/*发送的地址和有效载荷写入完成，进入发送模式，开始发送数据*/
	NRF24L01_Tx();
	
	/*指定超时时间，即循环读取状态寄存器的次数，具体值可以实测确定*/
	Timeout = 10000;
	
	/*循环读取状态寄存器*/
	while (1)
	{
		/*读取状态寄存器，保存至Status变量*/
		Status = NRF24L01_ReadStatus();
		
		/*超时计次*/
		Timeout --;
		if (Timeout == 0)			//如果计次减至0
		{
			SendFlag = 4;			//发送超时，置标志位为4
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		
		/*根据状态寄存器的值，判断发送状态*/
		/*用"按位与0x30"只保留bit4/bit5两个关心的标志位，其余位不影响判断。*/
		/*main.c的CalculateSuccessRatio()正是依据本函数返回1的次数统计最近10次成功率，作为OLED上的信号强度Sig。*/
		if ((Status & 0x30) == 0x30)		//状态寄存器位4（MAX_RT）和位5（TX_DS）同时为1
		{
			SendFlag = 3;			//状态寄存器的值不合法，置标志位为3
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		else if ((Status & 0x10) == 0x10)	//状态寄存器位4（MAX_RT）为1
		{
			SendFlag = 2;			//达到了最大重发次数仍未收到应答，置标志位为2
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		else if ((Status & 0x20) == 0x20)	//状态寄存器位5（TX_DS）为1
		{
			SendFlag = 1;			//发送成功，无错误，置标志位为1
			break;					//跳出循环
		}
	}
	
	/*给状态寄存器的位4（MAX_RT）和位5（TX_DS）写1，清标志位*/
	NRF24L01_WriteReg(NRF24L01_STATUS, 0x30);
	
	/*清空Tx FIFO的所有数据*/
	NRF24L01_FlushTx();
	
	/*发送完成后，恢复接收通道0原来的地址*/
	/*如果发送地址和接收通道0地址设置相同，则可不执行这一句*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
	
	/*发送完成，芯片恢复为接收模式*/
	NRF24L01_Rx();
		
	/*返回发送标志位*/
	return SendFlag;
}

/**
  * 函    数：NRF24L01接收数据包
  * 参    数：无
  * 返 回 值：接收标志位，方便用户了解接收状态
  * 			0：未接收到数据包
  * 			1：成功接收到一个数据包
  * 			2：状态寄存器的值不合法，可能是设备不存在、断路、短路或者引脚配置不正确
  * 			3：设备仍处于掉电模式，可能是设备未初始化、曾经断电过、断路、短路或者引脚配置不正确
  * 说    明：如果收到了数据包，则可直接从全局数组NRF24L01_RxPacket取数据
  */
uint8_t NRF24L01_Receive(void)
{
	uint8_t Status, Config;
	uint8_t ReceiveFlag;
	
	/*【接收流程】芯片平时（初始化后及每次Send结束后）都停在RX模式持续监听空中信号；*/
	/*一旦收到地址匹配且CRC正确的数据包，硬件就把它放进接收FIFO，并把STATUS的RX_DR(bit6)置1。*/
	/*本函数查询到RX_DR后：读出32字节到全局数组NRF24L01_RxPacket → 写1清除RX_DR → 冲刷RX FIFO。*/
	/*Mode=1时main.c每轮主循环都调用本函数，并按字节布局解析小车状态：*/
	/*[0]=0x02标识、[1]PWML、[2]PWMR、[4..7]/[8..11]/[12..15]为三个float(Angle/SpeedL/SpeedR)，*/
	/*其中float的还原方式是 *(float*)&RxPacket[4]：把连续4字节直接"重新解释"为一个32位浮点数。*/
	/*读取状态寄存器，保存至Status变量*/
	Status = NRF24L01_ReadStatus();
	
	/*读取配置寄存器，保存至Config变量*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
	
	/*根据配置寄存器和状态寄存器的值，判断接收状态*/
	if ((Config & 0x02) == 0x00)		//配置寄存器位1（PWR_UP）为0
	{
		ReceiveFlag = 3;				//设备仍处于掉电模式，置标志位为3
		NRF24L01_Init();				//接收出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
	}
	else if ((Status & 0x30) == 0x30)	//状态寄存器位4（MAX_RT）和位5（TX_DS）同时为1
	{
		ReceiveFlag = 2;				//状态寄存器的值不合法，置标志位为2
		NRF24L01_Init();				//接收出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
	}
	else if ((Status & 0x40) == 0x40)	//状态寄存器位6（RX_DR）为1
	{
		ReceiveFlag = 1;				//接收到数据，置标志位为1
		
		/*读接收有效载荷，存放在全局数组NRF24L01_RxPacket中，数据宽度为NRF24L01_RX_PACKET_WIDTH*/
		NRF24L01_ReadRxPayload(NRF24L01_RxPacket, NRF24L01_RX_PACKET_WIDTH);
		
		/*给状态寄存器的位6（RX_DR）写1，清标志位*/
		NRF24L01_WriteReg(NRF24L01_STATUS, 0x40);

		/*清空Rx FIFO的所有数据*/
		NRF24L01_FlushRx();
	}
	else
	{
		ReceiveFlag = 0;				//未接收到数据，置标志位为0
	}
	
	/*返回接收标志位*/
	return ReceiveFlag;
}

/**
  * 函    数：NRF24L01更新接收地址
  * 参    数：无
  * 返 回 值：无
  * 说    明：如果想在运行时动态修改接收地址，则可先向全局数组NRF24L01_RxAddress写入修改的地址
  * 		  然后再调用此函数，使修改的接收地址生效
  */
void NRF24L01_UpdateRxAddress(void)
{
	/*接收通道0地址，设置为全局数组NRF24L01_RxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
}

/*********************功能函数*/


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
