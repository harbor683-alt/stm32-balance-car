/*
* 文件名称：Serial.c
* 模块名称：USART1有线调试串口驱动（PA9发送、PA10接收）
* 功能简介：完成串口初始化（9600波特率、8位数据位、无校验、1位停止位），
*           提供字节、数组、字符串、定长数字以及printf风格等多种发送函数；
*           接收采用中断方式：每收到1个字节自动进入中断保存数据并置标志，
*           主程序可随时查询标志并取走数据
* 硬件接线：USART1_TX = PA9（配置为复用推挽输出），USART1_RX = PA10（配置为上拉输入）；
*           外接USB转TTL模块即可与电脑上的串口助手通信，
*           注意双方TX、RX要交叉连接，并且共地
* 调用关系：main函数开头调用Serial_Init；调试时可用printf或Serial_Printf输出变量；
*           收到的数据通过Serial_GetRxFlag判断、Serial_GetRxData读取
* 初学者提示：① 波特率表示每秒传输的码元数，收发双方必须设成相同波特率（这里9600）；
*           ② 8N1指8个数据位、无校验位、1个停止位，是最常见的串口帧格式；
*           ③ 中断接收不需要CPU轮询等待，不会卡住主循环，适合接收时机不确定的数据；
*           ④ 本文件重定义了fputc函数，之后工程里直接写printf，内容就会从本串口发出
*/

#include "stm32f10x.h"                  // Device header
#include <stdio.h>						//标准输入输出库：提供vsprintf，并重定义fputc以支持printf
#include <stdarg.h>						//可变参数库：让Serial_Printf支持不定个数的参数

uint8_t Serial_RxData;					//存放最近一次接收到的1个字节
uint8_t Serial_RxFlag;					//接收标志：收到字节置1，被主程序读取后清0

/*
* 函数名：Serial_Init
* 功  能：初始化USART1：配置收发引脚、9600-8N1通信参数、接收中断及其中断优先级
* 参  数：无
* 返回值：无
* 注意点：NVIC优先级分组整个工程只需配置一次，这里使用分组2（2位抢占优先级+2位响应优先级）
*/
void Serial_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);		//开启USART1时钟（USART1挂在APB2总线）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA时钟（PA9、PA10）
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;				//PA9是发送脚TX，交给USART外设输出 → 复用推挽输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;				//PA10是接收脚RX，配置为上拉输入（空闲时保持高电平，减少误码）
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;								//波特率9600，电脑端串口助手也必须设9600
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//不使用硬件流控（RTS/CTS）
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;			//同时使能发送和接收
	USART_InitStructure.USART_Parity = USART_Parity_No;						//无校验位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;					//1个停止位
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;				//8个数据位（与上面两项合称8N1）
	USART_Init(USART1, &USART_InitStructure);								//把以上参数写入USART1
	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);			//开启"接收寄存器非空(RXNE)"中断：每收到1个字节就触发一次中断
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);			//配置中断优先级分组2：2位抢占优先级、2位响应优先级（全工程统一）
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;						//选择USART1对应的中断通道
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;							//使能该中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;				//抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;						//响应优先级1（调试串口优先级低于平衡控制定时器）
	NVIC_Init(&NVIC_InitStructure);											//写入NVIC配置
	
	USART_Cmd(USART1, ENABLE);							//最后使能USART1外设，串口正式开始收发
}

/*
* 函数名：Serial_SendByte
* 功  能：通过USART1发送1个字节
* 参  数：Byte —— 待发送的8位数据
* 返回值：无
* 注意点：TXE标志=1表示发送数据寄存器已空（上一个字节已被取走），
*         必须等它置1才能写下一个字节，否则会覆盖丢数据。这里用while轮询等待
*/
void Serial_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);												//把字节写入发送数据寄存器，硬件自动开始移位发送
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);				//等待发送寄存器变空（TXE=1），空了才说明此字节已接管
}

/*
* 函数名：Serial_SendArray
* 功  能：连续发送一段字节数组（可用来发送任意二进制数据）
* 参  数：Array  —— 数组首地址（指针）
*         Length —— 要发送的字节个数
* 返回值：无
* 注意点：循环复用Serial_SendByte逐个字节发送
*/
void Serial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)
	{
		Serial_SendByte(Array[i]);			//每个元素按一个字节依次发出
	}
}

/*
* 函数名：Serial_SendString
* 功  能：发送一个C语言字符串
* 参  数：String —— 以'\0'结尾的字符串首地址
* 返回值：无
* 注意点：C字符串末尾有隐藏的结束符'\0'，循环遇到它就停止，结束符本身不发送
*/
void Serial_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)		//逐字符判断，直到遇到字符串结束符
	{
		Serial_SendByte(String[i]);
	}
}

