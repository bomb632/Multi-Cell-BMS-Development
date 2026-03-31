/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    bq769xx.h
 * @brief   BQ76920/BQ76940 AFE芯片寄存器定义和函数声明
 * @details 定义BQ769xx系列AFE芯片的寄存器地址、位定义和函数原型
 *
 *          BQ76920: 3-5串电池模拟前端 (AFE)
 *          BQ76940: 3-10串电池模拟前端 (AFE)
 *          通信接口: I2C
 *          地址: 0x08 (7位地址)
 */

#ifndef __BQ769XX_H
#define __BQ769XX_H
#include "sys.h"

/* ==================== I2C地址和寄存器地址定义 ==================== */
/**
 * @brief BQ769xx I2C 7位地址
 * @note BQ769xx使用标准I2C通信，CRC8校验（多项式0x07）
 */
#define BQ769xxAddr        0x08        // BQ769xx I2C 7位地址

// 寄存器地址定义
#define SYS_STAT_RegAddr   0x00        // 系统状态寄存器（告警标志）
#define CELLBAL1_RegAddr   0x01        // 均衡控制寄存器1 (Cell 1-5)
#define CELLBAL2_RegAddr   0x02        // 均衡控制寄存器2 (Cell 6-10)
#define CELLBAL3_RegAddr   0x03        // 均衡控制寄存器3 (Cell 11-15)
#define SYS_CTRL1_RegAddr  0x04        // 系统控制寄存器1（ADC使能、温度选择）
#define SYS_CTRL2_RegAddr  0x05        // 系统控制寄存器2（MOS控制、库仑计）
#define PROTECT1_RegAddr   0x06        // 保护寄存器1 (短路保护SCD)
#define PROTECT2_RegAddr   0x07        // 保护寄存器2 (过流保护OCD)
#define PROTECT3_RegAddr   0x08        // 保护寄存器3 (过压/欠压延迟)
#define OV_TRIP_RegAddr    0x09        // 过压阈值寄存器
#define UV_TRIP_RegAddr    0x0A        // 欠压阈值寄存器
#define VC1_HI_RegAddr     0x0C        // 电池电压寄存器起始地址
#define ADCGAIN1_RegAddr   0x50        // ADC增益寄存器1（高2位）
#define ADCOFFSET_RegAddr  0x51        // ADC偏移寄存器
#define ADCGAIN2_RegAddr   0x59        // ADC增益寄存器2（低3位）

/* ==================== 系统控制位定义 ==================== */
/**
 * @brief ADC使能控制
 * @details 控制BQ769xx内部ADC的开关状态
 */
#define ADC_ENS            1           // 使能ADC
#define ADC_DIS            0           // 禁用ADC

/**
 * @brief 温度传感器选择
 * @details 选择使用内部还是外部温度传感器
 */
#define EXTE_TEMP_ON       1           // 使用外部温度传感器（NTC）
#define EXTE_TEMP_OFF      0           // 使用内部温度传感器

/**
 * @brief 告警延迟控制
 * @details 控制保护功能的延迟检测
 */
#define ALARM_DELAY_ON     0           // 使能告警延迟（需要去抖）
#define ALARM_DELAY_OFF    1           // 禁用告警延迟（立即响应）

/**
 * @brief 库仑计模式控制
 * @details 选择库仑计工作模式
 */
#define CC_CONTINU_DIS     0           // 禁用连续库仑计（单次采样）
#define CC_CONTINU_EN      1           // 使能连续库仑计（持续积分）

/**
 * @brief MOSFET控制
 * @details 控制充电和放电MOSFET的开关状态
 */
#define CHG_O0N            1           // 开启充电MOSFET（CHG置1）
#define CHG_OFF            0           // 关闭充电MOSFET（CHG清0）
#define DSG_O0N            1           // 开开放电MOSFET（DSG置1）
#define DSG_OFF            0           // 关闭放电MOSFET（DSG清0）

/* ==================== 保护配置位定义 ==================== */
#define OCD_SCD_LOWER_RANGE 0           // 低量程电流检测
#define OCD_SCD_HIGH_RANGE  1           // 高量程电流检测

/* ==================== 阈值计算常量 ==================== */
/**
 * @brief 过压/欠压阈值计算基准值
 * @details 阈值寄存器值计算公式：
 *          Trip寄存器值 = ((目标电压_mV - ADC偏移) * 1000 / 增益_uV - 基准值) >> 4
 *
 *          示例：目标过压4200mV，偏移0mV，增益370000uV
 *          OVTrip = ((4200 - 0) * 1000 / 370000 - 0x2008) >> 4
 */
