/**
 * @file    DataBase.h
 * @brief   SLMC607 BMS数据结构定义
 * @details 定义BMS系统使用的所有数据结构和常量
 *          包括电池参数、充电参数、状态参数、均衡参数、告警参数、错误参数、系统模式等
 */

#ifndef __DATABASE_H
#define __DATABASE_H
#include "sys.h"
#include "bq769xx.h"

/* ==================== 返回值定义 ==================== */
#define   gRET_OK     0          // 操作成功
#define   gRET_NG     1          // 操作失败
#define   gTIM_NG     2          // 时间片未到

/* ==================== 系统模式定义 ==================== */
#define SYS_MODE_SLEEP      0    // 休眠模式：低功耗，电流绝对值>20mA唤醒
#define SYS_MODE_STANDBY     1    // 待机模式：静置状态，-20mA~20mA
#define SYS_MODE_DISCHARGE   2    // 放电模式：电流≤-20mA
#define SYS_MODE_CHARGE      3    // 充电模式：电流≥20mA

/* ==================== BMS工作模式定义 ==================== */
#define BMS_MODE_INIT       1    // 初始化模式
#define BMS_MODE_VCT        2    // 电压采集模式
#define BMS_MODE_CHG_DSG     3    // 充放电模式
#define BMS_MODE_RUNING      4    // 运行模式
#define BMS_MODE_PROTECT     5    // 保护模式

/* ==================== 时间片标志定义 ==================== */
#define gTIMER_FLAG_ON     1     // 时间片标志：开启
#define gTIMER_FLAG_OFF    0     // 时间片标志：关闭

/* ==================== 数组最大值定义 ==================== */
#define  PACK_CELL_MAX       15      // 最大电池串数：15串
#define  PACK_TEMPEXT_MAX     3      // 最大外部温度传感器数：3个
#define  PACK_BALACELL_MAX    5      // 最大均衡电池数：5串
#define  BQ769xxREG_MAX      55      // BQ769xx寄存器最大数量

/* ==================== 数据结构定义 ==================== */

/**
 * @brief 电池参数结构体
 * @details 存储电池的实时测量数据
 */
typedef struct BattPar
{
	u16 VoltLine;              // 总电压 (mV)
	i16 CurrLine;              // 电流 (mA)，正值为充电，负值为放电
	u16 VoltCell[PACK_CELL_MAX];      // 各串电池电压 (mV)
	u16 VoltCellDiff;          // 电压差 (mV)
	u16 VoltCellMax;           // 最高电池电压 (mV)
	u16 VoltCellMin;           // 最低电池电压 (mV)
	u16 TempCell[PACK_TEMPEXT_MAX];   // 各温度传感器值 (℃*10，即23.5℃存储为235)
	u16 TempPackDiff;          // 温度差 (℃*10)
	i16 TempPackMax;           // 最高温度 (℃*10)
	i16 TempPackMin;           // 最低温度 (℃*10)
	u16 SOCSys;                // 系统SOC (0.1%)
	u16 SOCcell[PACK_CELL_MAX];      // 各串电池SOC (0.1%)
	u32 CapSoc[PACK_CELL_MAX];       // 各串电池容量 (mAh)
	u16 Cells_Map;             // 电池映射掩码（用于BQ769xx识别激活通道）
} sBattPar;

/**
 * @brief 充电参数结构体
 * @details 存储充电相关的电压电流参数
 */
typedef struct ChargePar
{
	u16 Volt;                  // 充电电压 (mV)
	u16 Curr;                  // 充电电流 (mA)
} sChargePar;

/**
 * @brief 状态参数结构体
 * @details 存储继电器/MOSFET状态
 */
typedef struct sStatePar
{
	u8 RelayDichSW;            // 放电继电器/MOSFET开关状态
	u8 RelayChSW;              // 充电继电器/MOSFET开关状态
} sStatePar;

/**
 * @brief 均衡参数结构体
 * @details 存储电池均衡控制相关参数
 */
typedef struct sbalaPar
{
	u16 bala_dischg;           // 均衡放电寄存器值（写入BQ769xx的值）
	u32 bala_ctrl_sw;          // 均衡控制开关位图（逻辑电池编号）
	u32 bala_ctrl_dr;          // 均衡控制驱动位图
	u8 Bala_force_on;          // 强制均衡标志
	u8 bala_state;             // 均衡状态：0=未均衡，1=均衡中
	u8 bala_error;             // 均衡错误标志
} sbalaPar;

