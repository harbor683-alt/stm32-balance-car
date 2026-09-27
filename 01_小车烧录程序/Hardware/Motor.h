/*
* 文件名称：Motor.h
* 模块名称：直流减速电机驱动（头文件）
* 功能简介：对外声明电机初始化与"带符号占空比"调速函数
*/
#ifndef __MOTOR_H
#define __MOTOR_H

void Motor_Init(void);							//初始化电机方向脚（PB12~PB15）并初始化PWM，使用电机前调用一次
void Motor_SetPWM(uint8_t n, int16_t Duty);		//驱动n号电机（1左2右），Duty取-100~100：正为正转、负为反转、0停止

#endif
