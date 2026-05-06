/**
 * @file    debug-code-2026-04-13-soc.c
 * @brief   SOC算法调试代码
 * @date    2026-04-13
 * @details 验证SOC估算算法的三个部分：
 *          1. OCV查表法 - 初始SOC和静置校准
 *          2. 安时积分法 - 动态SOC计算
 *          3. 温度补偿 - 不同温度下的容量调整
 */

/* ==================== SOC调试函数 ==================== */

/**
 * @brief SOC算法调试输出
 * @details 输出SOC相关的所有关键变量
 *          用于验证SOC估算算法的正确性
 *
 * 添加位置：Source/App/main.c
 * 调用位置：Mix_Function_Pro() 函数中，500ms时间片调用
 *
 * 输出格式：RS485数据帧
 * 帧头：A8 0D FE 0E
 * 数据：14字节（7个u32变量）
 * 帧尾：AA + 校验
 */
void SOC_Algorithm_Debug(void)
{
    u8 DataBuff[20];
    u16 i;
    u16 checksum = 0;

    /* RS485数据帧格式 */
    DataBuff[0] = 0xA8;      // 帧头
    DataBuff[1] = 0x0D;      // 设备地址
    DataBuff[2] = 0xFE;      // 命令字
    DataBuff[3] = 0x0E;      // 数据长度（14字节）

    /* SOC关键数据 */
    // 1. SOC值 (0.1%)
    DataBuff[4] = (gBMSData.BattPar.SOCSys) >> 8;
    DataBuff[5] = gBMSData.BattPar.SOCSys & 0xFF;

    // 2. 额定容量 (u32, 单位：10mA/200ms)
    DataBuff[6] = (gBMSData.PackCap.pack_rated_cap >> 24) & 0xFF;
    DataBuff[7] = (gBMSData.PackCap.pack_rated_cap >> 16) & 0xFF;
    DataBuff[8] = (gBMSData.PackCap.pack_rated_cap >> 8) & 0xFF;
    DataBuff[9] = gBMSData.PackCap.pack_rated_cap & 0xFF;

    // 3. 实际容量 (u32, 温度补偿后)
    DataBuff[10] = (gBMSData.PackCap.pack_real_cap >> 24) & 0xFF;
    DataBuff[11] = (gBMSData.PackCap.pack_real_cap >> 16) & 0xFF;
    DataBuff[12] = (gBMSData.PackCap.pack_real_cap >> 8) & 0xFF;
    DataBuff[13] = gBMSData.PackCap.pack_real_cap & 0xFF;

    // 4. 剩余容量 (u32)
    DataBuff[14] = (gBMSData.PackCap.pack_rem_cap >> 24) & 0xFF;
    DataBuff[15] = (gBMSData.PackCap.pack_rem_cap >> 16) & 0xFF;
    DataBuff[16] = (gBMSData.PackCap.pack_rem_cap >> 8) & 0xFF;
    DataBuff[17] = gBMSData.PackCap.pack_rem_cap & 0xFF;

    // 5. 最低单体电压 (u16, mV) - 用于OCV查表
    DataBuff[18] = (gBMSData.BattPar.VoltCellMin) >> 8;
    DataBuff[19] = gBMSData.BattPar.VoltCellMin & 0xFF;

    /* 计算校验和（简单累加） */
    for(i = 4; i < 20; i++)
    {
        checksum += DataBuff[i];
    }

    /* 发送数据帧 */
    Uart2SendData(0xA8);
    for(i = 1; i < 20; i++)
    {
        Uart2SendData(DataBuff[i]);
    }
    Uart2SendData(0xAA);           // 帧尾
    Uart2SendData(checksum & 0xFF); // 校验和
}

/**
 * @brief OCV查表法调试
 * @details 手动测试OCV查表功能
 *          验证不同电压对应的SOC值是否正确
 *
 * 测试点：
 * - 4200mV (100% SOC)
 * - 3700mV (50% SOC)
 * - 3300mV (10% SOC)
 * - 3200mV (5% SOC)
 */
void OCV_Lookup_Table_Test(void)
{
    u16 test_voltages[] = {4200, 3700, 3500, 3300, 3200};
    u16 soc_values[5];
    u8 i;

    for(i = 0; i < 5; i++)
    {
        soc_values[i] = Seeka_OcvSoc_Table(test_voltages[i], OCV_Table);
    }

    // 输出测试结果
    // 格式：电压 -> SOC
    // 4200mV -> 1000 (100.0%)
    // 3700mV -> ~500 (50.0%)
    // 3500mV -> ~250 (25.0%)
    // 3300mV -> ~100 (10.0%)
    // 3200mV -> ~50 (5.0%)
}

/**
 * @brief 安时积分法验证
 * @details 验证充放电过程中SOC的动态变化
 *
 * 验证方法：
 * 1. 记录初始SOC和电流
 * 2. 等待一段时间（如1分钟）
 * 3. 检查SOC变化是否符合预期
 *
 * 预期结果：
 * - 充电：SOC增加
 * - 放电：SOC减少
 * - 待机：SOC基本不变
 */
void AH_Integration_Test(void)
{
    static u16 last_soc = 0;
    static i16 last_curr = 0;
    static u8 first_run = 1;

    u16 soc_delta;
    i16 current = gBMSData.BattPar.CurrLine;

    if(first_run)
    {
        last_soc = gBMSData.BattPar.SOCSys;
        last_curr = current;
        first_run = 0;
        return;
    }

    /* 计算SOC变化量 */
    soc_delta = gBMSData.BattPar.SOCSys - last_soc;

    /* 验证逻辑 */
    if(current > 20)  // 充电模式
    {
        if(soc_delta < 0)
        {
            // 异常：充电时SOC下降
            // 记录错误
        }
    }
    else if(current < -20)  // 放电模式
    {
        if(soc_delta > 0)
        {
            // 异常：放电时SOC上升
            // 记录错误
        }
    }

    last_soc = gBMSData.BattPar.SOCSys;
    last_curr = current;
}

