# Keil+ST-Link 调试指南

> 从串口调试升级到专业调试方法
> 日期：2026-04-14

---

## 🎯 为什么用Keil调试器

| 对比项 | 串口调试 | Keil断点调试 |
|--------|----------|--------------|
| 修改代码 | 需要重新编译烧录 | ✅ 无需重新编译 |
| 查看变量 | printf手动添加 | ✅ Watch窗口实时查看 |
| 单步执行 | ❌ 不支持 | ✅ 逐行/逐函数执行 |
| 查看调用栈 | ❌ 不支持 | ✅ Call Stack窗口 |
| 查看内存 | ❌ 不支持 | ✅ Memory窗口 |
| 设置条件断点 | ❌ 不支持 | ✅ 条件触发 |
| 效率 | 低（每次烧录5-10秒） | 高（即时查看） |

---

## 🚀 快速开始

### 1. 进入调试模式

```
1. 打开项目：SLMC607.uvproj
2. 点击菜单：Debug → Start/Stop Debug Session (或按 Ctrl+F5)
3. 首次使用会自动检测ST-Link
```

**成功标志**：
- 左侧出现调试工具栏
- 底部显示 "ARM Debugger"
- 主窗口进入调试视图

---

### 2. 调试窗口布局

```
┌─────────────────────────────────────────────────────────┐
│  菜单栏: File Edit View Debug  Flash                  │
├──────────────┬──────────────────────────────────────────┤
│              │  代码编辑区                               │
│  Project     │  (可设置断点、单步执行)                    │
│              │                                          │
│  ┌────────┐ │                                          │
│  │Watch 1 │ │                                          │
│  │        │ │                                          │
│  └────────┘ │                                          │
│              │                                          │
│  ┌────────┐ │                                          │
│  │Call    │ │                                          │
│  │Stack   │ │                                          │
│  └────────┘ │                                          │
├──────────────┴──────────────────────────────────────────┤
│  Command窗口 (可以输入调试命令)                          │
└─────────────────────────────────────────────────────────┘
```

---

## 📌 断点调试（核心功能）

### 设置断点的方法

| 方法 | 操作 |
|------|------|
| **方法1** | 在代码行号左侧双击（红色圆点） |
| **方法2** | 右键代码行 → Insert Breakpoint |
| **方法3** | 光标定位后按 F9 |

### 断点类型

#### 1. 普通断点
```
双击设置后，程序运行到这里会自动暂停
适用：大多数调试场景
```

#### 2. 条件断点（高级）
```
设置步骤：
1. 右键断点 → Breakpoint Properties
2. 勾选 "Expression"
3. 输入条件，例如：
   - gBMSData.BattPar.CurrLine > 1000  (电流>1000mA时触发)
   - i == 100  (循环第100次时触发)
   - gBMSData.BattPar.VoltCell[0] > 4200  (电压>4.2V时触发)
```

**使用场景**：
- 只在特定条件时暂停（如充电电流>1A）
- 跳过前N次循环
- 捕获异常状态

---

## 🔍 Watch窗口（变量观察）

### 打开Watch窗口
```
菜单：View → Watch Windows → Watch 1
快捷键：通常自动显示在左下角
```

### 添加变量到Watch

| 方法 | 操作 |
|------|------|
| **方法1** | 选中变量名 → 拖动到Watch窗口 |
| **方法2** | Watch窗口底部空白处双击 → 输入变量名 |
| **方法3** | 右键变量 → Add to Watch Window |

### Watch窗口显示内容

```
Watch 1
┌─────────────────────────────────────────────────┐
│ Name        │ Value    │ Type                  │
├─────────────────────────────────────────────────┤
│ gBMSData    │ ...      │ struct BMS_Data_Type  │
│  .BattPar   │ ...      │ struct Batt_Par_Type  │
│   .CurrLine │ 0        │ int                   │
│   .VoltLine │ 13740    │ unsigned int          │
│ SOC         │ 10       │ unsigned char         │
└─────────────────────────────────────────────────┘
```

### 高级用法：查看结构体成员

