/***************************************************************************************
  * 文件名称：NRF24L01_Define.h
  * 功    能：NRF24L01的SPI“指令码”与“寄存器地址”宏定义总表。
  *           主控通过SPI访问芯片的方式是：CSN拉低后，先交换1字节“指令”，告诉芯片
  *           这次要做什么（读/写寄存器、读写FIFO载荷、清空FIFO等），再交换后续数据。
  * 指令编码规则（结合下表理解）：
  *           R_REGISTER=0x00、W_REGISTER=0x20 这一类指令的高3位固定、低5位填
  *           “寄存器地址”，所以代码中写作 NRF24L01_R_REGISTER | RegAddress；
  *           其余如0x61/0xA0/0xE1等是单字节独立指令，后面不再跟寄存器地址。
  * 重要约定：本表宏值全部来自芯片数据手册，严禁修改；本文件只做名字到数值的映射。
  ***************************************************************************************
  */
#ifndef __NRF24L01_DEFINE_H
#define __NRF24L01_DEFINE_H

/*==================== 一、SPI指令码（CSN拉低后交换的第一个字节） ====================*/
/*NRF24L01指令宏定义*/
#define NRF24L01_R_REGISTER			0x00	//读寄存器，高3位为指令码，低5位为寄存器地址，后续跟1~5字节读数据
#define NRF24L01_W_REGISTER			0x20	//写寄存器，高3位为指令码，低5位为寄存器地址，后续跟1~5字节写数据
#define NRF24L01_R_RX_PAYLOAD		0x61	//读Rx有效载荷，后续跟1~32字节读数据
#define NRF24L01_W_TX_PAYLOAD		0xA0	//写Tx有效载荷，后续跟1~32字节写数据
#define NRF24L01_FLUSH_TX			0xE1	//清空Tx FIFO所有数据，单独指令
#define NRF24L01_FLUSH_RX			0xE2	//清空Rx FIFO所有数据，单独指令
#define NRF24L01_REUSE_TX_PL		0xE3	//重新使用最后一次发送的有效载荷，单独指令
#define NRF24L01_R_RX_PL_WID		0x60	//读取Rx FIFO最前面一个数据包的宽度，后续跟1字节读数据，仅适用于动态包长模式
#define NRF24L01_W_ACK_PAYLOAD		0xA8	//写应答附带的有效载荷，高5位为指令码，低3位为通道号，后续跟1~32字节写数据，仅适用于应答附带载荷模式
#define NRF24L01_W_TX_PAYLOAD_NOACK	0xB0	//写Tx有效载荷，不要求应答，后续跟1~32字节写数据，仅适用于不要求应答模式
#define NRF24L01_NOP				0xFF	//空操作，单独指令，可以用读取状态寄存器

/*==================== 二、片内寄存器地址（配合R/W_REGISTER指令访问） ====================*/
/*【本工程NRF24L01_Init中典型配置值速查（收发双方必须一致）】
  CONFIG      = 0x08  EN_CRC=1启用CRC、CRCO=0单字节CRC、PWR_UP=0先上电不启动、PRIM_RX=0
  EN_AA       = 0x3F  管道0~5全部开启“自动应答ACK”
  EN_RXADDR   = 0x01  只使能接收管道0
  SETUP_AW    = 0x03  AW=11 地址宽度5字节（01=3字节，10=4字节，11=5字节）
  SETUP_RETR  = 0x03  ARD=0000自动重发等待250us；ARC=0011最多重发3次
  RF_CH       = 0x02  射频频点 2400+2 = 2402MHz
  RF_SETUP    = 0x0E  RF_DR_HIGH=1速率2Mbps；RF_PWR=11发射功率0dBm(最大档之一)
  RX_PW_P0    = 32    管道0定长载荷32字节
  STATUS写0x70       向bit6 RX_DR / bit5 TX_DS / bit4 MAX_RT 写1，清除三个中断标志
  另外：STATUS(0x07)是“状态寄存器”——bit6=收到数据RX_DR，bit5=发送成功TX_DS，
        bit4=达到最大重发MAX_RT；这3位都是写1清零(W1C)。*/
/*NRF24L01寄存器地址宏定义*/
/*---- 基础配置/状态类寄存器（均为1字节） ----*/
#define NRF24L01_CONFIG				0x00	//配置寄存器，1字节
#define NRF24L01_EN_AA				0x01	//使能自动应答，1字节
#define NRF24L01_EN_RXADDR			0x02	//使能接收通道，1字节
#define NRF24L01_SETUP_AW			0x03	//设置地址宽度，1字节
#define NRF24L01_SETUP_RETR			0x04	//设置自动重传，1字节
#define NRF24L01_RF_CH				0x05	//射频通道，1字节
#define NRF24L01_RF_SETUP			0x06	//射频相关参数设置，1字节
#define NRF24L01_STATUS				0x07	//状态寄存器，1字节
#define NRF24L01_OBSERVE_TX			0x08	//发送观察寄存器，1字节
#define NRF24L01_RPD				0x09	//接收功率检测，1字节
/*---- 地址类寄存器（“门牌号”，决定谁能与谁通信） ----
  管道0/1及发送地址为完整多字节；管道2~5只有1字节低位地址，高位自动复用管道1*/
#define NRF24L01_RX_ADDR_P0			0x0A	//接收通道0地址，5字节
#define NRF24L01_RX_ADDR_P1			0x0B	//接收通道1地址，5字节
#define NRF24L01_RX_ADDR_P2			0x0C	//接收通道2地址，1字节，高位地址与接收通道1相同
#define NRF24L01_RX_ADDR_P3			0x0D	//接收通道3地址，1字节，高位地址与接收通道1相同
#define NRF24L01_RX_ADDR_P4			0x0E	//接收通道4地址，1字节，高位地址与接收通道1相同
#define NRF24L01_RX_ADDR_P5			0x0F	//接收通道5地址，1字节，高位地址与接收通道1相同
/*补充：PTX发包目标地址；开启ACK时也用它接收自动应答*/
#define NRF24L01_TX_ADDR			0x10	//发送地址，5字节
/*---- 各接收管道的定长有效载荷宽度（单位：字节，合法范围0~32） ----*/
#define NRF24L01_RX_PW_P0			0x11	//接收通道0有效载荷数据宽度，1字节
#define NRF24L01_RX_PW_P1			0x12	//接收通道1有效载荷的数据宽度，1字节
#define NRF24L01_RX_PW_P2			0x13	//接收通道2有效载荷的数据宽度，1字节
#define NRF24L01_RX_PW_P3			0x14	//接收通道3有效载荷的数据宽度，1字节
#define NRF24L01_RX_PW_P4			0x15	//接收通道4有效载荷的数据宽度，1字节
#define NRF24L01_RX_PW_P5			0x16	//接收通道5有效载荷的数据宽度，1字节
/*---- FIFO状态与高级功能（本工程未使用动态包长等高级特性） ----*/
#define NRF24L01_FIFO_STATUS		0x17	//发送和接收FIFO状态，1字节
#define NRF24L01_DYNPD				0x1C	//使能接收通道的动态包长模式，1字节
/*补充：动态载荷/ACK带载荷/NOACK等高级功能的总开关*/
#define NRF24L01_FEATURE			0x1D	//使能高级功能，1字节

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
