/***************************************************************************************
  * 模块名称：			Timer.c（TIM1定时中断驱动，遥控器端工程）
  * 模块功能：			配置高级定时器TIM1，每隔精确1ms产生一次更新中断
  * 在数据链路中的角色：	整个遥控器的"心跳节拍器"——中断服务函数TIM1_UP_IRQHandler写在main.c中，
  * 						它每1ms做两件事：①调用Key_Tick()推进按键20ms消抖扫描；
  * 						②对1ms计数，每累计100次(即100ms)把Flag置1，通知主循环发送一包无线数据。
  * 						硬件定时中断的好处：节拍精确、不阻塞主循环、即使主循环在忙也会准时打断执行。
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header

/**
  * 函    数：定时中断初始化
  * 参    数：无
  * 返 回 值：无
  */
void Timer_Init(void)
{
	/*TIM1是高级定时器，挂在APB2总线上，定时器输入时钟为72MHz*/
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);			//开启TIM1的时钟
	
	/*配置时钟源*/
	TIM_InternalClockConfig(TIM1);		//选择TIM1为内部时钟，若不调用此函数，TIM默认也为内部时钟
	
	/*时基单元初始化*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				//定义结构体变量
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //时钟分频，选择不分频，此参数用于配置滤波器时钟，不影响时基单元功能
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //计数器模式，选择向上计数
	/*【定时时长是怎么算出来的·重点】计数器时钟 = 72MHz / (PSC+1)，更新周期 = (PSC+1)×(ARR+1) / 72MHz。*/
	/*代入本工程：PSC = 72-1 → 计数时钟72MHz/72 = 1MHz（即每数1下用时1us）；*/
	/*ARR = 1000-1 → 从0数到999共1000下，1000×1us = 1ms，所以更新中断每1ms触发一次。*/
	/*PSC/ARR寄存器写入的是"分频值-1、周期-1"，因为硬件从0开始计数，这一点最容易写错。*/
	/*main.c再用软件对1ms计数100次分频，就得到了无线发包用的100ms节拍。*/
	TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;                //计数周期，即ARR的值
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;               //预分频器，即PSC的值
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;            //重复计数器，高级定时器会用到，此处配置为0
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);             //将结构体变量交给TIM_TimeBaseInit，配置TIM1的时基单元
	
	/*中断输出配置*/
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);			//清除定时器更新标志位
	                                                //TIM_TimeBaseInit函数末尾，手动产生了更新事件
	                                                //若不清除此标志位，则开启中断后，会立刻进入一次中断
	                                                //如果不介意此问题，则不清除此标志位也可
	
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);		//开启TIM1的更新中断
	
	/*【知识点·NVIC与中断优先级】NVIC（嵌套向量中断控制器）是Cortex-M3内核管理所有中断的"总调度台"。*/
	/*分组2把4位优先级拆成：2位抢占优先级(0~3) + 2位响应优先级(0~3)。*/
	/*抢占优先级高的中断可以"插队打断"正在执行的低抢占优先级中断（中断嵌套）；抢占级别相同时，*/
	/*响应优先级数字小的排队更靠前，但不能互相打断。本工程只有一个定时中断，给出(2,1)只要合法即可。*/
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//配置NVIC为分组2
	                                                //即抢占优先级范围：0~3，响应优先级范围：0~3
	                                                //此分组配置在整个工程中仅需调用一次
	                                                //若有多个中断，可以把此代码放在main函数内，while循环之前
	                                                //若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配置
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变量
	/*注意中断线名称：TIM1是高级定时器，它的更新事件在NVIC上对应独立的中断线TIM1_UP_IRQn*/
	/*（通用定时器TIM2~TIM4则是TIMx_IRQn）；对应的中断服务函数名固定为TIM1_UP_IRQHandler，*/
	/*函数名必须与启动文件startup_stm32f10x_md.s的中断向量表一字不差，写错名字中断就永远不会进。*/
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;          //选择配置NVIC的TIM1_UP线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;             //指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;   //指定NVIC线路的抢占优先级为2
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;          //指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);                             //将结构体变量交给NVIC_Init，配置NVIC外设
	
	/*TIM使能*/
	TIM_Cmd(TIM1, ENABLE);			//使能TIM1，定时器开始运行
}

/*【本工程实际的中断服务函数位置说明】下面注释里是定时器中断函数的标准模板（供参考），*/
/*而本工程真正的TIM1_UP_IRQHandler定义在main.c末尾：每1ms调用Key_Tick()，并软件分频出100ms的Flag。*/
/*中断服务函数无需在头文件声明、也无需手动调用，启动文件的中断向量表会在中断发生时自动跳转过去执行。*/

/* 定时器中断函数，可以复制到使用它的地方
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}
*/
