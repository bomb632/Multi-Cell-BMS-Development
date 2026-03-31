# SLMC607 BMS 项目深度研究报告

> 项目名称: SLMC607 BMS V1.6
> 研究日期: 2026-03-07
> 开发商: 上海芯联芯电子科技有限公司

---

## 目录

1. [项目概述](#1-项目概述)
2. [硬件架构](#2-硬件架构)
3. [软件架构](#3-软件架构)
4. [BQ76920 驱动深度分析](#4-bq76920-驱动深度分析)
5. [I2C 通信协议实现](#5-i2c-通信协议实现)
6. [均衡控制算法](#6-均衡控制算法)
7. [状态机架构设计](#7-状态机架构设计)
8. [数据采集流程](#8-数据采集流程)
9. [保护机制](#9-保护机制)
10. [配置与参数管理](#10-配置与参数管理)

---

## 1. 项目概述

### 1.1 项目简介

SLMC607 是一款基于 STM32F10x + BQ76920 架构的电池管理系统（BMS），支持 3-5 串锂电池组的智能管理。项目采用工业级设计，具备完整的电池保护、均衡控制和通信功能。

### 1.2 主要功能

| 功能模块 | 描述 |
|---------|------|
| 电压采集 | 支持 3-5 串电池电压独立采集，精度达 mV 级 |
| 电流监测 | 库仑计集成，支持充电/放电电流监测 |
| 温度监测 | 支持 3 路 NTC 温度传感器 |
| 均衡控制 | 智能选择高电压电池进行被动均衡 |
| 保护功能 | 过压(OV)/欠压(UV)/过流(OCD)/短路(SCD)保护 |
| 通信接口 | RS485 通信，波特率 115200 |
| 数据存储 | Flash 存储配置参数，掉电保存 |

### 1.3 技术规格

| 项目 | 规格 |
|------|------|
| MCU | STM32F10x (ARM Cortex-M3, 72MHz) |
| AFE 芯片 | TI BQ76920/BQ76940 |
| 电池串数 | 3-5 串可配置 |
| 电压采样精度 | ±5mV |
| 温度采样范围 | -40°C ~ +125°C |
| 通信协议 | 自定义 RS485 协议 |
| 开发环境 | Keil MDK (uVision) |

---

## 2. 硬件架构

### 2.1 系统框图

```
┌─────────────────────────────────────────────────────────────┐
│                      MCU (STM32F10x)                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │   GPIO   │  │   I2C    │  │   UART   │  │   ADC    │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘    │
│       │             │             │             │            │
└───────┼─────────────┼─────────────┼─────────────┼────────────┘
        │             │             │             │
        │         ┌───▼────┐       │         ┌───▼────┐
        │         │BQ76920 │       │         │NTC x3  │
        │         │  AFE   │       │         │温度传感器│
        │         └───┬────┘       │         └────────┘
        │             │            │
        │    ┌────────┼────────┐   │
        │    │        │        │   │
        │  Cell1   Cell2   Cell3-5  │
        │   电池    电池    电池组   │
        │                          │
        │                  ┌───────▼───────┐
        │                  │   RS485       │
        │                  │   收发器      │
        │                  └───────┬───────┘
        │                          │
        ▼                    ┌─────▼─────┐
    分流电阻                 │  上位机    │
                            └────────────┘
```

### 2.2 引脚映射

| 功能 | MCU 引脚 | 描述 |
|------|----------|------|
| I2C_SCL | PB6 | BQ76920 时钟线 |
| I2C_SDA | PB7 | BQ76920 数据线 |
| UART_TX | PA2 | RS485 发送 |
| UART_RX | PA3 | RS485 接收 |
| WAKE | GPIO | BQ76920 唤醒控制 |
| POWER_EN | GPIO | 系统电源控制 |

---

## 3. 软件架构

### 3.1 目录结构

```
SLMC607 V1.6 20220612/
├── Libraries/                           # STM32 HAL 库
│   ├── CMSIS/                          # ARM Cortex-M3 核心
│   └── STM32F10x_StdPeriph_Driver/     # STM32 外设驱动
├── Source/                             # 源代码
│   ├── App/                            # 应用层
│   │   ├── main.c                      # 主程序入口
│   │   ├── bq769xx.c/h                 # BQ76920 驱动
│   │   ├── TaskFun.c/h                 # 系统状态机
│   │   ├── ConfigPara.c/h              # 配置参数管理
│   │   ├── DataBase.h                  # 全局数据结构
│   │   ├── soc.c/h                     # SOC 算法
│   │   ├── Rs485Data.c/h               # RS485 通信
│   │   └── UartData.c/h                # UART 数据处理
│   └── Drive/                          # 驱动层
│       ├── iic.c/h                     # I2C 驱动
│       ├── gpio.c/h                    # GPIO 驱动
│       ├── timer.c/h                   # 定时器驱动
│       ├── adc.c/h                     # ADC 驱动
│       ├── usart.c/h                   # UART 驱动
│       ├── flash.c/h                   # Flash 驱动
│       └── wdt.c/h                     # 看门狗驱动
├── User/                               # 用户配置
└── OBJ/                                # 编译输出
```

### 3.2 主程序架构（时间片轮询）

```c
// main.c 主循环结构
while(1)
{
    Systim_Time_Run();      // 更新时间片标志
    Sys_Rece_Data();        // RS485 数据接收
    Sys_Send_Data();        // RS485 数据发送
    Get_Base_Data();        // 基础数据采集
    Mix_Function_Pro();     // 功能处理
    Clear_flag();           // 清除标志
}
```

### 3.3 时间片分配

| 时间片 | 周期 | 执行任务 |
|--------|------|----------|
| 1ms | 持续 | 基础计时 |
| 100ms | 100ms | MOS 开关控制、告警清除 |
| 200ms | 200ms | 数据采集、均衡控制 |
| 500ms | 500ms | 配置读取、唤醒处理 |
| 1s | 1000ms | SOC 更新、LED 指示 |

---

## 4. BQ76920 驱动深度分析

### 4.1 芯片概述

**BQ76920** 是德州仪器（TI）推出的模拟前端（AFE）芯片，专为 3-5 串锂电池组设计。

| 特性 | 描述 |
|------|------|
| 电池串数 | 3-5 串（BQ76920），3-10 串（BQ76940） |
| 通信接口 | I2C（地址 0x08） |
| ADC 精度 | 14 位，±5mV |
| 均衡方式 | 被动均衡（内部开关） |
| 保护功能 | 硬件级 OV/UV/OCD/SCD 保护 |
| 低功耗模式 | SHIP 模式（<1μA） |

### 4.2 寄存器映射

```c
// 系统状态寄存器
#define SYS_STAT_RegAddr   0x00    // 告警标志（OCD/SCD/OV/UV等）

// 均衡控制寄存器
#define CELLBAL1_RegAddr   0x01    // Cell 1-5 均衡控制
#define CELLBAL2_RegAddr   0x02    // Cell 6-10 均衡控制
#define CELLBAL3_RegAddr   0x03    // Cell 11-15 均衡控制

// 系统控制寄存器
#define SYS_CTRL1_RegAddr  0x04    // ADC 使能、温度选择
#define SYS_CTRL2_RegAddr  0x05    // MOS 控制、库仑计

// 保护寄存器
#define PROTECT1_RegAddr   0x06    // 短路保护（SCD）
#define PROTECT2_RegAddr   0x07    // 过流保护（OCD）
#define PROTECT3_RegAddr   0x08    // 过压/欠压延迟

// 阈值寄存器
#define OV_TRIP_RegAddr    0x09    // 过压阈值
#define UV_TRIP_RegAddr    0x0A    // 欠压阈值

// 数据采集寄存器
#define VC1_HI_RegAddr     0x0C    // 电池电压起始地址
```

### 4.3 驱动初始化流程

```
┌─────────────────────────────────────────────────────────────┐
│                     BQ769xx_Init()                         │
└─────────────────────────────────────────────────────────────┘
                          │
          ┌───────────────┼───────────────┐
          ▼               ▼               ▼
    ┌───────────┐  ┌───────────┐  ┌───────────────┐
    │ Set Cell  │  │ Init Para │  │ Read Gain     │
    │ Map       │  │ Structure │  │ & Offset      │
    └───────────┘  └───────────┘  └───────────────┘
                                              │
                                              ▼
                                    ┌─────────────────┐
                                    │ Write Config    │
                                    │ (with 5 retries)│
                                    └─────────────────┘
```

### 4.4 初始化代码分析

**文件位置**: `Source/App/bq769xx.c:247-276`

```c
void BQ769xx_Init(void)
{
    u8 rcd = 0;

    // 步骤1: 设置电池串数映射
    BQ769xx_Set_CellMap();

    // 步骤2: 初始化参数结构体
    BQ769xx_Init_Para();

    // 步骤3: 读取ADC增益和偏移
    BQ769xx_Read_Gain();

    // 步骤4: 写入配置到BQ769xx
    rcd = BQ769xx_Write_Config();

    // 判断初始化结果
    if(rcd == gRET_OK)
    {
        BQ769xx_Init_State = 1;
    }
    else
    {
        BQ769xx_Init_State = 0;
    }
}
```

### 4.5 电池串数映射

**文件位置**: `Source/App/bq769xx.c:86-107`

```c
void BQ769xx_Set_CellMap(void)
{
    switch(gBMSConfig.Type.CellNum_Ser)
    {
    case 5:  // 5串: 0x001F = 0001 1111B
        gBMSData.BattPar.Cells_Map = 0x001F;
        break;
    case 4:  // 4串: 0x0017 = 0001 0111B (跳过VC4)
        gBMSData.BattPar.Cells_Map = 0x0017;
        break;
    case 3:  // 3串: 0x0013 = 0001 0011B (跳过VC3,VC4)
        gBMSData.BattPar.Cells_Map = 0x0013;
        break;
    default:
        gBMSData.BattPar.Cells_Map = 0x001F;
        break;
    }
}
```

**映射说明**:
- `Cells_Map` 是一个 16 位掩码，对应 BQ76920 的 15 个物理通道
- 每个有效通道会参与实际的数据采集
- 逻辑电池编号需要通过映射转换为物理通道编号

### 4.6 配置参数初始化

**文件位置**: `Source/App/bq769xx.c:113-137`

```c
void BQ769xx_Init_Para(void)
{
    // 系统控制寄存器1
    Bq769xxReg.SysCtrl1.SysCtrl1Bit.ADC_EN = ADC_ENS;              // 使能ADC
    Bq769xxReg.SysCtrl1.SysCtrl1Bit.TEMP_SEL = EXTE_TEMP_ON;       // 外部温度传感器

    // 系统控制寄存器2
    Bq769xxReg.SysCtrl2.SysCtrl2Bit.DELAY_DIS = ALARM_DELAY_ON;    // 告警延迟使能
    Bq769xxReg.SysCtrl2.SysCtrl2Bit.CC_EN = CC_CONTINU_EN;         // 库仑计连续模式
    Bq769xxReg.SysCtrl2.SysCtrl2Bit.CHG_ON = CHG_OFF;              // 充电MOS初始关闭
    Bq769xxReg.SysCtrl2.SysCtrl2Bit.DSG_ON = DSG_OFF;              // 放电MOS初始关闭

    // 保护寄存器1: 短路保护
    Bq769xxReg.Protect1.Protect1Bit.RSNS = OCD_SCD_HIGH_RANGE;     // 高量程电流检测
    Bq769xxReg.Protect1.Protect1Bit.SCD_DELAY = gBMSConfig.BQ76Para.SCD_Delay;
    Bq769xxReg.Protect1.Protect1Bit.SCD_THRESH = gBMSConfig.BQ76Para.SCD_Thresh;

    // 保护寄存器2: 过流保护
    Bq769xxReg.Protect2.Protect2Bit.OCD_DELAY = gBMSConfig.BQ76Para.OCD_Delay;
    Bq769xxReg.Protect2.Protect2Bit.OCD_THRESH = gBMSConfig.BQ76Para.OCD_Thresh;

    // 保护寄存器3: 过压/欠压延迟
    Bq769xxReg.Protect3.Protect3Bit.OV_DELAY = gBMSConfig.BQ76Para.OV_Delay;
    Bq769xxReg.Protect3.Protect3Bit.UV_DELAY = gBMSConfig.BQ76Para.UV_Delay;
}
```

### 4.7 ADC 增益和偏移读取

**文件位置**: `Source/App/bq769xx.c:146-176`

```c
void BQ769xx_Read_Gain(void)
{
    u8 rcd = 0;

    // 读取ADC增益和偏移寄存器
    rcd = IIC_ReadByte(BQ769xxAddr, ADCGAIN1_RegAddr, &Bq769xxReg.ADCGain1.ADCGain1Byte);
    rcd = IIC_ReadByte(BQ769xxAddr, ADCGAIN2_RegAddr, &Bq769xxReg.ADCGain2.ADCGain2Byte);
    rcd = IIC_ReadByte(BQ769xxAddr, ADCOFFSET_RegAddr, &Bq769xxReg.ADCOffset);

    // 计算ADC增益（单位：uV）
    // 公式: Gain(uV) = 365 + ((ADCGAIN1[3:2] << 1) + (ADCGAIN2[7:5] >> 5))
    VoltCellGainUV = (365 + ((Bq769xxReg.ADCGain1.ADCGain1Byte & 0x0C) << 1) +
                           ((Bq769xxReg.ADCGain2.ADCGain2Byte & 0xE0) >> 5));
    VoltCellGainMV = VoltCellGainUV / 1000;
    VoltCellOffSet = Bq769xxReg.ADCOffset;

    // 限制OV/UV阈值在有效范围内
    gBMSConfig.BQ76Para.OV_Thresh = LimitMaxMin(gBMSConfig.BQ76Para.OV_Thresh, OV_THRESH_MAX, OV_THRESH_MIN);
    gBMSConfig.BQ76Para.UV_Thresh = LimitMaxMin(gBMSConfig.BQ76Para.UV_Thresh, UV_THRESH_MAX, UV_THRESH_MIN);

    // 计算OV/UV寄存器值
    // 公式: Trip值 = ((目标电压_mV - ADC偏移) * 1000 / 增益_uV - 基准值) >> 4
    Bq769xxReg.OVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.OV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - OV_THRESH_BASE) >> 4) & 0xff);
    Bq769xxReg.UVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.UV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - UV_THRESH_BASE) >> 4) & 0xff);
    Bq769xxReg.CCCfg = 0x19;  // 库仑计配置固定值

    (void) rcd;
}
```

### 4.8 配置写入与验证

**文件位置**: `Source/App/bq769xx.c:185-237`

```c
u8 BQ769xx_Write_Config(void)
{
    u8 ret = 0, rcd = 0, Count = 0, err = 0;
    u8 bqSysCtrProtectionConfig[11] = {0};

    // 配置写入和验证循环（最多重试5次）
    do
    {
        // 写入8个配置寄存器
        rcd = IIC_WritByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr,
                               &(Bq769xxReg.SysCtrl1.SysCtrl1Byte), 8);

        // 回读验证
        rcd = IIC_ReadByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr,
                               bqSysCtrProtectionConfig, 8);

        // 比较写入值和回读值
        if(bqSysCtrProtectionConfig[0] != Bq769xxReg.SysCtrl1.SysCtrl1Byte ||
           bqSysCtrProtectionConfig[1] != Bq769xxReg.SysCtrl2.SysCtrl2Byte ||
           bqSysCtrProtectionConfig[2] != Bq769xxReg.Protect1.Protect1Byte ||
           bqSysCtrProtectionConfig[3] != Bq769xxReg.Protect2.Protect2Byte ||
           bqSysCtrProtectionConfig[4] != Bq769xxReg.Protect3.Protect3Byte ||
           bqSysCtrProtectionConfig[5] != Bq769xxReg.OVTrip ||
           bqSysCtrProtectionConfig[6] != Bq769xxReg.UVTrip ||
           bqSysCtrProtectionConfig[7] != Bq769xxReg.CCCfg)
        {
            err++;  // 寄存器不匹配
        }
        Count++;
    } while((err != 0) && (Count < 5));

    // 判断配置结果
    if((err == 0) || (Count < 5))
    {
        ret = gRET_OK;
    }
    else
    {
        ret = gRET_NG;
        // 配置失败，清零所有寄存器值
        Bq769xxReg.SysCtrl1.SysCtrl1Byte = 0;
        Bq769xxReg.SysCtrl2.SysCtrl2Byte = 0;
        // ... 清零其他寄存器
    }
    return ret;
}
```

---

## 5. I2C 通信协议实现

### 5.1 I2C 硬件配置

| 参数 | 值 |
|------|-----|
| SCL 引脚 | PB6 |
| SDA 引脚 | PB7 |
| 通信方式 | 软件模拟 |
| 时钟频率 | ~100kHz |
| 从机地址 | 0x08（7 位） |
| 校验方式 | CRC8（多项式 0x07） |

### 5.2 GPIO 方向控制

**文件位置**: `Source/Drive/iic.c:51-76`

```c
// SDA 输出模式（开漏，支持双向传输）
void Sda_Set_Out_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;    // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

// SDA 输入模式（接收 ACK 时使用）
void Sda_Set_In_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}
```

### 5.3 I2C 时序实现

**起始信号**: `Source/Drive/iic.c:156-168`

```
时序图:
SDA:  _________
                \_________
SCL:  ________/"""\_______
      SCL高时SDA下降沿 = 起始信号
```

```c
unsigned char IICStart(void)
{
    SDA_OUT_HIGH;        // SDA=1
    SCK_OUT_HIGH;        // SCL=1
    Sda_Set_Out_Mode();
    delay_us(2);
    SDA_OUT_LOW;         // SDA=0 (下降沿)
    delay_us(2);
    SCK_OUT_LOW;         // SCL=0
    return gRET_OK;
}
```

**停止信号**: `Source/Drive/iic.c:182-191`

```
时序图:
SDA:           /---------
     _________/
SCL:  ________/"""\_______
      SCL高时SDA上升沿 = 停止信号
```

```c
void IICStop(void)
{
    SCK_OUT_LOW;
    SDA_OUT_LOW;
    Sda_Set_Out_Mode();
    delay_us(2);
    SCK_OUT_HIGH;        // SCL=1
    delay_us(2);
    SDA_OUT_HIGH;        // SDA=1 (上升沿)
}
```

### 5.4 CRC8 校验算法

**文件位置**: `Source/Drive/iic.c:286-313`

```c
u8 IIC_CRC8Bytes(u8 *ptr, u8 len, u8 key)
{
    u8 i;
    u8 crc = 0;

    while(len-- != 0)
    {
        for(i = 0x80; i != 0; i /= 2)
        {
            if((crc & 0x80) != 0)
            {
                crc *= 2;
                crc ^= key;    // 异或多项式
            }
            else
            {
                crc *= 2;
            }

            if((*ptr & i) != 0)
            {
                crc ^= key;
            }
        }
        ptr++;
    }
    return crc;
}
```

**CRC8 参数**:
- 多项式: 0x07 (x^8 + x^2 + x + 1)
- 初始值: 0x00
- 应用: 所有 I2C 通信数据

### 5.5 单字节写入

**文件位置**: `Source/Drive/iic.c:335-382`

```
帧格式:
[S][Slave+W][ACK][RegAddr][ACK][Data][ACK][CRC][ACK][P]

说明:
- S: 起始信号
- Slave+W: 从机地址 + 写位（0x08 << 1 | 0 = 0x10）
- RegAddr: 寄存器地址
- Data: 要写入的数据
- CRC: CRC8 校验值
- P: 停止信号
```

```c
unsigned char IIC_WritByte(unsigned char SloveAddr, unsigned char RegAddr, unsigned char Data)
{
    unsigned char ret = gRET_OK, rcd = 0;
    unsigned char BuffData[3] = {0};
    unsigned char crc = 0;

    IICStart();

    // 发送从机地址（写模式）
    rcd = IICSendByte((SloveAddr << 1) & 0xFE);
    BuffData[0] = ((SloveAddr << 1) & 0xFE);
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    // 发送寄存器地址
    rcd = IICSendByte(RegAddr);
    BuffData[1] = RegAddr;
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    // 发送数据
    rcd = IICSendByte(Data);
    BuffData[2] = Data;
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    // 计算并发送CRC
    crc = IIC_CRC8Bytes(BuffData, 3, 0x07);
    rcd = IICSendByte(crc);
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    IICStop();
    return ret;
}
```

### 5.6 单字节读取

**文件位置**: `Source/Drive/iic.c:405-452`

```
帧格式:
写: [S][Slave+W][ACK][RegAddr][ACK][S][Slave+R][ACK]
读: [Data][ACK][CRC][NACK][P]

说明:
- 先写寄存器地址
- 重新起始
- 读取 2 字节（数据 + CRC）
- 验证 CRC
```

```c
unsigned char IIC_ReadByte(unsigned char SloveAddr, unsigned char RegAddr, unsigned char* ReadData)
{
    unsigned char ret = gRET_OK, rcd = 0;
    unsigned char BuffData[3] = {0};
    unsigned char crc = 0;

    // 写寄存器地址
    IICStart();
    rcd = IICSendByte((SloveAddr << 1) & 0xFE);  // 从机地址（写模式）
    delay_us(5);
    rcd = IICSendByte(RegAddr);                   // 寄存器地址
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    // 读取数据
    IICStart();
    rcd = IICSendByte((SloveAddr << 1) | 0x01);   // 从机地址（读模式）
    BuffData[0] = (SloveAddr << 1) | 0x01;
    delay_us(5);
    if(rcd != gRET_OK) return gRET_NG;

    BuffData[1] = IICReceByte();  // 接收数据
    IICSendAck();
    delay_us(5);

    BuffData[2] = IICReceByte();  // 接收CRC
    IICSendNoAck();
    delay_us(5);

    IICStop();

    // 验证CRC
    crc = IIC_CRC8Bytes(BuffData, 2, 0x07);
    if(BuffData[2] != crc)
    {
        return gRET_NG;  // CRC校验失败
    }

    *ReadData = BuffData[1];
    return ret;
}
```

### 5.7 多字节读取（40 字节数据采集）

**文件位置**: `Source/Drive/iic.c:552-626`

```
帧格式:
写: [S][Slave+W][ACK][RegAddr][ACK][S][Slave+R][ACK]
读: [Data0][ACK][CRC0][ACK][Data1][ACK][CRC1][ACK]...[DataN][NACK][CRCN][P]

注意: 每个数据字节都有独立的 CRC 校验
```

**应用示例 - 数据采集**: `Source/App/bq769xx.c:327-414`

```c
void BQ769xx_GetData(void)
{
    u8 rcd = 0;
    u8 ia = 0, ib = 0;
    u16 cells_map = gBMSData.BattPar.Cells_Map;
    u8 VoltOffset = 0;
    u16 AdcCurr = 0;

    // 时间片检查：仅在200ms时间片执行
    if(TaskTimePare.Tim200ms_flag != 1)
    {
        return;
    }

    // 读取40字节ADC数据
    // VC1-VC15(30字节) + BAT(2字节) + TS1-TS3(6字节) + CC(2字节)
    rcd = IIC_ReadByteMore(BQ769xxAddr, VC1_HI_RegAddr, BQ769xxGatherData, 40);
    if(rcd == gRET_OK)
    {
        memcpy(&(Bq769xxReg.VCell1.VCell1Byte.VC1_HI), BQ769xxGatherData, 40);
    }

    // 电池电压计算（支持有符号偏移）
    for(ia = 0; ia < SYS_CELL_MAX; ia++)
    {
        if(cells_map & 0x01)
        {
            if(Bq769xxReg.ADCOffset & 0x80)  // 负偏移
            {
                VoltOffset = 0x100 - Bq769xxReg.ADCOffset;
                gBMSData.BattPar.VoltCell[ib] = (u16)(((unsigned long)((BQ769xxGatherData[ia * 2] * 256) +
                    BQ769xxGatherData[ia * 2 + 1]) * VoltCellGainUV) / 1000 - VoltOffset);
            }
            else  // 正偏移
            {
                VoltOffset = Bq769xxReg.ADCOffset;
                gBMSData.BattPar.VoltCell[ib] = (u16)(((unsigned long)((BQ769xxGatherData[ia * 2] * 256) +
                    BQ769xxGatherData[ia * 2 + 1]) * VoltCellGainUV) / 1000 + VoltOffset);
            }
            ib++;
        }
        cells_map = cells_map >> 1;
    }

    // 总电压计算
    gBMSData.BattPar.VoltLine = (u16)(((unsigned long)((BQ769xxGatherData[30] * 256) +
        BQ769xxGatherData[31]) * BATVOLTLSB) / 1000);

    // 温度计算
    for(ia = 0; ia < SYS_TEMPEXT_MAX; ia++)
    {
        VoltageTemp[ia] = ((unsigned long)((BQ769xxGatherData[ia * 2 + 32] * 256) +
            BQ769xxGatherData[ia * 2 + 32 + 1]) * 382) / 1000;
        gBMSData.BattPar.TempCell[ia] = TemChange(VoltageTemp[ia]);
    }

    // 电流计算（支持有符号电流）
    AdcCurr = (u16)(BQ769xxGatherData[38] * 256) + BQ769xxGatherData[39];
    if(AdcCurr & 0x8000)  // 负电流（放电）
    {
        AdcCurr = 0x10000 - AdcCurr;
        gBMSData.BattPar.CurrLine = 0x10000 - (u16)(((unsigned long)AdcCurr * CRUUVOLTLSB) / gBMSConfig.Type.ShuntSpec);
    }
    else  // 正电流（充电）
    {
        gBMSData.BattPar.CurrLine = (u16)(((unsigned long)AdcCurr * CRUUVOLTLSB) / gBMSConfig.Type.ShuntSpec);
    }

    // 更新最大最小值
    pack_temp_max_min();
    pack_cell_max_min();

    (void) rcd;
}
```

---

## 6. 均衡控制算法

### 6.1 均衡策略概述

| 参数 | 描述 |
|------|------|
| 均衡方式 | 被动均衡（电阻放电） |
| 均衡条件 | 待机模式 + 压差>阈值 + 电压>启动电压 |
| 选择算法 | 冒泡排序选择最高电压的 N 串 |
| 均衡时间 | 单次 180 秒 |
| 最大均衡数 | 可配置（1-5 串） |

### 6.2 均衡状态机

**文件位置**: `Source/App/bq769xx.c:841-984`

```
状态转换图:
┌──────┐     ┌──────┐     ┌──────┐     ┌────────┐
│ INIT │ --> │ WAIT │ --> │ CHK  │ --> │ SELECT │
└──────┘     └──────┘     └──────┘     └────────┘
    ↑                                    │
    │                         ┌─────────┘
    │                         ▼
    │                    ┌──────┐     ┌─────┐     ┌──────────┐
    │                    │  MAP │ --> │ SET │ --> │ BALA_TIM │
    │                    └──────┘     └─────┘     └──────────┘
    │                         │                       │
    └─────────────────────────┴───────────────────────┘

状态说明:
- INIT:    初始化，关闭所有均衡
- WAIT:    等待 2 秒稳定
- CHK:     检查均衡条件
- SELECT:  冒泡排序选择最高电压的 N 串
- MAP:     映射到物理寄存器位
- SET:     写入均衡寄存器
- BALA_TIM: 均衡计时 180 秒
```

### 6.3 均衡条件检查

**文件位置**: `Source/App/bq769xx.c:891-906`

```c
case PCB_SEQ_CHK:
    /* 检查均衡条件 */
    // 必须在待机模式
    if(gBMSData.Sys_Mod.sys_mode != SYS_MODE_STANDBY)
    {
        stup_balan = PCB_SEQ_INIT;
        break;
    }
    // 压差小于阈值 或 最低电压低于启动电压 -> 不均衡
    if(((gBMSData.BattPar.VoltCellMax - gBMSData.BattPar.VoltCellMin) < gBMSConfig.Balan.balanc_diffe_volt) ||
        (gBMSData.BattPar.VoltCellMin < gBMSConfig.Balan.balanc_start_volt))
    {
        stup_balan = PCB_SEQ_INIT;
        break;
    }
    stup_balan = PCB_SEQ_SELECT;
    break;
```

### 6.4 冒泡排序选择

**文件位置**: `Source/App/bq769xx.c:909-943`

```c
case PCB_SEQ_SELECT:
    /* 冒泡排序选择电压最高的N串电池 */
    // 初始化排序数组
    for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
    {
        volta_cell_sort[ia] = gBMSData.BattPar.VoltCell[ia];
        volta_flag_sort[ia] = ia;
    }

    // 冒泡排序（降序）
    for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
    {
        for(ib = 0; ib < (gBMSConfig.Type.CellNum_Ser - 1); ib++)
        {
            if(volta_cell_sort[ib] < volta_cell_sort[ib + 1])
            {
                // 交换电压值
                volta_temp = volta_cell_sort[ib + 1];
                volta_cell_sort[ib + 1] = volta_cell_sort[ib];
                volta_cell_sort[ib] = volta_temp;

                // 交换索引
                volta_flag = volta_flag_sort[ib + 1];
                volta_flag_sort[ib + 1] = volta_flag_sort[ib];
                volta_flag_sort[ib] = volta_flag;
            }
        }
    }

    // 选择前N串最高电压进行均衡
    for(ia = 0; ia < gBMSConfig.Balan.balanc_number_max; ia++)
    {
        gBMSData.BalaPar.bala_ctrl_sw |= (0x00000001 << volta_flag_sort[ia]);
    }
    stup_balan = PCB_SEQ_MAP;
    break;
```

### 6.5 逻辑-物理映射

**文件位置**: `Source/App/bq769xx.c:778-821`

```c
void cell_balan_locatget(u32 balaCellSw, u16* cellbalaset)
{
    u8 ia = 0, ib = 0, cntset = 0;
    u16 cells_map = 0;
    u32 outcellsw = 0;

    // 遍历所有逻辑电池
    for(ia = 0; ia < gBMSConfig.Type.CellNum_Ser; ia++)
    {
        cntset = 0;
        cells_map = gBMSData.BattPar.Cells_Map;

        // 遍历所有物理通道
        for(ib = 0; ib < SYS_CELL_MAX; ib++)
        {
            if(cells_map & 0x01)  // 该物理通道有效
            {
                if(cntset == ia)  // 找到第ia个有效通道
                {
                    cntset++;
                    if(balaCellSw & (0x0001 << ia))  // 该电池需要均衡
                    {
                        outcellsw |= 0x0001 << ib;  // 设置物理通道均衡位
                        break;
                    }
                }
                else
                {
                    cntset++;
                }
            }
            cells_map = cells_map >> 1;
        }
    }
    *cellbalaset = outcellsw;
}
```

**映射示例（4串，Cells_Map=0x0017）**:

| 逻辑电池 | 物理通道 | 寄存器位 |
|----------|----------|----------|
| 电池1 | VC1 | CELLBAL1.Bit0 |
| 电池2 | VC2 | CELLBAL1.Bit1 |
| 电池3 | VC5 | CELLBAL1.Bit4 |
| 电池4 | VC6 | CELLBAL2.Bit0 |

### 6.6 均衡寄存器写入

**文件位置**: `Source/App/bq769xx.c:708-753`

```c
u8 balance_write_regs(u8 cellbalanbyte1, u8 cellbalanbyte2, u8 cellbalanbyte3)
{
    u8 ret = gRET_OK, rcd = 0;
    u8 write_regs[3] = {0};
    u8 read_regs[3] = {0};
    u8 ia = 0;
    u16 cells_map = gBMSData.BattPar.Cells_Map;

    // 构建写入数据包
    write_regs[0] = cellbalanbyte1;  // CELLBAL1寄存器值
    write_regs[1] = cellbalanbyte2;  // CELLBAL2寄存器值
    write_regs[2] = cellbalanbyte3;  // CELLBAL3寄存器值

    // 写入均衡寄存器
    rcd = IIC_WritByteMore(BQ769xxAddr, CELLBAL1_RegAddr, write_regs, 3);
    if(rcd != gRET_OK)
    {
        return gRET_NG;
    }

    // 回读验证
    rcd = IIC_ReadByteMore(BQ769xxAddr, CELLBAL1_RegAddr, read_regs, 3);
    if(rcd != gRET_OK)
    {
        return gRET_NG;
    }

    // 比较写入值和回读值
    for(ia = 0; ia < 3; ia++)
    {
        if(read_regs[ia] != write_regs[ia])
        {
            ret = gRET_NG;
        }
    }
    return ret;
}
```

---

## 7. 状态机架构设计

### 7.1 三层状态机架构

```
┌─────────────────────────────────────────────────────────────┐
│                    第一层状态机                              │
│                 (TaskFun.c - 系统模式)                      │
│  SLEEP ─> STANDBY ─> CHARGE ─> DISCHARGE                    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    第二层状态机                              │
│              (bq769xx.c - 均衡控制)                         │
│  INIT ─> WAIT ─> CHK ─> SELECT ─> MAP ─> SET ─> BALA_TIM    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    第三层状态机                              │
│            (bq769xx.c - 通信操作)                          │
│  DSG_ON / DSG_OFF / CHG_ON / CHG_OFF / CLEAR_ALERT / SHIP   │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 第一层状态机 - 系统模式

| 状态 | 描述 | 触发条件 |
|------|------|----------|
| SLEEP | 睡眠模式 | 系统初始化或待机超时 |
| STANDBY | 待机模式 | 无充放电电流 |
| CHARGE | 充电模式 | 检测到充电电流 |
| DISCHARGE | 放电模式 | 检测到放电电流 |

### 7.3 第二层状态机 - 均衡控制

详见章节 6.2。

### 7.4 第三层状态机 - 通信操作

**文件位置**: `Source/App/bq769xx.c:1086-1128`

```c
void BQ769xx_Oper_Comm(void)
{
    if((TaskTimePare.Tim100ms_flag == 1) || (Bq769xx_Oper_EN == OPER_ON))
    {
        switch(Bq769xx_Oper_Type)
        {
        case COMM_DSG_ON:        // 开启放电MOS
            BQ769xx_DSGSET(OPER_ON);
            Bq769xx_Oper_EN = OPER_OFF;
            Bq769xx_Oper_Type = 0;
            break;

        case COMM_DSG_OFF:       // 关闭放电MOS
            BQ769xx_DSGSET(OPER_OFF);
            Bq769xx_Oper_EN = OPER_OFF;
            Bq769xx_Oper_Type = 0;
            break;

        case COMM_CHG_ON:        // 开启充电MOS
            BQ769xx_CHGSET(OPER_ON);
            Bq769xx_Oper_EN = OPER_OFF;
            Bq769xx_Oper_Type = 0;
            break;

        case COMM_CHG_OFF:       // 关闭充电MOS
            BQ769xx_CHGSET(OPER_OFF);
            Bq769xx_Oper_EN = OPER_OFF;
            Bq769xx_Oper_Type = 0;
            break;

        case COMM_CLEAR_ALERT:   // 清除告警
            // 清除告警操作
            break;

        case COMM_ENTER_SHIP:    // 进入关机模式
            Power_Down();
            break;

        default:
            break;
        }
    }
}
```

### 7.5 MOSFET 控制实现

**文件位置**: `Source/App/bq769xx.c:455-533`

```c
u8 BQ769xx_DSGSET(u8 ONOFF)
{
    u8 ret = gRET_OK, rcd = 0;

    if(ONOFF > 1) return gRET_NG;

    // 读取当前寄存器值
    rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &(Bq769xxReg.SysCtrl2.SysCtrl2Byte));
    if(rcd != gRET_OK) return gRET_NG;

    // 设置DSG位（读取-修改-写回）
    if(ONOFF == 1)
    {
        IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr,
                     Bq769xxReg.SysCtrl2.SysCtrl2Byte | 0x02);
    }
    else
    {
        IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr,
                     Bq769xxReg.SysCtrl2.SysCtrl2Byte & 0xFD);
    }
    return ret;
}

u8 BQ769xx_CHGSET(u8 ONOFF)
{
    u8 ret = gRET_OK, rcd = 0;

    if(ONOFF > 1) return gRET_NG;

    // 读取当前寄存器值
    rcd = IIC_ReadByte(BQ769xxAddr, SYS_CTRL2_RegAddr, &(Bq769xxReg.SysCtrl2.SysCtrl2Byte));
    if(rcd != gRET_OK) return gRET_NG;

    // 设置CHG位（读取-修改-写回）
    if(ONOFF == 1)
    {
        IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr,
                     Bq769xxReg.SysCtrl2.SysCtrl2Byte | 0x01);
    }
    else
    {
        IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr,
                     Bq769xxReg.SysCtrl2.SysCtrl2Byte & 0xFE);
    }
    return ret;
}
```

---

## 8. 数据采集流程

### 8.1 采集时序

```
时间片执行计划:
┌────────┬────────┬────────┬────────┬────────┬────────┐
│  0ms   │ 100ms  │ 200ms  │ 300ms  │ 400ms  │ 500ms  │
└────────┴────────┴────────┴────────┴────────┴────────┘
          │                 │                │
          ▼                 ▼                ▼
      MOS控制            数据采集          配置读取
      (第三层)          (40字节)         (12字节)
```

### 8.2 数据帧格式

**40 字节数据帧**:
```
偏移    字段       大小    描述
─────────────────────────────────────
0x00    VC1        2字节   Cell1 电压
0x02    VC2        2字节   Cell2 电压
...
0x1C    VC15       2字节   Cell15 电压
0x1E    BAT        2字节   总电压
0x20    TS1        2字节   温度传感器1
0x22    TS2        2字节   温度传感器2
0x24    TS3        2字节   温度传感器3
0x26    CC         2字节   库仑计/电流
─────────────────────────────────────
总计:              40字节
```

### 8.3 数据转换公式

| 数据类型 | 转换公式 | 常量 |
|----------|----------|------|
| 电池电压 | ADC × Gain(uV) / 1000 ± Offset | VoltCellGainUV |
| 总电压 | ADC × 1532 / 1000 | BATVOLTLSB |
| 温度 | ADC × 382 / 1000 → NTC查表 | TMEPVOLTLSB |
| 电流 | ADC × 844 / ShuntSpec | CRUUVOLTLSB |

### 8.4 最大最小值计算

**文件位置**: `Source/App/bq769xx.c:999-1068`

```c
// 温度最大最小值
static void pack_temp_max_min(void)
{
    u8 ia;
    u16 temp_max, temp_min;

    temp_max = gBMSData.BattPar.TempCell[0];
    temp_min = gBMSData.BattPar.TempCell[0];

    for(ia = 1; ia < gBMSConfig.Type.TempNum; ia++)
    {
        if(temp_min > gBMSData.BattPar.TempCell[ia])
        {
            temp_min = gBMSData.BattPar.TempCell[ia];
        }
        if(temp_max < gBMSData.BattPar.TempCell[ia])
        {
            temp_max = gBMSData.BattPar.TempCell[ia];
        }
    }

    gBMSData.BattPar.TempPackMax = temp_max;
    gBMSData.BattPar.TempPackMin = temp_min;
}

// 电池电压最大最小值
static void pack_cell_max_min(void)
{
    u8 ia;
    u16 volt_max, volt_min;

    volt_max = gBMSData.BattPar.VoltCell[0];
    volt_min = gBMSData.BattPar.VoltCell[0];

    for(ia = 1; ia < gBMSConfig.Type.CellNum_Ser; ia++)
    {
        if(volt_max < gBMSData.BattPar.VoltCell[ia])
        {
            volt_max = gBMSData.BattPar.VoltCell[ia];
        }
        if(volt_min > gBMSData.BattPar.VoltCell[ia])
        {
            volt_min = gBMSData.BattPar.VoltCell[ia];
        }
    }

    gBMSData.BattPar.VoltCellMax = volt_max;
    gBMSData.BattPar.VoltCellMin = volt_min;
}
```

---

## 9. 保护机制

### 9.1 保护类型汇总

| 保护类型 | 触发条件 | 响应动作 |
|----------|----------|----------|
| 过压保护 (OV) | 单串电压 > OV_Thresh | 关闭充电 MOS |
| 欠压保护 (UV) | 单串电压 < UV_Thresh | 关闭充放电 MOS |
| 过流保护 (OCD) | 充电电流 > OCD_Thresh | 关闭充电 MOS |
| 短路保护 (SCD) | 放电电流 > SCD_Thresh | 关闭放电 MOS |
| 温度保护 | 温度超出范围 | 关闭对应 MOS |

### 9.2 保护阈值计算

**文件位置**: `Source/App/bq769xx.c:169-173`

```c
// 过压阈值寄存器值计算
// 公式: ((目标电压_mV - ADC偏移) * 1000 / 增益_uV - 基准值) >> 4
Bq769xxReg.OVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.OV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - OV_THRESH_BASE) >> 4) & 0xff);

// 欠压阈值寄存器值计算
Bq769xxReg.UVTrip = (u8)(((((unsigned long)(gBMSConfig.BQ76Para.UV_Thresh - Bq769xxReg.ADCOffset) * 1000) / VoltCellGainUV - UV_THRESH_BASE) >> 4) & 0xff);
```

### 9.3 告警清除

**文件位置**: `Source/App/bq769xx.c:577-603`

```c
u8 BQ769xx_STAT_CLEAR(u8 statvalue)
{
    u8 ret = gRET_OK, BQ_Stat = 0, rcd = 0;

    // SYS_STAT寄存器特性：写1清除对应位
    rcd = IIC_WritByte(BQ769xxAddr, SYS_STAT_RegAddr, statvalue);

    // 回读验证
    rcd = IIC_ReadByte(BQ769xxAddr, SYS_STAT_RegAddr, &BQ_Stat);

    // 判断清除结果
    if(BQ_Stat & statvalue)
    {
        ret = gRET_OK;
    }
    else
    {
        ret = gRET_NG;
    }

    return ret;
}
```

**清除掩码定义**:
```c
#define OCD_CLE             0x01    // 清除过流告警
#define SCD_CLE             0x02    // 清除短路告警
#define OV_CLE              0x04    // 清除过压告警
#define UV_CLE              0x08    // 清除欠压告警
#define OVRD_ALERT_CLE      0x10    // 清除覆盖告警
#define DEVICE_XREADY_CLE   0x20    // 清除设备就绪告警
#define CC_READY_CLE        0x80    // 清除库仑计就绪告警
```

---

## 10. 配置与参数管理

### 10.1 配置结构体

```c
// ConfigPara.h
typedef struct _sSystem_BQ76Para
{
    u16 OV_Thresh;           // 过压阈值 (mV)
    u16 UV_Thresh;           // 欠压阈值 (mV)
    u8 OV_Delay;             // 过压延迟
    u8 UV_Delay;             // 欠压延迟
    u8 OCD_Thresh;           // 过流阈值
    u8 OCD_Delay;            // 过流延迟
    u8 SCD_Thresh;           // 短路阈值
    u8 SCD_Delay;            // 短路延迟
} sSystem_BQ76Para;

typedef struct _sSystem_Balan
{
    u8 balanc_number_max;    // 最大均衡电池数
    u16 balanc_diffe_volt;   // 均衡压差阈值 (mV)
    u16 balanc_start_volt;   // 均衡启动电压 (mV)
} sSystem_Balan;

typedef struct _sBMSConfig
{
    sSystem_Type Type;       // 硬件规格
    sSystem_Balan Balan;     // 均衡策略
    sSystem_BQ76Para BQ76Para; // 保护参数
} sBMSConfig;
```

### 10.2 数据结构

```c
// DataBase.h
typedef struct _sBatt_Para
{
    u16 VoltCell[PACK_CELL_MAX];    // 电池电压数组 (mV)
    u16 VoltLine;                    // 总电压 (mV)
    u16 CurrLine;                    // 电流 (mA)
    u16 TempCell[TEMP_MAX];          // 温度数组 (℃*10)
    u16 VoltCellMax;                 // 最高电池电压
    u16 VoltCellMin;                 // 最低电池电压
    u16 Cells_Map;                   // 电池映射掩码
} sBatt_Para;

typedef struct _sBala_Para
{
    u8 bala_state;                   // 均衡状态
    u32 bala_ctrl_sw;                // 逻辑均衡开关位图
    u16 bala_dischg;                 // 物理均衡寄存器值
    u8 Bala_force_on;                // 强制均衡标志
} sBala_Para;
```

### 10.3 Flash 存储

配置参数支持掉电保存，存储在 STM32 内部 Flash 中。

---

## 总结

### 项目亮点

1. **工业级可靠性设计**
   - CRC8 校验确保通信可靠性
   - 5 次重试机制
   - 配置参数掉电保存

2. **清晰的状态机架构**
   - 三层状态机设计，职责分明
   - 非阻塞时间片轮询架构
   - 易于维护和扩展

3. **完整的保护机制**
   - 硬件级 OV/UV/OCD/SCD 保护
   - 温度保护
   - 智能均衡控制

4. **灵活的配置系统**
   - 支持 3-5 串电池可配置
   - 保护阈值可调
   - 均衡策略可配置

### 技术要点

1. **I2C 软件模拟 + CRC8 校验**
2. **冒泡排序均衡算法**
3. **逻辑-物理映射机制**
4. **时间片轮询架构**
5. **非阻塞状态机设计**

---

*本文档基于 SLMC607 V1.6 BMS 项目源代码分析生成*