/**
 * @brief 温度补偿验证
 * @details 验证不同温度下容量补偿是否正确
 *
 * 测试场景：
 * - 高温（≥25℃）：容量100%
 * - 常温（10~25℃）：容量100% ± 1.5%
 * - 低温（0~10℃）：容量降低约2.5%
 * - 低温（-10~0℃）：容量降低约5%
 * - 超低温（<-20℃）：容量75%（最小值）
 */
void Temperature_Compensation_Test(void)
{
    i16 temp = gBMSData.BattPar.TempPackMin;
    u32 rated_cap = gBMSData.PackCap.pack_rated_cap;
    u32 real_cap = gBMSData.PackCap.pack_real_cap;
    u16 compensation_rate;

    /* 计算实际补偿比例 */
    if(rated_cap > 0)
    {
        compensation_rate = (real_cap * 1000) / rated_cap;
    }
    else
    {
        compensation_rate = 0;
    }

    /* 验证补偿比例是否在合理范围 */
    if(temp >= 250)  // ≥25℃
    {
        // 预期：1000 (100%)
        // 实际：compensation_rate
    }
    else if(temp >= 0 && temp < 250)  // 0~25℃
    {
        // 预期：750 ~ 1000 (75% ~ 100%)
        if(compensation_rate < 750 || compensation_rate > 1000)
        {
            // 异常：超出合理范围
        }
    }
    else if(temp >= -200 && temp < 0)  // -20~0℃
    {
        // 预期：750 ~ 950 (75% ~ 95%)
        if(compensation_rate < 750 || compensation_rate > 950)
        {
            // 异常：超出合理范围
        }
    }
    else  // <-20℃
    {
        // 预期：750 (75%)
        if(compensation_rate < 750 || compensation_rate > 780)
        {
            // 异常：应该接近75%
        }
    }
}

/* ==================== 数据帧解析 ==================== */

/**
 * @brief SOC调试数据帧格式说明
 * @details
 * 数据帧：A8 0D FE 0E [14字节数据] AA [校验]
 *
 * | 字节索引 | 含义 | 数据类型 | 说明 |
 * |----------|------|----------|------|
 * | 0-3 | 帧头 | - | A8 0D FE 0E |
 * | 4-5 | SOC值 | u16 | 单位0.1%，例如 03E8 = 1000 = 100.0% |
 * | 6-9 | 额定容量 | u32 | 单位：10mA/200ms，需要换算成mAh |
 * | 10-13 | 实际容量 | u32 | 温度补偿后的实际可用容量 |
 * | 14-17 | 剩余容量 | u32 | 当前剩余的容量 |
 * | 18-19 | 最低电压 | u16 | 单位mV，用于OCV查表 |
 * | 20 | 帧尾 | - | AA |
 * | 21 | 校验和 | u8 | 简单累加校验 |
 *
 * 容量换算：
 * pack_rated_cap (10mA/200ms) -> mAh
 * 公式：mAh = (pack_rated_cap * 10) / (5 * 60 * 60 / 200)
 * 简化：mAh = pack_rated_cap / 90
 *
 * 示例：
 * pack_rated_cap = 36000000
 * mAh = 36000000 / 90 = 400000mAh = 400Ah
 *
 * SOC计算验证：
 * SOC = (pack_rem_cap * 1000) / pack_real_cap
 * 示例：
 * pack_rem_cap = 18000000
 * pack_real_cap = 36000000
 * SOC = (18000000 * 1000) / 36000000 = 500 (50.0%)
 */

/* ==================== 测试结果记录 ==================== */

/**
 * @brief SOC算法测试结果
 * @details
 *
 * ========== 测试1：OCV查表法 ==========
 *
 * 测试条件：
 * - 电池静置状态
 * - 最低电压：2740mV
 *
 * 预期结果：
 * - SOC应该在低电量范围（<20%）
 *
 * 实际结果：
 * - SOC = [待测量]
 * - 结论：[待验证]
 *
 * ========== 测试2：安时积分法 ==========
 *
 * 测试条件：
 * - [待定]
 *
 * 预期结果：
 * - [待定]
 *
 * 实际结果：
 * - [待测量]
 * - 结论：[待验证]
 *
 * ========== 测试3：温度补偿 ==========
 *
 * 测试条件：
 * - 当前温度：[待测量]
 *
 * 预期结果：
 * - 实际容量应该在额定容量的75%~105%之间
 *
 * 实际结果：
 * - 额定容量 = [待测量]
 * - 实际容量 = [待测量]
 * - 补偿比例 = [待测量]%
 * - 结论：[待验证]
 */

/* ==================== 重要说明 ==================== */

/**
 * @note 添加位置
 * 1. SOC_Algorithm_Debug() 添加到 main.c 的 Mix_Function_Pro() 中
 * 2. 调用条件：500ms时间片（if(TaskTimePare.Tim500ms_flag == 1)）
 *
 * @note 调试步骤
 * 1. 添加函数声明到 main.c 顶部
 * 2. 添加函数实现到 main.c 末尾
 * 3. 在 Mix_Function_Pro() 中调用
 * 4. 编译烧录
 * 5. 使用串口助手接收RS485数据
 * 6. 解析数据帧验证SOC算法
 *
 * @note 验证重点
 * 1. OCV查表：不同电压下SOC是否合理
 * 2. 安时积分：充放电时SOC变化方向是否正确
 * 3. 温度补偿：不同温度下容量变化是否合理
 * 4. SOC精度：SOC = (pack_rem_cap * 1000) / pack_real_cap
 */
