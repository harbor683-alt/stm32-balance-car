/***************************************************************************************
  * 模块名称：MPU6050 六轴姿态传感器驱动（基于软件I2C）
  * 功能简介：通过MyI2C软件模拟I2C总线，配置并读取MPU6050内部寄存器，获得：
  *             - 三轴加速度计（AccX/AccY/AccZ）：感知线加速度与重力方向，单位g；
  *             - 三轴陀螺仪（GyroX/GyroY/GyroZ）：感知绕三轴的转动角速度，
  *               单位dps（degree per second，度/秒）；
  *             - 芯片内部还有一路温度传感器，本工程未使用。
  *           六轴数据合在一起可以解算出小车车身的倾斜角度（姿态）。
  * 硬件接线（以本工程代码为准，实际以原理图为准）：
  *           MPU6050的SCL -> PB10，SDA -> PB11（详见MyI2C.c），VCC/GND正常供电；
  *           AD0引脚接地时从机地址为0xD0（本工程即此值），接高则为0xD2。
  * 通信协议要点（I2C寄存器读写套路）：
  *           1. 从机地址共8位：高7位是设备编号，最低位R/W#决定方向：
  *              0xD0 = 写（bit0=0），0xD1 = 0xD0|0x01 = 读（bit0=1）；
  *           2. 写一个寄存器：起始 -> 发设备地址W+等ACK -> 发寄存器号+等ACK
  *              -> 发数据+等ACK -> 终止；
  *           3. 读寄存器要“先写后读”：先写设备地址W告诉芯片要读哪个寄存器号，
  *              再发一个“重复起始(Repeated Start)”，改发设备地址R，随后收数据；
  *           4. 连续多读：每收完1字节主机回ACK(0)表示继续，最后1字节回NACK(1)
  *              通知从机结束；MPU6050支持地址自增，故可一次连读14个寄存器。
  * 量程与分辨率（本工程初始化值）：
  *           加速度计 ±16g量程，灵敏度2048 LSB/g（原始值÷2048=多少个g）；
  *           陀螺仪   ±2000dps量程，灵敏度16.4 LSB/(°/s)（原始值÷16.4=度/秒）。
  * 在平衡车中的作用：
  *           主控制循环每隔10ms调用MPU6050_GetData读取一次原始数据：
  *           用加速度计的重力方向算静态倾角，用陀螺仪角速度做短时积分并与加速度
  *           角度互补滤波，融合出可靠的车身角度Angle，供直立环/速度环PID使用。
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include "MPU6050_Reg.h"

/*MPU6050的I2C从机地址（8位写法，最低位0=写）；读时需与0x01相或得到0xD1。
  该地址由芯片AD0引脚电平决定：AD0=0对应0xD0，AD0=1对应0xD2*/
#define MPU6050_ADDRESS		0xD0

/**
  * 函    数：MPU6050_WriteReg
  * 功    能：向MPU6050指定寄存器写入1字节（配置芯片时使用）
  * 参    数：RegAddress 寄存器地址（取值见MPU6050_Reg.h中的宏）
  * 参    数：Data 要写入的1字节数据
  * 返 回 值：无
  * 说    明：完整写时序 = 起始 + 设备地址(W) + 寄存器号 + 数据 + 终止，
  *           每个字节后都读一次从机应答（这里不检查应答结果）
  */
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	MyI2C_Start();						//起始信号S，占用总线
	MyI2C_SendByte(MPU6050_ADDRESS);	//发送设备地址+写方向（0xD0，R/W#=0）
	MyI2C_ReceiveAck();					//等待并读取从机应答ACK
	MyI2C_SendByte(RegAddress);			//发送要写入的寄存器地址（寄存器指针定位）
	MyI2C_ReceiveAck();					//应答
	MyI2C_SendByte(Data);				//发送要写入的数据，芯片自动存入该寄存器
	MyI2C_ReceiveAck();					//应答
	MyI2C_Stop();						//终止信号P，结束本次通信
}

