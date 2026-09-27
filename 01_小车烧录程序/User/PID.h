/**
  **********************************************************************
  * @file    PID.h
  * @brief   位置式PID控制算法模块的头文件：定义PID参数结构体PID_t
  * @note    【PID是什么】P比例、I积分、D微分三种控制作用的合称：
  *          P——误差越大纠正越用力；I——把历史误差累加，消除"差一点"
  *          的稳态误差；D——根据误差变化趋势提前制动，抑制超调晃动。
  *          main.c中用本结构体建立了三个实例：
  *          AnglePID角度环（保持车身竖直）、SpeedPID速度环（控制快慢）、
  *          TurnPID转向环（控制左右差速转向），三环构成串级控制：
  *          速度环输出修正角度环目标，角度环输出控制两轮平均PWM，
  *          转向环输出控制两轮差分PWM
  **********************************************************************
  */
#ifndef __PID_H		//头文件包含卫士
#define __PID_H

typedef struct {
	float Target;		//目标值（希望被控量达到的值，如目标角度/目标速度）
	float Actual;		//实际值（本次测量值，如MPU6050当前角度、编码器当前速度）
	float Actual1;		//上一次的实际值（供微分项D使用）
	float Out;			//PID最终输出值（本工程最终体现为电机PWM或内环目标）
	
	float Kp;			//比例系数：P项增益
	float Ki;			//积分系数：I项增益（设为0即关闭积分作用）
	float Kd;			//微分系数：D项增益
	
	float Error0;		//当前误差 = Target - Actual（即本次e(k)）
	float Error1;		//上次误差（本实现采用微分先行，Error1主要用于误差传递记录）
	
	float POut;			//比例项P的计算结果
	float IOut;			//积分项I的计算结果（带限幅，防积分饱和）
	float DOut;			//微分项D的计算结果（对实际值微分，即"微分先行"）
	
	float OutMax;		//输出上限（输出限幅用，防止PWM过大冲爆电机）
	float OutMin;		//输出下限（一般为负值，与OutMax对称）
	
	float OutOffset;	//输出偏移（死区补偿）：输出非零时同向叠加，
						//克服电机PWM过小转不动的静摩擦死区
} PID_t;

void PID_Init(PID_t *p);	//PID实例初始化：清零误差/输出等运行数据（不动Kp/Ki/Kd等参数）
void PID_Update(PID_t *p);	//执行一次PID计算：传入填好Target/Actual的结构体指针，
							//结果放在成员Out中，需周期性调用（本工程1ms节拍中调用）

#endif
