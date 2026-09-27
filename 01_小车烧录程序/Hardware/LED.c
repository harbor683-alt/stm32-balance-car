/*
* 文件名称：LED.c
* 模块名称：板载LED指示灯驱动
* 功能简介：初始化PC13引脚，并提供LED的点亮、熄灭、翻转三个操作函数
* 硬件接线：LED接在PC13引脚上，采用共阳接法（LED另一端经限流电阻接3.3V），
*           因此引脚输出低电平0时点亮，输出高电平1时熄灭
* 调用关系：main函数开头调用LED_Init完成初始化；主循环中用LED_ON/LED_OFF指示平衡车是否处于运行状态
* 初学者提示：STM32片上外设的时钟默认是关闭的，操作任何引脚之前，都必须先开启对应GPIO端口的时钟
*/

#include "stm32f10x.h"                  // Device header

/*
* 函数名：LED_Init
* 功  能：初始化LED所使用的PC13引脚
* 参  数：无
* 返回值：无
* 注意点：引脚配置为通用推挽输出。推挽输出指引脚内部上、下两个MOS管轮流导通，
*         能主动输出强高电平和强低电平，驱动能力较强，适合直接驱动LED
*/
void LED_Init(void)
{
	//开启GPIOC端口的外设时钟（时钟不开，对该端口寄存器的任何配置都不生效）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;						//定义GPIO配置结构体变量
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;			//模式：通用推挽输出，引脚能主动输出高/低电平
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;					//引脚：选择第13号引脚（即PC13）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;			//速率：50MHz（只限制电平翻转的最高速度，普通IO填50MHz即可）
	GPIO_Init(GPIOC, &GPIO_InitStructure);						//把以上配置写入GPIOC寄存器，配置正式生效
	
	GPIO_SetBits(GPIOC, GPIO_Pin_13);		//初始输出高电平，让LED上电默认熄灭，避免上电瞬间误指示
}

/*
* 函数名：LED_ON
* 功  能：点亮LED
* 参  数：无
* 返回值：无
* 注意点：本电路为低电平点亮（电流从3.3V经LED灌入引脚，也称灌电流方式）
*/
void LED_ON(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);		//将PC13清0，输出低电平，LED点亮
}

/*
* 函数名：LED_OFF
* 功  能：熄灭LED
* 参  数：无
* 返回值：无
* 注意点：与LED_ON相反，向引脚输出高电平即可
*/
void LED_OFF(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_13);		//将PC13置1，输出高电平，LED熄灭
}

/*
* 函数名：LED_Turn
* 功  能：翻转LED的亮灭状态（亮变灭、灭变亮）
* 参  数：无
* 返回值：无
* 注意点：这里读取的是输出数据寄存器ODR的电平（即程序给引脚设置的输出值），
*         不是引脚上的真实电平；本电路引脚直接接LED，两者结果一致
*/
void LED_Turn(void)
{
	if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) == 0)	//当前输出为0，说明LED正亮着
	{
		GPIO_SetBits(GPIOC, GPIO_Pin_13);					//输出高电平，熄灭LED
	}
	else
	{
		GPIO_ResetBits(GPIOC, GPIO_Pin_13);					//否则当前为灭，输出低电平点亮LED
	}
}
