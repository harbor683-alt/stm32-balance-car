/***************************************************************************************
  * 模块名称：BlueSerial 蓝牙串口通信驱动（USART2 + 接收环形缓冲区 + 文本帧解析）
  * 功能简介：利用STM32的USART2串口外接蓝牙串口模块（透传方式，空中是蓝牙，
  *           单片机侧看到的就是普通串口收发），实现小车与手机/上位机之间的
  *           数据收发，并把约定格式的文本数据包解析成字符串数组供main.c使用。
  * 硬件接线（以本工程代码为准，实际以原理图为准）：
  *           PA2 -> USART2_TX（单片机发送脚，配置为复用推挽AF_PP，接蓝牙模块的RXD）
  *           PA3 -> USART2_RX（单片机接收脚，配置为上拉输入IPU，接蓝牙模块的TXD）
  *           另接VCC/GND；注意收发交叉连接（TX接RX、RX接TX），双方还要共地。
  * 串口参数：波特率9600，8位数据位，无校验，1位停止位（常写作9600,8N1），
  *           无硬件流控；蓝牙模块出厂默认波特率通常也是9600，双方必须一致。
  * 通信协议要点（应用层文本帧，由手机APP按此格式发送）：
  *           1. 用一对方括号作为一包数据的边界，例如：
  *              [joystick,LH,LV,RH,RV]  摇杆包（4个摇杆分量，main.c实际取LV、RH）
  *              [key,1,up]               按键包（按键编号1/2/3 + 动作up等）；
  *           2. 方括号内部用英文逗号','把各字段隔开；
  *           3. 驱动先在连续字节流里找到'['开始、']'结束的一帧，再按逗号拆分，
  *              结果存入BlueSerial_StringArray[0..n]字符串数组供程序判断。
  * 软件设计要点——环形缓冲区（Ring Buffer / 循环队列）：
  *           串口每收到1个字节就会触发接收中断，在中断里只做“存入数组”这一件事；
  *           主循环再慢慢取走解析。存位置PutIndex和取位置GetIndex都在长度100的
  *           数组上“走到末尾就回绕到0”，首尾相接成环形，避免中断与主循环直接抢数据，
  *           是“中断生产、主循环消费”的经典做法。
  * 在平衡车中的作用：三种遥控方式之一（RF射频/蓝牙蓝牙APP/板载按键）。
  *           蓝牙模式下接收APP摇杆与按键指令设定速度环、转向环目标；
  *           同时也用BlueSerial_SendString向上位机回传RunFlag等状态字符串。
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header

/*环形缓冲区容量（单位：字节）。注意为了区分“满”和“空”，实际最多存SIZE-1个字节*/
#define BLUE_SERIAL_BUFFER_SIZE			100

uint8_t BlueSerial_Buffer[BLUE_SERIAL_BUFFER_SIZE];	//环形缓冲区本体：存放串口收到的原始字节
uint16_t BlueSerial_PutIndex = 0;					//写入位置（生产者指针，中断里移动）
uint16_t BlueSerial_GetIndex = 0;					//读取位置（消费者指针，主循环移动）

char BlueSerial_String[100];						//解析一帧后得到的中间字符串（方括号内原文）
char BlueSerial_StringArray[6][20];					//按逗号拆分后的字段数组：最多6个字段，每个最多19字符+'\0'

void BlueSerial_IRQHandler(uint8_t RxData);			//中断回调函数前置声明（定义在文件末尾）

/**
  * 函    数：BlueSerial_Init
  * 功    能：初始化USART2蓝牙串口（GPIO + 串口参数 + 接收中断 + NVIC）
  * 参    数：无
  * 返 回 值：无
  * 说    明：USART2挂在APB1总线上，其所用GPIOA挂在APB2上，两个时钟都要打开；
  *           PA2配置为复用推挽（串口外设接管引脚输出），PA3配置为上拉输入；
  *           打开RXNE（接收寄存器非空）中断，每收到1字节进一次中断
  */
