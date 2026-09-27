/***************************************************************************************
  * 文件名称：MPU6050_Reg.h
  * 功    能：MPU6050寄存器地址定义表。
  *           MPU6050内部所有功能都通过“寄存器”配置/读取：主机通过I2C先发寄存器
  *           地址（即下表中的十六进制编号），再读/写其中的数据。
  *           本文件只给每个寄存器地址起一个“见名知意”的宏名，驱动代码中一律使用
  *           宏名而不是裸数字，便于阅读和防止写错地址。
  * 说    明：所有宏值均取自MPU6050官方寄存器手册，不可随意更改；
  *           后缀_H表示该16位数据的高8位(High)寄存器，_L表示低8位(Low)寄存器。
  ***************************************************************************************
  */
#ifndef __MPU6050_REG_H
#define __MPU6050_REG_H

/*==================== 配置类寄存器（初始化时写入） ====================*/

/*采样率分频寄存器（Sample Rate Divider）：
  采样频率 = 陀螺仪输出速率 / (1 + SMPLRT_DIV)；
  CONFIG=0时陀螺仪输出率为8kHz，本工程写入0x07，即 8000/8 = 1000Hz（1ms一组）*/
#define	MPU6050_SMPLRT_DIV		0x19
/*配置寄存器（Configuration）：设置数字低通滤波DLPF带宽与外部帧同步，
  低3位取值0~6对应不同滤波强度；0=最宽带宽、延迟最小*/
#define	MPU6050_CONFIG			0x1A
/*陀螺仪量程配置寄存器：FS_SEL位[4:3]决定满量程——
  0=±250dps，1=±500dps，2=±1000dps，3=±2000dps；本工程写0x18即FS_SEL=3*/
#define	MPU6050_GYRO_CONFIG		0x1B
/*加速度计量程配置寄存器：AFS_SEL位[4:3]决定满量程——
  0=±2g，1=±4g，2=±8g，3=±16g；本工程写0x18即AFS_SEL=3*/
#define	MPU6050_ACCEL_CONFIG	0x1C

/*==================== 测量结果寄存器（只读，地址连续可批量读） ====================*/
/*说明：每个轴占2字节（先高后低），合成16位有符号补码数；
  从0x3B到0x48共14字节连续排列，MPU6050_GetData一次性连读：
  加速度X/Y/Z(6字节) -> 温度(2字节) -> 陀螺仪X/Y/Z(6字节)*/

/*三轴加速度计测量值（Accelerometer X/Y/Z Out，High/Low各1字节）；
  ±16g量程下原始值/2048即为以g为单位的加速度（静止时Z轴约为±2048）*/
#define	MPU6050_ACCEL_XOUT_H	0x3B
#define	MPU6050_ACCEL_XOUT_L	0x3C
#define	MPU6050_ACCEL_YOUT_H	0x3D
#define	MPU6050_ACCEL_YOUT_L	0x3E
#define	MPU6050_ACCEL_ZOUT_H	0x3F
#define	MPU6050_ACCEL_ZOUT_L	0x40
/*片内温度传感器测量值（Temperature Out，High/Low各1字节）；
  换算公式：温度(℃) = 原始值/340.0 + 36.53；本工程未使用*/
#define	MPU6050_TEMP_OUT_H		0x41
#define	MPU6050_TEMP_OUT_L		0x42
/*三轴陀螺仪测量值（Gyroscope X/Y/Z Out，High/Low各1字节）；
  ±2000dps量程下原始值/16.4即为角速度（度/秒），平衡车用Y轴角速度积分求倾角*/
#define	MPU6050_GYRO_XOUT_H		0x43
#define	MPU6050_GYRO_XOUT_L		0x44
#define	MPU6050_GYRO_YOUT_H		0x45
#define	MPU6050_GYRO_YOUT_L		0x46
#define	MPU6050_GYRO_ZOUT_H		0x47
#define	MPU6050_GYRO_ZOUT_L		0x48

/*==================== 电源管理与身份寄存器 ====================*/

/*电源管理寄存器1（Power Management 1）：
  bit6 DEVICE_RESET=1可复位芯片；bit6 SLEEP=1进入睡眠（上电默认睡眠，需写0唤醒）；
  bit3 TEMP_DIS可关温度传感器；低3位CLKSEL选择时钟源，001=X轴陀螺仪时钟（推荐）。
  本工程写入0x01：唤醒 + 选用X陀螺仪时钟*/
#define	MPU6050_PWR_MGMT_1		0x6B
/*电源管理寄存器2（Power Management 2）：低6位分别控制6个轴的待机(Standby)，
  某位写1表示该轴休眠；写0x00表示加速度XYZ与陀螺仪XYZ全部保持工作*/
#define	MPU6050_PWR_MGMT_2		0x6C
/*身份标识寄存器（Who Am I）：只读，固定返回0x68，
  常用于I2C通信自检——读不到0x68多半是接线/地址/初始化异常*/
#define	MPU6050_WHO_AM_I		0x75

#endif
