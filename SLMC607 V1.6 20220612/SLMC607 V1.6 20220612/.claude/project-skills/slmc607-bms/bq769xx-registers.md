# BQ769xx 寄存器配置指南

## 寄存器地址映射 (bq769xx.h:14-29)

| 地址 | 寄存器名称 | 说明 |
|------|-----------|------|
| 0x00 | SYS_STAT | 系统状态寄存器 |
| 0x01 | CELLBAL1 | 电芯均衡控制1 (CB1-CB5) |
| 0x02 | CELLBAL2 | 电芯均衡控制2 (CB6-CB10) |
| 0x03 | CELLBAL3 | 电芯均衡控制3 (CB11-CB15) |
| 0x04 | SYS_CTRL1 | 系统控制1 |
| 0x05 | SYS_CTRL2 | 系统控制2 (CHG/DSG控制) |
| 0x06 | PROTECT1 | 保护配置1 (SCD) |
| 0x07 | PROTECT2 | 保护配置2 (OCD) |
| 0x08 | PROTECT3 | 保护配置3 (OV/UV延时) |
| 0x09 | OV_TRIP | 过压阈值 |
| 0x0A | UV_TRIP | 欠压阈值 |
| 0x0C-0x26 | VC1_HI-VC15_LO | 电芯电压 (15串) |
| 0x2E-0x2F | BAT_HI/BAT_LO | 总电压 |
| 0x30-0x35 | TS1-TS3 | 温度传感器 |
| 0x38-0x39 | CC_HI/CC_LO | 库仑计电流 |
| 0x50 | ADCGAIN1 | ADC增益高字节 |
| 0x51 | ADCOFFSET | ADC偏移 |
| 0x59 | ADCGAIN2 | ADC增益低字节 |

---

## 关键寄存器详解

### 1. SYS_CTRL2 (0x05) - MOS控制
```
Bit 0: CHG_ON  (充电MOS)
  0 = 关闭
  1 = 开启

Bit 1: DSG_ON  (放电MOS)
  0 = 关闭
  1 = 开启
```

**操作示例** (bq769xx.c:275-328):
```c
// 打开放电MOS
IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, reg | 0x02);

// 关闭充电MOS
IIC_WritByte(BQ769xxAddr, SYS_CTRL2_RegAddr, reg & 0xFE);
```

---

### 2. PROTECT1 (0x06) - 短路保护 (SCD)
```
Bit 7: RSNS (分流电阻配置)
  0 = 低阻值范围 (25-500uΩ)
  1 = 高阻值范围 (500-2000uΩ)

Bit 6-5: SCD_DELAY (短路延时)
  00 = 70us
  01 = 100us
  10 = 200us
  11 = 400us

Bit 4-2: SCD_THRESH (短路阈值)
  见 ConfigPara.h:23-31 的详细表格
```

**配置示例** (ConfigPara.c:47-48):
```c
SCD_Delay=0x03;     // 200us延时
SCD_Thresh=0x07;    // 200mV阈值 (RSNS=1)
```

---

### 3. PROTECT2 (0x07) - 过流保护 (OCD)
```
Bit 7: RSNS (同上)

Bit 6-4: OCD_DELAY (过流延时)
  000 = 8ms
  001 = 20ms
  010 = 40ms
  011 = 80ms
  100 = 160ms
  101 = 320ms
  110 = 640ms
  111 = 1280ms

Bit 3-0: OCD_THRESH (过流阈值)
  见 ConfigPara.h:42-59 的详细表格
```

**配置示例** (ConfigPara.c:49-50):
```c
OCD_Delay=0x07;     // 1280ms延时
OCD_Thresh=0x0F;    // 100mV阈值 (RSNS=1)
```

---

### 4. PROTECT3 (0x08) - 过压/欠压延时
```
Bit 3-2: OV_DELAY (过压延时)
  00 = 1s
  01 = 4s
  10 = 8s
  11 = 16s

Bit 1-0: UV_DELAY (欠压延时)
  00 = 1s
  01 = 4s
  10 = 8s
  11 = 16s
```

**配置示例** (ConfigPara.c:51-52):
```c
UV_Delay=0x03;  // 16s
OV_Delay=0x03;  // 16s
```

---

### 5. OV_TRIP / UV_TRIP - 阈值计算

**计算公式** (bq769xx.c:100-101):
```c
// 过压阈值
OVTrip = (((OV_Thresh - ADCOffset) * 1000 / VoltCellGainUV - 0x2008) >> 4) & 0xFF;

// 欠压阈值
UVTrip = (((UV_Thresh - ADCOffset) * 1000 / VoltCellGainUV - 0x1000) >> 4) & 0xFF;
```

**参数说明**:
- `OV_Thresh`: 目标过压阈值 (mV)，范围 3150-4700mV
- `UV_Thresh`: 目标欠压阈值 (mV)，范围 1580-3100mV
- `ADCGain`: ADC增益 (μV/LSB)，从0x50/0x59寄存器读取
- `ADCOffset`: ADC偏移，从0x51寄存器读取

**配置示例** (ConfigPara.c:53-54):
```c
UV_Thresh=2500;  // 2.5V欠压
OV_Thresh=4200;  // 4.2V过压
```

---

### 6. CELLBAL1/2/3 (0x01-0x03) - 均衡控制

```
CELLBAL1 (0x01):
  Bit 0-4: CB1-CB5 (电芯1-5均衡使能)

CELLBAL2 (0x02):
  Bit 0-4: CB6-CB10 (电芯6-10均衡使能)

CELLBAL3 (0x03):
  Bit 0-4: CB11-CB15 (电芯11-15均衡使能)
```

**操作示例** (bq769xx.c:417-452):
```c
// 写入均衡寄存器
balance_write_regs(cellbalanbyte1, cellbalanbyte2, cellbalanbyte3);

// 读回验证
IIC_ReadByteMore(BQ769xxAddr, CELLBAL1_RegAddr, read_regs, 3);
```

---

## 寄存器读写-回读验证模式

**工业级推荐写法** (bq769xx.c:114-128):
```c
u8 err = 0;
u8 Count = 0;

do {
    // 1. 写入寄存器
    IIC_WritByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr, write_data, 8);

    // 2. 读回验证
    IIC_ReadByteMore(BQ769xxAddr, SYS_CTRL1_RegAddr, read_data, 8);

    // 3. 比较校验
    if(write_data != read_data) err++;

    Count++;
} while((err != 0) && (Count < 5));

if(err == 0) {
    ret = gRET_OK;  // 写入成功
} else {
    ret = gRET_NG;  // 写入失败
}
```

---

## 电压采集与换算

**电芯电压计算** (bq769xx.c:212-229):
```c
// 从寄存器读取原始值
u16 raw = (VC_HI << 8) | VC_LO;

// 转换为mV
u16 voltage_mV = (raw * VoltCellGainUV) / 1000 + VoltOffset;
```

**电流计算** (bq769xx.c:242-253):
```c
// 读取原始值 (有符号16位)
i16 raw_current = (CC_HI << 8) | CC_LO;

// 转换为mA
u16 current_mA = (raw_current * 8.44 * 1000) / ShuntSpec;
// 其中: 8.44μV = CRUUVOLTLSB, ShuntSpec单位为mΩ
```

---

## AI助手参考规范
当用户询问寄存器配置时：
1. 先给出寄存器地址和位定义
2. 提供配置示例代码
3. 标注代码位置 (文件名:行号)
4. 说明阈值计算公式
5. 提供读写-回读验证模板
