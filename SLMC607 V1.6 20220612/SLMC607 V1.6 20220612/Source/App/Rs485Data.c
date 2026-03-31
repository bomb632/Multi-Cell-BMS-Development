/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    Rs485Data.c
 * @brief   RS485通信协议处理
 * @details 实现BMS与上位机的RS485通信功能
 *          支持数据查询和远程控制命令
 */

#include "Rs485Data.h"
#include "timer.h"
#include "usart.h"
#include "bq769xx.h"
#include "DataBase.h"

/* ==================== 通信协议定义 ==================== */
#define RS485_BMS_ID   0x11        // BMS设备地址

// 功能码定义
#define RS485_FUN_COMM1  0x01      // 查询增益偏移
#define RS485_FUN_COMM2  0x02      // 查询BMS数据
#define RS485_FUN_COMM3  0x03      // 查询BQ769xx配置
#define RS485_FUN_COMM4  0x04      // 远程控制命令

/* ==================== 全局变量定义 ==================== */
u8 Bq769xx_Oper_EN = 0;           // BQ769xx操作使能标志
u8 Bq769xx_Oper_Type = 0;         // BQ769xx操作类型

/* ==================== 局部函数声明 ==================== */
static u16 Rs485_Rec_PCdata(u8* PCdtaBuff, u8 byte_size);  // 处理接收的PC数据
void Send_GainOffset_Data(void);                          // 发送增益偏移数据
void Send_BMS_Data(void);                                  // 发送BMS数据
void Send_BQ769xxConfig_Data(void);                        // 发送BQ769xx配置数据
static u16 PcComm_Pro(u8* dtaBuff, u8 size);               // 处理PC控制命令

/* ==================== 测试函数 ==================== */

/**
 * @brief RS485发送测试函数
 * @details 周期性发送测试数据（500ms）
 *          用于调试和通信测试
 */
void Rs485_Send_Test(void)
{
	u8 DataBuff[3] = {0};

	if(TaskTimePare.Tim500ms_flag == 1)
	{
		DataBuff[0] = 1;
		DataBuff[1] = 2;
		DataBuff[2] = 3;
		Uart2Send(RS485_BMS_ID, RS485_FUN_COMM1, sizeof(DataBuff), DataBuff);
	}
}

/* ==================== 数据接收处理 ==================== */

/**
 * @brief RS485数据读取入口
 * @param  resp_buf: 接收缓冲区指针
 * @param  size_buf: 缓冲区大小
 * @return gRET_OK - 处理成功, gRET_NG - 参数错误
 */
u16 Rs485_Read_Data(u8* resp_buf, u8 size_buf)
{
	u8 rsp_val;
	u16 ret = 0;

	// 参数检查
	if((resp_buf == NULL) || (0 == size_buf))
	{
		return gRET_NG;
	}

	// 获取功能码
	rsp_val = resp_buf[1];

	// 根据设备地址分发处理
	switch(rsp_val)
	{
	case RS485_BMS_ID:
		Rs485_Rec_PCdata(resp_buf, size_buf);
		break;

	default:
		Rs485_Rec_PCdata(resp_buf, size_buf);    // 默认处理
		break;
	}

	return ret;
}

/**
 * @brief 处理接收的PC数据
 * @param  PCdtaBuff: PC数据缓冲区
 * @param  size_buf: 数据大小
 * @return 处理结果
 * @details 根据功能码分发到不同的处理函数
 */
u16 Rs485_Rec_PCdata(u8* PCdtaBuff, u8 size_buf)
{
	u8 typedata = 0;
	u16 ret = 0;

	// 获取功能码
	typedata = PCdtaBuff[2];

	switch(typedata)
	{
	case RS485_FUN_COMM1:
		// 查询增益偏移
		Send_GainOffset_Data();
		break;

	case RS485_FUN_COMM2:
		// 查询BMS数据
		Send_BMS_Data();
		break;

	case RS485_FUN_COMM3:
		// 查询BQ769xx配置
		Send_BQ769xxConfig_Data();
		break;

	case RS485_FUN_COMM4:
		// 远程控制命令
		PcComm_Pro(PCdtaBuff, size_buf);
		break;

	default:
		break;
	}

	return ret;
}

/* ==================== 数据发送函数 ==================== */

/**
 * @brief 发送增益偏移数据
 * @details 发送BQ769xx的ADC增益和偏移校准参数
 *          数据格式：
 *          - Byte[0-1]: ADC增益 (uV)
 *          - Byte[2]: ADC偏移
 */
void Send_GainOffset_Data(void)
{
	u8 DataBuff[3] = {0};

	DataBuff[0] = (u8)((VoltCellGainUV & 0xFF00) >> 8);   // ADC增益高字节
	DataBuff[1] = (u8)(VoltCellGainUV & 0x00ff);          // ADC增益低字节
	DataBuff[2] = VoltCellOffSet;                         // ADC偏移

	Uart2Send(RS485_BMS_ID, RS485_FUN_COMM1, sizeof(DataBuff), DataBuff);
}

/**
 * @brief 发送BMS数据
 * @details 发送完整的BMS状态数据（46字节）
 *          数据格式：
 *          - Byte[0-29]: 15串电池电压 (每串2字节，高字节在前)
 *          - Byte[30-31]: 总电压
 *          - Byte[32-37]: 3路温度
 *          - Byte[38-39]: 电流
 *          - Byte[40-41]: SOC
 *          - Byte[42-45]: 均衡控制开关状态
 */
