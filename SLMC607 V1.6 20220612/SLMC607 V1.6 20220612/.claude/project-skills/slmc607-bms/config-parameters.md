# SLMC607 参数配置指南

## 配置文件位置
- **定义**: `Source/App/ConfigPara.h`
- **实现**: `Source/App/ConfigPara.c`
- **存储**: Flash (带CRC校验)

---

## 配置结构总览

```c
typedef struct BMSConfig {
    sSystem_Type      Type;       // 硬件规格
    sSystem_Balan     Balan;      // 均衡策略
    sSystem_BQ76Para  BQ76Para;   // 保护阈值
} sBMSConfig;
```

---

## 1. 硬件规格配置 (Type)

### 参数说明 (ConfigPara.c:34-40)

| 参数 | 类型 | 示例值 | 说明 | 修改建议 |
|------|------|--------|------|----------|
| `CapaRate` | u64 | 200 | 额定容量 (mAh) | 根据电池规格设置 |
| `VoltSpec` | u8 | 18 | 系统电压 (V) | 一般不需修改 |
| `CellNum_Ser` | u8 | 5 | 串联电芯数 | 3/4/5串可选 |
| `CellNum_Par` | u8 | 1 | 并联电芯数 | 根据电池组设置 |
| `TempNum` | u8 | 1 | 温度传感器数 | 1-3个 |
| `ShuntSpec` | u16 | 5000 | 分流电阻 (mΩ) | 根据硬件设计 |
| `OCVDaleyTime` | u16 | 600 | OCV静置时间 (s) | SOC校准用 |

### 常见配置模板

**3串电池组**:
```c
gBMSConfig.Type.CellNum_Ser = 3;
gBMSConfig.Type.VoltSpec = 11;  // 3.7V × 3
```

**4串电池组**:
```c
gBMSConfig.Type.CellNum_Ser = 4;
gBMSConfig.Type.VoltSpec = 14;  // 3.7V × 4
```

**5串电池组**:
```c
gBMSConfig.Type.CellNum_Ser = 5;
gBMSConfig.Type.VoltSpec = 18;  // 3.7V × 5
```

---

## 2. 均衡策略配置 (Balan)

### 参数说明 (ConfigPara.c:42-45)

| 参数 | 类型 | 示例值 | 说明 | 调优建议 |
|------|------|--------|------|----------|
| `balanc_start_volt` | u16 | 3000 | 启动电压 (mV) | 建议3.0V-3.5V |
| `balanc_diffe_volt` | u16 | 100 | 压差阈值 (mV) | 建议50-150mV |
| `balanc_number_max` | u16 | 3 | 最多均衡串数 | 建议2-5串 |
| `balanc_oneall_time` | u16 | 900 | 单次均衡时间 | 200ms单位 |

### 均衡策略详解

**启动条件** (同时满足):
1. 系统处于待机模式 (`SYS_MODE_STANDBY`)
2. 最低电芯电压 ≥ `balanc_start_volt`
3. 最大压差 ≥ `balanc_diffe_volt`

**均衡执行**:
1. 采集所有电芯电压
2. 冒泡排序，选电压最高的N串
3. 写入均衡寄存器
4. 持续 `balanc_oneall_time × 200ms`
5. 关闭均衡，重新检查

### 常见应用场景

**快充应用** (激进取均衡):
```c
balanc_start_volt = 3500;      // 3.5V启动
balanc_diffe_volt = 50;        // 50mV压差
balanc_number_max = 5;         // 5串全开
balanc_oneall_time = 2250;     // 450秒
```

**储能应用** (保守策略):
```c
balanc_start_volt = 3200;      // 3.2V启动
balanc_diffe_volt = 100;       // 100mV压差
balanc_number_max = 2;         // 2串同时
balanc_oneall_time = 900;      // 180秒
```

---

## 3. 保护阈值配置 (BQ76Para)

### 参数说明 (ConfigPara.c:47-54)

| 参数 | 类型 | 示例值 | 说明 | 调优建议 |
|------|------|--------|------|----------|
| `SCD_Delay` | u8 | 0x03 | 短路延时 | 见下表 |
| `SCD_Thresh` | u8 | 0x07 | 短路阈值 | 见下表 |
| `OCD_Delay` | u8 | 0x07 | 过流延时 | 见下表 |
| `OCD_Thresh` | u8 | 0x0F | 过流阈值 | 见下表 |
| `UV_Delay` | u8 | 0x03 | 欠压延时 | 1/4/8/16s |
| `OV_Delay` | u8 | 0x03 | 过压延时 | 1/4/8/16s |
| `UV_Thresh` | u16 | 2500 | 欠压阈值 (mV) | 2.0-3.1V |
| `OV_Thresh` | u16 | 4200 | 过压阈值 (mV) | 4.1-4.7V |

