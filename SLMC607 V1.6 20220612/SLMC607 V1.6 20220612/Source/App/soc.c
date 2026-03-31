/**
 * @file    soc.c
 * @brief   SOC (State of Charge) 荷电状态估算算法
 * @details 采用复合算法估算SOC：
 *          1. OCV查表法：开路电压法，用于初始SOC和静置校准
 *          2. 安时积分法：库仑计，用于动态SOC计算
 *          3. 温度补偿：根据温度调整实际可用容量
 *
 *          SOC单位：0.1% (0-1000)
 *          容量单位：10mA/200ms (累计值)
 */

#include "soc.h"
#include "stm32f10x.h"
#include "timer.h"
#include "DataBase.h"
#include "ConfigPara.h"
#include <stdlib.h>

/* ==================== 常量定义 ==================== */
#define TEMP_CAP_RATE_LIMITH_HIGH   1050    // 温度补偿上限：105%
#define TEMP_CAP_RATE_LIMITL_LOW    750     // 温度补偿下限：75%
#define SOC_LIMIT_MAX    1000              // SOC最大值：100.0%
#define CNT_H_200MS_NUM    18000           // 5小时的200ms计数 (60*60*5*5=18000)
#define STANBY_OCV_TIME   6000             // 20分钟静置时间 (20*60*5=6000)

/* ==================== 全局变量定义 ==================== */
static u8 soc_ovp_flag = TRUE;            // 过压充电满电标志
static u8 soc_uvp_flag = TRUE;            // 欠压放空标志
static u16 stanby_ocv_cnt = 0;            // 静置计数器

/* ==================== 局部函数声明 ==================== */
static void batt_temp_full_cap(void);              // 温度补偿计算
static u16 rate_limit_max_min(u16 rate_value);     // 温度比例限幅
static void batt_ocvsoc_calcua_soc(void);          // OCV法计算SOC
static u16 Seeka_OcvSoc_Table(u16 SingleVolt, const u16 *OcvTable);  // OCV查表
static void batt_AHinte_calcua_soc(void);          // 安时积分法计算SOC

/* ==================== OCV-SOC对照表 ==================== */
/**
 * @brief OCV-SOC对照表 (100个点)
 * @details 锂电池开路电压与SOC的关系表
 *          电压单位：mV
 *          SOC范围：100% ~ 1%
 */
const u16 OCV_Table[100] =
{
	4194,4171,4155,4142,4132,4124,4117,4111,4106,4100,  // 100%~91%
	4095,4089,4082,4074,4066,4056,4047,4036,4027,4016,  // 90%~81%
	4005,3995,3984,3974,3964,3954,3945,3936,3926,3918,  // 80%~71%
	3909,3900,3892,3883,3869,3856,3846,3836,3826,3815,  // 70%~61%
	3805,3793,3782,3770,3758,3746,3734,3723,3712,3702,  // 60%~51%
	3692,3683,3675,3667,3660,3654,3648,3643,3638,3633,  // 50%~41%
	3628,3624,3619,3615,3611,3607,3603,3597,3591,3586,  // 40%~31%
	3582,3577,3573,3568,3563,3557,3552,3546,3540,3533,  // 30%~21%
	3526,3518,3510,3501,3492,3482,3473,3461,3449,3436,  // 20%~11%
	3423,3409,3397,3387,3380,3371,3363,3353,3340,3292   // 10%~1%
};

/* ==================== SOC初始化 ==================== */

/**
 * @brief SOC算法初始化
 * @details 初始化SOC相关参数：
 *          - 初始SOC设为100%
 *          - 计算额定容量（单位：10mA/200ms）
 *          - 计算初始剩余容量
 */
void batt_cap_ocvsoc_init(void)
{
	// 初始SOC设为100%
	gBMSData.BattPar.SOCSys = 1000;

	// 计算额定容量 = 额定容量(mAh) * 并联数 * 5小时
	// 单位：10mA/200ms
	// 例：2000mAh * 1 * 18000 = 36000000 (10mA/200ms)
	gBMSData.PackCap.pack_rated_cap = gBMSConfig.Type.CapaRate * gBMSConfig.Type.CellNum_Par * CNT_H_200MS_NUM;

	// 初始实际容量 = 额定容量（后续会根据温度补偿调整）
	gBMSData.PackCap.pack_real_cap = gBMSData.PackCap.pack_rated_cap;

	// 初始剩余容量 = 实际容量 * SOC
	gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap * gBMSData.BattPar.SOCSys / 1000;
}

