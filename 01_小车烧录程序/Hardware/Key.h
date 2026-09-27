/*
* 文件名称：Key.h
* 模块名称：非阻塞式按键驱动（头文件）
* 功能简介：定义按键编号、事件标志位，对外声明按键模块的函数
* 初学者提示：事件标志是一个个独立的二进制位（0x01、0x02、0x04...），
*             可以用按位或"|"同时组合多个标志、按位与"&"检查某个标志
*/
#ifndef __KEY_H
#define __KEY_H

#define KEY_COUNT				4		//按键总个数（K1~K4）

//按键编号，同时也是Key_Flag数组的下标
#define KEY_1					0		//K1：PB1，主程序中用于启动/停止
#define KEY_2					1		//K2：PB0，主程序中用于前进
#define KEY_3					2		//K3：PA5，主程序中用于后退
#define KEY_4					3		//K4：PA4，短按停止移动，长按切换控制方式

//事件标志位（一个按键同一时刻可同时拥有多个事件，故每位代表一种事件）
#define KEY_HOLD				0x01	//按住中（只要按着就一直存在，查询后不清除）
#define KEY_DOWN				0x02	//刚按下（按下沿，只存在一次扫描周期）
#define KEY_UP					0x04	//刚抬起（松开沿）
#define KEY_SINGLE				0x08	//单击（快速按下并松开，且规定时间内无第二次按下）
#define KEY_DOUBLE				0x10	//双击（规定时间内连续按下两次）
#define KEY_LONG				0x20	//长按（按住时间超过1000ms）
#define KEY_REPEAT				0x40	//连发（长按后每100ms产生一次）

extern uint8_t Key_Flag[];				//按键事件标志数组（定义在Key.c中，声明为extern供外部访问）

void Key_Init(void);										//初始化4个按键引脚（上拉输入）
uint8_t Key_Check(uint8_t n, uint8_t Flag);					//查询按键n是否发生了Flag事件，发生返回1（除HOLD外读后自动清除）
void Key_Clear(void);										//清除所有按键的所有事件标志
void Key_Tick(void);										//按键扫描状态机，需在1ms定时中断中反复调用

#endif
