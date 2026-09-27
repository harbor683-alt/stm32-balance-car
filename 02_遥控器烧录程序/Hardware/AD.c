/***************************************************************************************
  * 模块名称：			AD.c（ADC模数转换驱动，遥控器端工程）
  * 模块功能：			使用STM32F103C8T6内部的ADC1外设，轮流采集4路摇杆电位器的模拟电压，
  * 						得到0~4095的数字量
  * 硬件接线：			左摇杆横向 → PA0（ADC通道0）	左摇杆纵向 → PA1（ADC通道1）
  * 						右摇杆横向 → PA2（ADC通道2）	右摇杆纵向 → PA3（ADC通道3）
  * 						摇杆内部是一只电位器（可变电阻），两端分别接3.3V和GND，滑臂端输出电压：
  * 						推到一头≈0V，推到另一头≈3.3V，回中≈1.65V
  * 数据链路角色：		本模块是遥控器数据链路的"数据源头"：
  * 						AD原始值(0~4095) → main.c的DataProcess()换算成摇杆百分比(-100~100)
  * 						→ 填入NRF24L01发送包[Mode,LH,LV,RH,RV,KEY] → 无线发给小车
  * 名词解释：			ADC（Analog-to-Digital Converter，模数转换器）：把连续变化的模拟电压
  * 						"量化"成离散数字的外设。本芯片ADC分辨率为12位，即把0~3.3V平均分成
  * 						2^12 = 4096个等级，输出结果范围0~4095，每一级约3.3V/4096 ≈ 0.8mV。
  * 						电压与数值的换算：电压 = AD值 × 3.3 / 4096
  ***************************************************************************************
  */

#include "stm32f10x.h"                  // Device header

/**
  * 函    数：AD初始化
  * 参    数：无
  * 返 回 值：无
  */
void AD_Init(void)
{
	/*【知识点1·外设时钟】STM32为了省电，所有外设上电时时钟默认都是关闭的，使用前必须先到*/
	/*RCC（Reset and Clock Control，复位和时钟控制单元）里手动开启，否则外设完全不工作。*/
	/*ADC1和GPIOA都挂载在APB2高速总线上，所以用RCC_APB2PeriphClockCmd开启。*/
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//开启ADC1的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	
	/*【知识点2·ADC专用时钟】ADC除了总线时钟外，还有一个自己的转换时钟ADCCLK，它由APB2的*/
	/*72MHz分频得到。数据手册规定ADCCLK不能超过14MHz，这里6分频得到12MHz，合规且转换速度快。*/
	/*设置ADC时钟*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);						//选择时钟6分频，ADCCLK = 72MHz / 6 = 12MHz
	
	/*【知识点3·模拟输入模式】GPIO_Mode_AIN（Analog IN，模拟输入）是ADC专用引脚模式：*/
	/*此时引脚内部的上拉/下拉电阻断开、数字输入用的施密特触发器也关闭，外部电压"原封不动"*/
	/*地直接送进ADC，避免数字部分干扰微弱的模拟信号。GPIO_Speed对模拟输入无实际意义，给默认值即可。*/
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//将PA0、PA1、PA2和PA3引脚初始化为模拟输入
	
	/*不在此处配置规则组序列，而是在每次AD转换前配置，这样可以灵活更改AD转换的通道*/
	
	/*【知识点4·ADC工作方式（本工程的选择）】ADC有两套转换清单：规则组（常规任务，最多16个通道）*/
	/*和注入组（高优先级插队任务）。本工程只用规则组，并且采用"独立模式 + 非扫描 + 单次转换 + 软件触发"：*/
	/*每次想读哪一路摇杆，就在转换前临时把该通道填进规则组的"序列1"位置，手动触发一次，等转换完成后读数。*/
	/*这样一个ADC就能轮流服务4个通道，不必同时配置4个序列。*/
	/*ADC初始化*/
	ADC_InitTypeDef ADC_InitStructure;						//定义结构体变量
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;		//模式，选择独立模式，即单独使用ADC1
	/*数据右对齐：转换结果是12位，而数据寄存器是16位。右对齐时结果数值就是0~4095，直接可用，最直观；*/
	/*若选左对齐，结果会被移到高12位，常用于只取高8位的快速应用。*/
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;	//数据对齐，选择右对齐
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//外部触发，使用软件触发，不需要外部触发
	/*连续转换失能+扫描模式失能 = 单次转换模式：软件触发一次只转换序列1这1个通道，转完就停下来等待下一次触发。*/
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;		//连续转换，失能，每转换一次规则组序列后停止
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;			//扫描模式，失能，只转换规则组的序列1这一个位置
	ADC_InitStructure.ADC_NbrOfChannel = 1;					//通道数，为1，仅在扫描模式下，才需要指定大于1的数，在非扫描模式下，只能是1
	ADC_Init(ADC1, &ADC_InitStructure);						//将结构体变量交给ADC_Init，配置ADC1
	
	/*ADC使能*/
	ADC_Cmd(ADC1, ENABLE);									//使能ADC1，ADC开始运行
	
	/*【知识点5·ADC自校准】芯片内部有专门的校准电路，可自动修正制造误差和温漂，让测量更准。*/
	/*官方固定流程（顺序不能乱）：①申请复位校准→②死等复位完成→③申请开始校准→④死等校准完成。*/
	/*整个过程由硬件自动完成，程序只需"发申请、等标志"，这是STM32 ADC初始化的标准收尾动作。*/
	/*ADC校准*/
	ADC_ResetCalibration(ADC1);								//固定流程，内部有电路会自动执行校准
	while (ADC_GetResetCalibrationStatus(ADC1) == SET);
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1) == SET);
}

/**
  * 函    数：获取AD转换的值
  * 参    数：ADC_Channel 指定AD转换的通道，范围：ADC_Channel_x，其中x可以是0/1/2/3
  * 返 回 值：AD转换的值，范围：0~4095
  */
uint16_t AD_GetValue(uint8_t ADC_Channel)
{
	/*【知识点6·单次转换的标准四步流程】①选通道和采样时间 → ②软件触发 → ③等待EOC完成标志 → ④读结果*/
	/*采样时间55.5个ADCCLK周期：指ADC内部的采样电容"跟踪"外部电压的时间。信号源内阻越大，需要的*/
	/*采样时间越长才准；摇杆电位器内阻很小，55.5周期绰绰有余。转换一次的总耗时约为*/
	/*(采样55.5 + 逐次逼近12.5) / 12MHz ≈ 5.7us，4路轮询一遍也只要二十几微秒，对100ms发包周期毫无压力。*/
	/*第3个参数"1"表示把通道填到规则组序列1的位置；本工程非扫描模式只认这一个位置。*/
	ADC_RegularChannelConfig(ADC1, ADC_Channel, 1, ADC_SampleTime_55Cycles5);	//在每次转换前，根据函数形参灵活更改规则组的通道1
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);					//软件触发AD转换一次
	/*EOC（End Of Conversion，转换结束标志位）：转换完成后由硬件自动置1。*/
	/*这里用查询方式"死等"它变成1（SET），保证读到的是本次刚转完的新数据，而不是旧数据。*/
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);	//等待EOC标志位，即等待AD转换结束
	/*读取数据寄存器DR即可拿到12位结果；注意：读DR这个动作会顺带把EOC标志硬件清零，无需手动清。*/
	return ADC_GetConversionValue(ADC1);					//读数据寄存器，得到AD转换的结果
}