/* ==================== SOC主处理函数 ==================== */

/**
 * @brief SOC检查与计算（200ms调用）
 * @details 综合三种方法计算SOC：
 *          1. 温度补偿：调整实际可用容量
 *          2. OCV查表法：静置时校准SOC
 *          3. 安时积分法：动态更新SOC
 *
 * @return gRET_OK - 计算完成, gRET_NG - 时间片未到
 */
u8 Batt_soc_check(void)
{
	u8 ret = gRET_OK;
	static u8 wait_tim_cnt = 5;    // 启动延时1秒

	/* 时间片检查：仅在200ms时间片执行 */
	if(TaskTimePare.Tim200ms_flag != 1)
	{
		return gRET_NG;
	}

	/* 启动延时 */
	if(wait_tim_cnt > 0)
	{
		wait_tim_cnt--;
		return gRET_NG;
	}

	/* 三种SOC计算方法 */
	batt_temp_full_cap();        // 1. 温度补偿
	batt_ocvsoc_calcua_soc();    // 2. OCV查表法校准
	batt_AHinte_calcua_soc();    // 3. 安时积分法

	return ret;
}

/* ==================== 温度补偿 ==================== */

/**
 * @brief 温度补偿计算
 * @details 根据电池温度调整实际可用容量
 *          温度范围与补偿比例：
 *
 *          温度范围       | 比例 | 补偿公式
 *          -------------|------|------------------
 *          ≥25℃         | 1    | 100%
 *          10~25℃       | 2    | 100% + (T-25)*0.1%
 *          0~10℃        | 3    | 100% + (T-25)*0.1%
 *          -10~0℃       | 4    | 100% + (T-25)*0.1%
 *          -20~-10℃     | 5    | 100% + (T-25)*0.1%
 *          <-20℃        | 6    | 75% (最小值)
 *
 * @note 温度变化>1℃时才更新容量
 */
static void batt_temp_full_cap(void)
{
	u8 ratio = 0;               // 温度分区
	u8 updata_flag = 0;         // 更新标志
	u16 temp_rate = 0;          // 温度补偿比例
	static i16 temp_last = 0;   // 上次温度值

	/* 确定温度分区 */
	if(gBMSData.BattPar.TempPackMin >= 250)                  // ≥25℃
	{
		ratio = 1;
	}
	else if((gBMSData.BattPar.TempPackMin < 250) && (gBMSData.BattPar.TempPackMin >= 100))  // 10~25℃
	{
		ratio = 2;
	}
	else if((gBMSData.BattPar.TempPackMin < 100) && (gBMSData.BattPar.TempPackMin >= 0))    // 0~10℃
	{
		ratio = 3;
	}
	else if((gBMSData.BattPar.TempPackMin < -10) && (gBMSData.BattPar.TempPackMin >= -200)) // -10~0℃
	{
		ratio = 4;
	}
	else if((gBMSData.BattPar.TempPackMin < -200) && (gBMSData.BattPar.TempPackMin >= -300)) // -20~-10℃
	{
		ratio = 5;
	}
	else    // <-20℃
	{
		ratio = 6;
	}

	/* 检测温度变化 >1℃ */
	if(gBMSData.BattPar.TempPackMin > temp_last)
	{
		if((gBMSData.BattPar.TempPackMin - temp_last) >= 10)    // 温度上升>1℃
		{
			updata_flag = 1;
			temp_last = gBMSData.BattPar.TempPackMin;
		}
	}
	else
	{
		if((temp_last - gBMSData.BattPar.TempPackMin) >= 10)    // 温度下降>1℃
		{
			updata_flag = 1;
			temp_last = gBMSData.BattPar.TempPackMin;
		}
	}

	/* 更新实际容量 */
	if(updata_flag == 1)
	{
		updata_flag = 0;

		// 计算温度补偿比例：1000 + (T-25)*0.1%
		// 例：20℃ = 1000 + (200-250)/10 = 995 (99.5%)
		temp_rate = 1000 + ratio * (gBMSData.BattPar.TempPackMin - 250) / 10;
		temp_rate = rate_limit_max_min(temp_rate);    // 限幅到75%~105%

		// 更新实际容量
		gBMSData.PackCap.pack_real_cap = gBMSData.PackCap.pack_rated_cap * temp_rate / 1000;

		// 更新剩余容量
		gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap * gBMSData.BattPar.SOCSys / 1000;
	}
}

