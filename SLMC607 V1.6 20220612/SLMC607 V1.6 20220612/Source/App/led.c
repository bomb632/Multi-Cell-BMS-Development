/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    led.c
 * @brief   LED指示灯控制
 * @details 控制运行状态指示灯
 *          LED连接在PB14引脚，500ms闪烁
 */

#include "led.h"
#include "timer.h"
#include "DataBase.h"

/**
 * @brief LED运行控制（500ms调用）
 * @details 500ms时间片执行，翻转LED状态实现闪烁效果
 *          用于指示BMS系统正常运行
 * @return gRET_OK
 */
u8 Led_Run_Cpu(void)
{
	if(TaskTimePare.Tim500ms_flag == 1)
	{
		LedRun_Set_Not();    // 翻转LED状态
	}
	return gRET_OK;
}

/**
 * @brief 点亮LED
 * @details PB14输出低电平，LED导通点亮
 */
void LedRun_Set_On(void)
{
	PBout(14) = 0;
}

/**
 * @brief 熄灭LED
 * @details PB14输出高电平，LED截止熄灭
 */
void LedRun_Set_Off(void)
{
	PBout(14) = 1;
}

/**
 * @brief LED状态翻转
 * @details PB14输出取反，实现闪烁效果
 */
void LedRun_Set_Not(void)
{
	PBout(14) = ~PBout(14);
}
