# SLMC607 BMS 快速参考

> ⚡ **快速查询手册** - 常见问题一键直达
>
> 需要深度分析？查看 [**research.md**](../../research.md) 完整技术研究报告

---

## 📊 深度分析索引

| 专题 | research.md 章节 |
|------|------------------|
| BQ769xx 驱动原理 | [第4章](../../research.md#4-bq76920-驱动深度分析) |
| I2C 通信协议 | [第5章](../../research.md#5-i2c-通信协议实现) |
| 均衡控制算法 | [第6章](../../research.md#6-均衡控制算法) |
| 状态机架构 | [第7章](../../research.md#7-状态机架构设计) |
| 数据采集流程 | [第8章](../../research.md#8-数据采集流程) |
| 保护机制 | [第9章](../../research.md#9-保护机制) |

---

## 常见问题速查

### Q1: 如何修改电池串数？
**文件**: `ConfigPara.c:36`
```c
gBMSConfig.Type.CellNum_Ser = 5;  // 改为3/4/5
```

### Q2: 如何调整过压/欠压阈值？
**文件**: `ConfigPara.c:53-54`
```c
gBMSConfig.BQ76Para.UV_Thresh = 2500;  // 欠压 2.5V
gBMSConfig.BQ76Para.OV_Thresh = 4200;  // 过压 4.2V
```

### Q3: 如何修改均衡策略？
**文件**: `ConfigPara.c:42-45`
```c
gBMSConfig.Balan.balanc_start_volt = 3000;    // 启动电压
gBMSConfig.Balan.balanc_diffe_volt = 100;     // 压差阈值
gBMSConfig.Balan.balanc_number_max = 3;       // 均衡串数
```
> 💡 深度分析：[第6章 - 均衡控制算法](../../research.md#6-均衡控制算法)

### Q4: 如何控制充放电MOS？
**文件**: `bq769xx.c:455-533`
```c
BQ769xx_CHGSET(1);  // 充电MOS开
BQ769xx_CHGSET(0);  // 充电MOS关
BQ769xx_DSGSET(1);  // 放电MOS开
BQ769xx_DSGSET(0);  // 放电MOS关
```
> 💡 深度分析：[第7章 - MOSFET控制实现](../../research.md#74-mosfet-控制实现)

### Q5: 系统模式如何判断？
**文件**: `TaskFun.c:40-74`
```
电流 ≥ 20mA   → 充电模式
电流 ≤ -20mA  → 放电模式
|电流| < 20mA → 待机模式
待机2小时    → 睡眠模式
```
> 💡 深度分析：[第7章 - 状态机架构设计](../../research.md#7-状态机架构设计)

### Q6: 如何重新初始化Flash参数？
**文件**: `main.c:80-87`
```c
FLASH_Update_Data();  // 擦除Flash并写入默认值
```

---

## 关键代码位置速查

| 功能 | 文件 | 行号 | 深度分析 |
|------|------|------|----------|
| 主循环 | main.c | 57-71 | [第3章](../../research.md#32-主程序架构时间片轮询) |
| 系统模式状态机 | TaskFun.c | 40-74 | [第7章](../../research.md#71-三层状态机架构) |
| 均衡状态机 | bq769xx.c | 841-984 | [第6章](../../research.md#62-均衡状态机) |
| MOS控制 | bq769xx.c | 455-533 | [第7.4节](../../research.md#74-mosfet-控制实现) |
| 参数初始化 | ConfigPara.c | 30-57 | [第10章](../../research.md#10-配置与参数管理) |
| 电压采集 | bq769xx.c | 327-414 | [第8章](../../research.md#8-数据采集流程) |
| 保护配置 | bq769xx.c | 113-176 | [第9章](../../research.md#9-保护机制) |
| I2C通信 | iic.c | 全文 | [第5章](../../research.md#5-i2c-通信协议实现) |
| 寄存器定义 | bq769xx.h | 全文 | [第4.2节](../../research.md#42-寄存器映射) |

---

## 常用配置模板

### 3串动力电池 (2Ah)
```c
CellNum_Ser = 3;
CapaRate = 2000;
ShuntSpec = 5000;
UV_Thresh = 2700; OV_Thresh = 4250;
SCD_Thresh = 0x07; OCD_Thresh = 0x0F;
```

### 5串储能电池 (10Ah)
```c
CellNum_Ser = 5;
CapaRate = 10000;
ShuntSpec = 5000;
UV_Thresh = 2500; OV_Thresh = 4200;
SCD_Thresh = 0x04; OCD_Thresh = 0x0A;
```

---

## 调试技巧

### 1. 查看系统状态
```c
// 在main.c的while循环中添加
printf("Mode=%d, Volt=%d, Curr=%d\n",
    gBMSData.Sys_Mod.sys_mode,
    gBMSData.BattPar.VoltLine,
    gBMSData.BattPar.CurrLine);
```

### 2. 查看电芯电压
```c
// bq769xx.c:256之后
for(i=0; i<gBMSConfig.Type.CellNum_Ser; i++) {
    printf("Cell%d=%d ", i+1, gBMSData.BattPar.VoltCell[i]);
}
```

### 3. 查看均衡状态
```c
// 在均衡状态机中
printf("BalState=%d, BalSw=%x\n",
    gBMSData.BalaPar.bala_state,
    gBMSData.BalaPar.bala_ctrl_sw);
```

---

## BQ769xx 寄存器速查

| 地址 | 名称 | 说明 |
|------|------|------|
| 0x00 | SYS_STAT | 状态告警 |
| 0x01-03 | CELLBAL | 均衡控制 |
| 0x05 | SYS_CTRL2 | CHG/DSG |
| 0x06-08 | PROTECT | 保护配置 |
| 0x09-0A | OV/UV_TRIP | 阈值 |
| 0x0C+ | VC1+ | 电芯电压 |
| 0x38+ | CC | 电流 |

---

## 常见问题排查

| 问题 | 可能原因 | 排查方法 |
|------|----------|----------|
| 充电器接上不充电 | CHG MOS未打开 | 检查CHGSET调用 |
| 电压读数为0 | I2C通信失败 | 检查IIC_ReadByte返回值 |
| 均衡不启动 | 压差不够/电压低 | 检查balanc参数 |
| MOS无法关闭 | 保护未清除 | 调用STAT_CLEAR |
| 参数未保存 | Flash错误 | 检查CRC校验 |

---

## 移植到其他芯片

### 移植到 BQ76952
1. 修改I2C地址 (0x08 → 0x10)
2. 调整寄存器地址
3. 更新保护阈值计算公式
4. 添加BQ76952特有功能 (温度补偿等)
> 💡 参考分析：[第4章 - BQ76920驱动](../../research.md#4-bq76920-驱动深度分析)

### 移植到 LTC6813
1. 改用SPI通信（当前I2C实现：[第5章](../../research.md#5-i2c-通信协议实现)）
2. 重新实现电压采集
3. 适配被动均衡方式
4. 调整状态机时序

---

## 联系与支持
- 项目: SLMC607 V1.6
- 日期: 2022-06-12
- 公司: 上海芯联集成电路有限公司
