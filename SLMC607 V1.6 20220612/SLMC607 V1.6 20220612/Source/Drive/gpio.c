/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    gpio.c
 * @brief   GPIO初始化驱动
 * @details 配置BMS系统相关的GPIO引脚
 *          - LED指示灯
 *          - BQ769xx控制信号（ALERT、WAKE）
 *          - RS485收发控制
 *          - USART2通信
 *          - IIC通信
 *
 *          引脚映射总览：
 *          | 引脚  | 功能         | 方向     | 说明                          |
 *          |-------|--------------|----------|-------------------------------|
 *          | PB5   | SYS_POWER    | 输出     | 系统电源控制，上电默认高     |
 *          | PB14  | LED_RUN      | 输出     | 运行指示灯                    |
 *          | PB8   | BQ_ALERT     | 输入     | BQ769xx告警信号，浮空输入    |
 *          | PB3   | BQ_WAKE     | 输出     | BQ769xx唤醒信号，低电平有效  |
 *          | PB15  | RS485_EN     | 输出     | RS485收发控制，低电平接收    |
 *          | PA2   | USART2_TX    | 复用输出 | RS485发送                    |
 *          | PA3   | USART2_RX    | 上拉输入 | RS485接收                    |
 *          | PB6   | IIC_SCK      | 开漏输出 | IIC时钟线                    |
 *          | PB7   | IIC_SDA      | 开漏输出 | IIC数据线                    |
 */

#include "gpio.h"
#include "TaskFun.h"

/* ==================== 系统电源控制 ==================== */

/**
 * @brief 系统电源控制引脚初始化
 * @details 配置PB5为系统电源控制引脚
 *          - 模式：推挽输出
 *          - 速度：50MHz
 *          - 初始状态：高电平（电源开启）
 *
 * @note   此引脚控制系统电源，初始化后默认开启
 */
void Init_GPIO_SYS_Power(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);  // 默认高电平（开启电源）
}

/* ==================== LED指示灯 ==================== */

/**
 * @brief LED运行指示灯初始化
 * @details 配置PB14为LED运行指示灯引脚
 *          - 模式：推挽输出
 *          - 速度：50MHz
 *          - 初始状态：高电平（LED熄灭）
 *
 * @note   LED为低电平点亮，高电平熄灭
 *         500ms闪烁表示系统正常运行
 */
void Init_GPIO_LED(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_14);  // 默认高电平（LED熄灭）
}

/* ==================== BQ769xx控制信号 ==================== */

/**
 * @brief BQ769xx ALERT引脚初始化
 * @details 配置PB8为BQ769xx告警信号输入引脚
 *          - 模式：浮空输入
 *          - 速度：50MHz
 *
 * @note   ALERT为BQ769xx的告警输出引脚
 *          低电平表示有告警事件发生
 *          需要读取BQ769xx寄存器确定告警类型
 */
void Init_GPIO_ALERT(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * @brief BQ769xx WAKE引脚初始化
 * @details 配置PB3为BQ769xx唤醒信号输出引脚
 *          - 模式：推挽输出
 *          - 速度：50MHz
 *          - 初始状态：低电平（唤醒状态）
 *
 * @note   WAKE为BQ769xx的唤醒控制引脚
 *          低电平保持BQ769xx处于正常工作模式
 *          高电平可让BQ769xx进入SHIP模式（关机）
 */
void Init_GPIO_BQ769xx_Wake(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_3);  // 默认低电平（唤醒状态）
}

/* ==================== RS485通信控制 ==================== */

/**
 * @brief RS485收发控制引脚初始化
 * @details 配置PB15为RS485收发控制引脚
 *          - 模式：推挽输出
 *          - 速度：50MHz
 *          - 初始状态：低电平（接收模式）
 *
 * @note   RS485收发控制逻辑：
 *          - 低电平：接收模式（485芯片处于接收状态）
 *          - 高电平：发送模式（485芯片处于发送状态）
 *          发送前需置高，发送后需置低
 */
void Init_GPIO_R485EN(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_15);  // 默认低电平（接收模式）
}

/* ==================== USART2通信 ==================== */

/**
 * @brief USART2引脚初始化
 * @details 配置USART2的TX和RX引脚用于RS485通信
 *          - PA2: TX (复用推挽输出)
 *          - PA3: RX (上拉输入)
 *
 * @note   与RS485芯片连接：
 *          PA2(TX) → 485芯片DI引脚
 *          PA3(RX) ← 485芯片RO引脚
 */
void Init_GPIO_Uart2(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

	/* 配置TX引脚 - PA2 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      // 复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 配置RX引脚 - PA3 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;        // 上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* ==================== IIC通信 ==================== */

/**
 * @brief IIC引脚初始化
 * @details 配置软件模拟IIC的时钟线和数据线
 *          - PB6: IIC_SCK (开漏输出)
 *          - PB7: IIC_SDA (开漏输出)
 *
 * @note   IIC引脚配置为开漏输出模式：
 *          - 需要外部上拉电阻（典型值4.7K）
 *          - 开漏模式支持双向传输
 *          - 与BQ769xx芯片通信
 */
void Init_GPIO_IIC(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/* 配置SCK引脚 - PB6 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;     // 开漏输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 配置SDA引脚 - PB7 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;     // 开漏输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