/**
 * @brief 告警参数结构体
 * @details 存储各类告警状态
 */
typedef struct AlarmPar
{
	// BMS软件告警
	u8 OVP;                    // 过压告警 (Over Voltage Protection)
	u8 UVP;                    // 欠压告警 (Under Voltage Protection)
	u8 OCP;                    // 过流告警 (Over Current Protection)
	u8 UCP;                    // 欠流告警 (Under Current Protection)
	u8 OTP;                    // 过温告警 (Over Temperature Protection)
	u8 UTP;                    // 欠温告警 (Under Temperature Protection)
	u8 USOC;                   // SOC过低告警
	u8 OVLP;                   // 过压告警锁定
	u8 UVLP;                   // 欠压告警锁定

	// BQ769xx硬件告警
	u8 SCD_BQ;                 // BQ769xx短路告警
	u8 OCD_BQ;                 // BQ769xx过流告警
	u8 UV_BQ;                  // BQ769xx欠压告警
	u8 OV_BQ;                  // BQ769xx过压告警
} sAlarmPar;

/**
 * @brief 错误参数结构体
 * @details 存储系统错误状态
 */
typedef struct ErrorPar
{
	u8 I2C_Err;                // I2C通信错误
	u8 Uart_Err;               // UART通信错误
	u8 RelayCh_Err;            // 充电继电器/MOSFET错误
	u8 RelayDich_Err;          // 放电继电器/MOSFET错误
} sErrorPar;

/**
 * @brief 系统模式结构体
 * @details 存储系统工作模式
 */
typedef struct Sys_Mod
{
	u8 sys_mode;               // 系统模式：SLEEP/STANDBY/CHARGE/DISCHARGE
	u8 bms_mode;               // BMS工作模式：INIT/VCT/CHG_DSG/RUNING/PROTECT
	u8 test_mode;              // 测试模式
} sSys_Mod;

/**
 * @brief 软件版本结构体
 * @details 存储软件版本号
 */
typedef struct Softw_Ver
{
	u8 ver_main;               // 主版本号
	u8 ver_sub1;               // 子版本号1
	u8 ver_sub2;               // 子版本号2
	u8 ver_sub3;               // 子版本号3
} sSoftw_Ver;

/**
 * @brief GPIO状态结构体
 * @details 存储GPIO输入状态
 */
typedef struct Gpio_State
{
	u8 IN0_State;              // GPIO输入0状态
	u8 Charge_Activa_State;    // 充电激活状态
	u8 BQ769xx_Alert_State;    // BQ769xx告警引脚状态
} sGpio_State;

/**
 * @brief 电池容量参数结构体
 * @details 存储电池容量相关信息
 */
typedef struct PackCap
{
	u32 pack_rated_cap;        // 额定容量 (mAh) - 设计容量
	u32 pack_real_cap;         // 实际容量 (mAh) - 不同温度下的实际可用容量不同
	u32 pack_rem_cap;          // 剩余容量 (mAh)
	u32 pack_chg_max_cap;      // 最大充电容量 (mAh)
	u32 pack_dsg_max_cap;      // 最大放电容量 (mAh)
	u32 pack_cap_dischg;       // 放电累计容量 (mAh)
	u32 pack_cap_chg;          // 充电累计容量 (mAh)
} sPackCap;

/**
 * @brief BMS数据总结构体
 * @details 包含所有BMS相关数据的总结构
 */
typedef struct BMSData
{
	sBattPar      BattPar;     // 电池参数
	sChargePar    ChargePar;   // 充电参数
	sStatePar     StatePar;    // 状态参数 MOSFET状态
	sbalaPar      BalaPar;     // 均衡参数
	sPackCap      PackCap;     // 容量参数
	sAlarmPar     AlarmPar;    // 告警参数
	sErrorPar     ErrorPar;    // 错误参数
	sSys_Mod      Sys_Mod;     // 系统模式
	sSoftw_Ver    Softw_Ver;   // 软件版本
	sGpio_State   Gpio_State;  // GPIO状态
} sBMSData;

/* 全局变量声明 */
extern sBMSData gBMSData;

#endif  // __DATABASE_H
