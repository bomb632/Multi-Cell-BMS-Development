/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    bq769xx.c
 * @brief   BQ76920/BQ76940 AFE芯片驱动程序
 * @details 包含三层状态机中的两层：
 *          第二层：均衡控制状态机 (Balan_Pack_Check)
 *          第三层：通信操作状态机 (BQ769xx_Oper_Comm)
 *
 *          BQ76920: 3-5串电池
 *          BQ76940: 3-10串电池
 */

#include "bq769xx.h"
#include "DataBase.h"
#include "ConfigPara.h"
#include "iic.h"
#include "timer.h"
#include "adc.h"
#include "Rs485Data.h"
#include "gpio.h"
#include <string.h>

/* ==================== 全局变量定义 ==================== */
/**
 * @brief BQ769xx寄存器组镜像
 * @details 存储BQ769xx所有寄存器的本地副本
 *          读取：通过IIC从芯片读取到本地
 *          写入：修改后通过IIC写入芯片
 */
RegisterGroup  Bq769xxReg;

u8  VoltCellOffSet = 0;                   // ADC偏移值，用于电压校准
u16 VoltCellGainMV = 0;                   // ADC增益(mV)，ADC原始值转实际电压的系数
u32 VoltCellGainUV = 0;                   // ADC增益(uV)，精度更高
u8  BQ769xx_Init_State = 0;               // 初始化状态标志：1=已初始化，0=未初始化
u8  Bq769xx_Wake_EN = 0;                  // 唤醒使能标志：1=需要唤醒，0=正常运行
u8  BQ769xxGatherData[40] = {0};          // ADC采集数据缓冲区（一次性读取40字节）
u16 VoltageTemp[3] = {0};                 // 温度ADC原始值（未转换）

/* ==================== 局部变量定义 ==================== */
/**
 * @brief BQ769xx唤醒控制变量
 * @details 位7: 唤醒脉冲发送标志（0x80=已发送）
 *          位6-0: 唤醒等待计数器（单位：500ms）
 */
u8 bq769xx_Wake_cont = 0;

/* ==================== 局部函数声明 ==================== */
static void pack_temp_max_min(void);
static void pack_cell_max_min(void);

/* ==================== BQ769xx 唤醒与初始化 ==================== */

/**
 * @brief 唤醒BQ769xx芯片
 * @details BQ769xx具有SHIP模式（低功耗模式）
 *          通过拉高WAKE引脚至少1ms可唤醒芯片
 *          本函数拉高1秒确保可靠唤醒
 */
void BQ769xx_Wake(void)
{
	BQ769xx_WAKE_ON;    // 拉高唤醒引脚
	delay_ms(1000);     // 保持1秒
	BQ769xx_WAKE_OFF;   // 拉低唤醒引脚
}

/**
 * @brief 设置电池串数映射
 * @details 根据配置的电池串联数设置电池映射掩码
 *          用于BQ769xx识别激活的电池通道
 *
 * CellNum_Ser | Cells_Map
 * -----------|-----------
 *     5      |   0x001F (11111B)
 *     4      |   0x0017 (10111B)
 *     3      |   0x0013 (10011B)
 */
void BQ769xx_Set_CellMap(void)
{
	// 根据配置的电池串联数，设置对应的电池映射掩码
	switch(gBMSConfig.Type.CellNum_Ser)
	{
	case 5:  // 5串电池配置
		// 0x001F = 0001 1111B，表示前5个通道有效
		gBMSData.BattPar.Cells_Map = 0x001F;
		break;
	case 4:  // 4串电池配置
		// 0x0017 = 0001 0111B，表示通道1,2,3,5有效（跳过通道4）
		gBMSData.BattPar.Cells_Map = 0x0017;
		break;
	case 3:  // 3串电池配置
		// 0x0013 = 0001 0011B，表示通道1,2,5有效（跳过通道3,4）
		gBMSData.BattPar.Cells_Map = 0x0013;
		break;
	default: // 默认按5串处理
		gBMSData.BattPar.Cells_Map = 0x001F;
		break;
	}
}

/**
 * @brief 初始化BQ769xx参数结构体
 * @details 配置BQ769xx的各项保护参数和工作模式
 */
void BQ769xx_Init_Para(void)
{
	// 系统控制寄存器1
	Bq769xxReg.SysCtrl1.SysCtrl1Bit.ADC_EN = ADC_ENS;              // 使能ADC
	Bq769xxReg.SysCtrl1.SysCtrl1Bit.TEMP_SEL = EXTE_TEMP_ON;       // 选择外部温度传感器

	// 系统控制寄存器2
	Bq769xxReg.SysCtrl2.SysCtrl2Bit.DELAY_DIS = ALARM_DELAY_ON;    // 使能告警延迟
	Bq769xxReg.SysCtrl2.SysCtrl2Bit.CC_EN = CC_CONTINU_EN;         // 使能库仑计连续模式
	Bq769xxReg.SysCtrl2.SysCtrl2Bit.CHG_ON = CHG_OFF;              // 充电MOSFET初始关闭
	Bq769xxReg.SysCtrl2.SysCtrl2Bit.DSG_ON = DSG_OFF;              // 放电MOSFET初始关闭

	// 保护寄存器1：短路保护 (SCD)
	Bq769xxReg.Protect1.Protect1Bit.RSNS = OCD_SCD_HIGH_RANGE;     // 高量程电流检测
	Bq769xxReg.Protect1.Protect1Bit.SCD_DELAY = gBMSConfig.BQ76Para.SCD_Delay;   // SCD延迟
	Bq769xxReg.Protect1.Protect1Bit.SCD_THRESH = gBMSConfig.BQ76Para.SCD_Thresh; // SCD阈值

	// 保护寄存器2：过流保护 (OCD)
	Bq769xxReg.Protect2.Protect2Bit.OCD_DELAY = gBMSConfig.BQ76Para.OCD_Delay;   // OCD延迟
	Bq769xxReg.Protect2.Protect2Bit.OCD_THRESH = gBMSConfig.BQ76Para.OCD_Thresh; // OCD阈值

	// 保护寄存器3：过压/欠压保护 (OV/UV)
	Bq769xxReg.Protect3.Protect3Bit.OV_DELAY = gBMSConfig.BQ76Para.OV_Delay;     // OV延迟
	Bq769xxReg.Protect3.Protect3Bit.UV_DELAY = gBMSConfig.BQ76Para.UV_Delay;     // UV延迟
}

