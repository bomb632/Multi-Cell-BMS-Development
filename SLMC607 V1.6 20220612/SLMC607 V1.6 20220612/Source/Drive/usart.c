/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    usart.c
 * @brief   RS485通信驱动 (USART2)
 * @details 实现基于USART2的RS485通信功能
 *          - 波特率可配置（默认9600）
 *          - 支持CRC校验
 *          - 循环缓冲区接收
 *          - 通信超时检测（500ms）
 *
 *          硬件连接：
 *          - PA2: USART2_TX (RS485_TXD)
 *          - PA3: USART2_RX (RS485_RXD)
 *          - PB15: RS485接收使能（低电平接收，高电平发送）
 */

#include "usart.h"
#include "sys.h"
#include "adc.h"
#include "timer.h"
#include "TaskFun.h"
#include "Flash.h"
#include "gpio.h"
#include "timer.h"
#include "delay.h"
#include "Rs485Data.h"

/* ==================== 全局变量定义 ==================== */

/* ==================== 局部变量定义 ==================== */
static u16 Uart1ErrCountTime;              // UART1错误计数时间（保留）
static u8 Uart2Recv[UARTNunD] = {0};       // UART2接收环形缓冲区
static u8 Uart2RecvBuf[UARTNunD] = {0};    // UART2接收数据缓冲区
static u8 RBUp2 = 0;                       // 环形缓冲区写指针
static u8 RBDown2 = 0;                     // 环形缓冲区读指针
static u8 Uart2ACC = 0;                    // CRC累加器
static u8 Uart2CntR = 0;                   // 当前接收数据计数
static u8 Uart2CntTT = 0;                  // 预期接收数据总数
static u8 Uart2Stt = 0;                    // 接收状态机状态
static u8 Uart2_State = 0;                 // UART2通信状态标志
static u16 Uart2ErrCountTime = 0;          // 通信超时计数器（bit15: 超时标志）

/* ==================== USART2初始化 ==================== */

/**
 * @brief USART2初始化函数
 * @param  bound: 波特率（如9600、115200等）
 * @details 配置USART2为RS485通信模式
 *          - 数据位：8位
 *          - 停止位：1位
 *          - 校验位：无
 *          - 中断优先级：抢占3，响应3
 *
 *          引脚映射：
 *          - PA2: TX (复用推挽输出)
 *          - PA3: RX (浮空输入)
 */
void Uart2_init(u32 bound)
{
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	USART_DeInit(USART2);  // 复位串口2
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);  // 使能USART2时钟

	/* 配置TX引脚 - PA2 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 配置RX引脚 - PA3 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* USART参数配置 */
	USART_InitStructure.USART_BaudRate = bound;                    // 波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;    // 8位数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;         // 1位停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;            // 无校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  // 无硬件流控
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;  // 收发模式
	USART_Init(USART2, &USART_InitStructure);  // 初始化串口

	/* 配置USART2中断优先级 */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;   // 抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;          // 响应优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  // 使能接收中断
	USART_Cmd(USART2, ENABLE);                      // 使能串口
}

/* ==================== 数据发送函数 ==================== */

/**
 * @brief USART2发送单字节数据
 * @param  Data: 要发送的字节数据
 * @details 阻塞等待发送缓冲区空后发送数据
 */
void Usart2SendData(unsigned char Data)
{
	while((USART2->SR & 0X40) == 0);  // 循环等待，直到发送完成
	USART2->DR = (u8) Data;
}

/**
 * @brief USART2发送数据包（带CRC校验）
 * @param  STData: 源地址
 * @param  FunCom: 功能码
 * @param  LengByte: 数据长度
 * @param  Data: 数据缓冲区指针
 * @details 数据帧格式：
 *          [起始符][源地址][功能码][长度][数据...][校验和]
 *          校验和 = 0 - (所有字节累加和)
 *
 *          发送流程：
 *          1. 切换为发送模式（RS485_Rce_OFF）
 *          2. 组装数据帧
 *          3. 发送数据
 *          4. 延时100us等待发送完成
 *          5. 切换为接收模式（RS485_Rce_ON）
 */
void Uart2Send(unsigned char STData, unsigned char FunCom, unsigned char LengByte, unsigned char* Data)
{
	unsigned char i = 0;
	unsigned char CRCnum = 0;
	unsigned char SendData[UARTNunD + 1] = {0};

	RS485_Rce_OFF;  // 切换为发送模式

	/* 组装数据帧 */
	SendData[0] = START_SYMBOL;
	CRCnum = START_SYMBOL;
	SendData[1] = STData;
	CRCnum += STData;
	SendData[2] = FunCom;
	CRCnum += FunCom;
	SendData[3] = LengByte;
	CRCnum += LengByte;

	/* 添加数据并计算累加和 */
	for(i = 0; i < LengByte; i++)
	{
		SendData[i + 4] = Data[i];
		CRCnum += SendData[i + 4];
	}
	SendData[LengByte + 4] = 0 - CRCnum;  // 计算校验和

	/* 发送数据帧 */
	for(i = 0; i < (LengByte + 5); i++)
	{
		Usart2SendData(SendData[i]);
	}
	delay_us(100);    // 等待发送完成
	RS485_Rce_ON;    // 切换为接收模式
}

/* ==================== USART2中断服务函数 ==================== */

/**
 * @brief USART2中断服务函数
 * @details 接收中断：将接收到的字节存入环形缓冲区
 *          环形缓冲区满时，新数据覆盖最旧数据
 */
