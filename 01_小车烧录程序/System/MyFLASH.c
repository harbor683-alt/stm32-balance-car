/**
  **********************************************************************
  * @file    MyFLASH.c
  * @brief   片内Flash（闪存）读写驱动：读字/半字/字节、页擦除、编程
  * @hardware STM32F103C8T6的主Flash，容量64KB，起始地址0x08000000，
  *          按"页"划分，每页1KB（中容量产品），即地址每0x400为一页
  * @role    是Store模块的底层工具。MPU6050校准偏移、速度档位等参数
  *          通过本模块写入Flash，实现"掉电保存"（断电后数据不丢失，
  *                   下次开机仍可读出）
  * @note    Flash的三条重要特性（务必理解）：
  *          1.擦除的最小单位是"页"，不能只擦一个字节；擦除后整页
  *            全部变为0xFFFF（所有位都是1）；
  *          2.编程（写入）的最小单位是"半字"(16位)，且硬件只能把
  *            位从1写成0，不能把0改回1，所以写入新数据前必须先擦除；
  *          3.Flash默认处于"加锁"状态以防程序跑飞误写，擦除/编程前
  *            要先FLASH_Unlock解锁，操作完再FLASH_Lock重新加锁
  **********************************************************************
  */
#include "stm32f10x.h"                  // Device header

/**
  * @brief  从指定地址读取一个32位"字"（4字节）
  * @param  Address 要读取的Flash绝对地址（如0x0800FC00）
  * @retval 该地址处连续4字节组成的32位数据
  * @note   读Flash不受锁保护，直接把地址强制转换成指针再解引用即可；
  *         __IO表示volatile，告诉编译器每次都真实读取、不要优化
  */
uint32_t MyFLASH_ReadWord(uint32_t Address)
{
	return *((__IO uint32_t *)(Address));
}

/**
  * @brief  从指定地址读取一个16位"半字"（2字节）
  * @param  Address 要读取的Flash绝对地址（必须2字节对齐，即偶数地址）
  * @retval 该地址处连续2字节组成的16位数据
  * @note   本工程Store模块按半字存储参数，用得最多的就是这个函数
  */
uint16_t MyFLASH_ReadHalfWord(uint32_t Address)
{
	return *((__IO uint16_t *)(Address));
}

/**
  * @brief  从指定地址读取一个8位"字节"
  * @param  Address 要读取的Flash绝对地址
  * @retval 该地址处的8位数据
  */
uint8_t MyFLASH_ReadByte(uint32_t Address)
{
	return *((__IO uint8_t *)(Address));
}

/**
  * @brief  擦除整片主Flash（全部页变为0xFFFF）
  * @param  无
  * @retval 无
  * @note   固定三步：解锁→擦除→重新加锁。
  *         警告：会连程序本身一起擦掉！本工程实际保存参数只擦除
  *         指定的最后一页（见MyFLASH_ErasePage），此函数仅作工具备用
  */
void MyFLASH_EraseAllPages(void)
{
	FLASH_Unlock();				//向Flash钥匙寄存器写入密钥序列，解除写保护
	FLASH_EraseAllPages();		//执行整片擦除（硬件自动完成，期间CPU暂停等待）
	FLASH_Lock();				//重新加锁，防止后续误操作改写Flash
}

/**
  * @brief  擦除指定的一个页（该页1KB全部变为0xFFFF）
  * @param  PageAddress 页内任意地址（库函数会自动对齐到页首地址）
  * @retval 无
  * @note   本工程擦除参数区时传入0x0800FC00（64KB Flash的最后一页），
  *         不会影响前面存放程序代码的页面
  */
void MyFLASH_ErasePage(uint32_t PageAddress)
{
	FLASH_Unlock();						//解锁
	FLASH_ErasePage(PageAddress);		//擦除指定页
	FLASH_Lock();						//加锁
}

/**
  * @brief  向指定地址编程（写入）一个32位"字"
  * @param  Address 目标绝对地址（4字节对齐）
  * @param  Data    要写入的32位数据
  * @retval 无
  * @note   写之前目标位置必须已被擦除（为0xFFFFFFFF），否则写不正确
  */
void MyFLASH_ProgramWord(uint32_t Address, uint32_t Data)
{
	FLASH_Unlock();						//解锁
	FLASH_ProgramWord(Address, Data);	//把一个字写入Flash
	FLASH_Lock();						//加锁
}

/**
  * @brief  向指定地址编程（写入）一个16位"半字"
  * @param  Address 目标绝对地址（2字节对齐）
  * @param  Data    要写入的16位数据
  * @retval 无
  * @note   半字是STM32F1 Flash编程的最小单位，Store模块保存参数时
  *         就是逐个半字调用本函数写入的
  */
void MyFLASH_ProgramHalfWord(uint32_t Address, uint16_t Data)
{
	FLASH_Unlock();							//解锁
	FLASH_ProgramHalfWord(Address, Data);	//把一个半字写入Flash
	FLASH_Lock();							//加锁
}
