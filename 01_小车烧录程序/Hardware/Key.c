/*
* 文件名称：Key.c
* 模块名称：非阻塞式按键驱动（支持单击、双击、长按、连发等事件）
* 功能简介：周期性采样4个按键的电平，经消抖和状态机判断后，把按下、抬起、
*           单击、双击、长按、连发等事件以"标志位"形式记录下来，
*           主程序随时调用Key_Check查询，而不必停下来等待按键
* 硬件接线：K1接PB1、K2接PB0、K3接PA5、K4接PA4；引脚使用内部上拉，按键另一端接地，
*           因此松开时引脚为高电平1，按下时被拉成低电平0
* 调用关系：main函数开头调用Key_Init完成初始化；Key_Tick由TIM1每1ms的定时中断调用一次；
*           主循环中用Key_Check获取事件（例如K1启动/停止、K4长按切换控制方式）
* 初学者提示：① 上拉输入（IPU）就是引脚内部通过电阻接到高电平，按键松开时默认读到1；
*           ② 本驱动不用delay死等，扫描在定时中断里进行，称为"非阻塞"，不会卡住主循环；
*           ③ 消抖方法是每20ms才采样一次电平，机械抖动发生在两次采样之间，读到的自然是稳定状态
*/

#include "stm32f10x.h"                  // Device header
#include "Key.h"

//以下两个宏把物理电平翻译成好理解的逻辑状态（本电路按下对应低电平0）
#define KEY_PRESSED				1		//逻辑状态：按下
#define KEY_UNPRESSED			0		//逻辑状态：松开

//以下时间阈值的单位都是ms（Key_Tick每1ms被调用一次，Time数组每Tick减1）
#define KEY_TIME_DOUBLE			0		//松开后留给"第二次按下"的判定窗口时间（0表示立即判定）
#define KEY_TIME_LONG			1000	//持续按住超过1000ms，判定为长按
#define KEY_TIME_REPEAT			100		//进入长按后，每隔100ms产生一次连发事件

uint8_t Key_Flag[KEY_COUNT];	//按键事件标志数组：每个按键占1个字节，按位存放HOLD/DOWN/单击等事件（位定义见Key.h）
								//遥控（蓝牙/NRF）收到按键指令时，也会直接置位本数组成员来"模拟"实体按键

/*
* 函数名：Key_Init
* 功  能：初始化4个按键所使用的GPIO引脚（PA4、PA5、PB0、PB1）
* 参  数：无
* 返回值：无
* 注意点：4个引脚统一配置为上拉输入（IPU），依靠内部上拉电阻，松开按键时引脚保持高电平，
*         无需在电路板上额外接上拉电阻
*/
void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA时钟（K3、K4在PA5、PA4）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB时钟（K1、K2在PB1、PB0）
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;				//模式：上拉输入，按键松开时引脚被内部电阻拉成高电平
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_0;		//选择PB1、PB0（对应K1、K2），用按位或可同时选多个引脚
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;			//输入模式下该参数无实际意义，按惯例填50MHz
	GPIO_Init(GPIOB, &GPIO_InitStructure);						//配置写入GPIOB
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_4;		//只改引脚号为PA5、PA4（对应K3、K4），模式仍沿用上面的上拉输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//配置写入GPIOA
}

/*
* 函数名：Key_GetState
* 功  能：读取指定按键当前的物理状态（按下还是松开）
* 参  数：n —— 按键编号，取值KEY_1、KEY_2、KEY_3、KEY_4（见Key.h）
* 返回值：KEY_PRESSED(1)表示按下，KEY_UNPRESSED(0)表示松开
* 注意点：本函数只反映这一瞬间的电平，不带消抖；消抖由Key_Tick通过"每20ms采样"完成。
*         上拉输入的按键按下时引脚接地，所以读到0才代表按下
*/
uint8_t Key_GetState(uint8_t n)
{
	if (n == KEY_1)
	{
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0)		//读PB1电平，为0说明K1被按下
		{
			return KEY_PRESSED;
		}
	}
	else if (n == KEY_2)
	{
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)		//读PB0电平，为0说明K2被按下
		{
			return KEY_PRESSED;
		}
	}
	else if (n == KEY_3)
	{
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5) == 0)		//读PA5电平，为0说明K3被按下
		{
			return KEY_PRESSED;
		}
	}
	else if (n == KEY_4)
	{
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == 0)		//读PA4电平，为0说明K4被按下
		{
			return KEY_PRESSED;
		}
	}
	return KEY_UNPRESSED;										//既不是按下的情况，统一返回"松开"
}

/*
* 函数名：Key_Check
* 功  能：查询某个按键的某个事件是否发生（主程序主要通过本函数使用按键）
* 参  数：n    —— 按键编号KEY_1~KEY_4
*         Flag —— 事件类型，如KEY_DOWN、KEY_SINGLE、KEY_DOUBLE、KEY_LONG、KEY_HOLD等（见Key.h）
* 返回值：1表示事件发生，0表示未发生
* 注意点：除KEY_HOLD（按住中）外，其他事件一旦被查询到就立即软件清除，
*         即"读一次就消失"，保证一次单击只触发一次动作，不会在循环里被重复响应
*/
uint8_t Key_Check(uint8_t n, uint8_t Flag)
{
	if (Key_Flag[n] & Flag)				//按位与：检查该事件对应的标志位是否为1
	{
		if (Flag != KEY_HOLD)			//HOLD表示持续按住，需要一直保留；其余事件
		{
			Key_Flag[n] &= ~Flag;		//~Flag把该位清0而不影响其他位（读后自动清除事件）
		}
		return 1;
	}
	return 0;
}