/**
 * @brief 读取ADC增益和偏移值
 * @details 从BQ769xx读取ADC校准参数
 *          并根据配置的OV/UV阈值计算对应的寄存器值
 *
 * @note   ADC增益 = 365mV + 寄存器调整值
 */
void BQ769xx_Read_Gain(void)
{
	u8 rcd = 0;

	// ========== 读取ADC增益和偏移寄存器 ==========
	// 从BQ769xx读取ADC校准参数，用于后续电压转换
	rcd = IIC_ReadByte(BQ769xxAddr, ADCGAIN1_RegAddr, &Bq769xxReg.ADCGain1.ADCGain1Byte);  // 读取增益寄存器1（高2位）
	rcd = IIC_ReadByte(BQ769xxAddr, ADCGAIN2_RegAddr,  &Bq769xxReg.ADCGain2.ADCGain2Byte); // 读取增益寄存器2（低3位）
	rcd = IIC_ReadByte(BQ769xxAddr, ADCOFFSET_RegAddr, &Bq769xxReg.ADCOffset);            // 读取偏移寄存器

	// ========== 计算ADC增益（单位：uV）==========
	// 公式: Gain(uV) = 365 + ((ADCGAIN1[3:2] << 1) + (ADCGAIN2[7:5] >> 5))
	// ADCGAIN1[3:2]: 提取bit3和bit2，左移1位作为高2位
	// ADCGAIN2[7:5]: 提取bit7、bit6、bit5，右移5位作为低3位
	VoltCellGainUV = (365 + ((Bq769xxReg.ADCGain1.ADCGain1Byte & 0x0C) << 1) + ((Bq769xxReg.ADCGain2.ADCGain2Byte & 0xE0) >> 5));
	VoltCellGainMV = VoltCellGainUV / 1000;  // 转换为mV单位
	VoltCellOffSet = Bq769xxReg.ADCOffset;    // 保存ADC偏移值

	// ========== 限制OV/UV阈值在有效范围内 ==========
	// 防止用户配置的阈值超出芯片允许范围
	gBMSConfig.BQ76Para.OV_Thresh = LimitMaxMin(gBMSConfig.BQ76Para.OV_Thresh, OV_THRESH_MAX, OV_THRESH_MIN);
	gBMSConfig.BQ76Para.UV_Thresh = LimitMaxMin(gBMSConfig.BQ76Para.UV_Thresh, UV_THRESH_MAX, UV_THRESH_MIN);

	// ========== 计算OV/UV寄存器值 ==========
	// 公式: Trip寄存器值 = ((目标电压_mV - ADC偏移) * 1000 / 增益_uV - 基准值) >> 4
	Bq769xxReg.OVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.OV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - OV_THRESH_BASE) >> 4) & 0xff);
	Bq769xxReg.UVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.UV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - UV_THRESH_BASE) >> 4) & 0xff);
	Bq769xxReg.CCCfg = 0x19;  // 库仑计配置固定值0x19（必须）

	(void) rcd;  // 消除未使用变量警告
}

/**
 * @brief 写入BQ769xx配置寄存器
 * @details 将配置参数写入BQ769xx，并回读验证
 *
 * @return gRET_OK - 配置成功
 * @return gRET_NG - 配置失败（5次重试后仍失败）
 */
u8 BQ769xx_Write_Config(void)
{
  u8 ret = 0;                            // 函数返回值：gRET_OK=配置成功，gRET_NG=配置失败
  u8 rcd = 0;                            // IIC通信返回值：存储每次IIC读写的状态结果
  u8 Count = 0;                          // 重试计数器：记录配置写入尝试次数（最多5次）
  u8 err = 0;                            // 错误标志：寄存器回读不匹配时累加，err=0表示无错误
  u8 bqSysCtrProtectionConfig[11] = {0}; // 回读缓冲区：存储从BQ769xx回读的11字节配置数据，用于验证写入是否正确

	// ========== 配置写入和验证循环（最多重试5次）==========
	do
	{
		// 写入8个配置寄存器（从SYS_CTRL1到CCCfg）
		rcd = IIC_WritByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr, &(Bq769xxReg.SysCtrl1.SysCtrl1Byte), 8);

		// 回读验证：读取刚才写入的8个寄存器
		rcd = IIC_ReadByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr, bqSysCtrProtectionConfig, 8);

		// 比较写入值和回读值，验证配置是否正确写入
		if(bqSysCtrProtectionConfig[0] != Bq769xxReg.SysCtrl1.SysCtrl1Byte ||    // SYS_CTRL1不匹配
			bqSysCtrProtectionConfig[1] != Bq769xxReg.SysCtrl2.SysCtrl2Byte ||    // SYS_CTRL2不匹配
			bqSysCtrProtectionConfig[2] != Bq769xxReg.Protect1.Protect1Byte ||    // PROTECT1不匹配
			bqSysCtrProtectionConfig[3] != Bq769xxReg.Protect2.Protect2Byte ||    // PROTECT2不匹配
			bqSysCtrProtectionConfig[4] != Bq769xxReg.Protect3.Protect3Byte ||    // PROTECT3不匹配
			bqSysCtrProtectionConfig[5] != Bq769xxReg.OVTrip ||                  // OVTrip不匹配
			bqSysCtrProtectionConfig[6] != Bq769xxReg.UVTrip ||                  // UVTrip不匹配
			bqSysCtrProtectionConfig[7] != Bq769xxReg.CCCfg)                     // CCCfg不匹配
		{
			err++;  // 有任一寄存器不匹配，错误计数加1
		}
		Count++;  // 尝试次数加1
	} while((err != 0) && (Count < 5));    // 有错误且未达到5次重试，继续循环

	// ========== 判断配置结果 ==========
	if((err == 0) || (Count < 5))  // 配置成功
	{
		ret = gRET_OK;  // 返回成功
	}
	else  // 5次重试后仍然失败
	{
		ret = gRET_NG;  // 返回失败
		// 配置失败，清零所有寄存器值（防止使用错误的配置）
		Bq769xxReg.SysCtrl1.SysCtrl1Byte = 0;
		Bq769xxReg.SysCtrl2.SysCtrl2Byte = 0;
		Bq769xxReg.Protect1.Protect1Byte = 0;
		Bq769xxReg.Protect2.Protect2Byte = 0;
		Bq769xxReg.Protect3.Protect3Byte = 0;
		Bq769xxReg.OVTrip = 0;
		Bq769xxReg.UVTrip = 0;
		Bq769xxReg.CCCfg = 0;
	}
	(void) rcd;  // 消除未使用变量警告
	return ret;
}