void BlueSerial_Init(void)
{
	/*开启USART2(APB1)和GPIOA(APB2)时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	/*PA2(TX发送脚)：复用推挽输出。“复用”指引脚交给USART外设控制，
	  “推挽”可主动输出高/低电平，能把数据位有力地发出去*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/*PA3(RX接收脚)：上拉输入。串口空闲电平为高，上拉可让未接设备时也稳定为高*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/*填充USART参数结构体：9600波特率、无流控、收发双使能、无校验、1停止位、8数据位（8N1）*/
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;						//每秒传输约9600个符号位，双方必须一致
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//不用RTS/CTS硬件流控
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;	//同时允许发送与接收
	USART_InitStructure.USART_Parity = USART_Parity_No;				//无校验位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;			//1位停止位
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;		//每帧8个数据位
	USART_Init(USART2, &USART_InitStructure);

	/*使能RXNE中断：每收到1个字节、接收数据寄存器变非空时触发中断*/
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	/*配置NVIC中断优先级分组2：2位抢占优先级 + 2位响应优先级（全工程统一分组）*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	/*配置USART2中断通道：抢占优先级1、响应优先级1，并使能该通道*/
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART2, ENABLE);	//最后一步：使能USART2外设，串口开始工作
}

/**
  * 函    数：BlueSerial_SendByte
  * 功    能：通过USART2阻塞式发送1个字节
  * 参    数：Byte 待发送字节
  * 返 回 值：无
  * 说    明：把数据写入发送数据寄存器后，等待TXE（发送寄存器空）标志置1，
  *           表示该字节已搬进移位寄存器发送，才可发下一字节
  */
void BlueSerial_SendByte(uint8_t Byte)
{
	USART_SendData(USART2, Byte);
	while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);	//死等发送寄存器空(TXE置1)
}

/**
  * 函    数：USART2_IRQHandler
  * 功    能：USART2中断服务函数（函数名固定，启动文件中的中断向量表会调用它）
  * 参    数：无
  * 返 回 值：无
  * 说    明：每收到1字节进一次本中断；中断里只做“读走字节->放入环形缓冲区”，
  *           尽量短，不做解析，保证不漏收后续字节
  */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)	//确认是RXNE接收中断
	{
		uint8_t RxData = USART_ReceiveData(USART2);		//读走这1字节（读操作也会清标志）
		BlueSerial_IRQHandler(RxData);					//交给回调函数存入环形缓冲区
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);	//清中断挂起标志（保险）
	}
}

/*BlueSerial*****************************************/

/*==================== 环形缓冲区：底层存取（生产者=中断，消费者=主循环） ====================*/

/**
  * 函    数：BlueSerial_Put
  * 功    能：向环形缓冲区写入1字节（在接收中断中调用，是“生产者”）
  * 参    数：Byte 新收到的字节
  * 返 回 值：0=写入成功；1=缓冲区已满，丢弃该字节
  * 说    明：“满”的判断刻意留出1个空位（PutIndex再走一步就追上GetIndex），
  *           这样可以区分两种状态：两指针相等=空；Put再进一步等于Get=满；
  *           所有下标都对SIZE取模，走到99后回绕到0，形成环
  */
uint8_t BlueSerial_Put(uint8_t Byte)
{
    if ((BlueSerial_PutIndex + 1) % BLUE_SERIAL_BUFFER_SIZE == BlueSerial_GetIndex)
    {
        return 1;	//写指针的下一个位置就是读指针：缓冲区满，丢弃本字节并返回1
    }
	BlueSerial_Buffer[BlueSerial_PutIndex] = Byte;	//把新字节放进写指针位置
	if (BlueSerial_PutIndex >= BLUE_SERIAL_BUFFER_SIZE - 1)	//已到数组末尾
	{
		BlueSerial_PutIndex = 0;					//回绕到0，体现“环形”
	}
	else
	{
		BlueSerial_PutIndex ++;						//否则写指针后移1格
	}
	return 0;	//写入成功
}

/**
  * 函    数：BlueSerial_Get
  * 功    能：从环形缓冲区取走1字节（主循环解析时调用，是“消费者”）
  * 参    数：Byte 输出参数，取到的字节通过指针带回
  * 返 回 值：0=取数成功；1=缓冲区为空，无数据可取（此时*Byte被置0）
  * 说    明：读指针与写指针相等即“空”；取走1字节后读指针后移并在末尾回绕
  */
