/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    adc.c
 * @brief   ADC采集驱动 - 温度采集
 * @details 实现基于ADC+DMA的多通道温度采集功能
 *          - 5通道ADC扫描模式（Channel 10/11/12/14/15）
 *          - DMA循环传输
 *          - NTC热敏电阻温度转换
 *          - 温度范围：-20℃ ~ +100℃
 *
 *          引脚映射：
 *          - PA0: ADC_Channel_10  (温度采集1)
 *          - PA1: ADC_Channel_11  (温度采集2)
 *          - PA2: ADC_Channel_12  (温度采集3)
 *          - PA4: ADC_Channel_14  (温度采集4)
 *          - PA5: ADC_Channel_15  (温度采集5)
 *
 *          ADC配置：
 *          - 分辨率：12位 (0~4095)
 *          - 时钟：PCLK2/4 = 18MHz
 *          - 采样时间：7.5周期
 */

#include "adc.h"

/* ==================== 局部变量定义 ==================== */
static u16 ADC_ConvertedValue[5] = {0};  // ADC转换结果缓冲区（5通道）
static u8 TemNA = 6;                      // 温度查找表索引初值

/* ==================== 温度查找表 ==================== */
/**
 * @brief NTC热敏电阻温度-电压查找表
 * @details 温度范围：-20℃ ~ +100℃
 *          电压值单位：mV
 *          温度间隔：4℃
 *
 *          索引对应温度：
 *          TemD[0]  = -20℃ (2890mV)
 *          TemD[1]  = -16℃ (2817mV)
 *          TemD[2]  = -12℃ (2735mV)
 *          ...
 *          TemD[30] = +100℃ (277mV)
 *
 * @note   NTC特性：温度升高，电压降低（负温度系数）
 */
const static unsigned int TemD[31] =
{
	// -20℃ ~ +12℃
	2890, 2817, 2735, 2642, 2541, 2431, 2318, 2202, 2127, 2007,
	// +16℃ ~ +52℃
	1846, 1687, 1546, 1422, 1309, 1202, 1099, 1001, 907, 817,
	// +56℃ ~ +100℃
	727, 677, 613, 550, 496, 452, 414, 379, 345, 311, 277
};

/* ==================== ADC初始化 ==================== */

/**
 * @brief ADC1初始化函数
 * @details 配置ADC1为多通道扫描模式
 *          - 模式：独立模式
 *          - 扫描模式：使能
 *          - 连续转换：使能
 *          - 对齐方式：右对齐
 *          - 通道数：5
 *          - 采样时间：7.5周期
 *
 *          时钟配置：
 *          - ADC时钟 = PCLK2 / 4 = 72MHz / 4 = 18MHz
 *          - 采样时间 = 7.5周期 ≈ 0.42μs
 */
void ADC_Init_Sam(void)
{
	ADC_InitTypeDef ADC_InitStructure;

	/* 使能ADC1时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* ADC复位 */
	ADC_DeInit(ADC1);

	/* 初始化ADC结构体 */
	ADC_StructInit(&ADC_InitStructure);
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;           // 独立模式
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;                 // 扫描模式（多通道）
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;           // 连续转换模式
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;  // 软件触发
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;       // 右对齐
	ADC_InitStructure.ADC_NbrOfChannel = 5;                      // 转换通道数为5
	ADC_Init(ADC1, &ADC_InitStructure);

	/* 配置ADC时钟：PCLK2 / 4 = 18MHz */
	RCC_ADCCLKConfig(RCC_PCLK2_Div4);

	/* 配置规则通道转换顺序和采样时间 */
	ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_7Cycles5);  // 通道10，第1个转换
	ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 2, ADC_SampleTime_7Cycles5);  // 通道11，第2个转换
	ADC_RegularChannelConfig(ADC1, ADC_Channel_12, 3, ADC_SampleTime_7Cycles5);  // 通道12，第3个转换
	ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 4, ADC_SampleTime_7Cycles5);  // 通道14，第4个转换
	ADC_RegularChannelConfig(ADC1, ADC_Channel_15, 5, ADC_SampleTime_7Cycles5);  // 通道15，第5个转换

	/* 使能ADC1 */
	ADC_Cmd(ADC1, ENABLE);

	/* 使能ADC DMA传输 */
	ADC_DMACmd(ADC1, ENABLE);

	/* ADC校准：复位校准寄存器 */
	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1));  // 等待复位完成

	/* ADC校准：开始校准 */
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1));  // 等待校准完成

	/* 启动ADC转换 */
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	// DMA已配置，ADC自动连续转换并传输到ADC_ConvertedValue[]
}

/**
 * @brief DMA初始化函数（用于ADC数据传输）
 * @details 配置DMA1通道1，将ADC数据自动传输到内存
 *          - 外设：ADC1数据寄存器
 *          - 内存：ADC_ConvertedValue数组
 *          - 方向：外设到内存
 *          - 模式：循环模式
 *          - 数据宽度：半字（16位）
 *
 *          DMA传输流程：
 *          ADC转换完成 → DMA自动传输到内存 → 缓冲区满后循环覆盖
 */
