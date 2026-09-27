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
#include "NRF24L01.h"
#include "Serial.h"
#include "DebugMode.h"
#include "Store.h"
#include <math.h>
#include <stdio.h>

uint8_t RunFlag;

uint8_t RunFlagUpdate;

int16_t AX, AY, AZ, GX, GY, GZ;
int16_t AX_Offset, AZ_Offset, GY_Offset;
int16_t GY_Cali;

float AngleAcc;
float AngleAcc_Offset;
float AngleAcc_Cali;
float AngleAcc_Filter;
float AngleDelta;
float Angle;

float SpeedLeft, SpeedRight;
float AveSpeed, DifSpeed;

float AvePWM, DifPWM;
int16_t PWML, PWMR;

uint16_t SpeedLevel = 4; 

/* 本机按键行驶速度目标值；正负方向如与实车相反，可互换 K2/K3 的符号 */
#define KEY_MOVE_SPEED		2.0f

/* K4长按依次切换：本机按键 -> NRF手柄 -> 手机蓝牙 -> 本机按键 */
#define CONTROL_MODE_KEY	0
#define CONTROL_MODE_RF		1
#define CONTROL_MODE_BT		2
#define REMOTE_TIMEOUT_MS	500

uint8_t ControlMode = CONTROL_MODE_KEY;
volatile uint16_t RemoteWatchdog;
volatile uint8_t RemoteTimeoutFlag;

uint8_t DebugFlag;

#define ANGLE_T				10
#define SPEED_T				50

/*角度环PID参数*/
PID_t AnglePID = {	
	.Kp = 3,
	.Ki = 0.1,
	.Kd = 3,
	
	.OutMax = 100,
	.OutMin = -100,
	
	.OutOffset = 3,
};

/*速度环PID参数*/
PID_t SpeedPID = {	
	.Kp = 2,
	.Ki = 0.05,
	.Kd = 0,
	
	.OutMax = 10,
	.OutMin = -10,
};

/*转向环PID参数*/
PID_t TurnPID = {	
	.Kp = 2,
	.Ki = 1,
	.Kd = 0,
	
	.OutMax = 30,
	.OutMin = -30,
};

