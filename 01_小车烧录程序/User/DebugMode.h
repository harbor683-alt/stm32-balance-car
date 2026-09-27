/**
  **********************************************************************
  * @file    DebugMode.h
  * @brief   调试模式模块头文件，对外声明参数存取与调试菜单三个函数
  * @note    HardwareTest()和SensorCalibration()仅供本模块菜单内部调用，
  *          故不在头文件中声明
  **********************************************************************
  */
#ifndef __DEBUG_MODE_H		//头文件包含卫士
#define __DEBUG_MODE_H

void SaveParam(void);	//把速度档位、GY零漂、角度偏移写入Flash（掉电保存）
void LoadParam(void);	//开机时从Store_Data还原各参数（需先调用Store_Init）
void DebugMode(void);	//进入调试模式菜单（硬件测试/传感器校准），长按K4退出

#endif