/*
* 函数名：Serial_Pow
* 功  能：计算X的Y次方（仅内部使用，供Serial_SendNumber逐位取数字）
* 参  数：X —— 底数，Y —— 指数（非负整数）
* 返回值：X^Y 的结果
* 注意点：本文件内部工具函数，未在Serial.h中对外声明
*/
uint32_t Serial_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;			//连乘Y次
	}
	return Result;
}

/*
* 函数名：Serial_SendNumber
* 功  能：以固定位数发送一个无符号整数的十进制文本（不足前面补0）
* 参  数：Number —— 待发送的数字
*         Length —— 指定发送几位，例如Number=25、Length=4会发出"0025"
* 返回值：无
* 注意点：串口只能发字符，数字要逐位转成ASCII码：
*         "除以10的n次方取整，再对10取余"得到某一位数字，加'0'即转为对应字符
*/
void Serial_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)
	{
		//从最高位到最低位依次取出一位数字，+'0'把数字0~9转成字符'0'~'9'
		Serial_SendByte(Number / Serial_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/*
* 函数名：fputc
* 功  能：重定义C库标准函数fputc（printf内部最终会调用它输出每个字符）
* 参  数：ch —— 待输出字符；f —— 文件流指针（此处未使用）
* 返回值：输出的字符
* 注意点：有了这个重定向，工程中直接调用printf就会把格式化文本从USART1发出；
*         个别工程还需在Keil中勾选"Use MicroLIB"才能正常使用printf
*/
int fputc(int ch, FILE *f)
{
	Serial_SendByte(ch);			//把printf要输出的每个字符改走本串口
	return ch;
}

/*
* 函数名：Serial_Printf
* 功  能：自定义的printf风格发送函数，用法与printf相同，如Serial_Printf("x=%d\r\n", x);
* 参  数：format —— 格式字符串；后面可跟任意多个待格式化的参数（即可变参数...）
* 返回值：无
* 注意点：① 先用vsprintf把格式化结果拼接到本地数组，再一次性按字符串发出；
*         ② 缓冲区只有100字节，格式化后的内容（含结尾'\0'）不能超过100字节，否则越界
*/
void Serial_Printf(char *format, ...)
{
	char String[100];				//存放格式化结果的缓冲区
	va_list arg;					//可变参数列表对象
	va_start(arg, format);			//从format之后开始收集可变参数
	vsprintf(String, format, arg);	//按格式字符串把参数拼成普通字符串（sprintf的可变参数版本）
	va_end(arg);					//结束可变参数收集
	Serial_SendString(String);		//把拼好的字符串从串口发出
}

/*
* 函数名：Serial_GetRxFlag
* 功  能：查询是否接收到了新字节
* 参  数：无
* 返回值：1表示自上次查询后又收到了数据，0表示没有新数据
* 注意点：与按键事件类似，返回1的同时自动清标志，保证一字节只被通知一次；
*         取数据请紧接着调用Serial_GetRxData
*/
uint8_t Serial_GetRxFlag(void)
{
	if (Serial_RxFlag == 1)
	{
		Serial_RxFlag = 0;			//读后清除标志
		return 1;
	}
	return 0;
}

/*
* 函数名：Serial_GetRxData
* 功  能：获取最近一次接收到的字节内容
* 参  数：无
* 返回值：接收到的1个字节
* 注意点：建议先用Serial_GetRxFlag判断有新数据后再调用本函数
*/
uint8_t Serial_GetRxData(void)
{
	return Serial_RxData;			//直接返回中断中保存的数据
}

/*
* 函数名：USART1_IRQHandler
* 功  能：USART1中断服务函数（函数名固定，由启动文件中的中断向量表调用，不能改名）
* 参  数：无
* 返回值：无
* 注意点：① 每收到1个字节硬件置起RXNE标志并跳到这里；
*         ② 中断里只做"取数据、置标志"这种最短的工作，复杂处理留给主循环，避免影响实时性；
*         ③ 退出前清除中断标志，防止同一中断重复进入
*/
void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)		//确认本次中断确实由"接收非空"引起
	{
		Serial_RxData = USART_ReceiveData(USART1);				//读出收到的字节（读数据寄存器本身也会清RXNE标志）
		Serial_RxFlag = 1;										//置接收标志，通知主程序有新数据
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);			//显式再清一次中断标志，双保险
	}
}
