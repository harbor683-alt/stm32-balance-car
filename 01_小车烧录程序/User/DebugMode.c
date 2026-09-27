/**
  **********************************************************************
  * @file    DebugMode.c
  * @brief   调试模式模块：开机菜单中的硬件测试、MPU6050传感器校准，
  *          以及参数保存到Flash/从Flash加载
  * @hardware OLED显示屏、4个按键(K1~K4)、编码电机+编码器、MPU6050
  *          六轴传感器、板载串口USART1、蓝牙串口、片内Flash(经Store)
  * @role    不入主控闭环，只在调试/校准阶段运行：main.c检测到进入调试
  *          界面后调用DebugMode()；校好的GY零漂、角度偏移等通过
  *          SaveParam()存入Flash，main.c开机用LoadParam()取回，
  *          因此校准一次，掉电后无需重新校准
  * @note    本文件大量使用extern引用main.c中定义的全局变量；extern意为
  *          "该变量在别的.c文件中定义，这里只声明、借用一下"，不另开辟内存
  **********************************************************************
  */
#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "LED.h"
#include "Key.h"
#include "Motor.h"
#include "Encoder.h"
#include "MPU6050.h"
#include "Timer.h"
#include "PID.h"
#include "BlueSerial.h"
#include "Serial.h"
#include "Store.h"
#include <math.h>
#include <stdio.h>

extern int16_t AX, AY, AZ, GX, GY, GZ;	//MPU6050六轴原始数据：AX/AY/AZ三轴加速度，GX/GY/GZ三轴角速度（定义在main.c）
extern int16_t GY_Offset;				//Y轴角速度(GY)零漂校准值，校准后叠加到读数上消除静止漂移

extern float AngleAcc;					//由加速度计解算出的车身当前倾角（单位：度）
extern float AngleAcc_Offset;			//竖直状态下角度的偏差校准值

extern uint8_t DebugFlag;				//调试模式标志：1=处于调试模式（main.c中断里据此跳过PID控制）
extern uint16_t SpeedLevel; 

/**
  * @brief  保存参数：把需要掉电记忆的变量填入Store_Data数组并写入Flash
  * @param  无
  * @retval 无
  * @note   Flash按16位半字存储，而AngleAcc_Offset是32位float，无法直接
  *         存放，于是把它的4字节内存拆成低、高两个半字，分别存到[3]、[4]；
  *         (uint16_t *)&变量 是先把float指针强转为半字指针，+1再取下一个半字，
  *         这是嵌入式里保存float的常用技巧（小端模式：低地址存低半字）
  */
void SaveParam(void)
{
	/*保存参数至FLASH*/
	Store_Data[1] = SpeedLevel;							//[1]速度档位
	Store_Data[2] = GY_Offset;							//[2]陀螺仪Y轴零漂校准值
	Store_Data[3] = *((uint16_t *)&AngleAcc_Offset);;	//[3]角度偏移(float)的低16位
	Store_Data[4] = *((uint16_t *)&AngleAcc_Offset + 1);;	//[4]角度偏移(float)的高16位
	Store_Save();										//调用Store模块整页擦写，真正落盘到Flash
}

/**
  * @brief  加载参数：开机时把Store_Data中的参数还原回各全局变量
  * @param  无
  * @retval 无
  * @note   加载顺序与SaveParam严格对应：[4]左移16位作高半字、与[3]拼成
  *         32位整数Temp，再把Temp的4字节内存强转回float，数值即被还原
  */
void LoadParam(void)
{
	/*从FLASH加载参数*/
	uint32_t Temp;										//暂存拼好的32位数据
	SpeedLevel = Store_Data[1];							//还原速度档位
	GY_Offset = Store_Data[2];							//还原陀螺仪零漂
	Temp = (Store_Data[4] << 16) | Store_Data[3];		//高半字<<16 与 低半字 拼成完整32位
	AngleAcc_Offset = *(float *)&Temp;					//把同样的32位二进制按float解释，还原浮点数值
}