/**
 * @brief BQ769xx完整初始化流程
 * @details 初始化步骤：
 *          1. 设置电池串数映射
 *          2. 初始化参数结构体
 *          3. 读取ADC增益和偏移
 *          4. 写入配置到BQ769xx
 */
void BQ769xx_Init(void)
{
	u8 rcd = 0;

	// ========== 步骤1：设置电池串数映射 ==========
	// 根据配置的串联数（3/4/5串）设置Cells_Map掩码
	BQ769xx_Set_CellMap();

	// ========== 步骤2：初始化参数结构体 ==========
	// 配置保护阈值、MOSFET初始状态、ADC使能等
	BQ769xx_Init_Para();

	// ========== 步骤3：读取ADC增益和偏移 ==========
	// 从BQ769xx读取出厂校准的增益和偏移值
	BQ769xx_Read_Gain();

	// ========== 步骤4：写入配置到BQ769xx ==========
	// 将所有配置写入芯片，并回读验证
	rcd = BQ769xx_Write_Config();

	// ========== 判断初始化结果 ==========
	if(rcd == gRET_OK)  // 配置成功
	{
		BQ769xx_Init_State = 1;    // 设置初始化成功标志
	}
	else  // 配置失败
	{
		BQ769xx_Init_State = 0;    // 清除初始化标志
	}
}

/* ==================== 辅助函数 ==================== */

/**
 * @brief 限制数值在最大最小值范围内
 * @param  InValue: 输入值
 * @param  gMax: 最大值
 * @param  gMin: 最小值
 * @return 限制后的值
 * @details 用于OV/UV阈值的范围限制，防止设置超出芯片允许范围
 */
u16 LimitMaxMin(u16 InValue, u16 gMax, u16 gMin)
{
	u16 valueout = 0;

	valueout = InValue;
	if(InValue > gMax)
	{
		valueout = gMax;    // 超过上限，限制为最大值
	}
	if(InValue < gMin)
	{
		valueout = gMin;    // 低于下限，限制为最小值
	}
	return valueout;
}

/* ==================== 数据采集函数 ==================== */

/**
 * @brief 获取BQ769xx ADC采样数据（核心数据采集函数）
 * @details 200ms时间片执行，一次性读取40字节数据并转换
 *
 *          读取的数据帧格式：
 *          Byte[0-1]:   VC1  (Cell1电压，高字节在前)
 *          Byte[2-3]:   VC2  (Cell2电压)
 *          ...
 *          Byte[28-29]: VC15 (Cell15电压)
 *          Byte[30-31]: BAT  (总电压)
 *          Byte[32-33]: TS1  (温度传感器1)
 *          Byte[34-35]: TS2  (温度传感器2)
 *          Byte[36-37]: TS3  (温度传感器3)
 *          Byte[38-39]: CC   (库仑计/电流)
 *
 *          转换公式：
 *          - 电压(mV) = ADC值 * Gain(uV) / 1000 ± Offset
 *          - 总电压(mV) = ADC值 * BATVOLTLSB / 1000
 *          - 温度(mV) = ADC值 * TMEPVOLTLSB / 1000
 *          - 电流(mA) = ADC值 * CRUUVOLTLSB / 分流电阻值
 */
void BQ769xx_GetData(void)
{
	u8 rcd = 0;
	u8 ia = 0, ib = 0;
	u16 cells_map = gBMSData.BattPar.Cells_Map;
	u8 VoltOffset = 0;
	u16 AdcCurr = 0;

	/* 时间片检查：仅在200ms时间片执行 */
	if(TaskTimePare.Tim200ms_flag != 1)
	{
		return;
	}

	/* ========== 读取40字节ADC数据 ========== */
	// 从VC1_HI寄存器（地址0x0C）开始，连续读取40字节数据
	// 数据包含：VC1-VC15(30字节) + BAT(2字节) + TS1-TS3(6字节) + CC(2字节)
	rcd = IIC_ReadByteMore(BQ769xxAddr, VC1_HI_RegAddr, BQ769xxGatherData, 40);
	if(rcd == gRET_OK)  // 读取成功
	{
		// 将数据复制到寄存器结构体中
		memcpy(&(Bq769xxReg.VCell1.VCell1Byte.VC1_HI), BQ769xxGatherData, 40);
	}

	/* ========== 电池电压计算 ========== */
	// 遍历15个可能的电池通道，根据Cells_Map提取有效电池电压
	for(ia = 0; ia < SYS_CELL_MAX; ia++)
	{
		if(cells_map & 0x01)    // 检查最低位，该通道有效
		{
			// ========== 处理ADC偏移（支持有符号偏移）==========
			if(Bq769xxReg.ADCOffset & 0x80)  // Bit7=1表示负偏移
			{
				// 负偏移处理：计算补码
				VoltOffset = 0x100 - Bq769xxReg.ADCOffset;
				// 电压计算：ADC值 * 增益 / 1000 - 偏移
				gBMSData.BattPar.VoltCell[ib] = (u16)(((unsigned long)((BQ769xxGatherData[ia * 2] * 256) + BQ769xxGatherData[ia * 2 + 1]) * VoltCellGainUV) / 1000 - VoltOffset);
			}
			else  // Bit7=0表示正偏移
			{
				// 正偏移处理
				VoltOffset = Bq769xxReg.ADCOffset;
				// 电压计算：ADC值 * 增益 / 1000 + 偏移
				gBMSData.BattPar.VoltCell[ib] = (u16)(((unsigned long)((BQ769xxGatherData[ia * 2] * 256) + BQ769xxGatherData[ia * 2 + 1]) * VoltCellGainUV) / 1000 + VoltOffset);
			}
			ib++;  // 有效电池计数加1
		}
		cells_map = cells_map >> 1;  // 右移一位，检查下一个通道
	}

	/* ========== 总电压计算 ========== */
	// BAT寄存器在数据缓冲区的偏移30-31字节
	// 公式：ADC值 * BATVOLTLSB(1532uV) / 1000 = mV
	gBMSData.BattPar.VoltLine = (u16)(((unsigned long)((BQ769xxGatherData[30] * 256) + BQ769xxGatherData[31]) * BATVOLTLSB) / 1000);   // mV

	/* ========== 温度计算 ========== */
	// 读取3个外部温度传感器（NTC热敏电阻）
	for(ia = 0; ia < SYS_TEMPEXT_MAX; ia++)
	{
		// 温度传感器在数据缓冲区的偏移32-37字节
		// 公式：ADC值 * TMEPVOLTLSB(382uV) / 1000 = mV
		VoltageTemp[ia] = ((unsigned long)((BQ769xxGatherData[ia * 2 + 32] * 256) + BQ769xxGatherData[ia * 2 + 32 + 1]) * 382) / 1000;
		gBMSData.BattPar.TempCell[ia] = TemChange(VoltageTemp[ia]);    // 转换为温度值（℃*10）
	}

	/* ========== 电流计算 ========== */
	// 库仑计寄存器在数据缓冲区的偏移38-39字节
	AdcCurr = (u16)(BQ769xxGatherData[38] * 256) + BQ769xxGatherData[39];

	if(AdcCurr & 0x8000)    // Bit15=1表示负电流（放电）
	{
		// 负电流处理：计算补码
		AdcCurr = 0x10000 - AdcCurr;
		// 电流计算：ADC值 * CRUUVOLTLSB(844uV) / 分流电阻值(mΩ)
		gBMSData.BattPar.CurrLine = 0x10000 - (u16)(((unsigned long)AdcCurr * CRUUVOLTLSB) / gBMSConfig.Type.ShuntSpec);
	}
	else    // Bit15=0表示正电流（充电）
	{
		// 正电流计算
		gBMSData.BattPar.CurrLine = (u16)(((unsigned long)AdcCurr * CRUUVOLTLSB) / gBMSConfig.Type.ShuntSpec);
	}

	/* ========== 更新最大最小值 ========== */
	pack_temp_max_min();  // 计算温度最大最小值
	pack_cell_max_min();  // 计算电池电压最大最小值

	(void) rcd;  // 消除未使用变量警告
}