/*
* 函数名：Key_Clear
* 功  能：清除全部按键的全部事件标志
* 参  数：无
* 返回值：无
* 注意点：常用于界面等待按键之后（如开机等待K4），把等待期间累积的标志清空，
*         防止同一次按下"穿过"界面误触发后续功能
*/
void Key_Clear(void)
{
	uint8_t i;
	for (i = 0; i < KEY_COUNT; i ++)
	{
		Key_Flag[i] = 0;
	}
}

/*
* 函数名：Key_Tick
* 功  能：按键扫描与事件状态机，是本模块的"发动机"
* 参  数：无
* 返回值：无
* 注意点：① 必须每1ms被定时中断调用一次（本工程在TIM1更新中断里调用），所有时间判定才准确；
*         ② 函数内部用static变量保存上次状态，退出函数后数据不丢失，但它们只在本函数内可见；
*         ③ 真正的电平采样每20ms进行一次（Count计数到20），相当于20ms消抖；
*         ④ 状态机变量S的含义：0空闲，1已按下正在计时（判断长按），
*            2已松开正在等待第二次按下（判断双击），3双击成立后等待松开，4长按中周期连发
*/
void Key_Tick(void)
{
	static uint8_t Count, i;									//Count：1ms计数，满20次扫描一次；i：循环下标
	static uint8_t CurrState[KEY_COUNT], PrevState[KEY_COUNT];	//每个按键的当前状态、上一次状态（用于判断按下沿/松开沿）
	static uint8_t S[KEY_COUNT];								//每个按键的状态机当前状态（0/1/2/3/4）
	static uint16_t Time[KEY_COUNT];							//每个按键的软件计时器，单位ms
	
	//每个Tick（1ms）把所有按键的计时器减1，实现长按、双击窗口等毫秒级计时
	for (i = 0; i < KEY_COUNT; i ++)
	{
		if (Time[i] > 0)
		{
			Time[i] --;
		}
	}
	
	Count ++;
	if (Count >= 20)					//每累计20个Tick（即20ms）才做一次按键采样，这就是消抖
	{
		Count = 0;
		
		for (i = 0; i < KEY_COUNT; i ++)	//逐个扫描4个按键
		{
			PrevState[i] = CurrState[i];	//先把上次采样结果保存下来
			CurrState[i] = Key_GetState(i);	//再读取本次（已经是20ms后的稳定电平）
			
			//以下维护HOLD（按住中）标志：只要当前按着就置位，松开就清除
			if (CurrState[i] == KEY_PRESSED)
			{
				Key_Flag[i] |= KEY_HOLD;
			}
			else
			{
				Key_Flag[i] &= ~KEY_HOLD;
			}
			
			//本次按下、上次松开：出现"按下沿"（刚按下去的一瞬间）
			if (CurrState[i] == KEY_PRESSED && PrevState[i] == KEY_UNPRESSED)
			{
				Key_Flag[i] |= KEY_DOWN;
			}
			
			//本次松开、上次按下：出现"松开沿"（刚抬起来的一瞬间）
			if (CurrState[i] == KEY_UNPRESSED && PrevState[i] == KEY_PRESSED)
			{
				Key_Flag[i] |= KEY_UP;
			}
			
			//状态0（空闲态）：一旦检测到按下，启动1000ms长按计时，转入状态1
			if (S[i] == 0)
			{
				if (CurrState[i] == KEY_PRESSED)
				{
					Time[i] = KEY_TIME_LONG;	//装载长按判定时间
					S[i] = 1;
				}
			}
			//状态1（按下等待态）：等"松开"或"长按时间到"，先发生哪个就走哪个分支
			else if (S[i] == 1)
			{
				if (CurrState[i] == KEY_UNPRESSED)
				{
					Time[i] = KEY_TIME_DOUBLE;	//未满1秒就松开：可能是单击，也可能是双击的前半下，进入状态2继续观察
					S[i] = 2;
				}
				else if (Time[i] == 0)
				{
					Time[i] = KEY_TIME_REPEAT;	//一直按着且1000ms到：判定长按，再装100ms连发间隔
					Key_Flag[i] |= KEY_LONG;	//产生一次长按事件
					S[i] = 4;					//进入长按连发态
				}
			}
			//状态2（松开等待态）：在双击窗口时间内再次按下→双击；超时仍未按→单击
			else if (S[i] == 2)
			{
				if (CurrState[i] == KEY_PRESSED)
				{
					Key_Flag[i] |= KEY_DOUBLE;	//窗口内又按下，判定为双击
					S[i] = 3;					//进入状态3，等双击的这次按下松开
				}
				else if (Time[i] == 0)
				{
					Key_Flag[i] |= KEY_SINGLE;	//窗口时间到也没第二次按下，判定为单击
					S[i] = 0;					//回到空闲态
				}
			}
			//状态3（双击收尾态）：等双击的第二次按下松开，然后回到空闲态
			else if (S[i] == 3)
			{
				if (CurrState[i] == KEY_UNPRESSED)
				{
					S[i] = 0;
				}
			}
			//状态4（长按连发态）：继续按住则每隔100ms产生一次REPEAT事件，松开则结束
			else if (S[i] == 4)
			{
				if (CurrState[i] == KEY_UNPRESSED)
				{
					S[i] = 0;
				}
				else if (Time[i] == 0)
				{
					Time[i] = KEY_TIME_REPEAT;	//重装100ms间隔
					Key_Flag[i] |= KEY_REPEAT;	//产生一次连发事件（效果类似电脑键盘长按）
					S[i] = 4;
				}
			}
		}
	}
}
