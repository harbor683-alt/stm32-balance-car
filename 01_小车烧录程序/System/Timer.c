/**
  **********************************************************************
  * @file    Timer.c
  * @brief   定时中断模块：把高级定时器TIM1配置成周期1ms的"更新中断"
  * @hardware TIM1（高级定时器，挂在APB2总线，时钟72MHz）、
  *          NVIC（嵌套向量中断控制器，内核中管理中断开关与优先级的部件）
  * @role    本工程的"心跳节拍"。每1ms触发一次中断，真正的中断服务
  *          函数TIM1_UP_IRQHandler写在main.c中，里面完成读编码器、
  *          读MPU6050、三级PID运算、更新电机PWM等周期性核心工作
  * @note    【1ms定时时间推导】（定时周期=分频×计数值/时钟频率）
  *          TIM_Prescaler预分频器=72-1 → 计数频率 = 72MHz/72 = 1MHz，
  *          即计数器每1us加1；
  *          TIM_Period自动重装值=1000-1 → 数1000次产生一次更新事件；
  *          中断周期 = 1000 / 1MHz = 1ms（中断频率=1kHz）
  *          注意寄存器要填"次数-1"：预分频器和计数器都从0开始数，
  *          数N个脉冲对应的值是N-1
  *          【什么是中断】CPU正常执行主程序时，定时器每隔1ms"打断"
  *          CPU一次，让它跳进中断函数做完紧要工作再回来继续，这种
  *          机制就叫中断；NVIC负责决定哪些中断能触发、谁先谁后
  **********************************************************************
  */
#include "stm32f10x.h"                  // Device header

/**
  * @brief  TIM1更新中断初始化，开机时在main函数中调用一次
  * @param  无
  * @retval 无
  * @note   配置顺序：开时钟→选内部时钟源→装定时参数→开更新中断→
  *         配NVIC优先级→启动定时器
  */
void Timer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);	//开启TIM1的时钟（外设用前必须先开时钟）
	
	TIM_InternalClockConfig(TIM1);	//选择TIM1内部时钟（CK_CNT），即72MHz经预分频后的计数时钟
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;	//时钟分频因子，仅影响滤波/采样，与定时周期无关
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;	//向上计数：从0数到ARR后回0并产生更新事件
	TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;			//ARR自动重装值，数1000个脉冲(=1000us)溢出一次
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;			//PSC预分频值，72MHz/72=1MHz，即1us计一次数
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;			//重复计数器：高级定时器特有，0表示每次更新都产生事件
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);	//把以上参数写入寄存器
	
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);	//手动清除一次更新标志：初始化时硬件会立即置位一次，
											//不清掉会导致刚开中断就误进一次中断函数
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);	//允许TIM1的"更新中断"，让定时事件能向NVIC发出中断申请
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//设置优先级分组为组2：2位抢占优先级+2位响应优先级
	//抢占优先级：高的可以打断低的（中断嵌套）；响应优先级：同时申请时数值小的先执行，不能互相打断
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;	//要配置的中断通道：TIM1更新中断
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		//使能该通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	//抢占优先级=2
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//响应优先级=1
	NVIC_Init(&NVIC_InitStructure);	//把中断通道配置写入NVIC
	
	TIM_Cmd(TIM1, ENABLE);	//启动TIM1，计数器开始计数，之后每1ms触发一次更新中断
}

/*——————————————————————————————————————————————————————————————————*/
/* 以下是TIM1更新中断服务函数的标准模板，本工程中已被注释掉。          */
/* 真正的TIM1_UP_IRQHandler写在main.c里（全工程只能有一个同名函数，   */
/* 否则重复定义会报错）。模板保留在此处供学习参考：                    */
/*  1.进入中断后先用TIM_GetITStatus确认确实是更新中断；               */
/*  2.处理用户任务；                                                   */
/*  3.最后必须用TIM_ClearITPendingBit清除中断标志，否则退出后会        */
/*    立刻重复进入中断。                                               */
/*——————————————————————————————————————————————————————————————————*/
/*
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}
*/
