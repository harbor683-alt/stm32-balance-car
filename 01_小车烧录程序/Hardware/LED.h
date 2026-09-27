/*
* 文件名称：LED.h
* 模块名称：板载LED指示灯驱动（头文件）
* 功能简介：对外声明LED模块提供的4个函数，其他文件包含本头文件后即可调用
* 初学者提示：#ifndef...#define...#endif 是头文件保护宏，防止同一文件被重复包含
*/
#ifndef __LED_H
#define __LED_H

void LED_Init(void);		//初始化LED所接的PC13引脚（使用LED前调用一次）
void LED_ON(void);			//点亮LED（PC13输出低电平）
void LED_OFF(void);			//熄灭LED（PC13输出高电平）
void LED_Turn(void);		//翻转LED状态（亮变灭、灭变亮）

#endif
