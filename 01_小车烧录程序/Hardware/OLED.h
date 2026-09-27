/***************************************************************************************
  * 文件名称：OLED.h
  * 功    能：0.96寸SSD1306 OLED驱动的对外接口与参数宏。
  *           头文件中把函数分成5组：初始化 / 显存刷新 / 显存控制(清屏取反) /
  *           显示(字符、字符串、各种进制数字、浮点、图片、printf) / 绘图(点线面圆等)。
  * 使用三步曲（重要）：
  *           1. 上电调用 OLED_Init()；
  *           2. 任意调用 OLED_ShowXxx / OLED_DrawXxx 修改“显存数组”（屏幕暂不变化）；
  *           3. 调用 OLED_Update()（全屏）或 OLED_UpdateArea()（局部）才真正显示。
  * 坐标约定：左上角(0,0)，X向右0~127，Y向下0~63；参数允许负数，超出屏幕部分不显示。
  * 字模数据（ASCII点阵、中文字库）放在OLED_Data.c/.h中，本文件包含其头文件。
  ***************************************************************************************
  */
#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>
#include "OLED_Data.h"

/*参数宏定义*********************/

/*FontSize参数取值*/
/*此参数值不仅用于判断，而且用于计算横向字符偏移，默认值为字体像素宽度*/
#define OLED_8X16				8	//8×16字体：宽8像素、高16像素（宏值=字符宽度，用于计算下一个字符的X偏移）
#define OLED_6X8				6	//6×8字体：宽6像素、高8像素（一行可显示更多内容，本车主界面多用此字体）

/*IsFilled参数数值*/
#define OLED_UNFILLED			0	//图形只画轮廓、不填充
#define OLED_FILLED				1	//图形内部填满

/*********************参数宏定义*/


/*函数声明*********************/

/*初始化函数*/
void OLED_Init(void);													//上电后调用一次

/*更新函数*/
/*补充：把显存数组推送到硬件，是“显示生效”的最后一步*/
void OLED_Update(void);													//全屏刷新
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);	//只刷新指定矩形区域，更省时

/*显存控制函数*/
/*补充：以下函数只改显存，仍需调用Update才会上屏*/
void OLED_Clear(void);														//全屏清空
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);	//区域清空
void OLED_Reverse(void);													//全屏亮灭取反
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);	//区域取反

/*显示函数*/
/*补充：坐标X,Y为左上角；FontSize取OLED_8X16或OLED_6X8*/
void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize);					//显示单个ASCII字符
void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize);				//显示字符串（支持中文混排，中文字库在OLED_Data.c）
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);		//无符号十进制数，Length为位数
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);	//带正负号十进制数
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);	//十六进制数(0~F)
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);	//二进制数(0/1)
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);	//小数，分别指定整数/小数位数
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);	//显示任意宽高的位图（字模数组）
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, char *format, ...);			//仿printf格式化显示，用法如OLED_Printf(0,0,OLED_6X8,"%d",x)

/*绘图函数*/
/*补充：坐标单位为像素；IsFilled取OLED_UNFILLED/OLED_FILLED；改完同样要Update。
  下方按“点/线/矩形/三角形/圆/椭圆/圆弧”顺序声明*/
void OLED_DrawPoint(int16_t X, int16_t Y);								//画一个点
uint8_t OLED_GetPoint(int16_t X, int16_t Y);							//读某点状态：1亮0灭
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);		//两点画直线（Bresenham整数算法）
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);	//画矩形
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);	//画三角形
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);	//画圆
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);	//画椭圆（A横半轴、B纵半轴）
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);	//画圆弧/扇形（角度-180~180）

/*********************函数声明*/

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
