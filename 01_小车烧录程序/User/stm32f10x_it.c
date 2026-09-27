/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
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
  * 本文件是"中断服务函数的集中放置处"，由ST标准库模板提供，集中存放
  * Cortex-M3内核异常和外设中断的处理函数。中断：CPU正常运行主程序时，
  * 内核/外设发生紧急事件，可强行打断主程序、跳到对应的Handler函数里
  * 处理事件、处理完再返回原处继续执行。
  * 注意：本工程实际使用的TIM1 1ms更新中断服务函数
  * TIM1_UP_IRQHandler 写在 main.c 中（连同控制代码一起），因此下面
  * 只保留内核异常的空模板；外设中断可参照文末注释掉的PPP模板添加，
  * 函数名必须与启动文件startup_stm32f10x_md.s中的向量名完全一致。
  **********************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
	/*用途：NMI不可屏蔽中断（时钟失效等严重硬件事件），本工程未使用，空函数*/
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /*用途：硬件错误异常（非法内存访问、堆栈溢出等），本工程未使用，原地死循环便于调试时定位故障点*/
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /*用途：内存管理异常（如MPU访问违规，M3未开启MPU时不会触发），本工程未使用，原地死循环*/
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /*用途：总线错误异常（访问了不存在的地址等总线级错误），本工程未使用，原地死循环*/
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /*用途：用法错误异常（执行未定义指令、非对齐访问、除零等），本工程未使用，原地死循环*/
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
	/*用途：系统服务调用异常（执行SVC指令触发，多给RTOS用），本工程无操作系统，空函数*/
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
	/*用途：调试监控异常（仿真调试时内核内部使用），本工程未使用，空函数*/
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
	/*用途：挂起系统调用异常（供RTOS做任务切换用），本工程无操作系统，空函数*/
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
	/*用途：SysTick系统滴答定时器中断；本工程的SysTick在Delay.c里用查询方式做延时，未开启中断，故留空*/
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*用途：外设中断函数的添加模板——用到某个外设中断时，把PPP改成启动文件中对应的中断名（如USART1_IRQHandler）并取消注释即可*/
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
