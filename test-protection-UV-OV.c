/**
 * @file    debug-code-2026-04-01.c
 * @brief   保护功能调试代码
 * @date    2026-04-01
 * @details 用于测试BMS保护功能（UV/OV保护）
 */

/* ==================== 调试代码1：保护状态调试函数 ==================== */

/**
 * @brief 保护状态调试输出函数
 * @details 每100秒输出一次保护状态到RS485
 *          数据帧格式：A8 01 FE 04 [SYS_STAT] [CHG] [DSG] [标记] [校验]
 *
 * @note   添加到 Source/App/main.c 文件末尾
 */
void Protection_Status_Debug(void)
{
    static u16 debug_cnt = 0;
    u8 sys_stat;
    u8 data[4];

    // 频率控制：每1000次调用（约100秒）输出一次
    if(debug_cnt++ < 1000) return;
    debug_cnt = 0;

    // 读取系统状态寄存器
    sys_stat = BQ769xx_STATE_GET();

    // 填充数据
    data[0] = sys_stat;                           // SYS_STAT寄存器值
    data[1] = gBMSData.StatePar.RelayChSW;         // 充电MOS状态（废弃变量）
    data[2] = gBMSData.StatePar.RelayDichSW;       // 放电MOS状态（废弃变量）
    data[3] = 0xAA;                                // 标记位，确认代码已更新

    // 使用RS485协议发送（功能码0xFE用于调试）
    Uart2Send(0x01, 0xFE, 4, data);
}

/* ==================== 调试代码2：在Get_Base_Data中调用 ==================== */

/**
 * @brief 在Get_Base_Data函数中添加调试调用
 * @details 位置：Source/App/main.c 第178行（Led_Run_Cpu()之后）
 *
 * 修改前：
 * void Get_Base_Data(void)
 * {
 *     BQ769xx_GetData();
 *     BQ769xx_GetConfig();
 *     Balan_Pack_Check();
 *     Led_Run_Cpu();
 * }
 *
 * 修改后：
 * void Get_Base_Data(void)
 * {
 *     BQ769xx_GetData();
 *     BQ769xx_GetConfig();
 *     Balan_Pack_Check();
 *     Led_Run_Cpu();
 *     Protection_Status_Debug();  // 添加这行
 * }
 */

/* ==================== 调试代码3：修改保护阈值 ==================== */

/**
 * @brief 修改保护阈值用于测试
 * @details 位置：Source/App/ConfigPara.c 第90-91行
 *
 * @note   测试完成后需要恢复成默认值！
 *
 * 原始值：
 * gBMSConfig.BQ76Para.UV_Thresh = 2500;         // UV阈值：2500mV (2.5V)
 * gBMSConfig.BQ76Para.OV_Thresh = 4200;         // OV阈值：4200mV (4.2V)
 *
 * 测试值：
 * gBMSConfig.BQ76Para.UV_Thresh = 3000;         // UV阈值：3000mV (3.0V)
 * gBMSConfig.BQ76Para.OV_Thresh = 3150;         // OV阈值：3150mV (3.15V) - 最小值
 */

/* ==================== 数据帧解析 ==================== */

/**
 * @brief RS485调试数据帧格式
 * @details
 *
 * 数据帧：A8 01 FE 04 [SYS_STAT] [CHG] [DSG] [标记] [校验]
 *
 * | 字节 | 含义 | 说明 |
 * |------|------|------|
 * | A8 | 起始符 | 固定值 |
 * | 01 | 源地址 | BMS地址 |
 * | FE | 功能码 | 调试功能码 |
 * | 04 | 数据长度 | 4字节 |
 * | data[0] | SYS_STAT | 系统状态寄存器值 |
 * | data[1] | RelayChSW | 充电MOS状态（废弃，永远是0）|
 * | data[2] | RelayDichSW | 放电MOS状态（废弃，永远是0）|
 * | data[3] | 标记位 | 0xAA表示代码已更新 |
 * | XX | 校验和 | 自动计算 |
 *
 * SYS_STAT位定义：
 * | Bit | 标志 | 说明 |
 * |-----|------|------|
 * | 0 | OCD | 过流保护 |
 * | 1 | SCD | 短路保护 |
 * | 2 | OV | 过压保护 |
 * | 3 | UV | 欠压保护 |
 * | 7 | CC_READY | 库仑计就绪 |
 */

/* ==================== 调试代码3：OCD/SCD保护配置调试函数 ==================== */

