/***************************************************************************************
  * 模块名称：			NRF24L01.h（NRF24L01无线驱动头文件，遥控器端工程）
  * 模块功能：			对外声明4个共享全局数组和全部驱动函数；包含本头文件即可使用无线功能
  * 典型用法（main.c）：	NRF24L01_Init()初始化一次 → 每100ms填好NRF24L01_TxPacket后调
  * 						NRF24L01_Send()发送 → 需要回传时调NRF24L01_Receive()，
  * 						从NRF24L01_RxPacket数组取小车状态数据
  ***************************************************************************************
  */

#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "NRF24L01_Define.h"

/*外部可调用全局数组***********/

/*发送地址（5字节"门牌号"，需与小车接收地址一致）和发送数据包（32字节，遥控器用前6字节）*/
/*extern关键字：声明"这两个数组在NRF24L01.c中定义"，本头文件只做引用声明，不重新分配内存*/
extern uint8_t NRF24L01_TxAddress[];
extern uint8_t NRF24L01_TxPacket[];

/*接收通道0地址（5字节，本工程收发地址相同）和接收数据包缓冲区（32字节，回传时前部16字节有效）*/
extern uint8_t NRF24L01_RxAddress[];
extern uint8_t NRF24L01_RxPacket[];

/***********外部可调用全局数组*/


/*函数声明*********************/

/*指令实现*/
uint8_t NRF24L01_ReadReg(uint8_t RegAddress);
void NRF24L01_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count);
void NRF24L01_WriteReg(uint8_t RegAddress, uint8_t Data);
void NRF24L01_WriteRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count);
void NRF24L01_ReadRxPayload(uint8_t *DataArray, uint8_t Count);
void NRF24L01_WriteTxPayload(uint8_t *DataArray, uint8_t Count);
void NRF24L01_FlushTx(void);
void NRF24L01_FlushRx(void);
uint8_t NRF24L01_ReadStatus(void);

/*功能函数*/
void NRF24L01_PowerDown(void);
void NRF24L01_StandbyI(void);
void NRF24L01_Rx(void);
void NRF24L01_Tx(void);

void NRF24L01_Init(void);
uint8_t NRF24L01_Send(void);
uint8_t NRF24L01_Receive(void);
void NRF24L01_UpdateRxAddress(void);

/*********************函数声明*/


#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