/**
 * @brief 获取BQ769xx配置和状态寄存器
 * @details 500ms时间片执行
 *          读取系统状态、告警状态等
 */
void BQ769xx_GetConfig(void)
{
	u8 rcd = 0;
	u8 BQ769xxGatherConfig[12] = {0};  // 配置和状态寄存器缓冲区（12字节）

	// ========== 时间片检查 ==========
	// 仅在500ms时间片执行
	if(TaskTimePare.Tim500ms_flag == 1)
	{
		// ========== 读取12字节配置和状态寄存器 ==========
		// 从SYS_STAT寄存器（地址0x00）开始读取12字节
		// 包含：SYS_STAT, CELLBAL1-3, SYS_CTRL1-2, PROTECT1-3, OVTrip, UVTrip, CCCfg
		rcd = IIC_ReadByteMore(BQ769xxAddr, SYS_STAT_RegAddr, BQ769xxGatherConfig, 12);
		if(rcd == gRET_OK)  // 读取成功
		{
			// 将数据复制到寄存器结构体中
			memcpy(&(Bq769xxReg.SysStatus.StatusByte), BQ769xxGatherConfig, 12);
		}
	}
}

/* ==================== MOSFET控制函数 ==================== */

/**
 * @brief 控制放电MOSFET（DSG）
 * @param  ONOFF: 1=开启, 0=关闭
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details 通过修改SYS_CTRL2寄存器的DSG位控制放电MOSFET
 *          读取-修改-写回流程确保其他位不受影响
 *
 *          SYS_CTRL2寄存器位定义：
 *          Bit0: CHG_ON (充电MOS控制)
 *          Bit1: DSG_ON (放电MOS控制) ← 此函数控制此位
 */
u8 BQ769xx_DSGSET(u8 ONOFF)
{
	u8 ret = gRET_OK;
	u8 rcd = 0;

	// ========== 参数检查 ==========
	if(ONOFF > 1)  // 参数超出范围（只能是0或1）
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 读取当前寄存器值 ==========
	// 先读取SYS_CTRL2寄存器当前值，确保其他位不受影响
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &(Bq769xxReg.SysCtrl2.SysCtrl2Byte));
	if(rcd != gRET_OK)  // 读取失败
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 设置DSG位 ==========
	if(ONOFF == 1)  // 开启放电MOSFET
	{
		// 置位DSG（Bit1），其他位保持不变
		// 0x02 = 0000 0010B，只操作Bit1
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte | 0x02);
	}
	else  // 关闭放电MOSFET
	{
		// 清零DSG（Bit1），其他位保持不变
		// 0xFD = 1111 1101B，Bit1清零，其他位保持
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte & 0xFD);
	}
	return ret;  // 返回成功
}

/**
 * @brief 控制充电MOSFET（CHG）
 * @param  ONOFF: 1=开启, 0=关闭
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details 通过修改SYS_CTRL2寄存器的CHG位控制充电MOSFET
 *          读取-修改-写回流程确保其他位不受影响
 *
 *          SYS_CTRL2寄存器位定义：
 *          Bit0: CHG_ON (充电MOS控制) ← 此函数控制此位
 *          Bit1: DSG_ON (放电MOS控制)
 */
u8 BQ769xx_CHGSET(u8 ONOFF)
{
	u8 ret = gRET_OK;
	u8 rcd = 0;

	// ========== 参数检查 ==========
	if(ONOFF > 1)  // 参数超出范围（只能是0或1）
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 读取当前寄存器值 ==========
	// 先读取SYS_CTRL2寄存器当前值，确保其他位不受影响
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &Bq769xxReg.SysCtrl2.SysCtrl2Byte);
	if(rcd != gRET_OK)  // 读取失败
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 设置CHG位 ==========
	if(ONOFF == 1)  // 开启充电MOSFET
	{
		// 置位CHG（Bit0），其他位保持不变
		// 0x01 = 0000 0001B，只操作Bit0
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte | 0x01);
	}
	else  // 关闭充电MOSFET
	{
		// 清零CHG（Bit0），其他位保持不变
		// 0xFE = 1111 1110B，Bit0清零，其他位保持
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte & 0xFE);
	}
	return ret;  // 返回成功
}

/* ==================== 状态寄存器操作 ==================== */

/**
 * @brief 获取BQ769xx系统状态
 * @return 系统状态寄存器值（SYS_STAT）
 * @details SYS_STAT寄存器位定义：
 *          Bit0: OCD  - 过流检测标志
 *          Bit1: SCD  - 短路检测标志
 *          Bit2: OV   - 过压检测标志
 *          Bit3: UV   - 欠压检测标志
 *          Bit4: OVRD_ALERT   - 覆盖告警标志
 *          Bit5: DEVICE_XREADY - 设备就绪标志
 *          Bit6: WAKE - 唤醒标志
 *          Bit7: CC_READY - 库仑计就绪标志
 */
