/***************************************************************************************
  * 文件名称：NRF24L01.h
  * 功    能：NRF24L01 2.4G无线收发驱动的头文件，对外声明全局数据包数组与全部接口。
  * 典型用法（本工程平衡车）：
  *           1. 上电调用一次 NRF24L01_Init()（默认进入接收模式PRX）；
  *           2. 主循环反复调用 NRF24L01_Receive()：返回1表示收到一包，从
  *              NRF24L01_RxPacket[]解析摇杆/按键；
  *           3. 需要回传时把数据填入 NRF24L01_TxPacket[]，调用 NRF24L01_Send()，
  *              根据返回值判断成功/重发耗尽/设备异常。
  * 说    明：寄存器指令码与地址宏统一放在 NRF24L01_Define.h 中。
  ***************************************************************************************
  */
#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "NRF24L01_Define.h"

/*外部可调用全局数组***********/

/*以下数组定义在NRF24L01.c中，extern声明后应用程序可直接读写：
  两个Address：5字节收/发地址（门牌号）；两个Packet：32字节收/发数据（载荷）*/
extern uint8_t NRF24L01_TxAddress[];	//发送地址（PTX发包的目标地址，5字节）
extern uint8_t NRF24L01_TxPacket[];		//发送数据包（调用NRF24L01_Send前填充，32字节）

extern uint8_t NRF24L01_RxAddress[];	//接收管道0地址（PRX监听的地址，5字节）
extern uint8_t NRF24L01_RxPacket[];		//接收数据包（NRF24L01_Receive返回1后读取，32字节）

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
/*补充：应用层最常用的是 Init / Send / Receive / UpdateRxAddress；
  PowerDown/StandbyI/Rx/Tx 为手动状态切换，一般由Init和Send内部自动调用*/
void NRF24L01_PowerDown(void);	//进入掉电模式（CE=0, PWR_UP=0），最省电
void NRF24L01_StandbyI(void);	//进入待机模式1（CE=0, PWR_UP=1），可快速启动收发
void NRF24L01_Rx(void);			//进入接收模式PRX（CE=1, PWR_UP=1, PRIM_RX=1）
void NRF24L01_Tx(void);			//进入发送模式PTX（CE=1, PWR_UP=1, PRIM_RX=0）

void NRF24L01_Init(void);		//初始化GPIO并配置全套寄存器，结束后默认处于PRX接收
uint8_t NRF24L01_Send(void);	//发送TxPacket一包；返回1成功，2重发耗尽，3状态非法，4超时
uint8_t NRF24L01_Receive(void);	//查询是否收到一包；返回1收到(数据在RxPacket)，0无包，2/3异常
void NRF24L01_UpdateRxAddress(void);	//修改RxAddress后调用，使新的接收管道0地址生效

/*********************函数声明*/


#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