uint8_t BlueSerial_Get(uint8_t *Byte)
{
	if (BlueSerial_GetIndex == BlueSerial_PutIndex)	//读写指针相同：没有新数据
	{
		*Byte = 0;
	    return 1;	//空，返回1
	}
	*Byte = BlueSerial_Buffer[BlueSerial_GetIndex];	//取出读指针位置的字节
	/*原作者在此预留了一句清空（默认注释掉、未启用），逻辑上也不必执行*/
//	BlueSerial_Buffer[BlueSerial_GetIndex] = 0x00;
	if (BlueSerial_GetIndex >= BLUE_SERIAL_BUFFER_SIZE - 1)	//到末尾则回绕
	{
		BlueSerial_GetIndex = 0;
	}
	else
	{
		BlueSerial_GetIndex ++;		//否则读指针后移1格
	}
	return 0;	//取数成功
}

/**
  * 函    数：BlueSerial_Length
  * 功    能：查询缓冲区中尚未取走的字节个数
  * 参    数：无
  * 返 回 值：当前有效数据长度（0~SIZE-1）
  * 说    明：先加一次SIZE再取模，是为了处理写指针已经回绕、数值上小于读指针
  *           的情况，保证结果恒为非负
  */
uint16_t BlueSerial_Length(void)
{
	return (BlueSerial_PutIndex + BLUE_SERIAL_BUFFER_SIZE - BlueSerial_GetIndex) % BLUE_SERIAL_BUFFER_SIZE;;
}

/**
  * 函    数：BlueSerial_Read
  * 功    能：“偷看”缓冲区中相对读指针偏移Index处的字节（不移动读指针、不取走数据）
  * 参    数：Index 相对当前读指针的偏移量（0表示最旧的1字节）
  * 返 回 值：该位置的字节
  * 说    明：供ReceiveFlag在不清空缓冲区的前提下扫描是否出现完整一帧[...]
  */
uint8_t BlueSerial_Read(uint16_t Index)
{
	return BlueSerial_Buffer[(BlueSerial_GetIndex + Index) % BLUE_SERIAL_BUFFER_SIZE];
}

/**
  * 函    数：BlueSerial_ClearBuffer
  * 功    能：清空接收缓冲区（读写指针归零，数组内容全部填0）
  * 参    数：无
  * 返 回 值：无
  * 说    明：切换模式或丢弃历史杂数据时调用，例如main.c进入/退出某些界面时清一次
  */
void BlueSerial_ClearBuffer(void)
{
	uint8_t i;
	BlueSerial_PutIndex = 0;
	BlueSerial_GetIndex = 0;
	for (i = 0; i < BLUE_SERIAL_BUFFER_SIZE; i ++)
	{
		BlueSerial_Buffer[i] = 0;
	}
}

/*==================== 发送封装：字节/数组/字符串 ====================*/

/**
  * 函    数：BlueSerial_SendArray
  * 功    能：连续发送Length个字节
  * 参    数：Array 待发送数组首地址；Length 字节数
  * 返 回 值：无
  */
void BlueSerial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)	//逐字节循环
	{
		BlueSerial_SendByte(Array[i]);
	}
}

/**
  * 函    数：BlueSerial_SendString
  * 功    能：发送一个以'\0'结尾的C字符串（结束符本身不发送）
  * 参    数：String 字符串首地址
  * 返 回 值：无
  * 说    明：main.c用它向蓝牙APP/上位机回传如 "RunFlag=1\r\n" 的状态文本
  */
void BlueSerial_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)	//遇到字符串结束符'\0'为止
	{
		BlueSerial_SendByte(String[i]);
	}
}

/*==================== 应用层帧解析：[]成帧、逗号分隔字段 ====================*/

/**
  * 函    数：BlueSerial_ReceiveFlag
  * 功    能：在“不取出、不清空”缓冲区的前提下，扫描是否已收到完整一帧[...]
  * 参    数：无
  * 返 回 值：1=缓冲区里同时出现了'['和']'，至少有一帧完整数据；0=尚未收齐
  * 说    明：main.c先调用本函数“问一声有没有完整包”，有才调用BlueSerial_Receive
  *           去真正解析，避免半帧数据被提前处理；扫描用BlueSerial_Read只读不移指针
  */
