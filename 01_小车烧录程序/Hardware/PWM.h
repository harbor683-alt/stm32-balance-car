/*
* 文件名称：PWM.h
* 模块名称：TIM2两路PWM输出驱动（头文件）
* 功能简介：对外声明PWM初始化和两路占空比设置函数，供电机驱动模块调用
*/
#ifndef __PWM_H
#define __PWM_H

void PWM_Init(void);								//初始化TIM2，在PA0/PA1输出20kHz PWM（由Motor_Init代为调用）
void PWM_SetCompare1(uint16_t Compare);				//设置通道1（PA0，左轮）比较值，Compare取0~100对应0%~100%占空比
void PWM_SetCompare2(uint16_t Compare);				//设置通道2（PA1，右轮）比较值，Compare取0~100对应0%~100%占空比

#endif
