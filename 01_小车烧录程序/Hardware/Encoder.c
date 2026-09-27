/*
* 文件名称：Encoder.c
* 模块名称：正交编码器测速驱动（利用定时器硬件编码器模式）
* 功能简介：把两个定时器配置为"编码器接口模式"，硬件自动对电机编码器的A/B相脉冲
*           进行加减计数：正转计数增加、反转计数减少；每隔一段时间读取一次计数值，
*           就可以算出电机转速和转向，作为PID速度环的反馈
* 硬件接线：左编码器 → TIM3：A相PA6（CH1）、B相PA7（CH2）；
*           右编码器 → TIM4：A相PB6（CH1）、B相PB7（CH2），引脚均为上拉输入
* 背景知识：正交编码器随轴转动输出两路相位相差90度的方波（A相、B相）；
*           定时器在编码器模式TI12下，两相的每个跳变沿都会让计数器加1或减1（四倍频），
*           无需CPU干预即可完成测速脉冲统计
* 调用关系：main函数开头调用Encoder_Init；TIM1的1ms中断里每50ms调用Encoder_Get(1)/(2)，
*           把返回的计数增量除以408（输出轴每转一圈的计数）和时间，得到车轮转速
* 初学者提示：① 16位计数器按有符号数int16_t解读，即可同时获得"转了多少"和"朝哪个方向转"；
*           ② 读完立刻清零，下一次读到的就是这50ms内的新增量；
*           ③ 左右两个定时器通道极性配置不同（见下文），是为了让两轮朝同一方向前进时计数值同号
*/

#include "stm32f10x.h"                  // Device header

/*
* 函数名：Encoder_Init
* 功  能：把TIM3（左编码器）、TIM4（右编码器）配置为编码器接口模式
* 参  数：无
* 返回值：无
* 注意点：配置顺序为 开时钟 → 引脚设为上拉输入 → 定时器时基（16位满量程、不分频）
*         → 输入捕获通道（带滤波）→ 编码器接口模式 → 启动计数器
*/
void Encoder_Init(void)
{
	/*========== 左编码器：TIM3，PA6/PA7 ==========*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);		//开启TIM3时钟（APB1总线）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA时钟（PA6、PA7）
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;				//上拉输入：编码器空闲时保持确定高电平，抗干扰
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;		//PA6、PA7即TIM3的通道1、通道2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
		
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;	//向上计数（编码器模式下实际由A/B相决定加减，此处保持默认即可）
	//ARR取满16位量程0~65535，计数器溢出后自然回绕，按int16_t解读正好是有符号数
	TIM_TimeBaseInitStructure.TIM_Period = 65536 - 1;		//ARR
	//PSC不分频：编码器送来的每个脉冲边沿都计数
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1 - 1;		//PSC
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	
	TIM_ICInitTypeDef TIM_ICInitStructure;
	TIM_ICStructInit(&TIM_ICInitStructure);					//输入捕获结构体先填默认值
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;		//先配置通道1（接A相PA6）
	TIM_ICInitStructure.TIM_ICFilter = 0xF;					//输入滤波取最大值0xF，滤除电机带来的毛刺干扰（值越大滤波越强）
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;		//再配置通道2（接B相PA7）
	TIM_ICInitStructure.TIM_ICFilter = 0xF;					//B相同样使用最强滤波
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	
	//编码器接口模式TI12：A、B两相的边沿都参与计数（四倍频）；Rising/Rising表示两相均不反相
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	
	TIM_Cmd(TIM3, ENABLE);									//启动TIM3，计数器开始随左轮转动自动加减
	
	
	/*========== 右编码器：TIM4，PB6/PB7（配置步骤与TIM3完全对称） ==========*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);		//开启TIM4时钟（APB1总线）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);		//开启GPIOB时钟（PB6、PB7）
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;				//复用上面已定义的结构体，只重新指定引脚即可，模式仍为上拉输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;		//PB6、PB7即TIM4的通道1、通道2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
		
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	//ARR同TIM3：取满16位量程0~65535
	TIM_TimeBaseInitStructure.TIM_Period = 65536 - 1;		//ARR
	//PSC同TIM3：不分频，编码器送来的每个脉冲边沿都计数
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1 - 1;		//PSC
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
	
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;		//通道1（A相PB6）
	TIM_ICInitStructure.TIM_ICFilter = 0xF;					//同样开启最强输入滤波
	TIM_ICInit(TIM4, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;		//通道2（B相PB7）
	TIM_ICInitStructure.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM4, &TIM_ICInitStructure);
	
	//注意右轮通道2极性配成Falling（下降沿），相当于把B相反相，计数器加减方向与TIM3相反；
	//这样两个轮子朝向同一个前进方向转动时，Encoder_Get得到的速度符号才能一致
	TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Falling);
	
	TIM_Cmd(TIM4, ENABLE);									//启动TIM4，计数器开始随右轮转动自动加减
}

/*
* 函数名：Encoder_Get
* 功  能：读取指定编码器自上次读取以来的计数增量（带方向），读后立即清零
* 参  数：n —— 编码器编号：1=左编码器(TIM3)，2=右编码器(TIM4)
* 返回值：int16_t有符号计数增量：正数表示正转、负数表示反转，绝对值表示脉冲多少；
*         编号非法时返回0
* 注意点：① 必须"先读再清零"成对执行，这样每次得到的都是固定时间间隔内的增量；
*         ② 主程序按固定周期（本工程50ms）调用，增量除以每转计数408和时间即为转速；
*         ③ 返回类型用有符号int16_t，计数器超过32767会被解读为负数，正好表达反转
*/
int16_t Encoder_Get(uint8_t n)
{
	int16_t Temp;
	if (n == 1)
	{
		Temp = TIM_GetCounter(TIM3);	//取出左编码器当前计数值
		TIM_SetCounter(TIM3, 0);		//立刻清零，开始统计下一个周期
		return Temp;
	}
	else if (n == 2)
	{
		Temp = TIM_GetCounter(TIM4);	//取出右编码器当前计数值
		TIM_SetCounter(TIM4, 0);		//立刻清零
		return Temp;
	}
	return 0;							//编号既不是1也不是2时的容错返回
}
