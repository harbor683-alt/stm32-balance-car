/***************************************************************************************
  * 模块名称：			Key.c（按键扫描驱动，遥控器端工程）
  * 模块功能：			扫描遥控器上的12个独立按键，进行20ms定时消抖和松手检测，输出1~12的键码
  * 硬件接线：			每个按键一端接GPIO引脚，另一端接GND，引脚内部使用上拉电阻：
  * 						键码1 → PA6		键码2 → PA7		键码3 → PB0		键码4 → PB1
  * 						键码5 → PB2		键码6 → PB10	键码7 → PA11	键码8 → PA12
  * 						键码9 → PB5（main.c中用作Mode模式切换键）
  * 						键码10 → PB6	键码11 → PA4	键码12 → PA5
  * 						（遥控器按键比小车端多，具体以本文件Key_GetState中的查询顺序为准）
  * 数据链路角色：		键码通过Key_GetNum()被main.c取走，存入KEY变量，最终作为无线数据包
  * 						[Mode,LH,LV,RH,RV,KEY]的第6字节发给小车
  * 工作机制：			消抖不靠Delay阻塞延时，而靠"定时节拍"——Key_Tick()由TIM1的1ms中断调用，
  * 						内部再分频到每20ms采样一次，并比较本次/上次状态，在松手瞬间提交一次键码
  * 名词解释：			上拉输入：引脚内部通过电阻接高电平，无按键时读到1；
  * 						按键按下把引脚对GND短路，读到0——称为"低电平有效"
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header
#include "Delay.h"

/*全局变量，用于存储按键键码*/
uint8_t Key_Num;

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//开启GPIOB的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	
	/*【知识点·上拉输入IPU】GPIO_Mode_IPU（Input Pull-Up，内部上拉输入）：*/
	/*引脚在芯片内部经一个电阻接到VCC，所以按键未按下（悬空）时引脚被稳稳拉成高电平，读到1；*/
	/*按键按下时引脚通过按键直接与GND短路，读到0。这样做的好处是按键只需要接GND一根信号线，*/
	/*且悬空状态电平确定，不会因为干扰来回乱跳。读到0即代表"按下"，所以叫低电平有效。*/
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//将PA6、PA7、PA11、PA12、PA4和PA5引脚初始化为上拉输入
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_10 | GPIO_Pin_5 | GPIO_Pin_6;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					//将PB0、PB1、PB2、PB10、PB5、和PB6引脚初始化为上拉输入
}

/**
  * 函    数：获取全局变量定义的按键键码
  * 参    数：无
  * 返 回 值：按键键码
  */
uint8_t Key_GetNum(void)
{
	/*【设计思想·读后清零】键码由中断里的Key_Tick异步写入全局变量Key_Num，主循环随时可能来取。*/
	/*"取走后立刻清零"保证一次按键事件只会被消费一次，不会因为主循环转得快而把同一次按键*/
	/*误当成连发了很多次命令。这是中断生产数据、主循环消费数据时常用的简单握手方式。*/
	uint8_t Temp;			//定义一个临时变量用于中转
	if (Key_Num)			//如果全局变量的键码不为0
	{
		/*这3句的目的是，实现读取键码并读后清零的效果*/
		Temp = Key_Num;		//先把键码存入临时变量
		Key_Num = 0;		//键码清零
		return Temp;		//返回临时变量，return语句执行后，函数直接结束
	}
	return 0;				//如果if不成立，键码为0，则默认返回0
}

/**
  * 函    数：获取按键状态
  * 参    数：无
  * 返 回 值：有按键按下，直接返回键码（非阻塞），没有按键按下，返回0
  */