#define OV_THRESH_BASE      0x2008      // 过压阈值基准值
#define UV_THRESH_BASE      0x1000      // 欠压阈值基准值
#define OV_THRESH_MAX       4700        // 过压阈值最大值 (mV)
#define OV_THRESH_MIN       3150        // 过压阈值最小值 (mV)
#define UV_THRESH_MAX       3100        // 欠压阈值最大值 (mV)
#define UV_THRESH_MIN       1580        // 欠压阈值最小值 (mV)

/* ==================== ADC LSB值定义 ==================== */
#define BATVOLTLSB          1532        // 总电压LSB (uV)
#define TMEPVOLTLSB         382         // 温度电压LSB (uV)
#define CRUUVOLTLSB         844         // 电流电压LSB (uV)

/* ==================== 告警清除掩码定义 ==================== */
/**
 * @brief 告警清除掩码
 * @details SYS_STAT寄存器写1清除对应告警标志
 *          可通过按位或运算组合多个清除操作
 *
 *          使用示例：
 *          BQ76940_STAT_CLEAR(OV_CLE | UV_CLE);    // 清除过压+欠压
 *          BQ76940_STAT_CLEAR(0x0F);               // 清除所有告警
 */
#define OCD_CLE             0x01        // 清除过流告警（Bit0）
#define SCD_CLE             0x02        // 清除短路告警（Bit1）
#define OV_CLE              0x04        // 清除过压告警（Bit2）
#define UV_CLE              0x08        // 清除欠压告警（Bit3）
#define OVRD_ALERT_CLE      0x10        // 清除覆盖告警（Bit4）
#define DEVICE_XREADY_CLE   0x20        // 清除设备就绪告警（Bit5）
#define CC_READY_CLE        0x80        // 清除库仑计就绪告警（Bit7）

/* ==================== 系统常量定义 ==================== */
#define SYS_CELL_MAX        15          // 最大电池串数
#define SYS_TEMPEXT_MAX     3           // 最大外部温度传感器数
#define SYS_BALACELL_MAX    5           // 最大均衡电池数
#define WAKE_TIME_OUT_1S    2           // 唤醒超时计数 (500ms*2=1s)

/* ==================== 库仑计偏移值定义 ==================== */
#define CC_OffEet_ValueN    47          // 负偏移值
#define CC_OffEet_ValueP    60          // 正偏移值

/* ==================== 全局变量声明 ==================== */
extern u8  VoltCellOffSet;             // ADC偏移值
extern u32 VoltCellGainUV;             // ADC增益 (uV)
extern u16 VoltCellGainMV;             // ADC增益 (mV)
extern u8  BQ769xx_Init_State;         // 初始化状态
extern u8  Bq769xx_Wake_EN;             // 唤醒使能

/* ==================== 均衡状态机枚举定义 ==================== */
/**
 * @brief 均衡控制状态机状态枚举
 */
typedef enum _ePCB_SEQ
{
	PCB_SEQ_INIT = 0,    // 初始化状态
	PCB_SEQ_WAIT,        // 等待状态
	PCB_SEQ_CHK,         // 检查状态
	PCB_SEQ_SELECT,      // 选择状态
	PCB_SEQ_MAP,         // 映射状态
	PCB_SEQ_SET,         // 设置状态
	PCB_SEQ_BALA_TIM,    // 均衡计时状态
	PCB_SEQ_MAX          // 最大状态
} ePCB_SEQ;

/* ==================== BQ769xx寄存器组结构体定义 ==================== */
/**
 * @brief BQ769xx寄存器组联合体结构体
 * @details 包含所有BQ769xx寄存器的位定义和字节访问
 */
