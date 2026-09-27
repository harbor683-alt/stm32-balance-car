
/***************************************************************************************
  * 本程序由江协科技创建并免费开源共享
  * 你可以任意查看、使用和修改，并应用到自己的项目之中
  * 程序版权归江协科技所有，任何人或组织不得将其据为己有
  * 
  * 程序名称：				NRF24L01无线通信模块驱动程序
  * 程序创建时间：			2025.6.9
  * 当前程序版本：			V1.0
  * 当前版本发布时间：		2024.6.9
  * 
  * 江协科技官方网站：		jiangxiekeji.com
  * 江协科技官方淘宝店：	jiangxiekeji.taobao.com
  * 程序介绍及更新动态：	jiangxiekeji.com/tutorial/nrf24l01.html
  * 
  * 如果你发现程序中的漏洞或者笔误，可通过邮件向我们反馈：feedback@jiangxiekeji.com
  * 发送邮件之前，你可以先到更新动态页面查看最新程序，如果此问题已经修改，则无需再发邮件
  ***************************************************************************************
  */

/***************************************************************************************
  * 模块名称：NRF24L01 2.4GHz无线收发模块驱动（GPIO模拟SPI，非硬件SPI外设）
  * 功能简介：驱动NRF24L01+无线数传芯片，实现最多32字节数据包的无线发送与接收。
  *           本驱动用4个普通GPIO模拟SPI时序（详见NRF24L01_SPI_SwapByte函数）。
  * 硬件接线（以本工程代码为准，实际以原理图为准）：
  *           PA8  -> CE    （芯片收发使能脚：高电平进入真正的收/发状态）
  *           PA15 -> CSN   （SPI片选，低有效：拉低表示本次SPI通信选中本芯片）
  *           PB3  -> SCK   （SPI时钟，主机输出）
  *           PB5  -> MOSI  （主机输出、从机输入：STM32 -> NRF）
  *           PB4  -> MISO  （主机输入、从机输出：NRF -> STM32）
  *           IRQ未使用（本程序采用“查询状态寄存器”的方式判断收发结果，故不接中断脚）
  *           特别注意：PA15/PB3/PB4默认是JTAG调试引脚，故初始化时执行了
  *           GPIO_Remap_SWJ_JTAGDisable（关闭JTAG、保留SWD下载调试）来释放这3个脚。
  * SPI协议入门（Serial Peripheral Interface，串行外设接口）：
  *           SPI用4根线：SCK时钟、CSN片选、MOSI主出从入、MISO主入从出；
  *           片选拉低期间，SCK每跳一个脉冲，主机与从机同时“交换”1位（一发一收），
  *           8个脉冲交换1字节，高位先行；本工程采用SPI模式0（CPOL=0空闲低电平，
  *           CPHA=0第一个边沿采样/输出），与NRF24L01要求一致。
  * NRF24L01无线核心概念（初学者必看）：
  *           1. PTX/PRX：发送方叫PTX（Primary TX，主发射），接收方叫PRX
  *              （Primary RX，主接收）。本小车平时处于PRX接收手柄指令；要回传数据
  *              时NRF24L01_Send会临时切成PTX，发完立刻切回PRX；
  *           2. 管道Pipe：PRX内部有6条接收管道（Pipe0~5），相当于6个“收信地址”，
  *              可同时监听不同地址；本工程只开了管道0；
  *           3. 地址Address：5字节，相当于“频道里的门牌号”，只有发送目标地址与
  *              接收管道地址完全一致才能通信。本工程双端统一为 11 22 33 44 55；
  *           4. 射频频道RF_CH：工作频点 = 2400MHz + RF_CH，本工程为2，即2.402GHz，
  *              双端必须相同；若现场有WiFi干扰可双方一起换频道；
  *           5. 自动应答ACK与自动重传（Enhanced ShockBurst）：PTX每发一包，PRX
  *              收到后自动回一个ACK，PTX没收到ACK就按配置自动重发（本工程重传3次、
  *              间隔250us），因此PTX发送时还需把管道0地址临时设成自己的发送地址，
  *              用于接收对方的ACK；
  *           6. 有效载荷Payload：真正的数据，固定长度32字节一包（FIFO先进先出缓存）。
  * 在平衡车中的作用（本工程数据包字节布局，32字节定长）：
  *           接收手柄包：RxPacket[0]=包ID(0x00/0x01)，[2]=左摇杆LV(int8)，
  *                      [3]=右摇杆RH(int8)，[5]=按键编号；
  *           回传数据包（ID=0x01时回复）：TxPacket[0]=0x02，[1]=PWML(int8)，
  *                      [2]=PWMR(int8)，[4..7]=float角度Angle，[8..11]=float左轮速度，
  *                      [12..15]=float右轮速度（float占4字节，小端，共用到16字节）。
  *           main.c主循环不断查询NRF24L01_Receive，收到手柄包就更新速度/转向目标，
  *           需要时调用NRF24L01_Send回传姿态与轮速到手柄屏幕。
  ***************************************************************************************
  */