uint8_t Key_GetState(void)
{
	/*【函数定位·只做"瞬时快照"】本函数只在被调用的那一瞬间逐个读引脚电平：谁被拉低就立刻返回谁的*/
	/*键码，全部为高则返回0。它不等待、不死循环、不做消抖（非阻塞），执行一次只要几微秒。*/
	/*消抖和松手判断全部交给Key_Tick按固定节拍完成，分工明确。*/
	/*多键同时按下时，按照下面if的书写顺序，只会返回排在最前面的那个键码，后面的被忽略。*/
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == 0)		//如果PA6引脚电平为0
	{
		return 1;		//直接返回键码1
	}
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == 0)		//如果PA7引脚电平为0
	{
		return 2;		//直接返回键码2
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)		//如果PB0引脚电平为0
	{
		return 3;		//直接返回键码3
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)		//如果PB1引脚电平为0
	{
		return 4;		//直接返回键码4
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2) == 0)		//如果PB2引脚电平为0
	{
		return 5;		//直接返回键码5
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == 0)		//如果PB10引脚电平为0
	{
		return 6;		//直接返回键码6
	}
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == 0)		//如果PA11引脚电平为0
	{
		return 7;		//直接返回键码7
	}
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12) == 0)		//如果PA12引脚电平为0
	{
		return 8;		//直接返回键码8
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5) == 0)		//如果PB5引脚电平为0
	{
		return 9;		//直接返回键码9
	}
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6) == 0)		//如果PB6引脚电平为0
	{
		return 10;		//直接返回键码10
	}
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == 0)		//如果PA4引脚电平为0
	{
		return 11;		//直接返回键码11
	}
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0)		//如果PA5引脚电平为0
	{
		return 12;		//直接返回键码12
	}
	return 0;
}

/**
  * 函    数：用于驱动按键模块运行的自定义按键定时中断函数
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数必须在主程序中每隔1ms自动执行一次
  */
void Key_Tick(void)
{
	/*================================按键扫描的核心：Tick节拍机制================================*/
	/*1) 本函数由main.c中的TIM1更新中断服务函数TIM1_UP_IRQHandler每隔1ms自动调用一次，节奏由硬件保证；*/
	/*2) Count对1ms计数，每满20次才真正扫描一次，相当于每20ms对按键电平采样一次；*/
	/*3)【为什么这样就能消抖】机械按键的金属弹片在接通/断开瞬间会弹跳，产生几ms到十几ms的高低电平*/
	/*   毛刺。若每次毛刺都算数，按一次可能被识别成按了好几下。每隔20ms只看一次"稳定后的电平"，*/
	/*   采样间隔比抖动时间长，抖动自然被跳过，这就是"定时消抖"（比Delay死等更省CPU，不阻塞主循环）；*/
	/*4)【松手检测】保存本次、上次两次采样值：只有"上次有键(非0)、本次无键(0)"的下降沿才提交键码，*/
	/*   即手指松开的那一瞬间才确认一次按键。这样长按不放也不会连续触发，符合遥控器"点按发一次指令"的需求；*/
	/*5) static静态变量只在第一次进入时初始化为0，函数返回后内存不释放，下次进入继续沿用，实现跨次记忆。*/
	/*定义静态变量（默认初值为0，函数退出后保留值和存储空间）*/
	static uint8_t Count;					//用于计次分频
	static uint8_t CurrState, PrevState;	//保存按键本次状态和上次状态
	
	Count ++;			//计次自增
	if (Count >= 20)	//如果计次20次，则if成立，即if每隔20ms进一次
	{
		Count = 0;		//计次清零，便于下次计次
		
		/*获取按键的本次状态和上次状态*/
		PrevState = CurrState;			//获取上次状态
		CurrState = Key_GetState();		//获取本次状态
		
		/*如果本次状态的键码为0，且上次键码不为0，即检测到按键松手瞬间*/
		/*补充：这里提交的是PrevState（松手前按着的那个键），避免松手瞬间读到0而丢失键码。*/
		if (CurrState == 0 && PrevState != 0)
		{
			/*将上次状态的键码复制给全局变量，后续读取此变量，即可得知哪个按键按下了*/
			Key_Num = PrevState;
		}
	}
}
