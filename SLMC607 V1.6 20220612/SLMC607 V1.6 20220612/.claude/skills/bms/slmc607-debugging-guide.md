# SLMC607 BMS 硬件调试实战指南

## 概述

本文档提供SLMC607 BMS项目的完整硬件调试指南，通过7天系统化调试计划，帮助工程师深入理解BMS工作原理并积累实战经验。

## 硬件需求

### 必需设备
- SLMC607 BMS板子
- ST-Link/J-Link 调试器（SWD接口）
- USB转TTL模块（RS485通信调试）

### 可选设备
- 万用表/示波器
- 逻辑分析仪
- 5串锂电池组

### 软件工具
- Keil MDK（编译/下载/调试）
- 串口调试助手
- IIC调试工具（可选）

---

## 第1天：环境搭建 + 点灯测试

### 目标
- 确认能正常下载程序
- 确认调试器能用
- 第一个"Hello World"——点灯

### 测试代码

```c
// 修改 led.c 中的 LED闪烁函数
void Led_Run_Cpu(void)
{
    static u16 led_cnt = 0;

    if(TaskTimePare.Tim500ms_flag == 1)
    {
        if(++led_cnt > 10)  // 改成快速闪烁
        {
            led_cnt = 0;
            LED1_TOGGLE();   // 切换LED状态
        }
    }
}
```

### 验证清单
- [ ] 程序能下载
- [ ] LED按预期闪烁
- [ ] 能设置断点调试

---

## 第2天：验证时间片轮询架构

### 目标
- 用调试器观察时间片标志
- 理解200ms、500ms时间片的工作方式

### 调试断点位置

```c
// 在 main.c 主循环中设置断点
while(1)
{
    Systim_Time_Run();

    // 【断点1】在这里设置断点，观察每次循环的时间间隔
    // 应该是约1ms（定时器中断周期）

    Sys_Rece_Data();
    Sys_Send_Data();
    Get_Base_Data();
    Mix_Function_Pro();
    Clear_flag();
}
```

### 观察记录表

| 观察项 | 预期结果 | 实际结果 |
|--------|----------|----------|
| 1ms定时器 | 每1ms进入一次主循环 | |
| 200ms标志 | 每200ms=1次 | |
| 500ms标志 | 每500ms=1次 | |

### 实战练习
**任务**：修改定时器配置，把200ms改成100ms，观察LED闪烁速度变化

---

## 第3天：验证系统模式状态机

### 目标
- 观察系统模式切换
- 验证电流阈值判断逻辑

### 调试代码

```c
// 在 TaskFun.c 的 Sys_Mode_Charge() 中添加观测点
void Sys_Mode_Charge(void)
{
    // 添加：观测电流和模式
    volatile u16 current_current = gBMSData.BattPar.CurrLine;
    volatile u8 current_mode = gBMSData.Sys_Mod.sys_mode;

    if(TaskTimePare.Tim200ms_flag != 1)
        return;

    // 【断点】在这里观察current_current的值
    // 充电时应该 > 20mA
    // 放电时应该 < -20mA
    // 静置时应该在 -20mA ~ 20mA之间

    if((gBMSData.BattPar.CurrLine < 20) &&
       (gBMSData.BattPar.CurrLine > -20))
    {
        gBMSData.Sys_Mod.sys_mode = SYS_MODE_STANDBY;
    }
    // ...
}
```

### 实验方法

1. **只接电池（无充放电）**
   - 观察：系统应该进入 STANDBY 模式
   - 验证：电流应该在 ±20mA 之间

2. **接入充电器**
   - 观察：系统应该进入 CHARGE 模式
   - 验证：电流应该 ≥ 20mA

3. **接入负载**
   - 观察：系统应该进入 DISCHARGE 模式
   - 验证：电流应该 ≤ -20mA

---

## 第4天：验证均衡功能

### 目标
- 触发均衡条件
- 观察均衡状态机运行
- 验证均衡寄存器写入

### 临时修改配置

```c
// 临时修改配置参数，强制触发均衡
void InitPara0(void)
{
    // ... 其他配置 ...

    // 【实验修改】降低均衡阈值，方便触发
    gBMSConfig.Balan.balanc_start_volt = 1000;  // 改成1V（原3V）
    gBMSConfig.Balan.balanc_diffe_volt = 10;    // 改成10mV（原100mV）
    gBMSConfig.Balan.balanc_number_max = 1;      // 只均衡1串（原3串）
}
```

### IIC观测

```c
// 在 balance_write_regs() 函数中添加观测
u8 balance_write_regs(u8 cellbalanbyte1, u8 cellbalanbyte2, u8 cellbalanbyte3)
{
    // 【断点1】观察写入的值
    printf("写入均衡: CellBal1=0x%02X\n", cellbalanbyte1);

    rcd = IIC_WritByteMore(BQ769xxAddr, CELLBAL1_RegAddr, write_regs, 3);

    // 【断点2】回读验证
    rcd = IIC_ReadByteMore(BQ769xxAddr, CELLBAL1_RegAddr, read_regs, 3);
    printf("回读均衡: CellBal1=0x%02X\n", read_regs[0]);
}
```