void USART2_IRQHandler(void)
{
	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)  // 检查接收中断标志
	{
		/* 检查缓冲区是否已满 */
		if(((RBDown2 - RBUp2) & UARTNunD) != 1)
		{
			/* 缓冲区未满，存入数据 */
			Uart2Recv[RBUp2++] = USART_ReceiveData(USART2);
			if(RBUp2 >= UARTNunD)
			{
				RBUp2 = 0;  // 指针回绕
			}
		}
		else
		{
			/* 缓冲区已满，丢弃最旧数据 */
			Uart2Recv[RBUp2] = USART_ReceiveData(USART2);
		}
	}
}

/* ==================== 数据接收处理 ==================== */

/**
 * @brief UART2数据包解析（状态机）
 * @details 状态机实现：
 *          - State 0: 等待起始符(0xA8)
 *          - State 1: 接收源地址
 *          - State 2: 接收功能码
 *          - State 3: 接收数据长度
 *          - State 4: 接收数据和校验和
 *
 *          数据帧格式：
 *          [起始符][源地址][功能码][长度][数据...][校验和]
 *            1字节   1字节    1字节   1字节  N字节   1字节
 *
 *          状态标志：
 *          - Uart2_State.bit0: 未收到起始字节
 *          - Uart2_State.bit1: CRC校验错误
 *          - Uart2_State.bit2: 状态机异常
 */
void Uart2Run_Pack(void)
{
	unsigned char ia = 0;

	if(RBDown2 != RBUp2)  // 检查是否有新数据
	{
		ia = Uart2Recv[RBDown2++];  // 读取一个字节
		if(RBDown2 >= UARTNunD)
		{
			RBDown2 = 0;  // 指针回绕
		}

		switch(Uart2Stt)
		{
		case 0:
			/* 等待起始字节 */
			if(ia == START_SYMBOL)
			{
				Uart2Stt = 1;
				Uart2ACC = ia;        // 累加校验和
				Uart2CntR = 0;        // 清空接收计数
				Uart2RecvBuf[Uart2CntR] = ia;
				Uart2CntR = 1;        // 接收计数+1
				Uart1ErrCountTime |= 0x8000;  // 开始计时（bit15置1）
			}
			else
			{
				Uart2_State |= 0x01;  // 未收到起始字节
				Uart2CntR = 0;
				Uart2Stt = 0;
				Uart2ACC = 0;
				Uart1ErrCountTime &= ~0x8000;  // 停止计时（bit15清零）
			}
			break;

		case 1:
			/* 接收源地址 */
			Uart2Stt++;
			Uart2ACC += ia;
			Uart2RecvBuf[Uart2CntR] = ia;
			Uart2CntR++;
			Uart2ErrCountTime &= ~0x7fff;  // 清除超时计数（低15位）
			break;

		case 2:
			/* 接收功能码 */
			Uart2Stt++;
			Uart2ACC += ia;
			Uart2RecvBuf[Uart2CntR] = ia;
			Uart2CntR++;
			Uart2ErrCountTime &= ~0x7fff;  // 清除超时计数
			break;

		case 3:
			/* 接收数据长度 */
			Uart2Stt++;
			Uart2ACC += ia;
			Uart2RecvBuf[Uart2CntR] = ia;
			Uart2CntTT = ia + 5;  // 计算总字节数（起始+地址+功能+长度+数据+CRC）
			Uart2CntR++;
			Uart2ErrCountTime &= ~0x7fff;  // 清除超时计数
			break;

		case 4:
			/* 接收数据和校验和 */
			Uart2ACC += ia;
			Uart2RecvBuf[Uart2CntR] = ia;
			Uart2CntR++;
			Uart2ErrCountTime &= ~0x7fff;  // 清除超时计数

			/* 检查是否接收完整 */
			if(Uart2CntR >= Uart2CntTT)
			{
				Uart2Stt = 0;
				Uart2ErrCountTime = 0;  // 停止计时

				/* CRC校验 */
				if(Uart2ACC == 0)  // 累加和为0表示校验通过
				{
					Rs485_Read_Data(Uart2RecvBuf, sizeof(Uart2RecvBuf));
				}
				else
				{
					Uart2CntR = 0;
					Uart2Stt = 0;
					Uart2ACC = 0;
					Uart2_State |= 0x02;  // CRC校验错误
				}
			}
			break;

		default:
			/* 状态异常 */
			Uart2CntR = 0;
			Uart2Stt = 0;
			Uart2ACC = 0;
			Uart2_State |= 0x04;  // 状态机异常
			break;
		}
	}
}

/**
 * @brief UART2通信超时检测
 * @details 检测数据接收超时（500ms）
 *          超时后重置接收状态机
 *
 *          超时检测逻辑：
 *          - Uart2ErrCountTime.bit15 = 1: 开始接收
 *          - Uart2ErrCountTime低15位: 超时计数
 *          - 超时阈值：500 * 1ms = 500ms
 */
void Uart2DataDropChk(void)
{
	if(Uart2ErrCountTime & 0x8000)  // 检查是否在接收状态
	{
		Uart2ErrCountTime++;
		if((Uart2ErrCountTime & 0x7fff) > 500)  // 超过500ms
		{
			Uart2CntR = 0;      // 重置接收计数
			Uart2Stt = 0;       // 重置状态机
			Uart2ACC = 0;       // 重置校验累加器
		}
	}
}
