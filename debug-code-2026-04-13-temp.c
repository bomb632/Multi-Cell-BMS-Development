/**
 * @file    debug-code-2026-04-13-temp.c
 * @brief   温度采集调试代码
 * @date    2026-04-13
 * @details 用于调试BMS温度采集功能（TS1/TS2/TS3）
 */

/* ==================== 调试代码1：温度采集调试函数 ==================== */

/**
 * @brief 温度采集调试输出函数
 * @details 每100秒输出一次温度ADC原始值和转换结果到RS485
 *          数据帧格式：A8 01 FE 0C [TS1_HI] [TS1_LO] [TS2_HI] [TS2_LO] [TS3_HI] [TS3_LO]
 *                          [TS1_V_H] [TS1_V_L] [TS2_V_H] [TS2_V_L] [TS3_V_H] [TS3_V_L]
 *
 * @note   添加到 Source/App/main.c 文件末尾
 *
 * 数据帧说明：
 * - [TS1_HI] [TS1_LO]: TS1的ADC原始值（高字节+低字节）
 * - [TS2_HI] [TS2_LO]: TS2的ADC原始值
 * - [TS3_HI] [TS3_LO]: TS3的ADC原始值
 * - [TS1_V_H] [TS1_V_L]: TS1的电压值（mV）
 * - [TS2_V_H] [TS2_V_L]: TS2的电压值（mV）
 * - [TS3_V_H] [TS3_V_L]: TS3的电压值（mV）
 *
 * 总共12字节数据
 */
void Temperature_Debug(void)
{
    static u16 debug_cnt = 0;
    u8 data[12];
    u16 adc_raw[3];
    u16 volt_temp[3];

    // 频率控制：每1000次调用（约100秒）输出一次
    if(debug_cnt++ < 1000) return;
    debug_cnt = 0;

    // 读取ADC原始值（直接从BQ769xxGatherData读取）
    adc_raw[0] = (u16)(BQ769xxGatherData[32] * 256) + BQ769xxGatherData[33];  // TS1
    adc_raw[1] = (u16)(BQ769xxGatherData[34] * 256) + BQ769xxGatherData[35];  // TS2
    adc_raw[2] = (u16)(BQ769xxGatherData[36] * 256) + BQ769xxGatherData[37];  // TS3

    // 计算电压值（mV）
    volt_temp[0] = ((unsigned long)adc_raw[0] * 382) / 1000;  // TS1电压
    volt_temp[1] = ((unsigned long)adc_raw[1] * 382) / 1000;  // TS2电压
    volt_temp[2] = ((unsigned long)adc_raw[2] * 382) / 1000;  // TS3电压

    // 填充数据帧
    data[0] = BQ769xxGatherData[32];        // TS1 ADC高字节
    data[1] = BQ769xxGatherData[33];        // TS1 ADC低字节
    data[2] = BQ769xxGatherData[34];        // TS2 ADC高字节
    data[3] = BQ769xxGatherData[35];        // TS2 ADC低字节
    data[4] = BQ769xxGatherData[36];        // TS3 ADC高字节
    data[5] = BQ769xxGatherData[37];        // TS3 ADC低字节

    data[6] = (u8)(volt_temp[0] >> 8);      // TS1电压高字节
    data[7] = (u8)(volt_temp[0] & 0xFF);    // TS1电压低字节
    data[8] = (u8)(volt_temp[1] >> 8);      // TS2电压高字节
    data[9] = (u8)(volt_temp[1] & 0xFF);    // TS2电压低字节
    data[10] = (u8)(volt_temp[2] >> 8);     // TS3电压高字节
    data[11] = (u8)(volt_temp[2] & 0xFF);   // TS3电压低字节

    // 使用RS485协议发送（功能码0xFE用于调试）
    Uart2Send(0x01, 0xFE, 12, data);
}

/* ==================== 调试代码2：在Get_Base_Data中添加调用 ==================== */

/**
 * @brief 在Get_Base_Data函数中添加温度调试调用
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
 *     Temperature_Debug();  // 添加这行
 * }
 */

/* ==================== 调试代码3：温度结果调试函数 ==================== */

