/***************************************************************************************
  * 文件名称：MyI2C.h
  * 功    能：软件模拟I2C底层驱动的头文件，对外声明6个基本时序函数。
  *           上层模块（本工程中是MPU6050.c）包含本头文件后，即可像“搭积木”一样
  *           用 起始+发地址+发寄存器+发数据+终止 等组合，完成对I2C从机的读写。
  * 调用层次：MPU6050.c（应用层：读姿态数据）
  *             └── MyI2C.c（本层：I2C时序）
  *                   └── GPIO（硬件层：PB10=SCL，PB11=SDA）
  * 说    明：#ifndef/#define/#endif 是头文件保护宏，防止同一文件被重复包含时
  *           出现函数/变量重复声明的编译错误
  ***************************************************************************************
  */
#ifndef __MYI2C_H
#define __MYI2C_H

void MyI2C_Init(void);				//初始化SCL(PB10)、SDA(PB11)为开漏输出并释放总线
void MyI2C_Start(void);				//产生I2C起始信号（SCL高时SDA下降沿）
void MyI2C_Stop(void);				//产生I2C终止信号（SCL高时SDA上升沿）
void MyI2C_SendByte(uint8_t Byte);	//主机发送1字节，高位先行，参数：待发字节
uint8_t MyI2C_ReceiveByte(void);	//主机接收1字节，高位先行，返回值：收到的字节
void MyI2C_SendAck(uint8_t AckBit);	//主机发应答位，参数：0=ACK继续，1=NACK结束
uint8_t MyI2C_ReceiveAck(void);		//主机收应答位，返回值：0=从机ACK，1=NACK

#endif
