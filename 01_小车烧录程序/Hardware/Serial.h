/*
* 文件名称：Serial.h
* 模块名称：USART1调试串口驱动（头文件）
* 功能简介：对外声明串口初始化、多种格式发送、中断接收查询等函数；
*           包含stdio.h后，包含本头文件的地方都能直接使用printf
*/
#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdio.h>

void Serial_Init(void);							//初始化USART1（PA9发/PA10收，9600-8N1，接收中断）
void Serial_SendByte(uint8_t Byte);				//发送1个字节
void Serial_SendArray(uint8_t *Array, uint16_t Length);	//发送Length个字节的数组
void Serial_SendString(char *String);			//发送一个以'\0'结尾的字符串
void Serial_SendNumber(uint32_t Number, uint8_t Length);	//把数字按Length位十进制文本发出（前面补0）
void Serial_Printf(char *format, ...);			//printf风格发送，用法同printf（结果不超过100字节）

uint8_t Serial_GetRxFlag(void);					//查询是否收到新字节（1=有，查询后标志自动清零）
uint8_t Serial_GetRxData(void);					//读取最近一次收到的字节

#endif