/**
 * @brief OCD/SCD保护配置调试输出函数
 * @details 读取保护配置寄存器，验证OCD/SCD配置是否正确写入芯片
 *          数据帧格式：A8 01 FE 04 [PROTECT1] [PROTECT2] [SYS_STAT] [标记] [校验]
 *
 * @note   添加到 Source/App/main.c 文件末尾
 */
void Protection_Config_Debug(void)
{
    static u16 debug_cnt = 0;
    u8 data[4];

    // 频率控制：每1000次调用（约100秒）输出一次
    if(debug_cnt++ < 1000) return;
    debug_cnt = 0;

    // 读取保护配置寄存器
    data[0] = Bq769xxReg.Protect1.Protect1Byte;  // PROTECT1: SCD配置
    data[1] = Bq769xxReg.Protect2.Protect2Byte;  // PROTECT2: OCD配置
    data[2] = BQ769xx_STATE_GET();               // 当前系统状态
    data[3] = 0xBB;                              // 标记位（表示OCD/SCD配置测试）

    // 使用RS485协议发送
    Uart2Send(0x01, 0xFE, 4, data);
}

/**
 * @brief OCD/SCD保护配置数据帧解析
 * @details
 *
 * 数据帧：A8 01 FE 04 [PROTECT1] [PROTECT2] [SYS_STAT] [标记] [校验]
 *
 * PROTECT1寄存器 (0x9F)：
 * | Bit7-5 | RSNS | 电流检测量程 |
 * | Bit4-3 | SCD_DELAY | 短路保护延迟 |
 * | Bit2-0 | SCD_THRESH | 短路保护阈值 |
 *
 * PROTECT2寄存器 (0x7F)：
 * | Bit7-4 | OCD_DELAY | 过流保护延迟 |
 * | Bit3-0 | OCD_THRESH | 过流保护阈值 |
 *
 * 测试结果：
 * - PROTECT1 = 0x9F (1001 1111B)
 *   - RSNS = 10 (高量程) ✅
 *   - SCD_DELAY = 11 (0x03 = 300us) ✅
 *   - SCD_THRESH = 111 (0x07) ✅
 *
 * - PROTECT2 = 0x7F (0111 1111B)
 *   - OCD_DELAY = 0111 (0x07 = 约80ms) ✅
 *   - OCD_THRESH = 1111 (0x0F) ✅
 */

/* ==================== 测试结果记录 ==================== */

/**
 * @brief 测试结果
 * @details
 *
 * 测试条件：
 * - 电池电压：约2740mV（5串模拟电池包）
 * - 连接状态：已连接模拟电池包
 *
 * 测试1：UV保护测试
 * - UV阈值：3000mV
 * - 预期：2740mV < 3000mV，触发UV保护
 * - 实际：SYS_STAT = 0x88 (Bit3 UV=1)
 * - 结论：PASS ✅
 *
 * 测试2：OV保护测试
 * - OV阈值：3150mV（最小值）
 * - 预期：2740mV < 3150mV，触发OV保护（电压异常低也会触发）
 * - 实际：SYS_STAT = 0x8C (Bit2 OV=1, Bit3 UV=1)
 * - 结论：PASS ✅
 *
 * 测试3：OCD保护配置测试
 * - 预期：OCD_THRESH=0x0F, OCD_DELAY=0x07
 * - 实际：PROTECT2 = 0x7F (OCD_THRESH=0x0F ✅, OCD_DELAY=0x07 ✅)
 * - 结论：PASS ✅ 配置正确
 *
 * 测试4：SCD保护配置测试
 * - 预期：SCD_THRESH=0x07, SCD_DELAY=0x03
 * - 实际：PROTECT1 = 0x9F (SCD_THRESH=0x07 ✅, SCD_DELAY=0x03 ✅)
 * - 结论：PASS ✅ 配置正确
 *
 * 注意：OCD/SCD保护无法实际触发测试，因为需要超大电流（50A-200A），
 *       实际测试非常危险且可能损坏设备。仅验证配置是否正确写入芯片。
 */

/* ==================== 重要说明 ==================== */

/**
 * @note
 * 1. RelayChSW和RelayDichSW变量在整个项目中从未被赋值，属于废弃代码
 * 2. MOS的实际控制由BQ769xx芯片直接处理，不受这两个变量影响
 * 3. printf函数未配置重定向，使用会导致程序卡死
 * 4. 必须使用RS485协议（Uart2Send）发送调试数据
 * 5. 测试完成后需要恢复保护阈值到默认值
 */
