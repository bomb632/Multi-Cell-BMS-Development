/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    TaskFun.c
 * @brief   SLMC607 BMS系统模式状态机
 * @details 实现BMS系统模式的状态机控制
 *          - 系统模式状态机: SLEEP/STANDBY/CHARGE/DISCHARGE
 *          - 基于电流阈值(±20mA)进行模式切换
 */

#include "TaskFun.h"
#include "gpio.h"
#include "adc.h"
#include "timer.h"
#include "iic.h"
#include "led.h"
#include "DataBase.h"

/* ==================== 全局变量定义 ==================== */

/* ==================== 局部变量定义 ==================== */
#define SELL_IN_TIME_2H   36000         // 睡眠进入时间：200ms * 36000 = 2小时
static u32 slee_info_cnt=0;            // 静置时间计数器
u8 Charge_Activa=0;                    // 充电激活标志

/* ==================== 局部函数声明 ==================== */
static void Sys_Mode_Charge(void);
static u8 Sys_State_Check(void);

/**
 * @brief 系统功能处理函数
 * @details 调用系统模式状态机和系统状态检查
 */
void Sys_Function_Pro(void)
{
	Sys_Mode_Charge();    // 系统模式状态机处理
	Sys_State_Check();    // 系统状态检查（MOSFET控制）
}

/**
 * @brief 系统模式状态机 - 第一层状态机
 * @details 根据电流大小判断并切换系统工作模式
 *          状态转换图：
 *          SLEEP --(电流>20mA或<-20mA)--> STANDBY
 *          STANDBY --(电流<=-20mA)--> DISCHARGE (放电模式)
 *          STANDBY --(电流>=20mA)--> CHARGE (充电模式)
 *          STANDBY --(静置2小时)--> SLEEP (休眠模式)
 *          DISCHARGE/CHARGE --(-20mA<电流<20mA)--> STANDBY
 *
 * @note   200ms时间片执行
 * @note   电流阈值：充电≥20mA，放电≤-20mA，静置-20mA~20mA
 */
void Sys_Mode_Charge(void)
{
	/* 时间片检查：仅在200ms时间片执行 */
	if(TaskTimePare.Tim200ms_flag != 1)
	{
		return;
	}

	/* ========== 状态1: SLEEP (休眠模式) ========== */
	if(gBMSData.Sys_Mod.sys_mode == SYS_MODE_SLEEP)
	{
		// 检测到充电器或负载接入（电流绝对值>20mA），唤醒系统
		if((gBMSData.BattPar.CurrLine > 20) || (gBMSData.BattPar.CurrLine < -20))
		{
			gBMSData.Sys_Mod.sys_mode = SYS_MODE_STANDBY;
		}
		return;
	}

	/* ========== 状态2/3/4: STANDBY/CHARGE/DISCHARGE ========== */
	// 判断当前电流状态，决定系统模式
	if((gBMSData.BattPar.CurrLine < 20) && (gBMSData.BattPar.CurrLine > -20))
	{
		/* 静置状态：电流在-20mA~20mA之间 */
		gBMSData.Sys_Mod.sys_mode = SYS_MODE_STANDBY;

		// 静置时间计数，累计2小时后进入休眠
		if(slee_info_cnt++ > SELL_IN_TIME_2H)
		{
			slee_info_cnt = 0;
			gBMSData.Sys_Mod.sys_mode = SYS_MODE_SLEEP;
		}
	}
	else if(gBMSData.BattPar.CurrLine <= -20)
	{
		/* 放电状态：电流≤-20mA */
		gBMSData.Sys_Mod.sys_mode = SYS_MODE_DISCHARGE;
		slee_info_cnt = 0;    // 清空静置计数
	}
	else if(gBMSData.BattPar.CurrLine >= 20)
	{
		/* 充电状态：电流≥20mA */
		gBMSData.Sys_Mod.sys_mode = SYS_MODE_CHARGE;
		slee_info_cnt = 0;    // 清空静置计数
	}
}

/**
 * @brief 系统状态检查函数 - MOSFET控制
 * @details 系统启动延时后开启充电和放电MOSFET
 *          防止启动瞬间误动作
 *
 * @note   100ms时间片执行
 * @return gRET_OK - 状态检查正常
 * @return gRET_NG - 时间片未到或在等待延时中
 */
static u8 Sys_State_Check(void)
{
	u8 ret = gRET_OK;
	static u8 wait_time_cnt = 10;       // 启动延时计数器：10 * 100ms = 1秒
	static u8 chg_dsg_off_flag = 1;     // MOSFET初始关闭标志

	/* 时间片检查：仅在100ms时间片执行 */
	if(TaskTimePare.Tim100ms_flag != 1)
	{
		return gRET_NG;
	}

	/* 启动延时：等待1秒后再开启MOSFET */
	if(wait_time_cnt > 0)
	{
		wait_time_cnt--;
		return gRET_NG;
	}

	/* 首次开启充电和放电MOSFET */
	if(chg_dsg_off_flag == 1)
	{
		chg_dsg_off_flag = 0;
		BQ769xx_DSGSET(1);    // 开启放电MOSFET (DSG)
		BQ769xx_CHGSET(1);    // 开启充电MOSFET (CHG)
	}

	return ret;
}