#include "stm32f10x.h"
#include "NRF24L01_Define.h"

/*全局变量*********************/

/*【教学提示】下面4个全局数组是应用程序与驱动之间交换数据的“窗口”：
  要发数据 -> 先把内容逐字节填进NRF24L01_TxPacket，再调NRF24L01_Send()；
  收到数据 -> NRF24L01_Receive()返回1后，直接从NRF24L01_RxPacket里逐字节解析。
  地址数组也可在运行时修改（改完接收地址需调NRF24L01_UpdateRxAddress生效）*/

/*发送部分*/
uint8_t NRF24L01_TxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};		//发送地址，固定5字节
#define NRF24L01_TX_PACKET_WIDTH		32							//发送数据包宽度，范围：1~32字节
uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH];				//发送数据包

/*接收部分*/
uint8_t NRF24L01_RxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};		//接收通道0地址，固定5字节
#define NRF24L01_RX_PACKET_WIDTH		32							//接收通道0数据包宽度，范围：1~32字节
uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH];				//接收数据包

/**
  * 提示：设备A和设备B进行通信
  * A发B收时，A的发送地址、发送数据包宽度要与B的接收地址、接收数据包宽度对应相同
  * B发A收时，B的发送地址、发送数据包宽度要与A的接收地址、接收数据包宽度对应相同
  * 通常情况下，可以将A和B的发送地址、接收地址全设置一样，A和B的发送数据包宽度、接收数据包宽度也全设置一样
  * 这样A和B可以使用完全一样的模块程序，操作更加方便，也不容易搞混
  * 
  */

/*********************全局变量*/


/*引脚配置*********************/

/**
  * 函    数：NRF24L01写CE高低电平
  * 参    数：要写入CE的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写CE时，此函数会被调用
  *           用户需要根据参数传入的值，将CE置为高电平或者低电平
  *           当参数传入0时，置CE为低电平，当参数传入1时，置CE为高电平
  */