### SCD (短路) 配置表

**SCD_Delay**:
| 值 | 延时 |
|----|------|
| 0x00 | 70μs |
| 0x01 | 100μs |
| 0x02 | 200μs |
| 0x03 | 400μs |

**SCD_Thresh** (RSNS=1):
| 值 | 阈值 | 电流(5mΩ) |
|----|------|-----------|
| 0x00 | 44mV | 8.8A |
| 0x01 | 67mV | 13.4A |
| 0x02 | 89mV | 17.8A |
| 0x03 | 111mV | 22.2A |
| 0x04 | 133mV | 26.6A |
| 0x05 | 155mV | 31.0A |
| 0x06 | 178mV | 35.6A |
| 0x07 | 200mV | 40.0A |

### OCD (过流) 配置表

**OCD_Delay**:
| 值 | 延时 |
|----|------|
| 0x00 | 8ms |
| 0x01 | 20ms |
| 0x02 | 40ms |
| 0x03 | 80ms |
| 0x04 | 160ms |
| 0x05 | 320ms |
| 0x06 | 640ms |
| 0x07 | 1280ms |

**OCD_Thresh** (RSNS=1):
| 值 | 阈值 | 电流(5mΩ) |
|----|------|-----------|
| 0x00 | 17mV | 3.4A |
| 0x05 | 44mV | 8.8A |
| 0x0A | 72mV | 14.4A |
| 0x0F | 100mV | 20.0A |

### OV/UV 延时配置

**OV_Delay / UV_Delay**:
| 值 | 延时 |
|----|------|
| 0x00 | 1s |
| 0x01 | 4s |
| 0x02 | 8s |
| 0x03 | 16s |

### 常见应用场景

**动力电池** (严格保护):
```c
SCD_Delay=0x03;      SCD_Thresh=0x07;  // 200us, 40A
OCD_Delay=0x03;      OCD_Thresh=0x0F;  // 80ms, 20A
UV_Thresh=2700;      UV_Delay=0x03;    // 2.7V, 16s
OV_Thresh=4250;      OV_Delay=0x03;    // 4.25V, 16s
```

**储能电池** (宽松保护):
```c
SCD_Delay=0x03;      SCD_Thresh=0x04;  // 200us, 26A
OCD_Delay=0x07;      OCD_Thresh=0x0A;  // 1280ms, 14A
UV_Thresh=2500;      UV_Delay=0x02;    // 2.5V, 8s
OV_Thresh=4200;      OV_Delay=0x02;    // 4.2V, 8s
```

---

## 4. 参数保存与加载

### Flash存储流程

**写入参数** (ConfigPara.c:60-63):
```c
void WriteAllData(void) {
    // 1. 计算CRC
    gBMSConfig.DataCRC = 0 - DataCRC(gBMSConfig.DataLeng, (u8*)&gBMSConfig);

    // 2. 写入Flash
    STMFLASH_Write(FLASH_SAVE_ADDR, gBMSConfig.DataLeng, (u16*)&gBMSConfig);
}
```

**读取参数** (ConfigPara.c:66-90):
```c
void System_Pare_Get(void) {
    // 1. 从Flash读取
    STMFLASH_Read(FLASH_SAVE_ADDR, gBMSReadConfig.DataLeng, (u16*)&gBMSReadConfig);

    // 2. CRC校验
    ucA = DataCRC(gBMSReadConfig.DataLeng, (u8*)&gBMSReadConfig);

    // 3. 校验失败用默认值
    if(ucA != 0) {
        InitPara0();          // 加载默认值
        WriteAllData();       // 写入Flash
    }
    // 4. 校验成功使用Flash值
    else {
        memcpy(&gBMSConfig, &gBMSReadConfig, gBMSReadConfig.DataLeng);
    }
}
```

### 修改参数步骤

1. **修改 `InitPara0()`** (ConfigPara.c:30-57)
   - 直接修改默认值

2. **编译烧录**
   - 首次运行会写入Flash

3. **后续运行**
   - 自动从Flash加载
   - 如需重新初始化，调用 `FLASH_Update_Data()` (main.c:80-87)

---

## AI助手参考规范
当用户询问参数配置时：
1. 说明参数在哪个结构体
2. 给出参数取值范围
3. 提供不同应用场景的配置模板
4. 说明参数保存/加载机制
5. 标注代码位置 (文件名:行号)