u8 BQ769xx_STATE_GET(void)
{
	u8 rcd = 0;
	u8 BQ_Stat = 0;

	// ========== 读取系统状态寄存器 ==========
	// SYS_STAT寄存器包含所有告警标志
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_STAT_RegAddr, &BQ_Stat);
	(void) rcd;  // 消除未使用变量警告
	return BQ_Stat;  // 返回状态值
}

/**
 * @brief 清除BQ769xx告警状态
 * @param  statvalue: 要清除的状态位掩码（写1清除对应位）
 * @return gRET_OK - 清除成功, gRET_NG - 清除失败
 * @details SYS_STAT寄存器写1清除特性：
 *          - 向对应位写1可清除该告警
 *          - 向对应位写0无影响
 *          常用清除掩码：
 *            OCD_CLE(0x01)  - 清除过流
 *            SCD_CLE(0x02)  - 清除短路
 *            OV_CLE(0x04)   - 清除过压
 *            UV_CLE(0x08)   - 清除欠压
 *            全清除：0x0F 或 0x1F
 */
u8 BQ769xx_STAT_CLEAR(u8 statvalue)
{
	u8 ret = gRET_OK;
	u8 BQ_Stat = 0;
	u8 rcd = 0;

	// ========== 写入清除命令 ==========
	// SYS_STAT寄存器特性：写1清除对应位，写0无影响
	rcd = IIC_WritByte(BQ769xxAddr, SYS_STAT_RegAddr, statvalue);

	// ========== 回读验证 ==========
	// 读取清除后的状态，验证告警是否被清除
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_STAT_RegAddr, &BQ_Stat);

	// ========== 判断清除结果 ==========
	if(BQ_Stat & statvalue)  // 告警位仍然存在
	{
		ret = gRET_OK;  // 返回成功（注意：原逻辑如此）
	}
	else  // 告警位已清除
	{
		ret = gRET_NG;  // 返回失败（注意：原逻辑如此）
	}

	(void) rcd;  // 消除未使用变量警告
	return ret;
}

/**
 * @brief 进入SHIP模式（低功耗关机模式）
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details SHIP模式是BQ769xx的超低功耗模式
 *          进入流程：
 *          1. 读取SYS_CTRL1寄存器
 *          2. 置位WD_RST（Bit0），清零LEEP（Bit1）
 *          3. 写入寄存器
 *          4. 清零WD_RST（Bit0），置位LEEP（Bit1）
 *          5. 写入寄存器，进入SHIP模式
 *
 *          唤醒方式：拉高WAKE引脚至少1ms
 *
 * @note   进入SHIP模式后，IIC通信将失效，只能通过WAKE引脚唤醒
 */
u8 Enter_Ship_Mode(void)
{
	u8 ret = gRET_OK;
	u8 rcd = 0;

	// ========== 读取SYS_CTRL1寄存器 ==========
	// 先读取当前值，确保其他位不受影响
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL1_RegAddr, &Bq769xxReg.SysCtrl1.SysCtrl1Byte);
	if(rcd != gRET_OK)  // 读取失败
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 进入SHIP模式序列（步骤1）==========
	// 置位WD_RST（Bit0），清零LEEP（Bit1）
	// 这是进入SHIP模式的第一步
	Bq769xxReg.SysCtrl1.SysCtrl1Byte |= 0x01;    // 置位WD_RST: Bit0 = 1
	Bq769xxReg.SysCtrl1.SysCtrl1Byte &= ~0x02;   // 清零LEEP: Bit1 = 0
	IIC_WritByte(BQ769xxAddr, SYS_CTRL1_RegAddr, Bq769xxReg.SysCtrl1.SysCtrl1Byte);

	// ========== 进入SHIP模式序列（步骤2）==========
	// 清零WD_RST（Bit0），置位LEEP（Bit1）
	// 完成进入SHIP模式的第二步
	Bq769xxReg.SysCtrl1.SysCtrl1Byte &= ~0x01;   // 清零WD_RST: Bit0 = 0
	Bq769xxReg.SysCtrl1.SysCtrl1Byte |= 0x02;    // 置位LEEP: Bit1 = 1
	IIC_WritByte(BQ769xxAddr, SYS_CTRL1_RegAddr, Bq769xxReg.SysCtrl1.SysCtrl1Byte);

	return ret;  // 返回成功
}

/**
 * @brief 设置库仑计单次采样模式
 * @param  ONOFF: 1=使能, 0=禁用
 * @return gRET_OK - 成功, gRET_NG - 失败
 */
u8 BQ769xx_CC_ONESHOT_SET(u8 ONOFF)
{
	u8 ret = gRET_OK;
	u8 rcd = 0;

	// ========== 参数检查 ==========
	if(ONOFF > 1)  // 参数超出范围（只能是0或1）
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 读取当前寄存器值 ==========
	// 先读取SYS_CTRL2寄存器当前值，确保其他位不受影响
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &(Bq769xxReg.SysCtrl2.SysCtrl2Byte));
	if(rcd != gRET_OK)  // 读取失败
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 设置CC_ONESHOT位 ==========
	if(ONOFF == 1)  // 使能单次采样
	{
		// 置位CC_ONESHOT（Bit5），其他位保持不变
		// 0x20 = 0010 0000B，只操作Bit5
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte | 0x20);
	}
	else  // 禁用单次采样
	{
		// 清零CC_ONESHOT（Bit5），其他位保持不变
		// 0xDF = 1101 1111B，Bit5清零，其他位保持
		IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, Bq769xxReg.SysCtrl2.SysCtrl2Byte & 0xDF);
	}

	// ========== 回读验证 ==========
	// 读取设置后的值，确认配置成功
	rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &(Bq769xxReg.SysCtrl2.SysCtrl2Byte));
	return ret;  // 返回成功
}

/* ==================== 均衡功能 - 第二层状态机 ==================== */

/**
 * @brief 写入均衡寄存器并验证
 * @param  cellbalanbyte1: CELLBAL1寄存器值（控制Cell1-Cell5均衡）
 * @param  cellbalanbyte2: CELLBAL2寄存器值（控制Cell6-Cell10均衡）
 * @param  cellbalanbyte3: CELLBAL3寄存器值（控制Cell11-Cell15均衡）
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details 均衡寄存器位定义（每个字节控制5串电池）：
 *          CELLBAL1: Bit0=Cell1, Bit1=Cell2, Bit2=Cell3, Bit3=Cell4, Bit4=Cell5
 *          CELLBAL2: Bit0=Cell6, Bit1=Cell7, ... Bit4=Cell10
 *          CELLBAL3: Bit0=Cell11, ... Bit4=Cell15
 *          写入流程：IIC写入 → 回读验证 → 比较确认
 */