void NRF24L01_W_CE(uint8_t BitValue)
{
	/*根据BitValue的值，将CE置高电平或者低电平*/
	GPIO_WriteBit(GPIOA, GPIO_Pin_8, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写CSN高低电平
  * 参    数：要写入CSN的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写CSN时，此函数会被调用
  *           用户需要根据参数传入的值，将CSN置为高电平或者低电平
  *           当参数传入0时，置CSN为低电平，当参数传入1时，置CSN为高电平
  */
void NRF24L01_W_CSN(uint8_t BitValue)
{
	/*根据BitValue的值，将CSN置高电平或者低电平*/
	GPIO_WriteBit(GPIOA, GPIO_Pin_15, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写SCK高低电平
  * 参    数：要写入SCK的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写SCK时，此函数会被调用
  *           用户需要根据参数传入的值，将SCK置为高电平或者低电平
  *           当参数传入0时，置SCK为低电平，当参数传入1时，置SCK为高电平
  */
void NRF24L01_W_SCK(uint8_t BitValue)
{
	/*根据BitValue的值，将SCK置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_3, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01写MOSI高低电平
  * 参    数：要写入MOSI的电平值，范围：0/1
  * 返 回 值：无
  * 说    明：当上层函数需要写MOSI时，此函数会被调用
  *           用户需要根据参数传入的值，将MOSI置为高电平或者低电平
  *           当参数传入0时，置MOSI为低电平，当参数传入1时，置MOSI为高电平
  */
void NRF24L01_W_MOSI(uint8_t BitValue)
{
	/*根据BitValue的值，将MOSI置高电平或者低电平*/
	GPIO_WriteBit(GPIOB, GPIO_Pin_5, (BitAction)BitValue);
}

/**
  * 函    数：NRF24L01读MISO高低电平
  * 参    数：无
  * 返 回 值：读取得到MISO的电平值，范围：0/1
  * 说    明：当上层函数需要读MISO时，此函数会被调用
  *           用户需要读取MISO引脚，返回此引脚的高低电平状态
  *           当MISO为高电平时，返回1，当MISO为低电平时，返回0
  */
uint8_t NRF24L01_R_MISO(void)
{
	/*取MISO引脚的高低电平并返回*/
	return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4);
}

/**
  * 本代码使用查询的方式获取设备状态，因此不需要使用IRQ引脚
  */

/**
  * 函    数：NRF24L01引脚初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：当上层函数需要初始化时，此函数会被调用
  *           用户需要将CSN、CE、MISO、SCK引脚初始化为推挽输出模式，MISO引脚初始化为上拉输入模式
  */
void NRF24L01_GPIO_Init(void)
{
	/*开启AFIO（复用功能IO）时钟——使用引脚重映射前必须先开AFIO时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	/*关闭JTAG调试、保留SWD调试：PA15(JTDI)、PB3(JTDO)、PB4(JNTRST)上电后默认被
	  JTAG占用，不关闭就无法当普通GPIO用（这正是SCK/MOSI/CSN/MISO所在的脚）；
	  SWD下载只占用PA13/PA14，不受影响，故程序仍可正常下载调试*/
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
	/*开启GPIO时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	/*将CE、CSN、SCK、MOSI引脚初始化为推挽输出模式*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/*将MISO引脚初始化为上拉输入模式*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/*置引脚初始化后的默认电平*/
	NRF24L01_W_CE(0);		//CE默认为0，退出收发模式
	NRF24L01_W_CSN(1);		//CSN默认为1，不选中从机
	NRF24L01_W_SCK(0);		//SCK默认为0，对应SPI模式0
	NRF24L01_W_MOSI(0);		//MOSI默认电平随意，1和0均可
}

/*********************引脚配置*/


/*通信协议*********************/

/**
  * 函    数：SPI交换一个字节
  * 参    数：Byte 要发送的一个字节数据，范围：0x00~0xFF
  * 返 回 值：接收得到的一个字节数据，范围：0x00~0xFF
  */
uint8_t NRF24L01_SPI_SwapByte(uint8_t Byte)
{
	uint8_t i;

	/*【教学提示】SPI是“全双工交换”而非单向读写：每个SCK脉冲里，主机通过MOSI
	  移出1位的同时，从机也通过MISO移入1位，所以本函数“发出去1字节”的同时必然
	  “收回来1字节”。发指令时收回的字节（常为状态寄存器）不用即可，读数据时则
	  发送NOP(0xFF)空指令，纯粹是为了产生8个时钟把从机数据“换”回来*/
	/*此处使用SPI模式0进行通信*/
	/*循环8次，主机依次移出和移入数据的每一位*/
	for (i = 0; i < 8; i ++)
	{
		/*SPI为高位先行，因此移出高位至MOSI引脚*/
		if (Byte & 0x80)			//判断Byte的最高位
		{
			NRF24L01_W_MOSI(1);		//如果为1，则给MOSI输出1
		}
		else
		{
			NRF24L01_W_MOSI(0);		//如果为0，则给MOSI输出0
		}
		Byte <<= 1;					//Byte左移一位，最低位空出来用于接收数据位
		
		/*产生SCK上升沿*/
		NRF24L01_W_SCK(1);
		
		/*从MISO引脚移入数据，存入Byte的最低位*/
		if (NRF24L01_R_MISO())		//读取MISO引脚
		{
			Byte |= 0x01;			//如果为1，则给Byte最低位置1
		}							//如果为0，则不做任何操作，因为左移后低位默认补0
		
		/*产生SCK下降沿*/
		NRF24L01_W_SCK(0);
	}
	
	/*返回Byte数据，此时的Byte为SPI交换接收得到的一个字节数据*/
	return Byte;
}

/*********************通信协议*/


/*指令实现*********************/

/**
  * 函    数：NRF24L01读取寄存器（一个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 返 回 值：指定寄存器的数据，范围：0x00~0xFF
  */
uint8_t NRF24L01_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
	
	/*发送读寄存器指令后，开始交换接收，得到指定地址的数据*/
	Data = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
	
	/*返回读到的一个字节数据*/
	return Data;
}

/**
  * 函    数：NRF24L01读取寄存器（多个字节）
  * 参    数：RegAddress 指定寄存器的地址，范围：0x00~0x1F
  * 参    数：DataArray 读取得到的数据数组，输出参数
  * 参    数：Count 指定读取的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER | RegAddress);
	
	/*发送读寄存器指令后，开始交换接收，循环接收多次，得到指定地址下的多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将接收到的数据写入到输出参数DataArray中*/
		DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入寄存器（一个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 参    数：Data 要写入的一个字节数据，范围：0x00~0xFF
  * 返 回 值：无
  */
void NRF24L01_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
	
	/*发送写寄存器指令后，开始交换发送，在指定地址下写入数据*/
	NRF24L01_SPI_SwapByte(Data);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入寄存器（多个字节）
  * 参    数：RegAddress 指定寄存器地址，范围：0x00~0x1F
  * 参    数：DataArray 要写入的数据数组，输入参数
  * 参    数：Count 指定写入的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_WriteRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写寄存器（低5位为寄存器地址）*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER | RegAddress);
	
	/*发送写寄存器指令后，开始交换发送，循环发送多次，在指定地址下写入多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将输入参数DataArray的数据写入到指定地址中*/
		NRF24L01_SPI_SwapByte(DataArray[i]);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01读取Rx有效载荷
  * 参    数：DataArray 读取得到的数据数组，输出参数
  * 参    数：Count 指定读取的数量，范围：0~32
  * 返 回 值：无
  */
void NRF24L01_ReadRxPayload(uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，读取Rx有效载荷*/
	NRF24L01_SPI_SwapByte(NRF24L01_R_RX_PAYLOAD);
	
	/*发送读取Rx有效载荷指令后，开始交换接收，循环接收多次，得到多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将读取的数据写入到输出参数DataArray中*/
		DataArray[i] = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01写入Tx有效载荷
  * 参    数：DataArray 要写入的数据数组，输入参数
  * 参    数：Count 指定写入的数量，范围：0~5
  * 返 回 值：无
  */
void NRF24L01_WriteTxPayload(uint8_t *DataArray, uint8_t Count)
{
	uint8_t i;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);
	
	/*交换发送一个字节，通信开始的第一个字节为指令码，写入Tx有效载荷*/
	NRF24L01_SPI_SwapByte(NRF24L01_W_TX_PAYLOAD);
	
	/*发送写入Tx有效载荷指令后，开始交换发送，循环发送多次，写入多个数据*/
	for (i = 0; i < Count; i ++)
	{
		/*将输入参数DataArray的数据写入到Tx有效载荷中*/
		NRF24L01_SPI_SwapByte(DataArray[i]);
	}
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01清空Tx FIFO的所有数据
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_FlushTx(void)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，清空Tx FIFO*/
	NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_TX);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01清空Rx FIFO的所有数据
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_FlushRx(void)
{
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，清空Rx FIFO*/
	NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_RX);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
}

/**
  * 函    数：NRF24L01读取状态寄存器
  * 参    数：无
  * 返 回 值：状态寄存器的值，范围：0x00~0xFF
  */
uint8_t NRF24L01_ReadStatus(void)
{
	uint8_t Status;
	
	/*CSN置低，通信开始*/
	NRF24L01_W_CSN(0);

	/*交换发送一个字节，通信开始的第一个字节为指令码，空指令*/
	/*第一个字节发送任意指令，都可以交换得到状态寄存器的值*/
	Status = NRF24L01_SPI_SwapByte(NRF24L01_NOP);
	
	/*CSN置高，通信结束*/
	NRF24L01_W_CSN(1);
	
	/*返回状态寄存器的值*/
	return Status;
}

/*********************指令实现*/


/*功能函数*********************/

/*==========================================================================
  * 【教学重点：NRF24L01工作状态是怎么切换的（PTX/PRX）】
  * 芯片处于什么模式，由两个量共同决定：
  *   ① CE引脚电平；② CONFIG寄存器的 PWR_UP(bit1上电) 与 PRIM_RX(bit0收发选择)。
  * 组合关系如下：
  *   CE=0, PWR_UP=0            -> 掉电PowerDown（最省电，寄存器内容仍保留）
  *   CE=0, PWR_UP=1            -> 待机Standby-I（随时可启动收发，切换很快）
  *   CE=1, PWR_UP=1, PRIM_RX=1 -> 接收模式PRX，持续监听空中数据包
  *   CE=1, PWR_UP=1, PRIM_RX=0 -> 发送模式PTX，发送FIFO中的数据包
  * 本工程典型节奏：初始化后停在PRX等手柄；需要回传时进PTX发一包，发完回PRX。
  * 下面4个函数就是对这几种状态的封装（均采用“读-改-写”只改目标位）。
  *==========================================================================*/

/**
  * 函    数：NRF24L01进入掉电模式（CE = 0，PWR_UP = 0）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_PowerDown(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config &= ~0x02;								//配置寄存器位1（PWR_UP）置0
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
}

/**
  * 函    数：NRF24L01进入待机模式1（CE = 0，PWR_UP = 1）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_StandbyI(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x02;									//配置寄存器位1（PWR_UP）置1
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
}

/**
  * 函    数：NRF24L01进入接收模式（CE = 1，PWR_UP = 1，PRIM_RX = 1）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_Rx(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x03;									//配置寄存器位1（PWR_UP）和位0（PRIM_RX）都置1
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
	
	/*CE置1，进入收发模式，因为PRIM_RX为1，所以进入接收模式*/
	NRF24L01_W_CE(1);
}