int main(void)
{
	/*初始化*/
	OLED_Init();
	LED_Init();
	Key_Init();
	Motor_Init();
	Encoder_Init();
	MPU6050_Init();
	BlueSerial_Init();
	NRF24L01_Init();
	Serial_Init();
	
	Timer_Init();
	
	/*显示初始界面*/
	OLED_Clear();
	OLED_ShowString(0, 0,  "   [江协科技]   ", OLED_8X16);
	OLED_ShowString(4, 16, " 平衡车测试程序 ", OLED_8X16);
	OLED_ShowString(0, 32, "      V1.0      ", OLED_8X16);
	OLED_ShowString(0, 48, "             K4>", OLED_8X16);
	OLED_Update();
	
	/*按K4键继续*/
	while (Key_Check(KEY_4, KEY_SINGLE) == 0);
	Key_Clear();
	
	/*初始化FLASH并检查FLASH中有没有存储参数*/
	if (Store_Init())
	{
		/*未存储参数，将程序中的默认参数保存到FLASH中*/
		SaveParam();
		
		/*提示已重置参数*/
		OLED_Clear();
		OLED_ShowString(0, 0,  "     [提示]     ", OLED_8X16);
		OLED_ShowString(0, 16, "   已重置参数   ", OLED_8X16);
		OLED_ShowString(0, 32, " 请注意执行校准 ", OLED_8X16);
		OLED_ShowString(0, 48, "             K4>", OLED_8X16);
		OLED_Update();
		
		/*按K4键继续*/
		while (Key_Check(KEY_4, KEY_SINGLE) == 0);
		Key_Clear();
	}
	
	/*上电后加载FLASH中保存的参数*/
	LoadParam();
	
	OLED_Clear();
	BlueSerial_ClearBuffer();
	
	while (1)
	{
		/*指示灯*/
		if (RunFlag) {LED_ON();} else {LED_OFF();}
		
		/*按键*/
		if (Key_Check(KEY_1, KEY_SINGLE))		//K1按下，启动/停止
		{
			if (RunFlag == 0)
			{
				PID_Init(&AnglePID);
				PID_Init(&SpeedPID);
				PID_Init(&TurnPID);
				Angle = AngleAcc_Filter;
				RunFlag = 1;
			}
			else
			{
				RunFlag = 0;
			}
			BlueSerial_SendString(RunFlag ? "RunFlag=1\r\n" : "RunFlag=0\r\n");
		}
		if (RunFlagUpdate)
		{
			RunFlagUpdate = 0;
			BlueSerial_SendString(RunFlag ? "RunFlag=1\r\n" : "RunFlag=0\r\n");
		}
		if (Key_Check(KEY_2, KEY_SINGLE))		//K2按下，前进
		{
			if (RunFlag)
			{
				SpeedPID.Target = KEY_MOVE_SPEED;
				TurnPID.Target = 0;
			}
			BlueSerial_SendString("KeyMove=Forward\r\n");
		}
		if (Key_Check(KEY_3, KEY_SINGLE))		//K3按下，后退
		{
			if (RunFlag)
			{
				SpeedPID.Target = -KEY_MOVE_SPEED;
				TurnPID.Target = 0;
			}
			BlueSerial_SendString("KeyMove=Backward\r\n");
		}
		if (Key_Check(KEY_4, KEY_SINGLE))		//K4短按，停止移动但继续保持平衡
		{
			PID_Init(&SpeedPID);
			PID_Init(&TurnPID);
			BlueSerial_SendString("KeyMove=Hold\r\n");
		}
		if (Key_Check(KEY_4, KEY_LONG))			//K4长按，依次切换KEY/RF/BT模式
		{
			ControlMode ++;
			if (ControlMode > CONTROL_MODE_BT) {ControlMode = CONTROL_MODE_KEY;}
			PID_Init(&SpeedPID);
			PID_Init(&TurnPID);
			RemoteWatchdog = 0;
			RemoteTimeoutFlag = 0;
			BlueSerial_ClearBuffer();
			BlueSerial_SendString(ControlMode == CONTROL_MODE_RF ? "ControlMode=RF\r\n" : (ControlMode == CONTROL_MODE_BT ? "ControlMode=BT\r\n" : "ControlMode=KEY\r\n"));
		}

		/* 无线模式连续500ms没有收到有效控制包时，清零行驶和转向目标 */
		if (RemoteTimeoutFlag)
		{
			RemoteTimeoutFlag = 0;
			PID_Init(&SpeedPID);
			PID_Init(&TurnPID);
			BlueSerial_SendString("RemoteTimeout=Stop\r\n");
		}

		/*NRF24L01手柄遥控：原厂数据包格式兼容*/
		if (ControlMode == CONTROL_MODE_RF && NRF24L01_Receive() == 1)
		{
			uint8_t ID = NRF24L01_RxPacket[0];
			if (ID == 0x00 || ID == 0x01)
			{
				int8_t LV = NRF24L01_RxPacket[2];
				int8_t RH = NRF24L01_RxPacket[3];
				uint8_t RemoteKey = NRF24L01_RxPacket[5];

				RemoteWatchdog = 0;
				SpeedPID.Target = LV / 25.0f;
				TurnPID.Target = RH / 25.0f;

				if (RemoteKey == 1) {Key_Flag[KEY_1] |= KEY_SINGLE;}
				else if (RemoteKey == 2) {Key_Flag[KEY_2] |= KEY_SINGLE;}
				else if (RemoteKey == 3) {Key_Flag[KEY_3] |= KEY_SINGLE;}

				if (ID == 0x01)
				{
					NRF24L01_TxPacket[0] = 0x02;
					NRF24L01_TxPacket[1] = (int8_t)PWML;
					NRF24L01_TxPacket[2] = (int8_t)PWMR;
					*(float *)&NRF24L01_TxPacket[4] = Angle;
					*(float *)&NRF24L01_TxPacket[8] = SpeedLeft;
					*(float *)&NRF24L01_TxPacket[12] = SpeedRight;
					NRF24L01_Send();
				}
			}
		}
		
		/*蓝牙串口*/
		if (BlueSerial_ReceiveFlag())	//判断是否收到蓝牙数据包
		{
			BlueSerial_Receive();		//收到数据包，接收并解析数据包
			
			/*摇杆数据包，格式为：[joystick,LH,LV,RH,RV]*/
			if (ControlMode == CONTROL_MODE_BT
			 && strcmp(BlueSerial_StringArray[0], "joystick") == 0)
			{
				/*得到摇杆数据，转换为数值*/
				int16_t LV = atoi(BlueSerial_StringArray[2]);
				int16_t RH = atoi(BlueSerial_StringArray[3]);
				if (LV > 100) {LV = 100;} else if (LV < -100) {LV = -100;}
				if (RH > 100) {RH = 100;} else if (RH < -100) {RH = -100;}
				
				/*设定目标速度和目标转向幅度*/
				SpeedPID.Target = LV / 100.0 * SpeedLevel;
				TurnPID.Target = RH / 100.0 * SpeedLevel;
				RemoteWatchdog = 0;
			}
			/*按键数据包，格式为：[key,按键名称,down/up]*/
			else if (ControlMode == CONTROL_MODE_BT
			      && strcmp(BlueSerial_StringArray[0], "key") == 0)
			{
				RemoteWatchdog = 0;
				if (strcmp(BlueSerial_StringArray[1], "1") == 0 
				 && strcmp(BlueSerial_StringArray[2], "up") == 0)		//收到数据包[key,1,up]
				{
					Key_Flag[KEY_1] |= KEY_SINGLE;		//触发平衡车K1键按下
				}
				else if (strcmp(BlueSerial_StringArray[1], "2") == 0 
					  && strcmp(BlueSerial_StringArray[2], "up") == 0)	//收到数据包[key,2,up]
				{
					Key_Flag[KEY_2] |= KEY_SINGLE;		//触发平衡车K2键按下
				}
				else if (strcmp(BlueSerial_StringArray[1], "3") == 0 
					  && strcmp(BlueSerial_StringArray[2], "up") == 0)	//收到数据包[key,3,up]
				{
					Key_Flag[KEY_3] |= KEY_SINGLE;		//触发平衡车K3键按下
				}
			}
		}
		
		/*OLED显示*/
		OLED_Printf(0, 0, OLED_6X8, "Angle  Speed  Turn");
		OLED_Printf(0, 10, OLED_6X8, "%+06.1f %+06.1f %+06.1f", AnglePID.Target, SpeedPID.Target, TurnPID.Target);
		OLED_Printf(0, 18, OLED_6X8, "%+06.1f %+06.1f %+06.1f", Angle, AveSpeed, DifSpeed);
		OLED_Printf(0, 26, OLED_6X8, "%+06.1f %+06.1f %+06.1f", AnglePID.Out, SpeedPID.Out, TurnPID.Out);
		
		OLED_DrawLine(0, 36, 127, 36);
		
		OLED_Printf(0, 40, OLED_6X8, "PWML:%+05d PWMR:%+05d", PWML, PWMR);
		OLED_Printf(0, 48, OLED_6X8, "SpdL:%+05.1f SpdR:%+05.1f", SpeedLeft, SpeedRight);
		OLED_Printf(0, 56, OLED_6X8, "Mode:%s SpdLevel:%1d", ControlMode == CONTROL_MODE_RF ? "RF" : (ControlMode == CONTROL_MODE_BT ? "BT" : "KEY"), SpeedLevel);
		OLED_Update();
	}
}