/**
  * @brief  硬件测试：OLED菜单驱动的三页自检程序
  * @param  无
  * @retval 无（长按K4直接return返回上一级菜单）
  * @note   三个测试页用PageSelect切换：
  *         第1页 编码电机——按键给定PWM，OLED显示PWM/速度/位置；
  *         第2页 MPU6050——显示六轴原始数据与器件ID；
  *         第3页 串口——板载串口与蓝牙串口互相收发测试。
  *         该函数内含while(1)死循环，退出唯一方式是长按K4
  */
void HardwareTest(void)			//硬件测试
{
	uint8_t PageSelect = 1;				//当前测试页编号(1~3)
	int16_t PWML = 0, PWMR = 0;		//左右电机PWM给定值
	int16_t SpdL = 0, SpdR = 0;		//左右编码器读到的速度
	int16_t LocL = 0, LocR = 0;		//由速度累加得到的粗略位置
	
	uint8_t TX1 = 0, RX1 = 0;		//板载串口的发送/接收测试字节
	uint8_t TX2 = 0, RX2 = 0;		//蓝牙串口的发送/接收测试字节
	
	OLED_Clear();					//进测试前清屏，防止上一级菜单残留
	
	while (1)
	{
		if (Key_Check(KEY_4, KEY_SINGLE))	//K4键切换测试项目
		{
			Key_Clear();			//清除按键事件，避免同一次按压被重复触发
			OLED_Clear();			//换页时清屏
			PageSelect ++;			//页码加1
			if (PageSelect > 3)		//只有3页，翻到第3页后回到第1页
			{
				PageSelect = 1;
			}
			PWML = 0;				//切页时电机停转，防止在别的页面电机继续运转
			PWMR = 0;
			Motor_SetPWM(1, PWML);
			Motor_SetPWM(2, PWMR);
		}
		if (Key_Check(KEY_4, KEY_LONG)) {Key_Clear(); return;}	//K4键长按退出
		
		if (PageSelect == 1)		//编码电机测试
		{
			/*PWM范围限定在-100~+100：正负号代表正转/反转方向，数值大小代表占空比（力气） */
			if (Key_Check(KEY_1, KEY_SINGLE))	//K1键增大PWM
			{
				if (PWML < 100) {PWML += 10;}
				if (PWMR < 100) {PWMR += 10;}
			}
			if (Key_Check(KEY_2, KEY_SINGLE))	//K2键减小PWM
			{
				if (PWML > -100) {PWML -= 10;}
				if (PWMR > -100) {PWMR -= 10;}
			}
			if (Key_Check(KEY_3, KEY_SINGLE))	//K3键PWM归零
			{
				PWML = 0;
				PWMR = 0;
			}
			
			Motor_SetPWM(1, PWML);		//设定PWM
			Motor_SetPWM(2, PWMR);
			
			SpdL = Encoder_Get(1);		//读取编码器速度
			SpdR = Encoder_Get(2);
			
			/*速度对时间累加即"积分"，近似得到轮子转过的位置；对1000取余防止数值无限增大、便于显示 */
			LocL += SpdL; LocL %= 1000;	//累加得到位置
			LocR += SpdR; LocR %= 1000;
						
			OLED_Printf(0, 0, OLED_8X16,  "编码电机测试 1/3");
			OLED_Printf(0, 16, OLED_8X16, "PWML:%+04d R:%+04d", PWML, PWMR);
			OLED_Printf(0, 32, OLED_8X16, "SpdL:%+04d R:%+04d", SpdL, SpdR);
			OLED_Printf(0, 48, OLED_8X16, "LocL:%+04d R:%+04d", LocL, LocR);
			OLED_Update();
			
			Delay_ms(50);
		}
		else if (PageSelect == 2)	//MPU6050测试
		{
			/*静止时加速度应为±1g附近、角速度应接近0；右下角显示器件ID，ID读对说明I2C通信正常 */
			MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);		//读取MPU6050原始数据
			
			OLED_Printf(0, 0, OLED_8X16,  "MPU6050测试  2/3");
			OLED_Printf(0, 16, OLED_8X16, "%+06d %+06d", AX, GX);
			OLED_Printf(0, 32, OLED_8X16, "%+06d %+06d", AY, GY);
			OLED_Printf(0, 48, OLED_8X16, "%+06d %+06d", AZ, GZ);
			OLED_Printf(112, 16, OLED_8X16, "%02X", MPU6050_GetID());
			OLED_Update();
			
			Delay_ms(50);
		}
		else if (PageSelect == 3)	//串口测试，TX1/RX1为板子右侧串口，TX2/RX2为板子左侧蓝牙串口
		{
			/*测试方法：K1/K2改变发送字节，K3分别从两路串口发出；对端（USB转串口模块/ */
			/*手机蓝牙助手）把收到的字节原样发回，板子收到后在RX1/RX2位置反色显示， */
			/*TX与RX一致即说明该路串口收发均正常 */
			if (Key_Check(KEY_1, KEY_SINGLE))		//K1键增加测试数据
			{
				TX1 += 1;
				TX2 += 2;
			}
			if (Key_Check(KEY_2, KEY_SINGLE))		//K2键减小测试数据
			{
				TX1 -= 1;
				TX2 -= 2;
			}
			if (Key_Check(KEY_3, KEY_SINGLE))		//K3键发送测试数据
			{
				Serial_SendByte(TX1);
				BlueSerial_SendByte(TX2);
				
				OLED_ReverseArea(32, 16, 16, 16);	//发送时反色闪烁
				OLED_ReverseArea(96, 16, 16, 16);
				OLED_Update();
				Delay_ms(100);
				OLED_ReverseArea(32, 16, 16, 16);
				OLED_ReverseArea(96, 16, 16, 16);
				OLED_Update();
			}
			
			if (Serial_GetRxFlag())			//判断串口是否收到数据
			{
				RX1 = Serial_GetRxData();	//收到时显示数据并反色闪烁
				OLED_Printf(0, 32, OLED_8X16,  "RX1:%02X  RX2:%02X", RX1, RX2);
				OLED_ReverseArea(32, 32, 16, 16);
				OLED_Update();
				Delay_ms(100);
				OLED_ReverseArea(32, 32, 16, 16);
				OLED_Update();
			}
			
			if (BlueSerial_Length() > 0)	//判断蓝牙串口是否收到数据
			{
				BlueSerial_Get(&RX2);		//收到时显示数据并反色闪烁
				OLED_Printf(0, 32, OLED_8X16,  "RX1:%02X  RX2:%02X", RX1, RX2);
				OLED_ReverseArea(96, 32, 16, 16);
				OLED_Update();
				Delay_ms(100);
				OLED_ReverseArea(96, 32, 16, 16);
				OLED_Update();
			}
			
			OLED_Printf(0, 0, OLED_8X16,  "串口测试     3/3");
			OLED_Printf(0, 16, OLED_8X16,  "TX1:%02X  TX2:%02X", TX1, TX2);
			OLED_Printf(0, 32, OLED_8X16,  "RX1:%02X  RX2:%02X", RX1, RX2);
			OLED_Update();
			
			Delay_ms(50);
		}
	}
}

