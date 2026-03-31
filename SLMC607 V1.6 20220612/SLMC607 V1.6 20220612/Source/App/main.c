/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    main.c
 * @brief   SLMC607 BMS主程序 - 时间片轮询架构
 * @details 采用时间片分片轮询方式，非阻塞状态机设计
 *          - 100ms: 继电器控制、通信操作
 *          - 200ms: 电压电流采集、模式判断
 *          - 500ms: 温度采集、配置读取
 */

#include "gpio.h"
#include "iic.h"
#include "led.h"
#include "sys.h"
#include "timer.h"
#include "adc.h"
#include "usart.h"
#include "TaskFun.h"
#include "Flash.h"
#include "delay.h"
#include "wdt.h"
#include "bq769xx.h"
#include "ConfigPara.h"
#include "Rs485Data.h"
#include "DataBase.h"
#include "soc.h"

/**
 * @brief 时间片标志结构体
 * @details 用于标记各个时间片是否到达执行时间
 */
TaskTime     TaskTimePare=TaskTime_DEFAULTS;

/* 函数声明 */
void SW_JTAGioConfig(void);
void FLASH_Update_Data(void);
void Sys_Rece_Data(void);
void Sys_Send_Data(void);
void Get_Base_Data(void);
void Mix_Function_Pro(void);

/**
 * @brief 主函数
 * @details BMS主程序入口，采用时间片轮询架构
 *          主循环各模块功能：
 *          1. Systim_Time_Run()     - 时间片标志更新
 *          2. Sys_Rece_Data()       - 接收外部通信数据
 *          3. Sys_Send_Data()       - 发送数据出去
 *          4. Get_Base_Data()       - 基础数据采集（电压/电流/温度）
 *          5. Mix_Function_Pro()    - 混合功能处理（状态机/通信操作/SOC计算）
 *          6. Clear_flag()          - 清除时间片标志
 */
int main(void)
{
	/* 系统初始化 */
	NVIC_Configuration();       // 配置NVIC中断分组：2位抢占优先级，2位响应优先级
	SW_JTAGioConfig();          // 禁用JTAG，释放PA15/PB3/PB4作为普通GPIO使用
	Init_GPIO_SYS_Power();      // 系统电源控制GPIO初始化
	InitPara0();                // 加载/初始化BMS配置参数

	// FLASH_Update_Data();    // 首次使用时取消注释，将默认参数写入Flash

	// System_Pare_Get();      // 从Flash读取保存的参数（InitPara0已处理）

	/* GPIO外设初始化 */
	Init_GPIO_LED();            // LED指示灯GPIO初始化
	Init_GPIO_ALERT();          // BQ769xx ALERT告警引脚初始化
	Init_GPIO_BQ769xx_Wake();   // BQ769xx唤醒引脚初始化
	Init_GPIO_R485EN();         // RS485收发控制引脚初始化
	Init_GPIO_IIC();            // I2C通信引脚初始化

	/* 通信接口初始化 */
	Uart2_init(115200);         // UART2初始化，波特率115200，用于RS485通信

	/* 定时器初始化 */
	Tim1_Init(7199,9);          // 72MHz/(7199+1)/(9+1) = 1kHz，即1ms中断一次
	delay_init();               // SYSTICK延迟函数初始化

	/* BQ769xx AFE芯片初始化 */
	BQ769xx_Wake();             // 唤醒BQ769xx芯片（拉高唤醒引脚1秒）
	BQ769xx_Init();             // 初始化BQ769xx寄存器配置
	batt_cap_ocvsoc_init();     // 电池容量和SOC算法初始化

	/* 系统初始状态设置 */
	gBMSData.Sys_Mod.sys_mode=SYS_MODE_STANDBY;   // 系统模式：待机模式

	// Init_Watchdog(3,1250);   // 看门狗初始化 (prer,rlr)  Tout=((4*2^prer)*rlr)/40 (ms). = 1000ms

	/* ==================== 主循环：时间片轮询架构 ==================== */
	while(1)
	{
		/* 1. 获取时间标志 - 更新各时间片标志位 */
		Systim_Time_Run();

		/* 2. 接收外部数据 - RS485通信接收处理 */
		Sys_Rece_Data();

		/* 3. 数据发送 - RS485通信发送处理 */
		Sys_Send_Data();

		/* 4. 基础数据采集 - 电压/电流/温度采集（200ms/500ms时间片） */
		Get_Base_Data();

		/* 5. 混合功能处理 - 状态机/通信操作/SOC计算 */
		Mix_Function_Pro();

		/* 6. 清除时间标志 - 清除各时间片标志位 */
		Clear_flag();
	}
}

