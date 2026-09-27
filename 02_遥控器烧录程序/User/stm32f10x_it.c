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

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"

/***************************************************************************************
  * 【遥控器工程中文教学说明】
  * 本文件是什么：		意法半导体(ST)官方模板文件，集中存放Cortex-M3内核的"系统异常处理函数"。
  * 						中断/异常发生时，CPU会自动从启动文件的中断向量表跳到同名函数执行。
  * 与本工程的关系：		遥控器实际用到的TIM1 1ms定时中断服务函数TIM1_UP_IRQHandler并不在本文件，
  * 						而是由作者写在了main.c的末尾（在那里调用Key_Tick并产生100ms发包标志）。
  * 						所以本文件在本工程中只保留下面这些"默认的内核异常兜底函数"。
  * 下面各函数简介：		NMI_HardFault等以_Fault结尾的是"错误异常"：程序跑飞、非法地址访问、
  * 						除零、未对齐访问等严重错误时触发；模板让它们进入while(1)死循环，方便调试时
  * 						断点定位（一旦卡死就说明出了对应错误）。SVC/PendSV/SysTick与操作系统、
  * 						系统滴答有关，本工程未用RTOS且SysTick采用查询方式，所以这三个函数留空。
  ***************************************************************************************/

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
	/*NMI（Non Maskable Interrupt，不可屏蔽中断）：时钟安全系统检测到外部时钟失效等*/
	/*极严重硬件故障时触发，无法被优先级屏蔽。本工程未使用时钟监测，故函数留空*/
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /*硬件错误异常：最常见的致命错误，例如函数指针跑飞、栈溢出、执行了非法指令等*/
  /* Go to infinite loop when Hard Fault exception occurs */
  /*进入死循环：把程序"钉"在现场，调试器连上后可直接查看出错位置和调用关系*/
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
  /*存储管理异常：访问了MPU（存储保护单元）禁止的内存区域等。本工程未开启MPU，正常不会进入*/
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
  /*总线错误异常：访问了不存在或未响应的存储器/外设地址（如读写超出芯片范围的地址）时触发*/
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
  /*用法错误异常：程序自身的错误，如执行未定义指令、非对齐访问、除以0（使能相应检查时）等*/
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
	/*SVC系统服务调用异常：执行SVC指令时触发，常用于RTOS内核的系统调用；本工程无操作系统，留空*/
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
	/*调试监控异常：供软件调试器使用的调试监控，本工程不使用，留空*/
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
	/*PendSV可挂起系统调用异常：RTOS常用它来做任务切换，可被推迟到无更高优先级中断时再执行；本工程留空*/
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
	/*SysTick定时器中断函数：Delay.c中采用的是"查询COUNTFLAG标志"的方式使用SysTick，并未开启其中断，*/
	/*所以本函数在遥控器工程中保持为空；定时节拍由TIM1更新中断（TIM1_UP_IRQHandler，在main.c中）承担*/
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/*【中文说明】按照模板设计，外设中断函数（如串口、外部中断、定时器等的IRQHandler）应添加在本区。*/
/*本遥控器工程唯一用到的外设中断是TIM1更新中断，其处理函数TIM1_UP_IRQHandler被作者放在了main.c中；*/
/*中断函数可以写在任意.c文件中，只要函数名与启动文件startup_stm32f10x_md.s向量表里的名字一致即可。*/
/*编写外设中断函数的固定套路：①判断中断标志 → ②执行中断任务 → ③手动清除中断标志（不清会反复进中断）。*/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