/**
  * @brief  传感器校准：测量MPU6050的两项零点偏移并保存到Flash
  * @param  无
  * @retval 无（单击K4确认并保存后自然返回；长按K4放弃校准直接返回）
  * @note   校准两项内容：
  *         ①GY_Offset——绝对静止时Y轴陀螺仪读数本应为0，实际有零漂，
  *           取其平均值的相反数作为补偿值，运行时叠加到角速度上；
  *         ②AngleAcc_Offset——车身绝对竖直时加速度解算角度本应为0，
  *           实际安装有偏差，同样取平均再取反作为角度补偿。
  *         校准时务必让小车保持竖直且完全静止，否则补偿值不准
  */
void SensorCalibration(void)		//传感器校准
{
	float GY_Array[20] = {0};				//存放最近20次GY角速度的环形数组（初始全0）
	float AngleAcc_Array[20] = {0};			//存放最近20次角度的环形数组（初始全0）
	uint8_t p = 0;							//环形数组写入位置下标(0~19循环)
	float GY_Sum, GY_Ave;					//GY的求和值与平均值
	float AngleAcc_Sum, AngleAcc_Ave;		//角度的求和值与平均值
	
	OLED_Clear();
	OLED_Printf(0, 0, OLED_8X16,  "GY:+00000.00    ");
	OLED_Printf(0, 16, OLED_8X16, "Angle:+000.00   ");
	OLED_Printf(0, 32, OLED_8X16, " 保持竖直且静止 ");
	OLED_Printf(0, 48, OLED_8X16, "  单击K4键确认  ");
	
	/*测试前保持平衡车绝对竖直且静止，否则校准值可能有误*/
	/*校准项目，第一个是静止时陀螺仪的漂移，第二个是竖直时中心角度的偏差*/
	
	while (1)
	{
		if (Key_Check(KEY_4, KEY_SINGLE)) {Key_Clear(); break;}		//K4键确认
		if (Key_Check(KEY_4, KEY_LONG)) {Key_Clear(); return;}		//K4键长按退出
		
		MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);		//读取MPU6050原始数据
		/*atan2(AX,AZ)利用重力在X/Z轴的分量比值求俯仰倾角（弧度）， */
		/*除以π再乘180换算成角度；车头方向取负号与本工程坐标约定保持一致 */
		AngleAcc = -atan2(AX, AZ) / 3.1415926535 * 180;		//根据角速度计计算中心角度
		
		/*取20次测量结果，计算平均值，避免噪声干扰*/
		GY_Array[p] = GY;
		AngleAcc_Array[p] = AngleAcc;
		p ++;
		p %= 20;		//下标到19后回0，新数据覆盖最旧数据，构成20元素环形缓冲区（滑动平均）
		
		GY_Sum = 0;
		AngleAcc_Sum = 0;
		for (uint8_t i = 0; i < 20; i ++)
		{
			GY_Sum += GY_Array[i];
			AngleAcc_Sum += AngleAcc_Array[i];
		}
		GY_Ave = GY_Sum / 20.0;
		AngleAcc_Ave = AngleAcc_Sum / 20.0;
		
		OLED_Printf(0, 0, OLED_8X16, "GY:%+09.2f", GY_Ave);
		OLED_Printf(0, 16, OLED_8X16, "Angle:%+07.2f", AngleAcc_Ave);
		OLED_Update();
		
	}
	
	/*确认后，退出循环，校准完成*/
	OLED_Printf(0, 32, OLED_8X16, "    校准完成    ");
	OLED_Printf(0, 48, OLED_8X16, "                ");
	OLED_Update();
	
	Delay_ms(2000);		//让"校准完成"提示停留2秒，便于用户看到
	
	/*补偿值取测量平均值的相反数：运行时读数+偏移，即可把静止零点拉回0 */
	GY_Offset = -GY_Ave;				//绝对静止时，负的陀螺仪漂移为校准值
	AngleAcc_Offset = -AngleAcc_Ave;	//绝对竖直时，负的中心角度为校准值
	
	SaveParam();		//保持参数至FLASH，掉电不丢失
}