typedef struct _Register_Group
{
	/* ========== 系统状态寄存器 ========== */
	union
	{
		struct
		{
			u8 OCD          :1;      // 过流检测标志
			u8 SCD          :1;      // 短路检测标志
			u8 OV           :1;      // 过压检测标志
			u8 UV           :1;      // 欠压检测标志
			u8 OVRD_ALERT   :1;      // 覆盖告警标志
			u8 DEVICE_XREADY:1;      // 设备就绪标志
			u8 WAKE         :1;      // 唤醒标志
			u8 CC_READY     :1;      // 库仑计就绪标志
		} StatusBit;
		u8 StatusByte;
	} SysStatus;

	/* ========== 均衡控制寄存器1-3 ========== */
	union
	{
		struct
		{
			u8 RSVD         :3;      // 保留位
			u8 CB5          :1;      // Cell 5 均衡控制
			u8 CB4          :1;      // Cell 4 均衡控制
			u8 CB3          :1;      // Cell 3 均衡控制
			u8 CB2          :1;      // Cell 2 均衡控制
			u8 CB1          :1;      // Cell 1 均衡控制
		} CellBal1Bit;
		u8 CellBal1Byte;
	} CellBal1;

	union
	{
		struct
		{
			u8 RSVD         :3;      // 保留位
			u8 CB10         :1;      // Cell 10 均衡控制
			u8 CB9          :1;      // Cell 9 均衡控制
			u8 CB8          :1;      // Cell 8 均衡控制
			u8 CB7          :1;      // Cell 7 均衡控制
			u8 CB6          :1;      // Cell 6 均衡控制
		} CellBal2Bit;
		u8 CellBal2Byte;
	} CellBal2;

	union
	{
		struct
		{
			u8 RSVD         :3;      // 保留位
			u8 CB15         :1;      // Cell 15 均衡控制
			u8 CB14         :1;      // Cell 14 均衡控制
			u8 CB13         :1;      // Cell 13 均衡控制
			u8 CB12         :1;      // Cell 12 均衡控制
			u8 CB11         :1;      // Cell 11 均衡控制
		} CellBal3Bit;
		u8 CellBal3Byte;
	} CellBal3;

	/* ========== 系统控制寄存器1 ========== */
	union
	{
		struct
		{
			u8 SHUT_B       :1;      // SHUTDOWN_B
			u8 SHUT_A       :1;      // SHUTDOWN_A
			u8 RSVD1        :1;      // 保留位
			u8 TEMP_SEL     :1;      // 温度传感器选择
			u8 ADC_EN       :1;      // ADC使能
			u8 RSVD2        :2;      // 保留位
			u8 LOAD_PRESENT :1;      // 负载检测
		} SysCtrl1Bit;
		u8 SysCtrl1Byte;
	} SysCtrl1;

	/* ========== 系统控制寄存器2 ========== */
	union
	{
		struct
		{
			u8 CHG_ON       :1;      // 充电MOSFET控制
			u8 DSG_ON       :1;      // 放电MOSFET控制
			u8 WAKE_T       :2;      // 唤醒时间
			u8 WAKE_EN      :1;      // 唤醒使能
			u8 CC_ONESHOT   :1;      // 库仑计单次采样
			u8 CC_EN        :1;      // 库仑计使能
			u8 DELAY_DIS    :1;      // 延迟禁用
		} SysCtrl2Bit;
		u8 SysCtrl2Byte;
	} SysCtrl2;

	/* ========== 保护寄存器1 (短路保护) ========== */
	union
	{
		struct
		{
			u8 SCD_THRESH   :3;      // 短路阈值
			u8 SCD_DELAY    :2;      // 短路延迟
			u8 RSVD         :2;      // 保留位
			u8 RSNS         :1;      // 电流检测量程
		} Protect1Bit;
		u8 Protect1Byte;
	} Protect1;

	/* ========== 保护寄存器2 (过流保护) ========== */
	union
	{
		struct
		{
			u8 OCD_THRESH   :4;      // 过流阈值
			u8 OCD_DELAY    :3;      // 过流延迟
			u8 RSVD         :1;      // 保留位
		} Protect2Bit;
		u8 Protect2Byte;
	} Protect2;

	/* ========== 保护寄存器3 (过压/欠压保护) ========== */
	union
	{
		struct
		{
			u8 RSVD         :4;      // 保留位
			u8 OV_DELAY     :2;      // 过压延迟
			u8 UV_DELAY     :2;      // 欠压延迟
		} Protect3Bit;
		u8 Protect3Byte;
	} Protect3;

	/* ========== 过压/欠压阈值寄存器 ========== */
	u8 OVTrip;                          // 过压阈值
	u8 UVTrip;                          // 欠压阈值
	u8 CCCfg;                           // 库仑计配置 (必须为0x19)

	/* ========== 电池电压寄存器 (VC1-VC15) ========== */
	union
	{
		struct
		{
			u8 VC1_HI;
			u8 VC1_LO;
		} VCell1Byte;
		unsigned short VCell1Word;
	} VCell1;

	union
	{
		struct
		{
			u8 VC2_HI;
			u8 VC2_LO;
		} VCell2Byte;
		unsigned short VCell2Word;
	} VCell2;

	union
	{
		struct
		{
			u8 VC3_HI;
			u8 VC3_LO;
		} VCell3Byte;
		unsigned short VCell3Word;
	} VCell3;

	union
	{
		struct
		{
			u8 VC4_HI;
			u8 VC4_LO;
		} VCell4Byte;
		unsigned short VCell4Word;
	} VCell4;

	union
	{
		struct
		{
			u8 VC5_HI;
			u8 VC5_LO;
		} VCell5Byte;
		unsigned short VCell5Word;
	} VCell5;

	union
	{
		struct
		{
			u8 VC6_HI;
			u8 VC6_LO;
		} VCell6Byte;
		unsigned short VCell6Word;
	} VCell6;

	union
	{
		struct
		{
			u8 VC7_HI;
			u8 VC7_LO;
		} VCell7Byte;
		unsigned short VCell7Word;
	} VCell7;

	union
	{
		struct
		{
			u8 VC8_HI;
			u8 VC8_LO;
		} VCell8Byte;
		unsigned short VCell8Word;
	} VCell8;

	union
	{
		struct
		{
			u8 VC9_HI;
			u8 VC9_LO;
		} VCell9Byte;
		unsigned short VCell9Word;
	} VCell9;

	union
	{
		struct
		{
			u8 VC10_HI;
			u8 VC10_LO;
		} VCell10Byte;
		unsigned short VCell10Word;
	} VCell10;

	union
	{
		struct
		{
			u8 VC11_HI;
			u8 VC11_LO;
		} VCell11Byte;
		unsigned short VCell11Word;
	} VCell11;

	union
	{
		struct
		{
			u8 VC12_HI;
			u8 VC12_LO;
		} VCell12Byte;
		unsigned short VCell12Word;
	} VCell12;

	union
	{
		struct
		{
			u8 VC13_HI;
			u8 VC13_LO;
		} VCell13Byte;
		unsigned short VCell13Word;
	} VCell13;

	union
	{
		struct
		{
			u8 VC14_HI;
			u8 VC14_LO;
		} VCell14Byte;
		unsigned short VCell14Word;
	} VCell14;

	union
	{
		struct
		{
			u8 VC15_HI;
			u8 VC15_LO;
		} VCell15Byte;
		unsigned short VCell15Word;
	} VCell15;

	/* ========== 总电压寄存器 ========== */
	union
	{
		struct
		{
			u8 BAT_HI;
			u8 BAT_LO;
		} VBatByte;
		unsigned short VBatWord;
	} VBat;

	/* ========== 温度传感器寄存器 (TS1-TS3) ========== */
	union
	{
		struct
		{
			u8 TS1_HI;
			u8 TS1_LO;
		} TS1Byte;
		unsigned short TS1Word;
	} TS1;

	union
	{
		struct
		{
			u8 TS2_HI;
			u8 TS2_LO;
		} TS2Byte;
		unsigned short TS2Word;
	} TS2;

	union
	{
		struct
		{
			u8 TS3_HI;
			u8 TS3_LO;
		} TS3Byte;
		unsigned short TS3Word;
	} TS3;

	/* ========== 库仑计寄存器 ========== */
	union
	{
		struct
		{
			u8 CC_HI;
			u8 CC_LO;
		} CCByte;
		unsigned short CCWord;
	} CC;

	/* ========== ADC增益和偏移寄存器 ========== */
	union
	{
		struct
		{
			u8 RSVD         :2;      // 保留位
			u8 ADCGAIN_4_3  :2;      // ADC增益位4-3
			u8 RSVD2        :4;      // 保留位
		} ADCGain1Bit;
		u8 ADCGain1Byte;
	} ADCGain1;

	u8 ADCOffset;                       // ADC偏移

	union
	{
		struct
		{
			u8 RSVD         :5;      // 保留位
			u8 ADCGAIN_2_0  :3;      // ADC增益位2-0
		} ADCGain2Bit;
		u8 ADCGain2Byte;
	} ADCGain2;

} RegisterGroup;