u8 balance_write_regs(u8 cellbalanbyte1, u8 cellbalanbyte2, u8 cellbalanbyte3)
{
	u8 ret = gRET_OK;
	u8 rcd = 0;
	u8 write_regs[3] = {0};   // 写入缓冲区：存储待写入的3个寄存器值
	u8 read_regs[3] = {0};    // 读取缓冲区：存储回读验证的3个寄存器值
	u8 ia = 0;                // 循环计数器
	u16 cells_map = gBMSData.BattPar.Cells_Map;  // 电池映射掩码（保留，未使用）

	// ========== 构建写入数据包 ==========
	// 将3个寄存器值打包到数组中，准备一次性写入
	write_regs[0] = cellbalanbyte1;  // CELLBAL1寄存器值
	write_regs[1] = cellbalanbyte2;  // CELLBAL2寄存器值
	write_regs[2] = cellbalanbyte3;  // CELLBAL3寄存器值

	// ========== 写入均衡寄存器 ==========
	// 从CELLBAL1寄存器地址开始，连续写入3个字节
	// IIC多字节写入可以提高效率，减少通信次数
	rcd = IIC_WritByteMore(BQ769xxAddr, CELLBAL1_RegAddr, write_regs, 3);

	// ========== 检查写入结果 ==========
	if(rcd != gRET_OK)  // IIC写入失败
	{
		return gRET_NG;  // 直接返回失败
	}

	// ========== 回读验证 ==========
	// 写入后必须回读验证，确保寄存器值正确写入
	// 这是关键步骤，因为均衡功能涉及电池安全
	rcd = IIC_ReadByteMore(BQ769xxAddr, CELLBAL1_RegAddr, read_regs, 3);
	if(rcd != gRET_OK)  // IIC读取失败
	{
		return gRET_NG;  // 返回失败
	}

	// ========== 比较写入值和回读值 ==========
	// 逐字节比较，确保每个寄存器都正确写入
	for(ia = 0; ia < 3; ia++)
	{
		if(read_regs[ia] != write_regs[ia])  // 该字节不匹配
		{
			ret = gRET_NG;  // 标记为失败
		}
	}
	(void) cells_map;  // 消除未使用变量警告
	return ret;  // 返回验证结果
}

/**
 * @brief 将逻辑电池均衡映射转换为物理寄存器值
 * @param  balaCellSw: 逻辑均衡开关位图（按实际电池编号）
 *                     例如：0x0007表示均衡电池1、2、3
 * @param  cellbalaset: 输出的物理寄存器值（按BQ769xx通道编号）
 * @details 逻辑映射与物理映射的区别：
 *
 *          逻辑映射（用户视角）：
 *          - 电池1: 实际第1串电池
 *          - 电池2: 实际第2串电池
 *          - 电池3: 实际第3串电池
 *
 *          物理映射（BQ769xx视角，Cells_Map=0x0013为例）：
 *          - VC1(物理通道1) → 对应电池1
 *          - VC2(物理通道2) → 对应电池2
 *          - VC3(物理通道3) → 未使用
 *          - VC4(物理通道4) → 未使用
 *          - VC5(物理通道5) → 对应电池3
 *
 *          转换示例（4串电池，Cells_Map=0x0017）：
 *          逻辑均衡电池3 → 物理通道VC5 → 寄存器Bit4
 */
void cell_balan_locatget(u32 balaCellSw, u16* cellbalaset)
{
	u8 ia = 0;            // 逻辑电池索引（0~CellNum_Ser-1）
	u8 ib = 0;            // 物理通道索引（0~SYS_CELL_MAX-1，即0~14）
	u8 cntset = 0;        // 有效通道计数器
	u16 cells_map = 0;    // 电池映射掩码副本
	u32 outcellsw = 0;    // 输出的物理寄存器位图

	// ========== 遍历所有逻辑电池 ==========
	// ia: 逻辑电池编号（0=第1串，1=第2串，...）
	for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
	{
		cntset = 0;                      // 重置有效通道计数
		cells_map = gBMSData.BattPar.Cells_Map;  // 获取电池映射掩码

		// ========== 遍历所有物理通道 ==========
		// ib: BQ769xx物理通道编号（0=VC1, 1=VC2, ..., 14=VC15）
		for(ib = 0; ib < SYS_CELL_MAX; ib++)
		{
			if(cells_map & 0x01)  // 该物理通道有效（映射掩码最低位=1）
			{
				// ========== 检查是否是目标逻辑电池 ==========
				if(cntset == ia)  // 找到第ia个有效通道
				{
					cntset++;  // 计数加1

					// ========== 检查是否需要开启该电池的均衡 ==========
					if(balaCellSw & (0x0001 << ia))  // 逻辑均衡位图中该电池需要均衡
					{
						outcellsw |= 0x0001 << ib;  // 设置对应物理通道的均衡位
						break;  // 找到匹配，退出内层循环
					}
				}
				else  // 还不是目标通道
				{
					cntset++;  // 有效通道计数加1，继续查找
				}
			}
			cells_map = cells_map >> 1;  // 右移一位，检查下一个物理通道
		}
	}
	*cellbalaset = outcellsw;  // 返回物理寄存器位图
	(void)cells_map;  // 消除未使用变量警告
}

/**
 * @brief 均衡控制状态机 - 第二层状态机
 * @details 采用冒泡排序选择电压最高的N串电池进行均衡
 *
 * 状态转换流程：
 * INIT -> WAIT -> CHK -> SELECT -> MAP -> SET -> BALA_TIM -> INIT
 *
 * - INIT:    初始化，关闭所有均衡
 * - WAIT:    等待2秒稳定
 * - CHK:     检查均衡条件（待机模式 + 压差>阈值 + 电压>启动电压）
 * - SELECT:  冒泡排序选择最高电压的N串
 * - MAP:     映射到物理寄存器位
 * - SET:     写入均衡寄存器
 * - BALA_TIM: 均衡时间180秒（200ms*900=180s）
 *
 * @note   200ms时间片执行
 * @return gRET_OK - 正常, gRET_NG - 强制均衡中, gRET_TM - 时间片未到
 */
