/*
* 文件名称：Motor.c
* 模块名称：直流减速电机驱动（含方向控制与PWM调速）
* 功能简介：用4个GPIO控制电机驱动芯片的两个方向输入端，决定每个电机正转/反转/停止；
*           用PWM模块输出的占空比决定转速；对外只用一个带正负号的函数即可控制电机
* 硬件接线：左轮方向脚 PB12、PB13（对应PWM通道1/PA0调速）；
*           右轮方向脚 PB14、PB15（对应PWM通道2/PA1调速）。
*           方向脚一高一低驱动芯片内部H桥导通方向不同，电机就正转或反转
* 调用关系：main函数开头调用Motor_Init（其内部会顺带调用PWM_Init）；
*           TIM1的1ms中断执行PID后，调用Motor_SetPWM(1, PWML)、Motor_SetPWM(2, PWMR)驱动左右轮
* 初学者提示：① 电机本身不能直接接STM32引脚，中间必须有电机驱动板（H桥），引脚只负责"下命令"；
*           ② 参数Duty带正负号：正数正转、负数反转、数值大小（0~100）代表转速占空比
*/

#include "stm32f10x.h"                  // Device header
#include "PWM.h"

/*
* 函数名：Motor_Init
* 功  能：初始化4个电机方向控制引脚，并调用PWM_Init准备好两路调速波形
* 参  数：无
* 返回值：无
* 注意点：方向脚配置为通用推挽输出，能稳定输出高低电平给电机驱动芯片
*/
void Motor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB时钟（PB12~PB15四个方向脚都在GPIOB）
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;												//通用推挽输出，用于向驱动板发送方向电平
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;		//一次选中PB12、PB13、PB14、PB15四个引脚
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);														//配置写入GPIOB
	
	PWM_Init();																					//顺带初始化TIM2的两路PWM（PA0/PA1），调速波形就绪
}

/*
* 函数名：Motor_SetPWM
* 功  能：设置指定电机的转向和转速
* 参  数：n    —— 电机编号：1=左电机，2=右电机
*         Duty —— 带符号占空比（范围-100~100）：
*                  正数正转、负数反转、0停止；绝对值越大转速越快
* 返回值：无
* 注意点：① PWM比较值只能是非负数，所以反转时先用-Duty取绝对值；
*         ② 左右轮"正转"对应的方向电平组合相反，这是由两块驱动板/电机的
*            实际接线以及车轮镜像安装决定的，若实车前进方向与预期相反，
*            可调换对应一组方向脚的高低电平
*/
void Motor_SetPWM(uint8_t n, int16_t Duty)
{
	if (n == 1)							//n为1：控制左电机（方向PB12/PB13，速度PWM通道1）
	{
		if (Duty >= 0)					//占空比非负：左电机正转
		{
			GPIO_ResetBits(GPIOB, GPIO_Pin_13);		//PB13输出0
			GPIO_SetBits(GPIOB, GPIO_Pin_12);		//PB12输出1，01组合对应左电机正转方向
			PWM_SetCompare1(Duty);					//占空比直接用正数（0~100）
		}
		else							//占空比为负：左电机反转
		{
			GPIO_SetBits(GPIOB, GPIO_Pin_13);		//PB13输出1
			GPIO_ResetBits(GPIOB, GPIO_Pin_12);		//PB12输出0，10组合对应反转方向（与上面相反）
			PWM_SetCompare1(-Duty);					//取负号把占空比变成正数0~100再送给PWM
		}
	}
	else if (n == 2)						//n为2：控制右电机（方向PB14/PB15，速度PWM通道2）
	{
		if (Duty >= 0)					//占空比非负：右电机正转
		{
			GPIO_SetBits(GPIOB, GPIO_Pin_15);		//PB15输出1
			GPIO_ResetBits(GPIOB, GPIO_Pin_14);		//PB14输出0，10组合对应右电机正转方向（与左轮相反，见函数说明）
			PWM_SetCompare2(Duty);					//占空比送PWM通道2（0~100）
		}
		else							//占空比为负：右电机反转
		{
			GPIO_ResetBits(GPIOB, GPIO_Pin_15);		//PB15输出0
			GPIO_SetBits(GPIOB, GPIO_Pin_14);		//PB14输出1，01组合对应反转方向
			PWM_SetCompare2(-Duty);					//取绝对值后再送给PWM
		}
	}
	
}