/* 全局寄存器组声明 */
extern RegisterGroup Bq769xxReg;

/* ==================== 函数声明 ==================== */
/**
 * @defgroup BQ769xx_Public_Functions BQ769xx驱动公共接口
 * @brief BQ769xx AFE芯片驱动函数接口
 * @details 包含初始化、数据采集、MOS控制、均衡控制等功能
 */

/** @{ */

/**
 * @brief 唤醒BQ769xx芯片
 * @details 拉高WAKE引脚1秒，使芯片从SHIP模式唤醒
 * @note   唤醒后需要重新初始化芯片
 */
void BQ769xx_Wake(void);

/**
 * @brief 初始化BQ769xx芯片
 * @details 完整初始化流程：
 *          1. 设置电池串数映射
 *          2. 初始化参数结构体（保护阈值、MOS控制等）
 *          3. 读取ADC增益和偏移
 *          4. 写入配置到BQ769xx
 * @note   芯片上电或从SHIP模式唤醒后必须调用此函数
 */
void BQ769xx_Init(void);

/**
 * @brief 获取BQ769xx ADC采样数据
 * @details 200ms时间片执行，读取40字节ADC数据：
 *          - 电池电压（VC1-VC15）
 *          - 总电压（BAT）
 *          - 温度传感器（TS1-TS3）
 *          - 库仑计/电流（CC）
 * @note   此函数会自动进行ADC原始值到工程量的转换
 */