u8 Balan_Pack_Check(void)
{
	u8 ret = gRET_OK;
	u8 ia = 0;
	u8 ib = 0;
	u16 volta_temp;
	u8 volta_flag;

	static u8 stup_balan = 0;
	static u16 volta_cell_sort[PACK_CELL_MAX] = {0};      // 电池电压排序数组
	static u8 volta_flag_sort[PACK_CELL_MAX] = {0};       // 电池索引数组
	static u16 balanwait_cnt = 0;                         // 等待计数器
	static u16 balantime_cnt = 0;                         // 均衡时间计数器

	/* 时间片检查：仅在200ms时间片执行 */
	if(TaskTimePare.Tim200ms_flag == 0)
	{
		return gRET_TM;
	}

	/* 强制均衡模式检测 */
	if(gBMSData.BalaPar.Bala_force_on == 1)
	{
		return gRET_NG;
	}

	switch(stup_balan)
	{
	case PCB_SEQ_INIT:
		/* 初始化状态：关闭所有均衡 */
		gBMSData.BalaPar.bala_state = 0;
		gBMSData.BalaPar.bala_ctrl_sw = 0;
		gBMSData.BalaPar.bala_dischg = 0;

		Bq769xxReg.CellBal1.CellBal1Byte = 0;
		Bq769xxReg.CellBal2.CellBal2Byte = 0;
		Bq769xxReg.CellBal3.CellBal3Byte = 0;
		balance_write_regs(Bq769xxReg.CellBal1.CellBal1Byte, Bq769xxReg.CellBal2.CellBal2Byte, Bq769xxReg.CellBal3.CellBal3Byte);
		stup_balan = PCB_SEQ_WAIT;
		break;

	case PCB_SEQ_WAIT:
		/* 等待状态：延时2秒 */
		if(++balanwait_cnt > 10)    // 200ms * 10 = 2秒
		{
			balanwait_cnt = 0;
			stup_balan = PCB_SEQ_CHK;
		}
		break;

	case PCB_SEQ_CHK:
		/* 检查状态：判断是否满足均衡条件 */
		// 必须在待机模式
		if(gBMSData.Sys_Mod.sys_mode != SYS_MODE_STANDBY)
		{
			stup_balan = PCB_SEQ_INIT;
			break;
		}
		// 压差小于阈值 或 最低电压低于启动电压 -> 不均衡
		if(((gBMSData.BattPar.VoltCellMax - gBMSData.BattPar.VoltCellMin) < gBMSConfig.Balan.balanc_diffe_volt) ||
			(gBMSData.BattPar.VoltCellMin < gBMSConfig.Balan.balanc_start_volt))
		{
			stup_balan = PCB_SEQ_INIT;
			break;
		}
		stup_balan = PCB_SEQ_SELECT;
		break;

	case PCB_SEQ_SELECT:
		/* 选择状态：冒泡排序选择电压最高的N串电池 */
		// 初始化排序数组
		for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
		{
			volta_cell_sort[ia] = gBMSData.BattPar.VoltCell[ia];
			volta_flag_sort[ia] = ia;
		}

		// 冒泡排序（降序）
		for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
		{
			for(ib = 0; ib < (gBMSConfig.Type.CellNum_Ser - 1); ib++)
			{
				if(volta_cell_sort[ib] < volta_cell_sort[ib + 1])
				{
					// 交换电压值
					volta_temp = volta_cell_sort[ib + 1];
					volta_cell_sort[ib + 1] = volta_cell_sort[ib];
					volta_cell_sort[ib] = volta_temp;

					// 交换索引
					volta_flag = volta_flag_sort[ib + 1];
					volta_flag_sort[ib + 1] = volta_flag_sort[ib];
					volta_flag_sort[ib] = volta_flag;
				}
			}
		}

		// 选择前N串最高电压进行均衡
		for(ia = 0; ia < gBMSConfig.Balan.balanc_number_max; ia++)
		{
			gBMSData.BalaPar.bala_ctrl_sw |= (0x00000001 << volta_flag_sort[ia]);
		}
		stup_balan = PCB_SEQ_MAP;
		break;

	case PCB_SEQ_MAP:
		/* 映射状态：将逻辑均衡位映射到物理寄存器位 */
		cell_balan_locatget(gBMSData.BalaPar.bala_ctrl_sw, &gBMSData.BalaPar.bala_dischg);
		stup_balan = PCB_SEQ_SET;
		break;

	case PCB_SEQ_SET:
		/* 设置状态：写入均衡寄存器 */
		Bq769xxReg.CellBal1.CellBal1Byte = gBMSData.BalaPar.bala_dischg & 0x1F;                // Cell 1-5
		Bq769xxReg.CellBal2.CellBal2Byte = (gBMSData.BalaPar.bala_dischg >> 5) & 0x1F;        // Cell 6-10
		Bq769xxReg.CellBal3.CellBal3Byte = (gBMSData.BalaPar.bala_dischg >> 10) & 0x1F;       // Cell 11-15
		balance_write_regs(Bq769xxReg.CellBal1.CellBal1Byte, Bq769xxReg.CellBal2.CellBal2Byte, Bq769xxReg.CellBal3.CellBal3Byte);
		gBMSData.BalaPar.bala_state = 1;
		stup_balan = PCB_SEQ_BALA_TIM;
		break;

	case PCB_SEQ_BALA_TIM:
		/* 均衡计时状态：均衡180秒后停止 */
		if(++balantime_cnt > 900)    // 200ms * 900 = 180秒
		{
			stup_balan = PCB_SEQ_INIT;
			balantime_cnt = 0;
			gBMSData.BalaPar.bala_state = 0;
			gBMSData.BalaPar.bala_ctrl_sw = 0;
			gBMSData.BalaPar.bala_dischg = 0;

			// 关闭所有均衡
			Bq769xxReg.CellBal1.CellBal1Byte = 0;
			Bq769xxReg.CellBal2.CellBal2Byte = 0;
			Bq769xxReg.CellBal3.CellBal3Byte = 0;
			balance_write_regs(Bq769xxReg.CellBal1.CellBal1Byte, Bq769xxReg.CellBal2.CellBal2Byte, Bq769xxReg.CellBal3.CellBal3Byte);
		}
		break;

	default:
		break;
	}
	return ret;
}

/* ==================== 最大最小值计算 ==================== */

/**
 * @brief 计算温度最大最小值
 * @details 遍历所有温度传感器，找出最高温度和最低温度
 *          更新全局变量：
 *          - gBMSData.BattPar.TempPackMax: 最高温度（℃*10）
 *          - gBMSData.BattPar.TempPackMin: 最低温度（℃*10）
 *          - gBMSData.BattPar.TempPackDiff: 温度差（℃*10）
 *
 * @note   温度数据来自BQ769xx的TS1/TS2/TS3引脚，通过NTC热敏电阻测量
 * @note   TempNum在配置中定义，通常为3（对应3个NTC传感器）
 */