/**
 * @brief 温度比例限幅
 * @param  rate_value: 输入比例值
 * @return 限幅后的比例值 (75%~105%)
 */
static u16 rate_limit_max_min(u16 rate_value)
{
	u16 ret_value = 0;

	if(rate_value > TEMP_CAP_RATE_LIMITH_HIGH)
	{
		ret_value = TEMP_CAP_RATE_LIMITH_HIGH;    // 上限105%
	}
	else if(rate_value < TEMP_CAP_RATE_LIMITL_LOW)
	{
		ret_value = TEMP_CAP_RATE_LIMITL_LOW;     // 下限75%
	}
	else
	{
		ret_value = rate_value;
	}

	return ret_value;
}

/* ==================== OCV查表法 ==================== */

/**
 * @brief OCV查表法计算SOC
 * @details 开路电压法，用于初始SOC和静置校准
 *
 * 工作流程：
 * 1. 首次调用：使用最低电压查表，作为初始SOC
 * 2. 静置校准：待机20分钟后，使用OCV校准SOC
 *
 * @note OCV法是最准确的SOC估算方法，但需要电池静置
 */
static void batt_ocvsoc_calcua_soc(void)
{
	static u8 soc_flag = 1;    // 首次调用标志

	/* 首次调用：初始化SOC */
	if(soc_flag == 1)
	{
		soc_flag = 0;
		// 使用最低电压查OCV表获取SOC
		gBMSData.BattPar.SOCSys = Seeka_OcvSoc_Table(gBMSData.BattPar.VoltCellMin, OCV_Table);
		gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap * gBMSData.BattPar.SOCSys / 1000;
		return;
	}

	/* 静置校准 */
	// 必须在待机模式
	if(gBMSData.Sys_Mod.sys_mode != SYS_MODE_STANDBY)
	{
		stanby_ocv_cnt = 0;
		return;
	}

	// 静置20分钟后校准
	if(stanby_ocv_cnt++ >= STANBY_OCV_TIME)
	{
		stanby_ocv_cnt = 0;
		// 使用最低电压查OCV表校准SOC
		gBMSData.BattPar.SOCSys = Seeka_OcvSoc_Table(gBMSData.BattPar.VoltCellMin, OCV_Table);
		gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap * gBMSData.BattPar.SOCSys / 1000;
	}
}

/**
 * @brief OCV查表（二分查找）
 * @param  SingleVolt: 单体电压 (mV)
 * @param  OcvTable: OCV对照表指针
 * @return SOC值 (0-1000)
 * @details 使用二分查找在OCV表中查找对应SOC
 */
static u16 Seeka_OcvSoc_Table(u16 SingleVolt, const u16 *OcvTable)
{
	u16 SocVale = 0;
	u8 TopVale = 0;
	u8 MidVale = 49;
	u8 BotVale = 99;

	/* 二分查找 */
	while(TopVale < BotVale)
	{
		if(SingleVolt <= OcvTable[MidVale])
		{
			TopVale = MidVale + 1;
		}
		else
		{
			BotVale = MidVale - 1;
		}
		MidVale = (TopVale + BotVale) / 2;
	}

	/* 线性插值 */
	if((MidVale != 0) && (MidVale != 99))
	{
		if(SingleVolt < OCV_Table[MidVale])
		{
			// 电压低于查表值，向下插值
			SocVale = MidVale * 10 + ((OCV_Table[MidVale] - SingleVolt) * 10) / (OCV_Table[MidVale] - OCV_Table[MidVale + 1]);
		}
		else if(SingleVolt > OCV_Table[MidVale])
		{
			// 电压高于查表值，向上插值
			SocVale = MidVale * 10 - ((SingleVolt - OCV_Table[MidVale]) * 10) / (OCV_Table[MidVale - 1] - OCV_Table[MidVale]);
		}
		else
		{
			// 电压等于查表值
			SocVale = MidVale * 10;
		}
	}
	else
	{
		// 边界值
		SocVale = MidVale * 10;
	}

	return 1000 - SocVale;
}