```
输入结构体变量名，点击 "+" 展开：
gBMSData
├─ BattPar
│  ├─ CurrLine    (当前电流)
│  ├─ VoltLine    (总电压)
│  └─ VoltCell[0] (第1串电压)
├─ Sys_Mod
│  └─ sys_mode    (系统模式)
└─ StatePar
   ├─ CHG_PD      (充电MOS状态)
   └─ DSG_PD      (放电MOS状态)
```

---

## 🎮 调试控制

### 调试工具栏

```
┌──────────────────────────────────────────────┐
│ [Reset] [Run] [Stop] [Step] [StepOver]      │
│  RST     F5    ESC   F11      F10           │
└──────────────────────────────────────────────┘
```

| 按钮 | 快捷键 | 功能 | 使用场景 |
|------|--------|------|----------|
| **Reset** | | 复位程序 | 重新从头开始 |
| **Run** | F5 | 连续运行 | 运行到下一个断点 |
| **Stop** | ESC | 停止运行 | 暂停程序 |
| **Step** | F11 | 单步进入 | 进入函数内部 |
| **Step Over** | F10 | 单步跳过 | 不进入函数内部 |
| **Step Out** | Ctrl+F11 | 跳出函数 | 从当前函数跳出 |

### 单步调试技巧

```
示例代码：
void BQ769xx_GatherData(void)
{
    u8 stat = IIC_ReadByte(0x08, 0x00);  ← ← 光标在这里
    // ...

调试操作：
1. F10 (Step Over)  → 执行当前行，跳到下一行
2. F11 (Step Into)  → 进入IIC_ReadByte()函数内部
3. Ctrl+F11         → 从IIC_ReadByte()返回
```

---

## 🔧 实战调试案例

### 案例1：查看SOC计算过程

**目标**：观察SOC如何从OCV查表法计算

```c
// 文件：Source/App/soc.c
void batt_cap_ocvsoc_init(void)
{
    u16 min_volt = gBMSData.BattPar.VoltCell[min_index];  ← ← 断点1
    u8 soc = OCV_Find(min_volt);                          ← ← 断点2
    gBMSData.CapPar.SOC = soc;                            ← ← 断点3
}
```

**调试步骤**：
```
1. 在函数入口设置断点
2. Watch窗口添加：
   - gBMSData.BattPar.VoltCell[0~4]
   - min_volt
   - gBMSData.CapPar.SOC
3. F5运行到断点1
4. 查看Watch窗口，记录各串电压
5. F10单步执行到断点2
6. 查看OCV_Find()返回值
7. F10执行到断点3
8. 验证SOC是否正确写入
```

---

### 案例2：电流方向判断

**目标**：验证充电/放电电流方向是否正确

```c
// 文件：Source/App/TaskFun.c
void Task_System_Mode(void)
{
    s16 curr = gBMSData.BattPar.CurrLine;  ← ← 断点

    if(curr > 20)
    {
        gBMSData.Sys_Mod.sys_mode = CHARGE;  ← ← 断点
    }
    else if(curr < -20)
    {
        gBMSData.Sys_Mod.sys_mode = DISCHARGE;  ← ← 断点
    }
}
```

**调试步骤**：
```
1. 在curr读取处设置断点
2. Watch窗口添加：
   - gBMSData.BattPar.CurrLine
   - gBMSData.Sys_Mod.sys_mode
3. 连接充电器 → F5运行
4. 观察curr是否为正值（如+500mA）
5. F10单步，验证是否进入CHARGE分支
6. 断开充电器，连接负载 → F5运行
7. 观察curr是否为负值（如-800mA）
8. F10单步，验证是否进入DISCHARGE分支
```

---

### 案例3：温度值异常排查

**问题**：温度显示48℃，实际25℃

```c
// 文件：Source/Drive/adc.c
u16 Get_NTC_Temperature(u16 adc_value)
{
    // TemD查找表
    const u16 TemD[] = {...};  ← ← 断点1

    u16 temp = TemD[index];    ← ← 断点2
    temp = temp * 90 / 100;    ← ← 断点3 (0.9补偿)

    return temp;
}
```

**调试步骤**：
```
1. 在函数入口设置断点
2. Watch窗口添加：
   - adc_value (原始ADC值)
   - TemD[0~10] (查找表内容)
   - index (查表索引)
   - temp (中间计算值)
3. F5运行到断点1
4. 在Watch窗口查看adc_value = 4205
5. 单步到断点2，查看查表结果
6. 发现 TemD[index] = 53 (表示53℃)
7. 问题定位：查找表基于4.7kΩ上拉，实际硬件10kΩ
8. 解决：更换查找表
```