uint8_t BlueSerial_ReceiveFlag(void)
{
	uint8_t Flag = 0;						//是否已遇到左方括号'['
	uint16_t Length = BlueSerial_Length();	//当前缓冲区有效字节数
	for (uint16_t i = 0; i < Length; i ++)
	{
		if (BlueSerial_Read(i) == '[')
		{
			Flag = 1;
		}
		else if (BlueSerial_Read(i) == ']' && Flag == 1)
		{
			return 1;
		}
	}
	return 0;
}

/**
  * 函    数：BlueSerial_Receive
  * 功    能：从缓冲区真正取走并解析一帧数据（分两阶段）
  *           阶段1：逐字节取出，截取 '[' 与 ']' 之间的内容到BlueSerial_String，
  *                  方括号外的杂字节被丢弃；
  *           阶段2：把BlueSerial_String按英文逗号','拆分成多个子串，依次存入
  *                  BlueSerial_StringArray[0]、[1]、[2]……每个子串以'\0'结尾。
  * 参    数：无
  * 返 回 值：无（解析结果放全局变量BlueSerial_String与BlueSerial_StringArray）
  * 使    例：收到 [joystick,0,80,60,0] 后，
  *           StringArray[0]="joystick"、[2]="80"、[3]="60"；
  *           main.c用strcmp比较命令名、atoi把数字串转成整数
  */
void BlueSerial_Receive(void)
{
	uint16_t p = 0, k = 0;		//p：帧内/字段下标；k：当前字段内的字符下标
	uint8_t Flag = 0;			//是否已进入一对方括号内部
	uint16_t Length = BlueSerial_Length();
	/*---- 阶段1：从字节流中提取一对方括号内的内容（同时把数据从缓冲区取走） ----*/
	for (uint16_t i = 0; i < Length; i ++)
	{
		uint8_t Byte;
		BlueSerial_Get(&Byte);		//取走1字节（读指针移动）
		if (Byte == '[')			//遇到帧头：进入记录状态，p复位重新开始
		{
			Flag = 1;
			p = 0;
		}
		else if (Byte == ']' && Flag == 1)	//遇到帧尾且当前在帧内
		{
			BlueSerial_String[p] = '\0';	//给提取出的字符串补结束符
			break;							//本帧提取完成，跳出（缓冲区剩余字节留待下次）
		}
		else if (Flag == 1)			//帧内的普通字符
		{
			BlueSerial_String[p] = Byte;	//追加到中间字符串
			p ++;
		}
	}

	/*---- 阶段2：按逗号把字符串拆成字段二维数组 ----*/
	p = 0;						//p=当前是第几个字段，k=该字段内的字符位置
	for (uint16_t i = 0; BlueSerial_String[i] != '\0'; i ++)
	{
		if (BlueSerial_String[i] == ',')	//遇到逗号：当前字段结束
		{
			BlueSerial_StringArray[p][k] = '\0';	//给当前字段补字符串结束符
			p ++;								//切换到下一个字段
			k = 0;								//字段内字符位置清零
		}
		else									//不是逗号：普通字符
		{
			BlueSerial_StringArray[p][k] = BlueSerial_String[i];	//原样存入当前字段
			k ++;
		}
	}
	BlueSerial_StringArray[p][k] = '\0';	//循环结束后，别忘了给最后一个字段补结束符
}

/**
  * 函    数：BlueSerial_IRQHandler
  * 功    能：串口接收中断的回调函数（被USART2_IRQHandler调用）
  * 参    数：RxData 刚收到的1字节
  * 返 回 值：无
  * 说    明：中断上下文中只做一件事——把字节放进环形缓冲区；解析放主循环，
  *           保证中断足够短、不会阻塞其他中断（如1ms定时器与PID时序）
  */
void BlueSerial_IRQHandler(uint8_t RxData)
{
	BlueSerial_Put(RxData);	//生产者：中断收到的字节入队
}