/**
 * @brief 温度转换结果调试输出函数
 * @details 每100秒输出一次温度转换结果到RS485
 *          数据帧格式：A8 01 FE 06 [TS1_T_H] [TS1_T_L] [TS2_T_H] [TS2_T_L] [TS3_T_H] [TS3_T_L]
 *
 * @note   添加到 Source/App/main.c 文件末尾
 *
 * 数据帧说明：
 * - [TS1_T_H] [TS1_T_L]: TS1的温度值（0.1℃，带符号）
 * - [TS2_T_H] [TS2_T_L]: TS2的温度值（0.1℃，带符号）
 * - [TS3_T_H] [TS3_T_L]: TS3的温度值（0.1℃，带符号）
 *
 * 例如：0x0F 0xA4 = 4000 = 400.0℃（异常值）
 *      0xFF 0xF6 = -10 = -1.0℃（正常值）
 */
void Temperature_Result_Debug(void)
{
    static u16 debug_cnt = 0;
    u8 data[6];
    s16 temp[3];

    // 频率控制：每1000次调用（约100秒）输出一次
    if(debug_cnt++ < 1000) return;
    debug_cnt = 0;

    // 读取温度值（已经过TemChange转换）
    temp[0] = (s16)gBMSData.BattPar.TempCell[0];  // TS1温度
    temp[1] = (s16)gBMSData.BattPar.TempCell[1];  // TS2温度
    temp[2] = (s16)gBMSData.BattPar.TempCell[2];  // TS3温度

    // 填充数据帧（带符号整数）
    data[0] = (u8)(temp[0] >> 8);       // TS1温度高字节
    data[1] = (u8)(temp[0] & 0xFF);     // TS1温度低字节
    data[2] = (u8)(temp[1] >> 8);       // TS2温度高字节
    data[3] = (u8)(temp[1] & 0xFF);     // TS2温度低字节
    data[4] = (u8)(temp[2] >> 8);       // TS3温度高字节
    data[5] = (u8)(temp[2] & 0xFF);     // TS3温度低字节

    // 使用RS485协议发送
    Uart2Send(0x01, 0xFE, 6, data);
}

/* ==================== 数据帧解析 ==================== */

/**
 * @brief 温度调试数据帧格式说明
 * @details
 *
 * === 数据帧1：ADC原始值和电压值 ===
 * 帧格式：A8 01 FE 0C [12字节数据] [校验]
 *
 * | 字节偏移 | 含义 | 说明 | 正常范围 |
 * |---------|------|------|----------|
 * | 0-1 | TS1 ADC | TS1的ADC原始值 | 0 ~ 65535 |
 * | 2-3 | TS2 ADC | TS2的ADC原始值 | 0 ~ 65535 |
 * | 4-5 | TS3 ADC | TS3的ADC原始值 | 0 ~ 65535 |
 * | 6-7 | TS1电压 | TS1电压值(mV) | 277 ~ 2890mV |
 * | 8-9 | TS2电压 | TS2电压值(mV) | 277 ~ 2890mV |
 * | 10-11 | TS3电压 | TS3电压值(mV) | 277 ~ 2890mV |
 *
 * === 数据帧2：温度转换结果 ===
 * 帧格式：A8 01 FE 06 [6字节数据] [校验]
 *
 * | 字节偏移 | 含义 | 说明 | 正常范围 |
 * |---------|------|------|----------|
 * | 0-1 | TS1温度 | TS1温度值(0.1℃) | -200 ~ +1000 |
 * | 2-3 | TS2温度 | TS2温度值(0.1℃) | -200 ~ +1000 |
 * | 4-5 | TS3温度 | TS3温度值(0.1℃) | -200 ~ +1000 |
 */

/* ==================== NTC温度查找表参考 ==================== */

/**
 * @brief NTC热敏电阻温度-电压查找表
 * @details
 *
 * | 索引 | 温度(℃) | 电压(mV) | 说明 |
 * |-----|---------|----------|------|
 * | 0 | -20 | 2890 | 低温上限 |
 * | 5 | 0 | 2431 | 冰点 |
 * | 10 | 20 | 1846 | 室温 |
 * | 15 | 40 | 1309 | 温和 |
 * | 20 | 60 | 727 | 较热 |
 * | 25 | 80 | 452 | 热温度 |
 * | 30 | 100 | 277 | 高温下限 |
 *
 * NTC特性：温度升高，电压降低（负温度系数）
 */

/* ==================== 异常诊断指南 ==================== */