void TIM1_UP_IRQHandler(void)
{
	static uint16_t SensorCount0, SensorCount1;
	static uint16_t RunCount0, RunCount1;
	
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
		
		/*定时器定时中断，每隔1ms自动执行一次*/
		
		/*驱动非阻塞式按键*/
		Key_Tick();

		/*RF或蓝牙遥控失联计时，达到阈值后仅触发一次停车处理*/
		if ((ControlMode == CONTROL_MODE_RF || ControlMode == CONTROL_MODE_BT) && RunFlag)
		{
			if (RemoteWatchdog < REMOTE_TIMEOUT_MS)
			{
				RemoteWatchdog ++;
				if (RemoteWatchdog == REMOTE_TIMEOUT_MS)
				{
					RemoteTimeoutFlag = 1;
				}
			}
		}
		else
		{
			RemoteWatchdog = 0;
		}
		
		/*进入调试模式后，不执行后续PID程序，以免干扰调试*/
		if (DebugFlag) {return;}
		
		SensorCount0 ++;
		if (SensorCount0 >= ANGLE_T)		//每隔ANGLE_T定义的时间执行一次
		{
			SensorCount0 = 0;
			
			/*姿态解析，得到平衡车角度*/
			
			/*获取陀螺仪加速度计原始数据*/
			MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
			
			/*陀螺仪数据校准*/
			GY_Cali = GY + GY_Offset;
			
			/*计算加速度计角度*/
			AngleAcc = -atan2(AX, AZ) / 3.1415926535 * 180;
			
			/*计算陀螺仪角度增量*/
			AngleDelta = GY_Cali / 32768.0 * 2000.0 * (ANGLE_T / 1000.0);
			
			/*中心角度校准*/
			AngleAcc_Cali = AngleAcc + AngleAcc_Offset;
			
			/*对加速度计角度进行一阶低通滤波，使其更平滑*/
			float Alpha0 = 0.8;
			AngleAcc_Filter = Alpha0 * AngleAcc_Filter + (1 - Alpha0) * AngleAcc_Cali;
			
			/*角度累加陀螺仪角度增量，得到新的角度值*/
			Angle += AngleDelta;
			
			/*新的角度值与加速度计角度进行互补滤波，抑制漂移*/
			float Alpha1 = fabs(DifSpeed) / 5.0 * 0.02 + 0.005;		//根据差速动态调整滤波参数
			if (Alpha1 > 0.02) {Alpha1 = 0.02;}						//参数限幅
			Angle = Alpha1 * AngleAcc_Filter + (1 - Alpha1) * Angle;//互补滤波得到角度值
			
			/*平衡车倒地后，自动停止*/
			if (Angle > 50 || Angle < -50)
			{
				if (RunFlag)
				{
					RunFlag = 0;
					RunFlagUpdate = 1;
				}
			}
		}
		
		SensorCount1 ++;
		if (SensorCount1 >= SPEED_T)		//每隔SPEED_T定义的时间执行一次
		{
			SensorCount1 = 0;
			
			/*通过编码器获取电机旋转速度*/
			SpeedLeft = Encoder_Get(1) / 408.0 / (SPEED_T / 1000.0);
			SpeedRight = Encoder_Get(2) / 408.0 / (SPEED_T / 1000.0);
			
			/*获取均速和差速*/
			AveSpeed = (SpeedLeft + SpeedRight) / 2.0;
			DifSpeed = SpeedLeft - SpeedRight;
		}
		
		/*PID控制*/
		if (RunFlag)
		{
			RunCount0 ++;
			if (RunCount0 >= ANGLE_T)		//每隔ANGLE_T定义的时间执行一次
			{
				RunCount0 = 0;
				
				/*角度环PID计算*/
				AnglePID.Actual = Angle;
				PID_Update(&AnglePID);
				AvePWM = -AnglePID.Out;		//角度环输出作用于平均PWM
				
				/*平均PWM和差分PWM，合成左轮PWM和右轮PWM*/
				PWML = AvePWM + DifPWM;
				PWMR = AvePWM - DifPWM;
				
				/*PWM限幅*/
				if (PWML > 100) {PWML = 100;} else if (PWML < - 100) {PWML = -100;}
				if (PWMR > 100) {PWMR = 100;} else if (PWMR < - 100) {PWMR = -100;}
				
				/*PWM输出至电机*/
				Motor_SetPWM(1, PWML);
				Motor_SetPWM(2, PWMR);
			}
			
			RunCount1 ++;
			if (RunCount1 >= SPEED_T)		//每隔SPEED_T定义的时间执行一次
			{
				RunCount1 = 0;
				
				/*速度环PID计算*/
				SpeedPID.Actual = AveSpeed;
				PID_Update(&SpeedPID);
				AnglePID.Target = SpeedPID.Out;	//速度环输出作用于角度环输入
				
				/*转向环PID计算*/
				TurnPID.Actual = DifSpeed;
				PID_Update(&TurnPID);
				DifPWM = TurnPID.Out;		//转向环输出作用于差分PWM
			}
		}
		else
		{
			Motor_SetPWM(1, 0);				//RunFlag为0时，电机停止
			Motor_SetPWM(2, 0);
		}
	}
}