/* ==================== 安时积分法 ==================== */

/**
 * @brief 安时积分法计算SOC
 * @details 库仑计法，动态计算SOC变化
 *
 * 工作原理：
 * 1. 充电时：剩余容量 += 电流
 * 2. 放电时：剩余容量 -= 电流
 * 3. SOC = 剩余容量 / 实际容量
 *
 * 特殊处理：
 * - 充满电：电压接近OV阈值时，SOC=100%
 * - 放空：电压接近UV阈值时，SOC=0%
 */
static void batt_AHinte_calcua_soc(void)
{
	u16 curr_value = 0;

	/* 充电满电标志管理 */
	if((gBMSData.Sys_Mod.sys_mode == SYS_MODE_CHARGE) && (soc_uvp_flag == FALSE))
	{
		soc_uvp_flag = TRUE;    // 离开放电模式，重置欠压标志
	}

	if((gBMSData.Sys_Mod.sys_mode != SYS_MODE_CHARGE) && (soc_ovp_flag == FALSE))
	{
		soc_ovp_flag = TRUE;    // 离开充电模式，重置过压标志
	}

	/* 充满电检测 */
	if((gBMSData.BattPar.VoltCellMax >= (gBMSConfig.BQ76Para.OV_Thresh - 2)) &&
		(gBMSData.Sys_Mod.sys_mode == SYS_MODE_CHARGE) &&
		(soc_ovp_flag == TRUE))
	{
		soc_ovp_flag = FALSE;
		gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap;    // 剩余容量=实际容量
		gBMSData.BattPar.SOCSys = 1000;    // SOC=100%
		return;
	}

	/* 放空检测 */
	if((gBMSData.BattPar.VoltCellMin <= (gBMSConfig.BQ76Para.UV_Thresh + 2)) &&
		(gBMSData.Sys_Mod.sys_mode != SYS_MODE_CHARGE) &&
		(soc_uvp_flag == TRUE))
	{
		soc_uvp_flag = FALSE;
		gBMSData.PackCap.pack_rem_cap = 0;   // 剩余容量=0
		gBMSData.BattPar.SOCSys = 0;         // SOC=0%
		return;
	}

	/* 安时积分计算 */
	curr_value = abs(gBMSData.BattPar.CurrLine);    // 取电流绝对值

	if(gBMSData.Sys_Mod.sys_mode == SYS_MODE_CHARGE)
	{
		/* 充电模式：增加剩余容量 */
		if(gBMSData.PackCap.pack_real_cap >= (gBMSData.PackCap.pack_rem_cap + curr_value))
		{
			gBMSData.PackCap.pack_rem_cap += curr_value;
		}
		else
		{
			gBMSData.PackCap.pack_rem_cap = gBMSData.PackCap.pack_real_cap;    // 限制最大值
		}
	}
	else if(gBMSData.Sys_Mod.sys_mode == SYS_MODE_DISCHARGE)
	{
		/* 放电模式：减少剩余容量 */
		if(gBMSData.PackCap.pack_rem_cap >= curr_value)
		{
			gBMSData.PackCap.pack_rem_cap -= curr_value;
		}
		else
		{
			gBMSData.PackCap.pack_rem_cap = 0;    // 限制最小值
		}
	}
	else
	{
		/* 待机模式：可选扣除静态电流（当前代码注释掉） */
		// if(gBMSData.PackCap.pack_rem_cap >= 1)    // 静态10mA
		// {
		//      gBMSData.PackCap.pack_rem_cap -= 1;
		// }
		// else
		// {
		//      gBMSData.PackCap.pack_rem_cap = 0;
		// }
	}

	/* 更新SOC */
	gBMSData.BattPar.SOCSys = gBMSData.PackCap.pack_rem_cap * 1000 / gBMSData.PackCap.pack_real_cap;

	/* SOC限幅 */
	if(gBMSData.BattPar.SOCSys > SOC_LIMIT_MAX)
	{
		gBMSData.BattPar.SOCSys = 1000;
	}
}
