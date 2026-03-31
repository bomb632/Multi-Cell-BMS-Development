/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    ConfigPara.c
 * @brief   BMS配置参数管理 - Flash存储与加载
 * @details 提供参数初始化、Flash读写、CRC校验功能
 *          参数存储在Flash中，掉电不丢失
 */

#include "ConfigPara.h"
#include "Flash.h"
#include <string.h>

/* ==================== 全局变量定义 ==================== */
sBMSConfig   gBMSConfig;          // BMS配置参数（运行时使用）
sBMSConfig   gBMSReadConfig;      // 从Flash读取的配置参数

/* ==================== CRC校验函数 ==================== */

/**
 * @brief 计算数据的CRC校验和
 * @param  leng: 数据长度
 * @param  ucpStr: 数据指针
 * @return CRC校验和（简单累加和）
 * @note   使用累加和作为CRC，对数据完整性进行校验
 */
u8 DataCRC(u8 leng, u8 *ucpStr)
{
	u8 ucCRC = 0, ucN = 0;

	for(ucN = 0; ucN < leng; ucN++)
	{
		ucCRC += *ucpStr;
		ucpStr++;
	}
	return ucCRC;
}

/* ==================== 参数初始化函数 ==================== */

/**
 * @brief 初始化默认参数
 * @details 设置BMS的所有默认配置参数
 *          首次使用或Flash数据损坏时使用此函数恢复默认值
 *
 * 参数分类：
 * 1. Type:    电池硬件规格（串/并数、容量、分流电阻）
 * 2. Balan:   均衡策略（启动电压、压差阈值、均衡串数、均衡时间）
 * 3. BQ76Para: BQ769xx保护阈值（SCD/OCD/OV/UV延迟和阈值）
 */
void InitPara0(void)
{
	/* 设置数据长度（用于CRC计算） */
	gBMSConfig.DataLeng = sizeof(gBMSConfig);

	/* ========== 硬件规格参数 ========== */
	gBMSConfig.Type.CapaRate = 200;       // 额定容量：200mAh (2.00Ah)
	gBMSConfig.Type.VoltSpec = 18;        // 额定电压：18V (5串锂电池标称)
	gBMSConfig.Type.CellNum_Ser = 5;     // 串联数：5串 (支持3/4/5串配置)
	gBMSConfig.Type.CellNum_Par = 1;     // 并联数：1并
	gBMSConfig.Type.TempNum = 1;         // 温度点数：1个温度传感器
	gBMSConfig.Type.ShuntSpec = 5000;    // 分流电阻规格：5000mΩ (5mΩ)
	gBMSConfig.Type.OCVDaleyTime = 600;  // OCV延迟时间：600秒（静置10分钟后才记录OCV）

	/* ========== 均衡策略参数 ========== */
	gBMSConfig.Balan.balanc_start_volt = 3000;    // 均衡启动电压：3000mV (3.0V)
	gBMSConfig.Balan.balanc_diffe_volt = 100;     // 均衡压差阈值：100mV (0.1V)
	gBMSConfig.Balan.balanc_number_max = 3;       // 最多同时均衡串数：3串
	gBMSConfig.Balan.balanc_oneall_time = 900;    // 单次均衡时间：900 * 200ms = 180秒

	/* ========== BQ769xx保护阈值参数 ========== */
	// 短路保护 (SCD - Short Circuit Detection)
	gBMSConfig.BQ76Para.SCD_Delay = 0x03;         // SCD延迟：300us
	gBMSConfig.BQ76Para.SCD_Thresh = 0x07;        // SCD阈值：约200mV分流电压

	// 过流保护 (OCD - Over Current Detection)
	gBMSConfig.BQ76Para.OCD_Delay = 0x07;         // OCD延迟：约80ms
	gBMSConfig.BQ76Para.OCD_Thresh = 0x0F;        // OCD阈值：约100mV分流电压

	// 欠压/过压保护 (UV/OV - Under/Over Voltage)
	gBMSConfig.BQ76Para.UV_Delay = 0x03;          // UV延迟：约2s
	gBMSConfig.BQ76Para.OV_Delay = 0x03;          // OV延迟：约2s
	gBMSConfig.BQ76Para.UV_Thresh = 2500;         // UV阈值：2500mV (2.5V)
	gBMSConfig.BQ76Para.OV_Thresh = 4200;         // OV阈值：4200mV (4.2V)

	/* ========== CRC校验码 ========== */
	// CRC = 0 - 累加和，保证整个结构体的累加和为0
	gBMSConfig.DataCRC = 0 - DataCRC(gBMSConfig.DataLeng, (u8 *)&gBMSConfig);
}

/* ==================== Flash读写函数 ==================== */

/**
 * @brief 将所有配置参数写入Flash
 * @details 将gBMSConfig结构体写入Flash存储
 *          用于保存修改后的参数
 */
void WriteAllData(void)
{
	STMFLASH_Write(FLASH_SAVE_ADDR, gBMSConfig.DataLeng, (u16 *)&gBMSConfig);
}

/**
 * @brief 从Flash读取参数并校验
 * @details 从Flash读取配置参数，通过CRC校验验证数据完整性
 *          校验失败时使用默认参数
 *
 * 处理流程：
 * 1. 从Flash读取数据到 gBMSReadConfig
 * 2. 计算CRC校验
 * 3. 如果CRC校验失败：
 *    - 重新读取一次（防止读取错误）
 *    - 如果仍然失败，使用默认参数并写入Flash
 * 4. 如果CRC校验成功：
 *    - 将Flash数据复制到 gBMSConfig
 */
void System_Pare_Get(void)
{
	u16 ucA = 0;

	// 设置读取长度
	gBMSReadConfig.DataLeng = sizeof(gBMSReadConfig);

	// 从Flash读取数据
	STMFLASH_Read(FLASH_SAVE_ADDR, gBMSReadConfig.DataLeng, (u16 *)&gBMSReadConfig);

	// 计算CRC校验
	ucA = DataCRC(gBMSReadConfig.DataLeng, (u8 *)&gBMSReadConfig);

	if(ucA != 0)    // CRC校验失败
	{
		// 重新读取一次（可能是读取错误）
		STMFLASH_Read(FLASH_SAVE_ADDR, gBMSReadConfig.DataLeng, (u16 *)&gBMSReadConfig);
		ucA = DataCRC(gBMSReadConfig.DataLeng, (u8 *)&gBMSReadConfig);

		if(ucA != 0)    // 仍然失败，Flash数据损坏
		{
			// 使用默认参数并写入Flash
			InitPara0();
			WriteAllData();
		}
		else    // 重读成功
		{
			// 复制到运行时配置
			memcpy((u8 *)&gBMSConfig, (u8 *)&gBMSReadConfig, gBMSReadConfig.DataLeng);
		}
	}
	else    // CRC校验成功
	{
		// 复制到运行时配置
		memcpy((u8 *)&gBMSConfig, (u8 *)&gBMSReadConfig, gBMSReadConfig.DataLeng);
	}
}