void Send_BMS_Data(void)
{
	u8 DataBuff[46] = {0};
	u8 ia = 0;

	// 15串电池电压（每串2字节）
	for(ia = 0; ia < 15; ia++)
	{
		DataBuff[2 * ia] = (gBMSData.BattPar.VoltCell[ia] & 0xFF00) >> 8;
		DataBuff[2 * ia + 1] = gBMSData.BattPar.VoltCell[ia] & 0xFF;
	}

	// 总电压
	DataBuff[30] = (gBMSData.BattPar.VoltLine) >> 8;
	DataBuff[31] = gBMSData.BattPar.VoltLine & 0xFF;

	// 3路温度
	DataBuff[32] = (gBMSData.BattPar.TempCell[0]) >> 8;
	DataBuff[33] = gBMSData.BattPar.TempCell[0] & 0xFF;
	DataBuff[34] = (gBMSData.BattPar.TempCell[1]) >> 8;
	DataBuff[35] = gBMSData.BattPar.TempCell[1] & 0xFF;
	DataBuff[36] = (gBMSData.BattPar.TempCell[2]) >> 8;
	DataBuff[37] = gBMSData.BattPar.TempCell[2] & 0xFF;

	// 电流
	DataBuff[38] = (gBMSData.BattPar.CurrLine) >> 8;
	DataBuff[39] = gBMSData.BattPar.CurrLine & 0xFF;

	// SOC
	DataBuff[40] = (gBMSData.BattPar.SOCSys) >> 8;
	DataBuff[41] = gBMSData.BattPar.SOCSys & 0xFF;

	// 均衡控制开关状态（32位，从高到低）
	DataBuff[42] = (gBMSData.BalaPar.bala_ctrl_sw >> 24) & 0xff;
	DataBuff[43] = (gBMSData.BalaPar.bala_ctrl_sw >> 16) & 0xff;
	DataBuff[44] = (gBMSData.BalaPar.bala_ctrl_sw >> 8) & 0xff;
	DataBuff[45] = gBMSData.BalaPar.bala_ctrl_sw & 0xff;

	Uart2Send(RS485_BMS_ID, RS485_FUN_COMM2, sizeof(DataBuff), DataBuff);
}

/**
 * @brief 发送BQ769xx配置数据
 * @details 发送BQ769xx的寄存器配置（12字节）
 *          数据格式：
 *          - Byte[0]: SYS_STAT（系统状态）
 *          - Byte[1-3]: CELLBAL1-3（均衡寄存器）
 *          - Byte[4-5]: SYS_CTRL1-2（系统控制）
 *          - Byte[6-8]: PROTECT1-3（保护配置）
 *          - Byte[9]: OVTrip（过压阈值）
 *          - Byte[10]: UVTrip（欠压阈值）
 *          - Byte[11]: CCCfg（库仑计配置）
 */
void Send_BQ769xxConfig_Data(void)
{
	u8 DataBuff[12] = {0};

	DataBuff[0] = Bq769xxReg.SysStatus.StatusByte;         // 系统状态
	DataBuff[1] = Bq769xxReg.CellBal1.CellBal1Byte;         // 均衡寄存器1
	DataBuff[2] = Bq769xxReg.CellBal2.CellBal2Byte;         // 均衡寄存器2
	DataBuff[3] = Bq769xxReg.CellBal3.CellBal3Byte;         // 均衡寄存器3
	DataBuff[4] = Bq769xxReg.SysCtrl1.SysCtrl1Byte;         // 系统控制1
	DataBuff[5] = Bq769xxReg.SysCtrl2.SysCtrl2Byte;         // 系统控制2
	DataBuff[6] = Bq769xxReg.Protect1.Protect1Byte;         // 保护配置1
	DataBuff[7] = Bq769xxReg.Protect2.Protect2Byte;         // 保护配置2
	DataBuff[8] = Bq769xxReg.Protect3.Protect3Byte;         // 保护配置3
	DataBuff[9] = Bq769xxReg.OVTrip;                        // 过压阈值
	DataBuff[10] = Bq769xxReg.UVTrip;                       // 欠压阈值
	DataBuff[11] = Bq769xxReg.CCCfg;                        // 库仑计配置

	Uart2Send(RS485_BMS_ID, RS485_FUN_COMM3, sizeof(DataBuff), DataBuff);
}

/* ==================== 远程控制命令处理 ==================== */

/**
 * @brief 处理PC远程控制命令
 * @param  dtaBuff: 接收数据缓冲区
 * @param  size: 数据大小
 * @return 处理结果
 * @details 支持以下控制命令：
 *          - COMM_DSG_ON:     开启放电MOS
 *          - COMM_DSG_OFF:    关闭放电MOS
 *          - COMM_CHG_ON:     开启充电MOS
 *          - COMM_CHG_OFF:    关闭充电MOS
 *          - COMM_CLEAR_ALERT: 清除告警
 *          - COMM_ENTER_SHIP:  进入关机模式
 *
 *          命令格式：dtaBuff[4] = 操作类型
 */
u16 PcComm_Pro(u8* dtaBuff, u8 size)
{
	u8 typecomm = 0;
	u16 ret = 0;

	// 获取操作类型
	typecomm = dtaBuff[4];

	switch(typecomm)
	{
	case COMM_DSG_ON:
		// 开启放电MOS
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_DSG_ON;
		break;

	case COMM_DSG_OFF:
		// 关闭放电MOS
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_DSG_OFF;
		break;

	case COMM_CHG_ON:
		// 开启充电MOS
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_CHG_ON;
		break;

	case COMM_CHG_OFF:
		// 关闭充电MOS
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_CHG_OFF;
		break;

	case COMM_CLEAR_ALERT:
		// 清除告警
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_CLEAR_ALERT;
		break;

	case COMM_ENTER_SHIP:
		// 进入关机模式
		Bq769xx_Oper_EN = OPER_ON;
		Bq769xx_Oper_Type = COMM_ENTER_SHIP;
		break;

	default:
		break;
	}

	return ret;
}
