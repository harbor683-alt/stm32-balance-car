/**
  **********************************************************************
  * @file    Store.c
  * @brief   参数掉电存储模块：把MPU6050校准偏移、PID/速度等参数存入
  *          片内Flash，开机时再从Flash加载回内存
  * @hardware 占用STM32F103C8T6主Flash的最后一页（1KB）：
  *          起始地址0x0800FC00 = 0x08000000 + 64KB - 1KB，
  *          专门留给参数使用，与前面存放程序代码的页面互不影响
  * @role    上层DebugMode.c调用：校准时SaveParam()→Store_Save()写入；
  *          main.c开机时Store_Init()自动判断并加载，LoadParam()取回
  * @note    设计思路（"RAM镜像+整页重写"）：
  *          1.Flash不能按字节随意改写（只能1→0、写前要擦除整页），
  *            所以在RAM中开辟数组Store_Data[512]作为参数的"镜像"；
  *          2.512个半字=512*2=1024字节，恰好占满1页；
  *          3.程序运行中只改RAM数组，需要保存时先擦页再把整个数组
  *            顺序写回Flash；
  *          4.数组第0个半字固定存"标志钥匙"STORE_KEY，开机读它判断
  *            这一页是"已初始化的参数区"还是"从未用过的空白Flash"
  **********************************************************************
  */
#include "stm32f10x.h"                  // Device header
#include "MyFLASH.h"

#define STORE_START_ADDRESS		0x0800FC00	//参数存储区首地址（64KB Flash的最后一页）
#define STORE_COUNT				512			//参数容量：512个半字，共1024字节=恰好1页

#define STORE_KEY				0xB5B5		//参数区有效标志（钥匙），固定放在第0个半字

uint16_t Store_Data[STORE_COUNT];			//参数的RAM镜像：程序里读写都操作这个数组，
											//下标0固定为STORE_KEY，下标1起为各参数

/**
  * @brief  参数模块初始化：开机时调用，检查Flash参数区并加载到RAM数组
  * @param  无
  * @retval 0：参数区有效，直接加载了已有参数；
  *          1：首次使用（无钥匙标志），已擦页并建立空白参数区，
  *             调用者(main.c)据此把程序内默认参数写入并提示重新校准
  * @note   Flash擦除后每个半字都是0xFFFF，不可能恰好等于0xB5B5，
  *         所以用钥匙值能可靠区分"空白页"和"已初始化页"
  */
uint8_t Store_Init(void)
{
	uint8_t Flag = 0;	//返回标志，默认0（已有参数）
	
	if (MyFLASH_ReadHalfWord(STORE_START_ADDRESS) != STORE_KEY)	//首地址处不是钥匙→参数区无效
	{
		MyFLASH_ErasePage(STORE_START_ADDRESS);					//先擦除整页（全部变0xFFFF）
		MyFLASH_ProgramHalfWord(STORE_START_ADDRESS, STORE_KEY);	//在第0个半字写入钥匙标志
		for (uint16_t i = 1; i < STORE_COUNT; i ++)
		{
			MyFLASH_ProgramHalfWord(STORE_START_ADDRESS + i * 2, 0x0000);	//其余位置全部写成0
			//半字占2字节，所以第i个半字的地址=首地址 + i*2
		}
		Flag = 1;	//标记：本次是首次上电、参数区刚被初始化
	}
	
	for (uint16_t i = 0; i < STORE_COUNT; i ++)
	{
		Store_Data[i] = MyFLASH_ReadHalfWord(STORE_START_ADDRESS + i * 2);	//把整页512个半字
																			//读到RAM数组备用
	}
	
	return Flag;	//把"是否首次使用"的结果告诉调用者
}

/**
  * @brief  保存参数：把RAM镜像Store_Data整页写入Flash（掉电不丢失）
  * @param  无
  * @retval 无
  * @note   无论改了几个参数，都必须"先擦整页、再逐个半字重写"，
  *         这是Flash"只能1→0"的硬件特性决定的；写第0个半字时
  *         Store_Data[0]中的钥匙标志也会一并写回
  */
void Store_Save(void)
{
	MyFLASH_ErasePage(STORE_START_ADDRESS);	//擦除参数页
	for (uint16_t i = 0; i < STORE_COUNT; i ++)
	{
		MyFLASH_ProgramHalfWord(STORE_START_ADDRESS + i * 2, Store_Data[i]);	//逐半字写回
	}
}

/**
  * @brief  清空参数：把除钥匙外的全部参数清零并立即保存
  * @param  无
  * @retval 无
  * @note   下标0的STORE_KEY保留不动，否则下次开机会被误认为空白页
  */
void Store_Clear(void)
{
	for (uint16_t i = 1; i < STORE_COUNT; i ++)
	{
		Store_Data[i] = 0x0000;	//只在RAM数组中清零
	}
	Store_Save();				//再统一写入Flash
}
