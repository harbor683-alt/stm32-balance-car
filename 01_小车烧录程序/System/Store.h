/**
  **********************************************************************
  * @file    Store.h
  * @brief   参数掉电存储模块头文件
  * @note    参数区布局（每个元素是16位半字）：
  *          Store_Data[0]：固定钥匙标志STORE_KEY(0xB5B5)
  *          Store_Data[1]：速度档位SpeedLevel
  *          Store_Data[2]：陀螺仪零漂偏移GY_Offset
  *          Store_Data[3]：角度偏移AngleAcc_Offset低16位
  *          [4]：AngleAcc_Offset高16位（float拆成两个半字存放）
  **********************************************************************
  */
#ifndef __STORE_H		//头文件包含卫士
#define __STORE_H

extern uint16_t Store_Data[];	//参数RAM镜像，extern声明供其他文件（如DebugMode.c）
								//直接读写各参数，具体定义在Store.c

uint8_t Store_Init(void);		//开机初始化并加载参数，返回1=首次使用、0=已有参数
void Store_Save(void);			//把Store_Data整页写入Flash，掉电保存
void Store_Clear(void);			//清空全部参数（保留钥匙）并保存

#endif