/**
  * 函    数：NRF24L01进入发送模式（CE = 1，PWR_UP = 1，PRIM_RX = 0）
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_Tx(void)
{
	uint8_t Config;
	
	/*CE置0，退出收发模式*/
	NRF24L01_W_CE(0);
	
	/*读-改-写操作流程，单独修改配置寄存器的某些位而不影响其他位*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);		//读取配置寄存器
	if (Config == 0xFF) {return;}					//配置寄存器全为1，出错，退出函数
	Config |= 0x02;									//配置寄存器位1（PWR_UP）置1
	Config &= ~0x01;								//配置寄存器位0（PRIM_RX）置0
	NRF24L01_WriteReg(NRF24L01_CONFIG, Config);		//写回配置寄存器
	
	/*CE置1，进入收发模式，因为PRIM_RX为0，所以进入发送模式*/
	NRF24L01_W_CE(1);
}

/**
  * 函    数：NRF24L01初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用前，需要调用此初始化函数
  */
void NRF24L01_Init(void)
{
	/*先调用底层的端口初始化*/
	NRF24L01_GPIO_Init();

	/*【教学提示：下面这批寄存器参数是“收发双方的共同语言”，两端必须完全一致，
	  包括CRC方式、地址宽度、自动重传、射频频道(2)、速率(2Mbps)、包长(32字节)、
	  地址(11 22 33 44 55)；任何一项不一致都会导致收不到包或一直重传。
	  寄存器位定义见NRF24L01_Define.h，每个写入值的逐位含义见下方行尾注释*/
	/*初始化配置一系列寄存器，寄存器值的意义需参考手册中的寄存器描述*/
	/*以下配置通信双方必须保持一致，否则无法进行通信*/
	NRF24L01_WriteReg(NRF24L01_CONFIG, 0x08);		//配置寄存器，不屏蔽中断，使能CRC，CRC为1字节，PWR_UP = 0，PRIM_RX = 0
	NRF24L01_WriteReg(NRF24L01_EN_AA, 0x3F);		//使能自动应答，开启接收通道0~通道5的自动应答
	NRF24L01_WriteReg(NRF24L01_EN_RXADDR, 0x01);	//使能接收通道，只开启接收通道0
	NRF24L01_WriteReg(NRF24L01_SETUP_AW, 0x03);		//设置地址宽度，地址宽度为5字节
	NRF24L01_WriteReg(NRF24L01_SETUP_RETR, 0x03);	//设置自动重传，间隔250us，重传3次
	NRF24L01_WriteReg(NRF24L01_RF_CH, 0x02);		//射频通道，频率为(2400 + 2)MHz = 2.402GHz
	NRF24L01_WriteReg(NRF24L01_RF_SETUP, 0x0E);		//射频设置，通信速率为2Mbps，发射功率为0dBm
	
	/*接收通道0的数据包宽度，设置为宏定义NRF24L01_RX_PACKET_WIDTH指定的值*/
	NRF24L01_WriteReg(NRF24L01_RX_PW_P0, NRF24L01_RX_PACKET_WIDTH);
	
	/*接收通道0地址，设置为全局数组NRF24L01_RxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
	
	/*清空Tx FIFO的所有数据*/
	NRF24L01_FlushTx();
	
	/*清空Rx FIFO的所有数据*/
	NRF24L01_FlushRx();
	
	/*给状态寄存器的位4（MAX_RT）、位5（TX_DS）和位6（RX_DR）写1，清标志位*/
	NRF24L01_WriteReg(NRF24L01_STATUS, 0x70);
	
	/*初始化配置完成，芯片默认进入接收模式*/
	NRF24L01_Rx();
}

