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

/*
  **********************************************************************
  * 【中文说明（本工程教学注释）】
  * 本头文件声明stm32f10x_it.c中那9个Cortex-M3内核异常处理函数。
  * 本工程实际使用的TIM1更新中断TIM1_UP_IRQHandler定义在main.c中，
  * 故不在此处声明（在.c中直接定义即可被启动文件的中断向量找到）。
  **********************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/* 以下为Cortex-M3内核异常处理函数声明（具体实现与用途见stm32f10x_it.c） */
void NMI_Handler(void);			//不可屏蔽中断（本工程空函数）
void HardFault_Handler(void);		//硬件错误（死循环）
void MemManage_Handler(void);		//内存管理错误（死循环）
void BusFault_Handler(void);		//总线错误（死循环）
void UsageFault_Handler(void);		//用法错误（死循环）
void SVC_Handler(void);				//系统服务调用（供RTOS，本工程空函数）
void DebugMon_Handler(void);			//调试监控（空函数）
void PendSV_Handler(void);			//挂起系统调用（供RTOS任务切换，空函数）
void SysTick_Handler(void);			//SysTick滴答中断（Delay用查询方式，空函数）

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_IT_H */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