---

## 📊 Memory窗口（内存查看）

### 打开Memory窗口
```
菜单：View → Memory Windows → Memory 1
快捷键：无（手动打开）
```

### 查看变量内存地址

```
在Memory窗口输入：
1. 变量名：&gBMSData
2. 地址：0x20000000

显示：
0x20000000  XX XX XX XX XX XX XX XX  ...
             ↑ SOC ↑ 电流高字节 ↑ 电流低字节
```

**使用场景**：
- 检查内存越界
- 查看数组内容
- 验证DMA数据

---

## 🎯 逻辑分析（Logic Analyzer）

### 查看变量变化曲线

```
1. View → Analysis Windows → Logic Analyzer
2. 添加要跟踪的变量：
   - gBMSData.BattPar.CurrLine
   - gBMSData.BattPar.VoltCell[0]
3. 运行程序
4. 停止后显示变量随时间变化曲线
```

**使用场景**：
- 分析电流采样波形
- 观察SOC变化趋势
- 排查时序问题

---

## ⚡ 调试技巧总结

### 高效调试流程

```
1. 设置断点在可疑代码附近
2. Watch窗口添加关键变量
3. F5运行到断点
4. 检查Watch窗口变量值
5. F10单步执行，观察变化
6. 发现问题 → 修改代码 → F5重新运行
```

### 常用快捷键

| 快捷键 | 功能 |
|--------|------|
| F5 | 运行到断点 |
| F10 | 单步跳过 |
| F11 | 单步进入 |
| Ctrl+F5 | 开始/停止调试 |
| F9 | 设置/取消断点 |
| Ctrl+Shift+F5 | 重启调试 |
| ESC | 停止运行 |

---

## 🔥 实战练习建议

### 练习1：SOC初始化调试
```
目标：验证BMS启动时SOC初始化流程
断点：batt_cap_ocvsoc_init() 函数入口
Watch：gBMSData.CapPar.SOC, 各串电压
操作：F5 → 观察OCV查表 → 验证SOC值
```

### 练习2：保护功能调试
```
目标：观察UV保护触发过程
断点：BQ769xx_Protection_Detect() 函数
Watch：SYS_STAT寄存器, UV_TRIP阈值
操作：降低电池电压 → F5 → 观察保护触发
```

### 练习3：RS485通信调试
```
目标：验证数据发送流程
断点：Rs485_Send_Data() 函数
Watch：发送缓冲区 DataBuff[], 发送长度
操作：F5 → 单步查看发送过程
```

---

## 📝 调试记录模板

```markdown
### 日期：2026-04-14

#### 调试目标：[填写目标]

#### 断点设置：
- 文件：xxx.c 行号：xxx
- 条件：[如适用]

#### Watch窗口：
- 变量1：预期值 = 实际值
- 变量2：预期值 = 实际值

#### 调试过程：
1. F5运行到断点
2. 查看Watch窗口
3. F10单步执行
4. 发现问题：...

#### 结论：
- 问题原因：
- 解决方案：
```

---

## 🚨 常见问题

### Q1：无法进入调试模式
```
检查：
1. ST-Link是否连接
2. Keil设置：Debug → Use → ST-Link Debugger
3. 驱动是否安装
```

### Q2：断点不起作用
```
原因：代码优化导致

解决：
1. 临时关闭优化：
   Options → C/C++ → Optimization → Level (-O0)
2. 或在关键变量前加 volatile
```

### Q3：Watch显示 <not in scope>
```
原因：变量不在当前作用域

解决：
1. 单步执行到变量定义之后
2. 或查看全局变量（gBMSData等）
```

---

## 📚 下一步

1. **今天试试**：用断点调试SOC初始化
2. **明天试试**：用Watch观察电流方向
3. **后天试试**：用Logic Analyzer看波形

---

**记住**：
- 断点调试比串口调试快10倍
- Watch窗口能实时看变量
- 遇到问题先单步，不要瞎猜

**祝调试顺利！** 🚀
