/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    timer.c
 * @brief   时间片管理 - BMS轮询架构核心
 * @details 实现基于定时器的时间片分片管理
 *          提供100ms/200ms/500ms/1s/10s/1min等时间片标志
 *
 *          时间片层级结构：
 *          1ms(中断) → 10ms → 100ms → 200ms → 500ms → 1s → 10s → 1min
 *
 *          各时间片用途：
 *          - 100ms: 继电器控制、通信操作状态机
 *          - 200ms: 电压电流采集、模式判断、均衡状态机、SOC计算
 *          - 500ms: 温度采集、配置读取、BQ769xx唤醒处理
 */

#include "timer.h"
#include "gpio.h"
#include "usart.h"
#include "led.h"

/* ==================== 定时器初始化 ==================== */

/**
 * @brief TIM1定时器初始化
 * @param  arr: 自动重装载值 (ARR)
 * @param  psc: 预分频值 (PSC)
 * @details 定时器频率计算：
 *          Ftim = Fclk / ((PSC+1) * (ARR+1))
 *
 *          主频72MHz，配置为PSC=9, ARR=7199：
 *          Ftim = 72MHz / (10 * 7200) = 1kHz
 *          即每1ms产生一次中断
 */
void Tim1_Init(u16 arr, u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	// 使能TIM1时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

	// 定时器时间基准配置
	TIM_TimeBaseStructure.TIM_Period = arr;                     // 自动重装载值 (ARR)
	TIM_TimeBaseStructure.TIM_Prescaler = psc;                  // 预分频值 (PSC)
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数模式
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;                // 时钟分割
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;            // 重复计数器
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	// 清除中断标志
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);
	TIM_ClearITPendingBit(TIM1, TIM_IT_Update);

	// 使能更新中断
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

	// 使能定时器
	TIM_Cmd(TIM1, ENABLE);

	// 配置TIM1中断优先级
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;   // 抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         // 响应优先级0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/* ==================== 定时器中断服务函数 ==================== */

/**
 * @brief TIM1更新中断服务函数
 * @details 每1ms执行一次，驱动时间片计数器
 */
void TIM1_UP_IRQHandler(void)
{
	// 检查更新中断标志
	if(TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
	{
		TIM_ClearFlag(TIM1, TIM_IT_Update);    // 清除中断标志

		// 10ms计数器递增
		TaskTimePare.Tim10ms_count++;
	}
}

/* ==================== 时间片标志管理 ==================== */

/**
 * @brief 时间片运行函数（主循环调用）
 * @details 根据计数器状态设置各时间片标志
 *
 *          时间片生成逻辑：
 *          - Tim10ms_count = 10 → Tim10ms_flag = 1
 *          - Tim10ms_flag = 1, Tim100ms_count = 10 → Tim100ms_flag = 1
 *          - Tim100ms_flag = 1, Tim200ms_count = 2 → Tim200ms_flag = 1
 *          - Tim100ms_flag = 1, Tim500ms_count = 5 → Tim500ms_flag = 1
 *          - Tim500ms_flag = 1, Tim1s_count = 2 → Tim1s_flag = 1
 *          - Tim1s_flag = 1, Tim10s_count = 10 → Tim10s_flag = 1
 *          - Tim10s_flag = 1, Tim1min_count = 6 → Tim1min_flag = 1
 *
 * @note  此函数在主循环中调用，负责更新所有时间片标志
 */
void Systim_Time_Run(void)
{
	/* 10ms时间片 */
	if(TaskTimePare.Tim10ms_count >= 10)
	{
		TaskTimePare.Tim10ms_count = 0;
		TaskTimePare.Tim10ms_flag = 1;

		/* 100ms时间片 (10 * 10ms = 100ms) */
		if(++TaskTimePare.Tim100ms_count >= 10)
		{
			TaskTimePare.Tim100ms_count = 0;
			TaskTimePare.Tim100ms_flag = 1;
		}
	}

	/* 200ms时间片 (2 * 100ms = 200ms) */
	if(TaskTimePare.Tim100ms_flag == 1)
	{
		if(++TaskTimePare.Tim200ms_count >= 2)
		{
			TaskTimePare.Tim200ms_count = 0;
			TaskTimePare.Tim200ms_flag = 1;
		}
	}

	/* 500ms时间片 (5 * 100ms = 500ms) */
	if(TaskTimePare.Tim100ms_flag == 1)
	{
		if(++TaskTimePare.Tim500ms_count >= 5)
		{
			TaskTimePare.Tim500ms_count = 0;
			TaskTimePare.Tim500ms_flag = 1;
		}
	}

	/* 1s时间片 (2 * 500ms = 1s) */
	if(TaskTimePare.Tim500ms_flag == 1)
	{
		if(++TaskTimePare.Tim1s_count >= 2)
		{
			TaskTimePare.Tim1s_count = 0;
			TaskTimePare.Tim1s_flag = 1;
		}
	}

	/* 10s时间片 (10 * 1s = 10s) */
	if(TaskTimePare.Tim1s_flag == 1)
	{
		if(++TaskTimePare.Tim10s_count >= 10)
		{
			TaskTimePare.Tim10s_count = 0;
			TaskTimePare.Tim10s_flag = 1;
		}
	}

	/* 1min时间片 (6 * 10s = 1min) */
	if(TaskTimePare.Tim10s_flag == 1)
	{
		if(++TaskTimePare.Tim1min_count >= 6)
		{
			TaskTimePare.Tim1min_count = 0;
			TaskTimePare.Tim1min_flag = 1;
		}
	}
}

/**
 * @brief 清除所有时间片标志
 * @details 在主循环末尾调用，清除当前周期的时间片标志
 *          为下一周期做准备
 */
void Clear_flag(void)
{
	TaskTimePare.Tim10ms_flag = 0;
	TaskTimePare.Tim100ms_flag = 0;
	TaskTimePare.Tim200ms_flag = 0;
	TaskTimePare.Tim500ms_flag = 0;
	TaskTimePare.Tim1s_flag = 0;
	TaskTimePare.Tim10s_flag = 0;
	TaskTimePare.Tim1min_flag = 0;
}

/* ==================== 延时函数（已注释） ==================== */
/*
void delay_us(unsigned int num)
{
	unsigned int ia = 0;
	for(ia = 0; ia < num; ia++)
	{
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
	}
}

void delay_ms(unsigned int num)
{
	unsigned int ia = 0;
	for(ia = 0; ia < num; ia++)
	{
		delay_us(1000);
	}
}
*/