void DMA_Init_Adc(void)
{
	DMA_InitTypeDef DMA_InitStructure;

	/* 使能DMA1时钟 */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	/* 复位DMA1通道1 */
	DMA_DeInit(DMA1_Channel1);

	/* 配置DMA参数 */
	DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;                    // 外设地址：ADC1数据寄存器
	DMA_InitStructure.DMA_MemoryBaseAddr = (unsigned int)&ADC_ConvertedValue;      // 内存地址：转换结果数组
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;                             // 方向：外设到内存
	DMA_InitStructure.DMA_BufferSize = 5;                                          // 缓冲区大小：5个数据
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;              // 外设地址不递增
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                        // 内存地址递增
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;   // 外设数据宽度：16位
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;           // 内存数据宽度：16位
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                                // 循环模式
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;                            // 优先级：高
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                                   // 禁止内存到内存传输
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);

	/* 使能DMA通道 */
	DMA_Cmd(DMA1_Channel1, ENABLE);
}

/**
 * @brief ADC采集函数（预留接口）
 * @details 由于使用DMA自动传输，此函数预留
 *          实际数据直接从ADC_ConvertedValue[]数组读取
 */
void ADC_GetSample(void)
{
	// DMA自动传输，无需手动读取
	// 数据可直接从ADC_ConvertedValue[]数组获取
}

/* ==================== 温度转换函数 ==================== */

/**
 * @brief ADC电压值转温度值
 * @param  uiADCV: ADC采样值（mV）
 * @return 温度值（单位：0.1℃，带符号）
 *         例如：-200 表示 -20.0℃
 *               500 表示 +50.0℃
 *
 * @details 温度转换算法：
 *          1. 查找表定位：根据上次索引TemNA快速定位
 *          2. 线性插值：在相邻两点间进行线性插值
 *          3. 温度补偿：乘以0.9进行校准
 *
 *          查找表索引计算：
 *          - 温度 = -200 + 索引 × 40 (单位：0.1℃)
 *          - 例如：索引5 → 温度 = -200 + 5×40 = 0℃
 *
 * @note   此函数实现了二分查找+线性插值的组合算法
 */
signed int TemChange(unsigned int uiADCV)
{
	unsigned char ucA = 32, TemNK = 0;
	signed int uiD = 0;

	TemNK = TemNA;  // 使用上次索引作为起点，加速查找

	/* ========== 情况1：当前电压低于查找表中值 ==========
	 * 说明温度在下降，向高温方向（索引增大）查找
	 */
	if(uiADCV < TemD[TemNK])
	{
		while(--ucA)
		{
			if(TemNK < 30)
			{
				TemNK++;
				if(uiADCV >= TemD[TemNK])
				{
					/* 找到区间，进行线性插值 */
					uiD = -200 + TemNK * (int)40;  // 基准温度
					// 插值计算：uiD = uiD - 40 × (当前电压-下限电压) / (上限电压-下限电压)
					uiD = uiD - ((int)40) * (uiADCV - TemD[TemNK]) / (TemD[TemNK - 1] - TemD[TemNK]);
					break;
				}
			}
			else
			{
				uiD = 1000;  // 超出范围，返回100℃
				break;
			}
		}
	}
	/* ========== 情况2：当前电压高于查找表中值 ==========
	 * 说明温度在上升，向低温方向（索引减小）查找
	 */
	else if(uiADCV >= TemD[TemNK - 1])
	{
		while(--ucA)
		{
			if(TemNK > 1)
			{
				TemNK--;
				if(uiADCV < TemD[TemNK - 1])
				{
					/* 找到区间，进行线性插值 */
					uiD = -200 + TemNK * (int)40;  // 基准温度
					uiD = uiD - ((int)40) * (uiADCV - TemD[TemNK]) / (TemD[TemNK - 1] - TemD[TemNK]);
					break;
				}
			}
			else
			{
				uiD = -200;  // 超出范围，返回-20℃
				break;
			}
		}
	}
	/* ========== 情况3：当前电压在当前区间内 ========== */
	else
	{
		uiD = -200 + TemNK * (int)40;  // 基准温度
		uiD = uiD - ((int)40) * (uiADCV - TemD[TemNK]) / (TemD[TemNK - 1] - TemD[TemNK]);
	}

	TemNA = TemNK;  // 保存当前索引，下次使用

	/* ========== 温度补偿：乘以0.9进行校准 ========== */
	if(uiD & 0x8000)  // 负数处理
	{
		uiD &= ~0x8000;    // 取符号位
		uiD = 0x8000 - uiD;  // 取绝对值
		uiD = uiD * 90 / 100;  // 乘以0.9
		uiD &= 0x7fff;      // 清除高位
		uiD |= 0x8000;      // 恢复符号位
	}
	else  // 正数处理
	{
		uiD = uiD * 90 / 100;  // 乘以0.9
	}

	return uiD;
}