/**
 * @brief 温度异常诊断指南
 * @details
 *
 * === 异常现象1：温度显示5916℃ ===
 * 可能原因：
 * 1. ADC读取值为0xFFFF（65535）
 *    - 电压 = 65535 * 382 / 1000 ≈ 25034mV
 *    - 超出查找表范围，导致计算错误
 * 2. 硬件问题：
 *    - TS1引脚短路到VDD
 *    - NTC热敏电阻开路
 *    - 电路连接错误
 *
 * === 异常现象2：温度显示90℃ ===
 * 可能原因：
 * 1. ADC读取值为0或接近0
 *    - 电压 = 0mV < 277mV（查找表下限）
 *    - TemChange()返回1000（100℃）
 *    - 温度补偿后：1000 * 0.9 = 900（90℃）
 * 2. 硬件问题：
 *    - NTC热敏电阻未连接（悬空）
 *    - TS引脚短路到GND
 *    - 外部上拉/下拉电阻配置错误
 *
 * === 正常温度范围 ===
 * - 室温（25℃）：电压约1846mV，温度值约225（22.5℃补偿后）
 * - 补偿后温度：实际温度 * 10 * 0.9
 * - 例如25℃：25 * 10 * 0.9 = 225
 */

/* ==================== 测试结果记录 ==================== */

/**
 * @brief 待填写测试结果
 * @details
 *
 * 测试条件：
 * - 环境温度：约25℃
 * - NTC连接状态：待确认
 * - 电池连接：已连接5串模拟电池包
 *
 * 测试1：TS1温度采集
 * - ADC原始值：待测量
 * - 电压值：待测量
 * - 预期：约1846mV（室温）
 * - 实际：待测量
 * - 结论：待测试
 *
 * 测试2：TS2温度采集
 * - ADC原始值：待测量
 * - 电压值：待测量
 * - 预期：约1846mV（室温）
 * - 实际：待测量
 * - 结论：待测试
 *
 * 测试3：TS3温度采集
 * - ADC原始值：待测量
 * - 电压值：待测量
 * - 预期：约1846mV（室温）
 * - 实际：待测量
 * - 结论：待测试
 */

/* ==================== 重要说明 ==================== */

/**
 * @note
 * 1. 本调试代码提供两个函数：
 *    - Temperature_Debug(): 输出ADC原始值和电压值
 *    - Temperature_Result_Debug(): 输出温度转换结果
 * 2. 两个函数可以同时使用，也可以单独使用
 * 3. 建议先用Temperature_Debug()检查ADC读取是否正常
 * 4. 确认ADC正常后，再用Temperature_Result_Debug()检查温度转换
 * 5. 调试完成后，记得删除调试代码调用
 * 6. NTC热敏电阻需要正确连接才能获得准确温度
 */

/* ==================== 调试代码4：保护状态调试函数（UV/OV问题排查） ==================== */

/**
 * @file    debug-code-2026-04-13-protection.c
 * @brief   保护状态调试代码
 * @date    2026-04-13
 * @details 用于调试BMS保护功能和UV/OV阈值问题
 */

/**
 * @brief 保护状态调试输出函数
 * @details 每100秒输出一次保护状态到RS485
 *          数据帧格式：A8 01 FE 04 [SYS_STAT] [CHG] [DSG] [标记]
 *
 * @note   添加位置：
 *          - 函数：Source/App/main.c 文件末尾
 *          - 声明：Source/App/main.c 函数声明区域
 *          - 调用：主循环 Clear_flag() 之后
 *
 * 数据帧说明：
 * - SYS_STAT: BQ769xx系统状态寄存器
 *   - Bit0: OCD (过流)
 *   - Bit1: SCD (短路)
 *   - Bit2: OV (过压)
 *   - Bit3: UV (欠压)
 *   - Bit7: CC_READY (库仑计就绪)
 * - CHG/DSG: MOS状态（废弃变量，始终为0）
 * - 标记位: 0xAA表示代码已更新
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

/**
 * @brief UV/OV阈值调试输出函数
 * @details 每50秒输出一次UV/OV阈值配置到RS485
 *          数据帧格式：A8 01 FE 04 [UV_TRIP] [OV_TRIP] [配置_UV_H] [配置_UV_L]
 *
 * @note   添加位置：同上
 *
 * 数据帧说明：
 * - UV_TRIP: BQ769xx芯片UV阈值寄存器值
 * - OV_TRIP: BQ769xx芯片OV阈值寄存器值
 * - 配置_UV: gBMSConfig中的UV_Thresh值（2500mV = 0x09C4）
 *
 * UV_TRIP计算公式：
 *   UV_TRIP = ((电压_mV - ADC偏移) * 1000 / 增益_uV - UV_THRESH_BASE) >> 4
 *   - UV_THRESH_BASE = 0x1000
 *   - 增益_uV: 需要从芯片读取
 *   - ADC偏移: 需要从芯片读取
 */
