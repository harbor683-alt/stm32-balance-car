/***************************************************************************************
  * 文件名称：BlueSerial.h
  * 功    能：蓝牙串口（USART2）驱动头文件，对外暴露初始化、收发、环形缓冲区操作
  *           以及文本帧解析接口；并声明两个供main.c直接读取的解析结果全局数组。
  * 应用层典型流程（见main.c蓝牙模式）：
  *           BlueSerial_Init()初始化；
  *           主循环中 if(BlueSerial_ReceiveFlag()) { BlueSerial_Receive();
  *               用strcmp(BlueSerial_StringArray[0], "joystick"/"key")判断命令；
  *               用atoi(BlueSerial_StringArray[n])把数字字段转成整数 }
  * 说    明：包含string.h/stdlib.h是为了在main.c中使用strcmp、atoi等字符串函数。
  ***************************************************************************************
  */
#ifndef __BLUE_SERIAL_H
#define __BLUE_SERIAL_H

#include <string.h>
#include <stdlib.h>

/*解析结果（在BlueSerial.c中定义）：
  String      存放一帧方括号内的原始内容（未拆分）；
  StringArray 按逗号拆分后的字段，最多6个字段、每个最多19个字符+'\0'*/
extern char BlueSerial_String[];
extern char BlueSerial_StringArray[][20];


/*初始化与发送*/
void BlueSerial_Init(void);								//初始化USART2(9600,8N1)并开启接收中断
void BlueSerial_SendByte(uint8_t Byte);					//阻塞发送1字节
void BlueSerial_SendArray(uint8_t *Array, uint16_t Length);	//发送Length个字节
void BlueSerial_SendString(char *String);				//发送以'\0'结尾的字符串

/*环形缓冲区操作（一般由驱动内部使用，也可手动存取）*/
uint8_t BlueSerial_Put(uint8_t Byte);					//入队1字节，返回0成功/1已满
uint8_t BlueSerial_Get(uint8_t *Byte);					//出队1字节，返回0成功/1已空
uint16_t BlueSerial_Length(void);						//查询缓冲区中有效字节数
void BlueSerial_ClearBuffer(void);						//清空缓冲区

/*应用层帧解析：先问有没有完整一帧，再解析*/
uint8_t BlueSerial_ReceiveFlag(void);					//返回1表示已收到完整[...]帧
void BlueSerial_Receive(void);							//取走并解析一帧到String/StringArray


#endif
