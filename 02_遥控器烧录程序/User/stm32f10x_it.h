/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.h 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */ 

/*【遥控器工程中文教学说明】本头文件与stm32f10x_it.c配套，声明其中9个Cortex-M3内核系统异常
  处理函数。它们的函数名由内核架构固定，不可更改，启动文件的中断向量表会引用这些名字。
  注意：本工程实际使用的TIM1定时中断函数TIM1_UP_IRQHandler在main.c中，不在本文件声明范围内；
  中断服务函数由硬件自动调用，一般不需要用户在代码里手动声明和调用。*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

/*如果被C++编译器引用，则按C语言的规则编译这些声明（防止C++名称改写导致链接失败），C工程中可忽略*/
#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/*以下9个均为Cortex-M3内核系统异常处理函数的声明，实现在stm32f10x_it.c中：
  NMI_Handler        不可屏蔽中断（严重硬件故障）
  HardFault_Handler  硬件错误（跑飞/非法指令等，进入死循环便于定位）
  MemManage_Handler  存储保护错误
  BusFault_Handler   总线错误（访问不存在的地址等）
  UsageFault_Handler 用法错误（未定义指令、非对齐访问、除零等）
  SVC_Handler        系统服务调用（RTOS用）
  DebugMon_Handler   调试监控
  PendSV_Handler     可挂起系统调用（RTOS任务切换用）
  SysTick_Handler    系统滴答定时器中断（本工程用查询方式，故为空）*/
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_IT_H */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
