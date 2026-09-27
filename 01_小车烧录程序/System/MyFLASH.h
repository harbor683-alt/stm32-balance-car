/**
  **********************************************************************
  * @file    MyFLASH.h
  * @brief   片内Flash读写驱动的头文件，对外声明读/擦/写共7个函数
  * @note    使用前请先阅读MyFLASH.c文件头中关于"页擦除、半字编程、
  *          解锁加锁"的说明
  **********************************************************************
  */
#ifndef __MYFLASH_H		//头文件包含卫士
#define __MYFLASH_H

/*—— 读取类：读Flash无需解锁，直接按地址读 ——*/
uint32_t MyFLASH_ReadWord(uint32_t Address);		//读一个32位字（4字节）
uint16_t MyFLASH_ReadHalfWord(uint32_t Address);	//读一个16位半字（2字节）
uint8_t MyFLASH_ReadByte(uint32_t Address);			//读一个8位字节

/*—— 擦除类：最小单位是页，擦后整页为0xFFFF ——*/
void MyFLASH_EraseAllPages(void);					//擦除整片Flash（慎用！会擦掉程序）
void MyFLASH_ErasePage(uint32_t PageAddress);		//只擦除指定地址所在的一页（1KB）

/*—— 编程（写入）类：内部已自动完成解锁/加锁 ——*/
void MyFLASH_ProgramWord(uint32_t Address, uint32_t Data);		//写一个32位字
void MyFLASH_ProgramHalfWord(uint32_t Address, uint16_t Data);	//写一个16位半字

#endif