/**
  * 函    数：NRF24L01发送数据包
  * 参    数：无
  * 返 回 值：发送标志位，方便用户了解发送状态
  * 			1：发送成功，无错误
  * 			2：达到了最大重发次数仍未收到应答，可能是收发双方配置不一致、接收方不存在、接收FIFO已满或者多个发送数据包碰撞
  * 			3：状态寄存器的值不合法，可能是设备不存在、断路、短路或者引脚配置不正确
  * 			4：发送超时，可能是设备未初始化、断路、短路或者引脚配置不正确
  * 说    明：调用此函数前，直接修改全局数组NRF24L01_TxAddress和NRF24L01_TxPacket来设置发送的地址和数据
  */
uint8_t NRF24L01_Send(void)
{
	uint8_t Status;
	uint8_t SendFlag;
	uint32_t Timeout;
	
	/*发送地址，设置为全局数组NRF24L01_TxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_TX_ADDR, NRF24L01_TxAddress, 5);
	
	/*接收通道0地址，此处必须也设置为发送地址，用于接收应答*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_TxAddress, 5);
	
	/*写发送有效载荷，写入全局数组NRF24L01_TxPacket指定的数据，数据宽度为NRF24L01_TX_PACKET_WIDTH*/
	NRF24L01_WriteTxPayload(NRF24L01_TxPacket, NRF24L01_TX_PACKET_WIDTH);
	
	/*发送的地址和有效载荷写入完成，进入发送模式，开始发送数据*/
	NRF24L01_Tx();

	/*【教学提示：一次PTX发送在芯片内部自动完成的全过程（Enhanced ShockBurst）】
	  CE拉高超过10us后，芯片自动：组帧(加前导码+地址+CRC) -> 通过2.402GHz空口发出
	  -> 等待PRX回ACK -> 收到ACK则置位TX_DS(bit5，发送成功)；超时未收到则按
	  SETUP_RETR配置自动重发，重发次数用尽仍失败则置位MAX_RT(bit4)。
	  CPU要做的只是“反复读STATUS寄存器”看哪个标志置1，这就是下面while循环的意义。
	  注意：这3个标志位都是“写1清零”（W1C），处理完必须向对应位写1才能清除*/
	/*指定超时时间，即循环读取状态寄存器的次数，具体值可以实测确定*/
	Timeout = 10000;
	
	/*循环读取状态寄存器*/
	while (1)
	{
		/*读取状态寄存器，保存至Status变量*/
		Status = NRF24L01_ReadStatus();
		
		/*超时计次*/
		Timeout --;
		if (Timeout == 0)			//如果计次减至0
		{
			SendFlag = 4;			//发送超时，置标志位为4
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		
		/*根据状态寄存器的值，判断发送状态*/
		if ((Status & 0x30) == 0x30)		//状态寄存器位4（MAX_RT）和位5（TX_DS）同时为1
		{
			SendFlag = 3;			//状态寄存器的值不合法，置标志位为3
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		else if ((Status & 0x10) == 0x10)	//状态寄存器位4（MAX_RT）为1
		{
			SendFlag = 2;			//达到了最大重发次数仍未收到应答，置标志位为2
			NRF24L01_Init();		//发送出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
			break;					//跳出循环
		}
		else if ((Status & 0x20) == 0x20)	//状态寄存器位5（TX_DS）为1
		{
			SendFlag = 1;			//发送成功，无错误，置标志位为1
			break;					//跳出循环
		}
	}
	
	/*给状态寄存器的位4（MAX_RT）和位5（TX_DS）写1，清标志位*/
	NRF24L01_WriteReg(NRF24L01_STATUS, 0x30);
	
	/*清空Tx FIFO的所有数据*/
	NRF24L01_FlushTx();
	
	/*发送完成后，恢复接收通道0原来的地址*/
	/*如果发送地址和接收通道0地址设置相同，则可不执行这一句*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
	
	/*发送完成，芯片恢复为接收模式*/
	NRF24L01_Rx();
		
	/*返回发送标志位*/
	return SendFlag;
}

/**
  * 函    数：NRF24L01接收数据包
  * 参    数：无
  * 返 回 值：接收标志位，方便用户了解接收状态
  * 			0：未接收到数据包
  * 			1：成功接收到一个数据包
  * 			2：状态寄存器的值不合法，可能是设备不存在、断路、短路或者引脚配置不正确
  * 			3：设备仍处于掉电模式，可能是设备未初始化、曾经断电过、断路、短路或者引脚配置不正确
  * 说    明：如果收到了数据包，则可直接从全局数组NRF24L01_RxPacket取数据
  */
uint8_t NRF24L01_Receive(void)
{
	uint8_t Status, Config;
	uint8_t ReceiveFlag;

	/*【教学提示：PRX接收采用“查询法”，不使用IRQ中断脚】
	  芯片处于接收模式时，一旦正确收到一包(地址、CRC都对)，就把数据放进3级
	  Rx FIFO缓存，并把状态寄存器的RX_DR(bit6)置1。本函数每次被main循环调用时
	  读一次STATUS：发现RX_DR=1就读出FIFO里的这包数据到NRF24L01_RxPacket，
	  随后写1清RX_DR并FlushRx，准备接收下一包；没有包就返回0，不阻塞主循环*/
	/*读取状态寄存器，保存至Status变量*/
	Status = NRF24L01_ReadStatus();
	
	/*读取配置寄存器，保存至Config变量*/
	Config = NRF24L01_ReadReg(NRF24L01_CONFIG);
	
	/*根据配置寄存器和状态寄存器的值，判断接收状态*/
	if ((Config & 0x02) == 0x00)		//配置寄存器位1（PWR_UP）为0
	{
		ReceiveFlag = 3;				//设备仍处于掉电模式，置标志位为3
		NRF24L01_Init();				//接收出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
	}
	else if ((Status & 0x30) == 0x30)	//状态寄存器位4（MAX_RT）和位5（TX_DS）同时为1
	{
		ReceiveFlag = 2;				//状态寄存器的值不合法，置标志位为2
		NRF24L01_Init();				//接收出错，重新初始化一次设备，这样有助于设备从错误中恢复正常
	}
	else if ((Status & 0x40) == 0x40)	//状态寄存器位6（RX_DR）为1
	{
		ReceiveFlag = 1;				//接收到数据，置标志位为1
		
		/*读接收有效载荷，存放在全局数组NRF24L01_RxPacket中，数据宽度为NRF24L01_RX_PACKET_WIDTH*/
		NRF24L01_ReadRxPayload(NRF24L01_RxPacket, NRF24L01_RX_PACKET_WIDTH);
		
		/*给状态寄存器的位6（RX_DR）写1，清标志位*/
		NRF24L01_WriteReg(NRF24L01_STATUS, 0x40);

		/*清空Rx FIFO的所有数据*/
		NRF24L01_FlushRx();
	}
	else
	{
		ReceiveFlag = 0;				//未接收到数据，置标志位为0
	}
	
	/*返回接收标志位*/
	return ReceiveFlag;
}

/**
  * 函    数：NRF24L01更新接收地址
  * 参    数：无
  * 返 回 值：无
  * 说    明：如果想在运行时动态修改接收地址，则可先向全局数组NRF24L01_RxAddress写入修改的地址
  * 		  然后再调用此函数，使修改的接收地址生效
  */
void NRF24L01_UpdateRxAddress(void)
{
	/*接收通道0地址，设置为全局数组NRF24L01_RxAddress指定的地址，地址宽度固定为5字节*/
	NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress, 5);
}

/*********************功能函数*/


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