void BQ769xx_GetData(void);

/**
 * @brief 获取BQ769xx配置和状态寄存器
 * @details 500ms时间片执行，读取：
 *          - 系统状态寄存器（SYS_STAT）
 *          - 均衡控制寄存器（CELLBAL1-3）
 *          - 系统控制寄存器（SYS_CTRL1-2）
 */
void BQ769xx_GetConfig(void);

/**
 * @brief 控制放电MOSFET
 * @param  ONOFF: 1=开启, 0=关闭
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details 通过修改SYS_CTRL2寄存器的DSG位控制放电MOSFET
 */
u8 BQ769xx_DSGSET(u8 ONOFF);

/**
 * @brief 控制充电MOSFET
 * @param  ONOFF: 1=开启, 0=关闭
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details 通过修改SYS_CTRL2寄存器的CHG位控制充电MOSFET
 */
u8 BQ769xx_CHGSET(u8 ONOFF);

/**
 * @brief 获取BQ769xx系统状态
 * @return 系统状态寄存器值（SYS_STAT）
 * @details 返回值各位含义：
 *          - Bit0: OCD  - 过流检测标志
 *          - Bit1: SCD  - 短路检测标志
 *          - Bit2: OV   - 过压检测标志
 *          - Bit3: UV   - 欠压检测标志
 *          - Bit4: OVRD_ALERT   - 覆盖告警标志
 *          - Bit5: DEVICE_XREADY - 设备就绪标志
 *          - Bit6: WAKE - 唤醒标志
 *          - Bit7: CC_READY - 库仑计就绪标志
 */
u8 BQ769xx_STATE_GET(void);

/**
 * @brief 设置库仑计单次采样模式
 * @param  ONOFF: 1=使能单次采样, 0=禁用
 * @return gRET_OK - 成功, gRET_NG - 失败
 */
u8 BQ769xx_CC_ONESHOT_SET(u8 ONOFF);

/**
 * @brief 限制数值在最大最小值范围内
 * @param  InValue: 输入值
 * @param  gMax: 最大值
 * @param  gMin: 最小值
 * @return 限制后的值
 * @details 用于OV/UV阈值的范围限制
 */
u16 LimitMaxMin(u16 InValue, u16 gMax, u16 gMin);

/**
 * @brief 清除BQ769xx告警状态
 * @param  statvalue: 要清除的状态位掩码
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details SYS_STAT寄存器写1清除对应位：
 *          - OCD_CLE(0x01)  - 清除过流
 *          - SCD_CLE(0x02)  - 清除短路
 *          - OV_CLE(0x04)   - 清除过压
 *          - UV_CLE(0x08)   - 清除欠压
 */
u8 BQ76940_STAT_CLEAR(u8 statvalue);

/**
 * @brief 进入SHIP模式（低功耗关机模式）
 * @return gRET_OK - 成功, gRET_NG - 失败
 * @details SHIP模式是BQ769xx的超低功耗模式
 *          唤醒方式：拉高WAKE引脚至少1ms
 */
u8 Enter_Ship_Mode(void);

/**
 * @brief 均衡控制状态机 - 第二层状态机
 * @details 200ms时间片执行，自动均衡电压最高的N串电池
 *          状态流程：INIT -> WAIT -> CHK -> SELECT -> MAP -> SET -> BALA_TIM
 * @return gRET_OK - 正常, gRET_NG - 强制均衡中, gRET_TM - 时间片未到
 */
u8 Balan_Pack_Check(void);

/**
 * @brief BQ769xx通信操作状态机 - 第三层状态机
 * @details 100ms时间片执行，处理MOS开关、告警清除、关机等操作
 */
void BQ769xx_Oper_Comm(void);

/**
 * @brief 系统关机函数
 * @details 关机流程：
 *          1. 进入SHIP模式（关闭BQ769xx）
 *          2. 清除操作标志
 *          3. 关闭系统电源
 */
void Power_Down(void);

/**
 * @brief BQ769xx唤醒通信处理
 * @details 500ms时间片执行，处理BQ769xx从SHIP模式唤醒的流程
 */
void BQ769xx_Wake_Comm(void);

/** @} */ /* End of group BQ769xx_Public_Functions */

#endif  // __BQ769XX_H