/**
  * 函    数：MPU6050_ReadReg
  * 功    能：从MPU6050指定寄存器读取1字节（如读WHO_AM_I、单个配置寄存器）
  * 参    数：RegAddress 寄存器地址（取值见MPU6050_Reg.h中的宏）
  * 返 回 值：读到的1字节寄存器数据
  * 说    明：I2C读寄存器分两阶段：
  *           阶段一（伪写）：起始+设备地址W+寄存器号，只用于把芯片内部寄存器指针
  *                   指到目标位置，不读数据；
  *           阶段二（真读）：重复起始+设备地址R(0xD1)，随后收1字节数据；
  *           只读1字节，读完后主机回NACK(1)表示不再继续，最后终止。
  *           “重复起始”即不发Stop直接再发一次Start，可保证指针对准后方向不丢
  */
uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;

	/*阶段一：用写方向把要读的寄存器地址告诉芯片*/
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);	//设备地址+写0xD0
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);		//指定起始寄存器号
	MyI2C_ReceiveAck();

	/*阶段二：重复起始，改用读方向，读取1字节*/
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);	//地址最低位置1 -> 0xD1，表示读
	MyI2C_ReceiveAck();
	Data = MyI2C_ReceiveByte();		//从SDA收1字节
	MyI2C_SendAck(1);				//只收1个字节即结束，回NACK(1)通知从机停止输出
	MyI2C_Stop();

	return Data;					//返回读到的寄存器值
}

/**
  * 函    数：MPU6050_ReadRegs
  * 功    能：从MPU6050指定寄存器开始，连续读取多个字节（利用寄存器地址自增）
  * 参    数：RegAddress 起始寄存器地址
  * 参    数：DataArray 输出参数，调用者提供的数组首地址，读到的数据依次存入
  * 参    数：Count 要连续读取的字节数
  * 返 回 值：无
  * 说    明：MPU6050规定：主机每回一个ACK，内部寄存器指针就自动加1，故一次
  *           I2C事务就能把相邻寄存器批量搬出来，比逐字节读效率高很多；
  *           前Count-1字节回ACK(0)表示“继续”，最后1字节回NACK(1)表示“结束”。
  *           本工程用它一次连读14字节（加速度6+温度2+陀螺仪6）
  */
void MPU6050_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count)
{
	/*阶段一：伪写，定位起始寄存器*/
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();

	/*阶段二：重复起始，改读方向，连续接收Count个字节（地址自动递增）*/
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);
	MyI2C_ReceiveAck();
	for (uint8_t i = 0; i < Count; i ++)
	{
		DataArray[i] = MyI2C_ReceiveByte();	//依次收1字节存入数组
		if (i < Count - 1)					//不是最后一个字节
		{
			MyI2C_SendAck(0);				//回ACK(0)：我还要，地址指针继续自增
		}
		else								//已经是最后一个字节
		{
			MyI2C_SendAck(1);				//回NACK(1)：到此为止，通知从机释放SDA
		}
	}
	MyI2C_Stop();							//终止信号
}

/**
  * 函    数：MPU6050_Init
  * 功    能：MPU6050上电初始化：先初始化软件I2C引脚，再配置6个关键寄存器
  * 参    数：无
  * 返 回 值：无
  * 说    明：MPU6050上电默认处于睡眠模式，必须先唤醒并设置时钟源，否则读不到数据。
  *           各寄存器配置值逐行解释如下（寄存器地址见MPU6050_Reg.h）
  */