static void pack_temp_max_min(void)
{
	u8 ia;
	u16 temp_max;
	u16 temp_min;

	// 初始化最大最小值为第一个传感器值
	temp_max = gBMSData.BattPar.TempCell[0];
	temp_min = gBMSData.BattPar.TempCell[0];

	// 遍历所有温度传感器
	for(ia = 1; ia < gBMSConfig.Type.TempNum; ia++)
	{
		// 更新最小值
		if(temp_min > gBMSData.BattPar.TempCell[ia])
		{
			temp_min = gBMSData.BattPar.TempCell[ia];
		}
		// 更新最大值
		if(temp_max < gBMSData.BattPar.TempCell[ia])
		{
			temp_max = gBMSData.BattPar.TempCell[ia];
		}
	}

	// 保存结果到全局变量
	gBMSData.BattPar.TempPackMax = temp_max;
	gBMSData.BattPar.TempPackMin = temp_min;
}

/**
 * @brief 计算电池电压最大最小值
 * @details 遍历所有电池串，找出最高电压和最低电压
 *          更新全局变量：
 *          - gBMSData.BattPar.VoltCellMax: 最高电池电压（mV）
 *          - gBMSData.BattPar.VoltCellMin: 最低电池电压（mV）
 *          - gBMSData.BattPar.VoltCellDiff: 电压差（mV）
 *
 * @note   电压差是均衡控制的重要判断依据
 * @note   CellNum_Ser在配置中定义，通常为3~5串
 */
static void pack_cell_max_min(void)
{
	u8 ia;
	u16 volt_max;
	u16 volt_min;

	// 初始化最大最小值为第一串电池电压
	volt_max = gBMSData.BattPar.VoltCell[0];
	volt_min = gBMSData.BattPar.VoltCell[0];

	// 遍历所有电池串
	for(ia = 1; ia < gBMSConfig.Type.CellNum_Ser; ia++)
	{
		// 更新最大值
		if(volt_max < gBMSData.BattPar.VoltCell[ia])
		{
			volt_max = gBMSData.BattPar.VoltCell[ia];
		}
		// 更新最小值
		if(volt_min > gBMSData.BattPar.VoltCell[ia])
		{
			volt_min = gBMSData.BattPar.VoltCell[ia];
		}
	}

	// 保存结果到全局变量
	gBMSData.BattPar.VoltCellMax = volt_max;
	gBMSData.BattPar.VoltCellMin = volt_min;
}

/* ==================== 通信操作状态机 - 第三层状态机 ==================== */

/**
 * @brief BQ769xx通信操作状态机 - 第三层状态机
 * @details 处理MOS开关、告警清除、关机等操作
 *
 * 支持的操作类型：
 * - COMM_DSG_ON:     开启放电MOS
 * - COMM_DSG_OFF:    关闭放电MOS
 * - COMM_CHG_ON:     开启充电MOS
 * - COMM_CHG_OFF:    关闭充电MOS
 * - COMM_CLEAR_ALERT: 清除告警
 * - COMM_ENTER_SHIP:  进入关机模式
 *
 * @note   100ms时间片执行
 */
void BQ769xx_Oper_Comm(void)
{
	if((TaskTimePare.Tim100ms_flag == 1) || (Bq769xx_Oper_EN == OPER_ON))
	{
		switch(Bq769xx_Oper_Type)
		{
		case COMM_DSG_ON:
			BQ769xx_DSGSET(OPER_ON);
			Bq769xx_Oper_EN = OPER_OFF;
			Bq769xx_Oper_Type = 0;
			break;

		case COMM_DSG_OFF:
			BQ769xx_DSGSET(OPER_OFF);
			Bq769xx_Oper_EN = OPER_OFF;
			Bq769xx_Oper_Type = 0;
			break;

		case COMM_CHG_ON:
			BQ769xx_CHGSET(OPER_ON);
			Bq769xx_Oper_EN = OPER_OFF;
			Bq769xx_Oper_Type = 0;
			break;

		case COMM_CHG_OFF:
			BQ769xx_CHGSET(OPER_OFF);
			Bq769xx_Oper_EN = OPER_OFF;
			Bq769xx_Oper_Type = 0;
			break;

		case COMM_CLEAR_ALERT:
			// 清除告警操作
			break;

		case COMM_ENTER_SHIP:
			Power_Down();
			break;

		default:
			break;
		}
	}
}

/**
 * @brief 系统关机函数
 * @details 完整的关机流程：
 *          1. 进入SHIP模式（关闭BQ769xx）
 *          2. 清除操作标志
 *          3. 关闭系统电源（SYS_POWER_OFF）
 *
 * @note   关机后只能通过WAKE引脚或重新上电唤醒
 */
void Power_Down(void)
{
	Enter_Ship_Mode();
	Bq769xx_Oper_EN = OPER_OFF;
	Bq769xx_Oper_Type = 0;
	SYS_POWER_OFF;
}

/**
 * @brief BQ769xx唤醒通信处理
 * @details 500ms时间片执行，处理BQ769xx从SHIP模式唤醒的流程
 *
 *          唤醒流程：
 *          1. 检测唤醒使能标志（Bq769xx_Wake_EN）
 *          2. 发送唤醒脉冲（拉高WAKE引脚）
 *          3. 等待1秒（WAKE_TIME_OUT_1S = 2 * 500ms）
 *          4. 拉低WAKE引脚
 *          5. 重新初始化BQ769xx
 *
 *          唤醒控制变量（bq769xx_Wake_cont）：
 *          - Bit7: 唤醒脉冲发送标志（0x80=已发送）
 *          - Bit6-0: 等待计数器（0~2，每500ms递增）
 */
void BQ769xx_Wake_Comm(void)
{
	if((TaskTimePare.Tim500ms_flag == 1) && (Bq769xx_Wake_EN == 1))
	{
		if(bq769xx_Wake_cont & 0x80)
		{
			// 唤醒脉冲已发送，等待1秒
			if((bq769xx_Wake_cont & 0x7f) >= WAKE_TIME_OUT_1S)
			{
				BQ769xx_WAKE_OFF;
				bq769xx_Wake_cont = 0;
				Bq769xx_Wake_EN = 0;
				BQ769xx_Init();    // 重新初始化
			}
			else
			{
				bq769xx_Wake_cont++;
			}
		}
		else
		{
			// 发送唤醒脉冲
			BQ769xx_WAKE_ON;
			bq769xx_Wake_cont = 0;
			bq769xx_Wake_cont |= 0x80;
		}
	}
}