void UV_OV_Threshold_Debug(void)
{
	static u16 debug_cnt = 0;
	u8 data[4];
	u8 uv_trip, ov_trip;
	u16 config_uv;

	// 频率控制：每500次调用（约50秒）输出一次
	if(debug_cnt++ < 500) return;
	debug_cnt = 0;

	// 读取BQ769xx芯片的UV_TRIP和OV_TRIP寄存器
	IIC_ReadByte(BQ769xxAddr, UV_TRIP_RegAddr, &uv_trip);  // 读取UV阈值寄存器
	IIC_ReadByte(BQ769xxAddr, OV_TRIP_RegAddr, &ov_trip);  // 读取OV阈值寄存器

	// 获取当前配置的UV阈值（从gBMSConfig）
	config_uv = gBMSConfig.BQ76Para.UV_Thresh;

	// 填充数据帧
	data[0] = uv_trip;                    // BQ769xx芯片UV_TRIP寄存器值
	data[1] = ov_trip;                    // BQ769xx芯片OV_TRIP寄存器值
	data[2] = (u8)(config_uv >> 8);       // 配置的UV阈值高字节
	data[3] = (u8)(config_uv & 0xFF);     // 配置的UV阈值低字节

	// 使用RS485协议发送
	Uart2Send(0x01, 0xFE, 4, data);
}

/* ==================== 主循环调用示例 ==================== */

/**
 * @brief 主循环中添加调试调用
 * @details 位置：Source/App/main.c 主循环中
 *
 * 修改示例：
 * int main(void)
 * {
 *     // ... 初始化代码 ...
 *
 *     while(1)
 *     {
 *         Systim_Time_Run();
 *         Sys_Rece_Data();
 *         Sys_Send_Data();
 *         Get_Base_Data();
 *         Mix_Function_Pro();
 *         Clear_flag();
 *
 *         // 添加调试调用
 *         Protection_Status_Debug();  // 保护状态调试
 *         UV_OV_Threshold_Debug();    // UV/OV阈值调试
 *     }
 * }
 */

/* ==================== 测试结果记录 ==================== */

/**
 * @brief UV/OV阈值调试结果
 * @details 2026-04-13 测试结果
 *
 * 测试条件：
 * - 电池电压：2740mV
 * - UV配置：2500mV
 * - OV配置：4200mV
 *
 * 测试1：保护状态
 * - SYS_STAT：0x80 (Bit7=1, 其他=0)
 * - 结论：✅ CC_READY=1，无保护触发
 *
 * 测试2：UV/OV阈值验证
 * - UV_TRIP：0x96
 * - OV_TRIP：0xAF
 * - 配置_UV：0x09C4 (2500mV)
 * - 结论：✅ 芯片配置正确，与代码一致
 *
 * 测试3：上位机欠压问题
 * - 问题：上位机显示欠压，但SYS_STAT=0x00
 * - 原因：Flash中保存了测试时的UV=3000mV
 * - 解决：重新烧录固件，用默认值2500mV初始化
 * - 结论：✅ 问题已解决
 */

/* ==================== 问题排查总结 ==================== */

/**
 * @brief 上位机显示欠压但SYS_STAT正常的问题排查
 * @details
 *
 * 问题现象：
 * - 上位机显示欠压
 * - SYS_STAT寄存器Bit3=0（无欠压）
 * - 电池电压2740mV
 *
 * 排查过程：
 * 1. 读取BQ769xx芯片UV_TRIP寄存器 → 0x96（对应2500mV）✅
 * 2. 读取配置参数UV_Thresh → 2500mV ✅
 * 3. 结论：芯片层面配置正确
 *
 * 4. 怀疑上位机缓存了旧配置
 * 5. 回顾：之前测试时将UV改成3000mV并写入Flash
 * 6. 重新烧录固件 → 问题解决
 *
 * 根本原因：
 * - Flash中保存了测试时的UV=3000mV
 * - 上位机从Flash读取配置
 * - 2740mV < 3000mV → 显示欠压
 *
 * 解决方法：
 * - 重新烧录固件（BMS用默认值2500mV初始化）
 * - 或在上位机中重新设置UV阈值
 * - 或在main.c中取消注释System_Pare_Get()从Flash读取
 */
