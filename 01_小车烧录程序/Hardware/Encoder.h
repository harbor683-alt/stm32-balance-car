/*
* 文件名称：Encoder.h
* 模块名称：正交编码器测速驱动（头文件）
* 功能简介：对外声明编码器初始化与计数读取函数
*/
#ifndef __ENCODER_H
#define __ENCODER_H

void Encoder_Init(void);					//初始化TIM3/TIM4编码器接口（左PA6/PA7、右PB6/PB7）
int16_t Encoder_Get(uint8_t n);				//读取n号编码器（1左2右）的周期计数增量（正=正转，负=反转）并清零

#endif
