/*
* 文件名称：PWM.c
* 模块名称：PWM（脉宽调制）波形输出驱动，基于通用定时器TIM2
* 功能简介：在PA0、PA1上输出两路频率固定为20kHz、占空比可调的PWM波形，用来给左右两个电机调速
* 硬件接线：TIM2通道1 → PA0（左电机调速PWM），TIM2通道2 → PA1（右电机调速PWM）；
*           引脚配置为"复用推挽输出"，波形由定时器硬件自动产生，输出过程不占用CPU
* 参数计算：定时器内部时钟为72MHz，预分频PSC = 36-1 = 35，计数频率 = 72MHz / 36 = 2MHz；
*           自动重装ARR = 100-1 = 99，计数器每计100个数溢出一次，PWM频率 = 2MHz / 100 = 20kHz；
*           比较寄存器CCR取0~100，占空比 = CCR / 100，即0%~100%
* 调用关系：Motor_Init内部调用PWM_Init完成初始化；Motor_SetPWM再通过PWM_SetCompare1/2改变占空比
* 初学者提示：① 占空比 = 一个周期内高电平时间 / 周期总时间，占空比越大，电机平均电压越高、转得越快；
*           ② 频率选20kHz是因为它高于多数人耳可听上限（约20kHz），电机不易发出刺耳啸叫
*/

#include "stm32f10x.h"                  // Device header

/*
* 函数名：PWM_Init
* 功  能：配置TIM2输出两路PWM波形（PA0通道1、PA1通道2，20kHz，初始占空比0）
* 参  数：无
* 返回值：无
* 注意点：TIM2挂在APB1总线上，GPIOA挂在APB2总线上，两条外设总线的时钟要分别开启
*/
void PWM_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);		//开启TIM2定时器时钟（APB1总线）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA时钟（APB2总线）
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;				//复用推挽输出（AF_PP）：引脚交给定时器外设控制，普通的GPIO置位/复位对它无效
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;		//选择PA0（TIM2_CH1）和PA1（TIM2_CH2）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;			//输出速率50MHz
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//配置写入GPIOA
	
	TIM_InternalClockConfig(TIM2);		//选择内部时钟（72MHz）作为计数器时钟源；不写此行上电默认也是内部时钟，写上更直观
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;			//时钟分频（只影响滤波采样，不影响计数频率），不分频即可
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;		//向上计数：计数器从0一直加到ARR再回0
	//ARR自动重装值：计数周期100（寄存器写99，因为从0开始），决定PWM周期，PWM频率=2MHz/100=20kHz
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;		//ARR
	//PSC预分频：72MHz分频36倍，得到2MHz计数频率（寄存器写35）
	TIM_TimeBaseInitStructure.TIM_Prescaler = 36 - 1;		//PSC
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;	//重复计数器仅高级定时器TIM1/TIM8使用，TIM2填0
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);	//把时基配置写入TIM2
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);					//先给结构体赋默认值，防止个别成员没配置而成为随机值
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;			//PWM模式1：计数器CNT < CCR时输出有效电平，否则输出无效电平
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	//有效电平为高电平（即先高后低，CCR越大高电平越宽）
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;	//使能通道输出
	//CCR比较值初始为0，即初始占空比0%，上电时电机不转
	TIM_OCInitStructure.TIM_Pulse = 0;		//CCR
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);					//把同一份输出比较配置应用到通道1（PA0）
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);					//再应用到通道2（PA1），两路PWM参数一致
		
	TIM_Cmd(TIM2, ENABLE);				//启动TIM2计数器，PWM波形从此刻开始从PA0、PA1输出
}

/*
* 函数名：PWM_SetCompare1
* 功  能：设置TIM2通道1（PA0，左电机）的PWM比较值CCR，即调节占空比
* 参  数：Compare —— 比较值，范围0~100，对应占空比0%~100%
* 返回值：无
* 注意点：本函数只改比较值，定时器不停，可在程序运行中随时调用，实现无级调速
*/
void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM2, Compare);		//写CCR1寄存器
}

/*
* 函数名：PWM_SetCompare2
* 功  能：设置TIM2通道2（PA1，右电机）的PWM比较值CCR，即调节占空比
* 参  数：Compare —— 比较值，范围0~100，对应占空比0%~100%
* 返回值：无
* 注意点：与PWM_SetCompare1用法相同，只是对应另一个轮子
*/
void PWM_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare2(TIM2, Compare);		//写CCR2寄存器
}