void MPU6050_Init(void)
{
	MyI2C_Init();										//先初始化底层I2C引脚PB10/PB11

	/*电源管理1 = 0x01：bit6(睡眠位)=0唤醒芯片；低3位CLKSEL=001，
	  选择X轴陀螺仪输出作为时钟源（比内部RC时钟更稳定，是官方推荐用法）*/
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
	/*电源管理2 = 0x00：6个轴的待机位全为0，加速度X/Y/Z、陀螺仪X/Y/Z全部正常工作*/
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
	/*采样率分频 = 0x07（即分频系数7+1=8）：CONFIG=0时陀螺仪输出率8kHz，
	  采样率 = 8000/(1+7) = 1000Hz，即每1ms可采一组数据*/
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x07);
	/*配置寄存器 = 0x00：关闭外部帧同步，数字低通滤波器DLPF取最宽带宽档位*/
	MPU6050_WriteReg(MPU6050_CONFIG, 0x00);
	/*陀螺仪配置 = 0x18（二进制0001 1000）：量程选择位FS_SEL=11b(3)，
	  对应满量程±2000dps（度/秒），原始值16.4个LSB对应1dps*/
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);
	/*加速度计配置 = 0x18（二进制0001 1000）：量程选择位AFS_SEL=11b(3)，
	  对应满量程±16g，原始值2048个LSB对应1g*/
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);
}

/**
  * 函    数：MPU6050_GetID
  * 功    能：读取WHO_AM_I（我是谁）身份寄存器，用于判断芯片是否在线、接线是否正常
  * 参    数：无
  * 返 回 值：芯片ID号，MPU6050固定返回0x68
  * 说    明：上电后可调一次，若返回0x68说明I2C通信成功；若返回0x00或0xFF，
  *           通常是接线、地址或初始化有问题（本工程主流程未强制检查）
  */
uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);	//读0x75寄存器
}


/**
  * 函    数：MPU6050_GetData
  * 功    能：一次性读出三轴加速度、三轴陀螺仪共6个16位有符号原始数据
  * 参    数：AccX/AccY/AccZ 输出参数，三轴加速度原始值指针（int16有符号）
  * 参    数：GyroX/GyroY/GyroZ 输出参数，三轴陀螺仪原始值指针（int16有符号）
  * 返 回 值：无
  * 说    明：MPU6050每个轴都是“高8位寄存器+低8位寄存器”两个字节，
  *           且为有符号补码（最高位是符号位），故合成后必须用int16_t接收。
  *           从0x3B开始连续14字节的布局如下（中间Data[6]、Data[7]是温度，本工程跳过）：
  *
  *           下标  寄存器        含义          合成方式
  *           [0,1] 0x3B/0x3C  加速度X高/低字节  AccX  = (D0<<8)|D1
  *           [2,3] 0x3D/0x3E  加速度Y高/低字节  AccY  = (D2<<8)|D3
  *           [4,5] 0x3F/0x40  加速度Z高/低字节  AccZ  = (D4<<8)|D5
  *           [6,7] 0x41/0x42  温度高/低字节     （未使用）
  *           [8,9] 0x43/0x44  陀螺仪X高/低字节  GyroX = (D8<<8)|D9
  *           [10,11]0x45/0x46 陀螺仪Y高/低字节  GyroY = (D10<<8)|D11
  *           [12,13]0x47/0x48 陀螺仪Z高/低字节  GyroZ = (D12<<8)|D13
  *
  *           物理量换算（本工程±16g、±2000dps）：
  *           加速度(g) = 原始值 / 2048.0； 角速度(dps) = 原始值 / 16.4
  *           main.c中每10ms调用一次：GyroY用于积分角度增量，AccX/AccZ用于atan2
  *           求加速度倾角，二者互补滤波得到车身角度
  */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
						int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
	uint8_t Data[14];										//存放连续读出的14字节
	MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, Data, 14);		//从0x3B起连读14字节

	*AccX = (Data[0] << 8) | Data[1];	//加速度X：高8位左移8位，再或上低8位拼成16位

	*AccY = (Data[2] << 8) | Data[3];	//加速度Y

	*AccZ = (Data[4] << 8) | Data[5];	//加速度Z

	*GyroX = (Data[8] << 8) | Data[9];	//陀螺仪X（Data[6][7]是温度，这里跳过）

	*GyroY = (Data[10] << 8) | Data[11];	//陀螺仪Y（平衡车俯仰角速度，直立控制的关键量）

	*GyroZ = (Data[12] << 8) | Data[13];	//陀螺仪Z
}