### 实验任务
用万用表测量均衡开启时，对应电池的电压是否下降（约50-100mV）

---

## 第5天：验证RS485通信

### 目标
- 抓取RS485通信数据
- 验证协议帧格式
- 测试远程控制命令

### 硬件连接
```
PC ←[USB转TTL]→ RS485收发器 ←[RS485总线]→ BMS板
```

### 串口助手设置
```
波特率：115200
数据位：8
停止位：1
校验位：None
```

### 通信测试代码

```c
// 修改测试代码，周期性发送数据
void Rs485_Send_Test(void)
{
    u8 DataBuff[3] = {0xAA, 0xBB, 0xCC};  // 特征数据

    if(TaskTimePare.Tim500ms_flag == 1)
    {
        Uart2Send(RS485_BMS_ID, RS485_FUN_COMM1, sizeof(DataBuff), DataBuff);
    }
}
```

### 查询命令格式
上位机发送查询BMS数据：
```
帧格式：[设备地址][功能码][数据长度][数据][CRC]
示例：[0x11][0x02][0x00][CRC]
```

BMS应回复46字节数据帧。

---

## 第6天：验证保护机制

### ⚠️ 安全警告
过压/欠压测试需要小心，可能损坏电池！

### 安全测试方法

```c
// 临时修改保护阈值（软件层保护）
void InitPara0(void)
{
    // 【实验修改】降低保护阈值，方便测试
    gBMSConfig.BQ76Para.OV_Thresh = 3500;  // 改成3.5V（原4.2V）
    gBMSConfig.BQ76Para.UV_Thresh = 3000;  // 改成3.0V（原2.5V）

    // 注意：这只是软件阈值，硬件BQ769xx有自己的保护
}
```

### 观测保护状态

```c
// 在 BQ769xx_GetConfig() 中添加观测
void BQ769xx_GetConfig(void)
{
    // ...
    rcd = IIC_ReadByteMore(..., BQ769xxGatherConfig, ...);

    // 【断点】观察SYS_STAT寄存器
    printf("SYS_STAT: 0x%02X\n", Bq769xxReg.SysStatus.StatusByte);

    // SYS_STAT位定义：
    // Bit0: OCD  - 过流标志
    // Bit1: SCD  - 短路标志
    // Bit2: OV   - 过压标志
    // Bit3: UV   - 欠压标志
}
```

---

## 第7天：综合实验

### 综合实验：完整充放电测试

```
测试场景：完整充放电循环

步骤：
1. 接入放电负载 → 观察DISCHARGE模式
2. 等待SOC降到50% → 观察SOC计算
3. 断开负载 → 观察STANDBY模式
4. 接入充电器 → 观察CHARGE模式
5. 等待SOC回到100% → 观察满电检测
```

### 数据记录表

| 时间 | 模式 | 电流(mA) | 电压(mV) | SOC | 温度(℃) |
|------|------|-----------|----------|-----|----------|
| 0min | STANDBY | 0 | 16500 | 50% | 25 |
| 5min | | | | | |
| 10min | | | | | |

---

## 调试技巧总结

### 1. 断点调试法
```c
void example_function(void)
{
    u16 value = gBMSData.BattPar.VoltCell[0];
    value += 100;  // ← 在这里设断点
    gBMSData.BattPar.VoltCell[0] = value;
}
```

### 2. printf调试法
```c
printf("[DEBUG] Time=%dms, Mode=%d, Current=%dmA\n",
       TaskTimePare.TickCnt,
       gBMSData.Sys_Mod.sys_mode,
       gBMSData.BattPar.CurrLine);
```

### 3. IIC数据监控
```c
printf("[IIC] Write: Addr=0x%02X, Data=0x%02X\n", reg_addr, data);
printf("[IIC] Read:  Addr=0x%02X, Data=0x%02X\n", reg_addr, data);
```

### 4. 状态机状态追踪
```c
switch(stup_balan)
{
case PCB_SEQ_INIT:
    printf("[BALA] INIT → WAIT\n");
    stup_balan = PCB_SEQ_WAIT;
    break;
}
```

---

## 调试前准备检查表

### 硬件准备
- [ ] ST-Link/J-Link连接正常
- [ ] RS485转USB连接正常
- [ ] 电池组连接正常（注意正负极！）
- [ ] 万用表准备好

### 软件准备
- [ ] Keil MDK能正常编译
- [ ] 能正常下载程序到板子
- [ ] 能设置断点和单步调试
- [ ] 串口助手准备就绪

### 文档准备
- [ ] BQ769xx芯片手册
- [ ] 板子原理图
- [ ] 本次调试笔记本

---

## 学习成果

完成7天调试后，你将：

| 天数 | 内容 | 收获 |
|------|------|------|
| Day 1 | 点灯测试 | 熟悉调试环境 |
| Day 2 | 时间片验证 | 理解轮询架构 |
| Day 3 | 状态机验证 | 理解模式切换 |
| Day 4 | 均衡验证 | 理解均衡逻辑 |
| Day 5 | 通信验证 | 理解协议栈 |
| Day 6 | 保护验证 | 理解安全机制 |
| Day 7 | 综合实验 | 完整系统理解 |

完成后，实际调试能力将提升到高级工程师水平！