/**
  * @brief  调试模式主菜单：循环显示选项，等待按键进入硬件测试或传感器校准
  * @param  无
  * @retval 无（长按K4退出循环后返回到main.c主流程）
  * @note   进入本函数前main.c会把DebugFlag置1，TIM1中断里检测到该标志
  *         会跳过PID运算，避免菜单/测试期间电机被闭环控制干扰；
  *         退出时把DebugFlag清0，主循环恢复正常平衡控制
  */
void DebugMode(void)
{
	while (1)
	{
		OLED_Clear();	//每轮循环重绘菜单，保证从子功能返回后画面恢复正常
		OLED_Printf(0, 0, OLED_8X16,  "   [调试模式]   ");
		OLED_Printf(0, 16, OLED_8X16, "K1：硬件测试    ");
		OLED_Printf(0, 32, OLED_8X16, "K2：传感器校准  ");
		OLED_Printf(0, 48, OLED_8X16, "  长按K4键返回  ");
		OLED_Update();
		
		if (Key_Check(KEY_1, KEY_SINGLE)) {Key_Clear();HardwareTest();}			//K1键硬件测试
		if (Key_Check(KEY_2, KEY_SINGLE)) {Key_Clear();SensorCalibration();}	//K2键传感器校准
		if (Key_Check(KEY_4, KEY_LONG)) {DebugFlag = 0; OLED_Clear(); Key_Clear(); break;}	//K4键长按退出
		//DebugFlag清0：通知1ms中断恢复PID平衡控制；break跳出菜单循环，函数返回
	}
}
