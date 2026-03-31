/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    ConfigPara.h
 * @brief   BMS配置参数结构定义
 * @details 定义BMS系统的所有可配置参数结构体
 *          包括硬件规格、均衡策略、BQ769xx保护阈值等
 *
 *          参数存储：Flash存储，掉电不丢失
 *          参数校验：CRC校验
 */

#ifndef __CONFIGPARA_H
#define __CONFIGPARA_H
#include "sys.h"

#define EP_HEADER_ADDR   0x4000      // Flash起始地址

/* ==================== BQ769xx保护阈值配置说明 ==================== */
/*
 * 短路保护 (SCD - Short Circuit Detection)
 * gBMSConfig.BQ76Para.SCD_Delay:
 *   0x00 → 70us
 *   0x01 → 100us
 *   0x02 → 200us
 *   0x03 → 300us
 *
 * gBMSConfig.BQ76Para.SCD_Thresh:
 *   RSNS=1 (高量程):    RSNS=0 (低量程):
 *   0x00 → 44mV          0x00 → 22mV
 *   0x01 → 67mV          0x01 → 33mV
 *   0x02 → 89mV          0x02 → 44mV
 *   0x03 → 111mV         0x03 → 56mV
 *   0x04 → 133mV         0x04 → 67mV
 *   0x05 → 155mV         0x05 → 78mV
 *   0x06 → 178mV         0x06 → 89mV
 *   0x07 → 200mV         0x07 → 100mV
 *
 * 过流保护 (OCD - Over Current Detection)
 * gBMSConfig.BQ76Para.OCD_Delay:
 *   0x00 → 8ms
 *   0x01 → 20ms
 *   0x02 → 40ms
 *   0x03 → 80ms
 *   0x04 → 160ms
 *   0x05 → 320ms
 *   0x06 → 640ms
 *   0x07 → 1280ms
 *
 * gBMSConfig.BQ76Para.OCD_Thresh:
 *   RSNS=1 (高量程):    RSNS=0 (低量程):
 *   0x00 → 17mV          0x00 → 8mV
 *   0x01 → 22mV          0x01 → 11mV
 *   0x02 → 28mV          0x02 → 14mV
 *   0x03 → 33mV          0x03 → 17mV
 *   0x04 → 39mV          0x04 → 19mV
 *   0x05 → 44mV          0x05 → 22mV
 *   0x06 → 50mV          0x06 → 25mV
 *   0x07 → 56mV          0x07 → 28mV
 *   0x08 → 61mV          0x00 → 31mV
 *   0x09 → 67mV          0x01 → 33mV
 *   0x0A → 72mV          0x02 → 36mV
 *   0x0B → 78mV          0x03 → 39mV
 *   0x0C → 83mV          0x04 → 42mV
 *   0x0D → 89mV          0x05 → 44mV
 *   0x0E → 94mV          0x06 → 47mV
 *   0x0F → 100mV         0x07 → 50mV
 *
 * 过压/欠压保护 (OV/UV - Over/Under Voltage)
 * gBMSConfig.BQ76Para.UV_Delay / OV_Delay:
 *   0x00 → 1s
 *   0x01 → 4s
 *   0x02 → 8s
 *   0x03 → 16s
 *
 * gBMSConfig.BQ76Para.UV_Thresh: 欠压阈值 (mV)
 * gBMSConfig.BQ76Para.OV_Thresh: 过压阈值 (mV)
 */

/* ==================== 硬件规格参数结构体 ==================== */
/**
 * @brief 电池硬件规格参数
 * @details 定义电池组的硬件规格参数
 */
typedef struct System_Type
{
	u64 CapaRate;        // 额定容量 (mAh)
	u8 VoltSpec;         // 额定电压
	u8 CellNum_Ser;      // 串联数 (3/4/5可选)
	u8 CellNum_Par;      // 并联数
	u8 TempNum;          // 温度传感器数量
	u16 ShuntSpec;       // 分流电阻规格 (mΩ)
	u16 OCVDaleyTime;    // OCV延迟时间 (秒，静置后才能校准OCV)
} sSystem_Type;

/* ==================== 均衡策略参数结构体 ==================== */
/**
 * @brief 均衡策略参数
 * @details 定义电池均衡控制策略参数
 */
typedef struct System_Balan
{
	u16 balanc_start_volt;     // 均衡启动电压 (mV)
	u16 balanc_diffe_volt;     // 均衡压差阈值 (mV)
	u16 balanc_number_max;     // 最多同时均衡串数
	u16 balanc_oneall_time;    // 单次均衡时间 (200ms计数单位)
} sSystem_Balan;

/* ==================== BQ769xx保护参数结构体 ==================== */
/**
 * @brief BQ769xx保护参数
 * @details 定义BQ769xx AFE芯片的保护阈值参数
 */
typedef struct System_Alar_BQ76Para
{
	u8 SCD_Delay;        // 短路保护延迟
	u8 SCD_Thresh;       // 短路保护阈值
	u8 OCD_Delay;        // 过流保护延迟
	u8 OCD_Thresh;       // 过流保护阈值
	u8 UV_Delay;         // 欠压保护延迟
	u8 OV_Delay;         // 过压保护延迟
	u16 UV_Thresh;        // 欠压保护阈值 (mV)
	u16 OV_Thresh;        // 过压保护阈值 (mV)
} sSystem_BQ76Para;

/* ==================== BMS配置参数总结构体 ==================== */
/**
 * @brief BMS配置参数总结构
 * @details 包含所有BMS可配置参数的结构体
 *          存储在Flash中，掉电不丢失
 */
typedef struct BMSConfig
{
	u8 DataLeng;                 // 数据长度（用于CRC计算）
	sSystem_Type Type;          // 硬件规格参数
	sSystem_Balan Balan;        // 均衡策略参数
	sSystem_BQ76Para BQ76Para;  // BQ769xx保护参数
	u8 DataCRC;                  // CRC校验码
} sBMSConfig;

/* ==================== 全局变量声明 ==================== */
extern sBMSConfig gBMSConfig;    // BMS配置参数（运行时使用）

/* ==================== 函数声明 ==================== */
void InitPara0(void);           // 初始化默认参数
void WriteAllData(void);        // 将参数写入Flash
void System_Pare_Get(void);     // 从Flash读取参数

#endif  // __CONFIGPARA_H