/**
 * @brief 禁用JTAG功能，释放PA15/PB3/PB4作为普通GPIO
 * @details STM32F10x的JTAG默认占用PA15/PB3/PB4/PB4引脚
 *          本函数将JTAG禁用，释放这些引脚作为普通GPIO使用
 */
void SW_JTAGioConfig(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);          // 使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);     // 禁用JTAG，保留SWD
}

/**
 * @brief Flash数据更新函数
 * @details 首次使用或恢复出厂设置时调用
 *          流程：解锁Flash -> 擦除页 -> 初始化默认参数 -> 写入Flash -> 上锁
 */
void FLASH_Update_Data(void)
{
	FLASH_Unlock();                      // Flash解锁
	FLASH_ErasePage(FLASH_SAVE_ADDR);    // 擦除指定Flash页
	InitPara0();                         // 加载默认参数
	WriteAllData();                      // 将参数写入Flash
	FLASH_Lock();                        // Flash上锁
}

/**
 * @brief 系统数据接收处理
 * @details RS485通信接收处理函数
 *          在100ms时间片内处理接收到的通信数据包
 */
void Sys_Rece_Data(void)
{
	Uart2Run_Pack();    // UART2数据包解析处理
}

/**
 * @brief 系统数据发送处理
 * @details RS485通信发送处理函数
 *          可以在此处添加周期性数据发送逻辑
 */
void Sys_Send_Data(void)
{
	// Rs485_Send_Test();    // 测试用RS485发送函数
}

/**
 * @brief 基础数据采集函数
 * @details 采集BMS基础数据，在各自时间片内执行：
 *          - BQ769xx_GetData()    : 200ms时间片，采集电压电流
 *          - BQ769xx_GetConfig()  : 500ms时间片，读取配置寄存器
 *          - Balan_Pack_Check()   : 200ms时间片，均衡状态机
 *          - Led_Run_Cpu()        : LED状态更新
 */
void Get_Base_Data(void)
{
	BQ769xx_GetData();      // 获取BQ769xx采样数据（电压/电流/温度）
	BQ769xx_GetConfig();    // 获取BQ769xx配置和状态寄存器
	Balan_Pack_Check();     // 均衡功能检查和控制状态机
	Led_Run_Cpu();          // LED指示灯运行状态更新
}

/**
 * @brief 混合功能处理函数
 * @details 处理各种混合功能，包括：
 *          - BQ769xx_Oper_Comm() : 100ms时间片，BQ769xx通信操作状态机（MOS开关/告警清除/关机）
 *          - BQ769xx_Wake_Comm() : 500ms时间片，BQ769xx唤醒通信处理
 *          - Sys_Function_Pro()  : 系统功能处理（系统模式状态机）
 *          - Batt_soc_check()    : 电池SOC估算更新
 */
void Mix_Function_Pro(void)
{
	BQ769xx_Oper_Comm();    // BQ769xx通信操作状态机（MOS开关控制/告警清除/关机）
	BQ769xx_Wake_Comm();    // BQ769xx唤醒通信处理
	Sys_Function_Pro();     // 系统功能处理（系统模式状态机）
	Batt_soc_check();       // 电池SOC估算和更新
}
