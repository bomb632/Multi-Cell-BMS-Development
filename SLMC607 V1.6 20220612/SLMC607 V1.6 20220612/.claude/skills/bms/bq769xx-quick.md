# BQ769xx 快速参考

## 寄存器地址
| 地址 | 名称 | 说明 |
|------|------|------|
| 0x00 | SYS_STAT | 状态告警 |
| 0x01-03 | CELLBAL | 均衡控制 (CB1-CB15) |
| 0x05 | SYS_CTRL2 | CHG(bit0)/DSG(bit1) |
| 0x06 | PROTECT1 | SCD配置 |
| 0x07 | PROTECT2 | OCD配置 |
| 0x08 | PROTECT3 | OV/UV延时 |
| 0x09 | OV_TRIP | 过压阈值 |
| 0x0A | UV_TRIP | 欠压阈值 |
| 0x0C+ | VC1_HI/LO | 电芯电压 |
| 0x38+ | CC_HI/LO | 库仑计电流 |

## 常用操作

### MOS控制
```c
// 充电MOS
IIC_WritByte(BQ769xxAddr, 0x05, reg | 0x01);  // 开
IIC_WritByte(BQ769xxAddr, 0x05, reg & 0xFE);  // 关

// 放电MOS
IIC_WritByte(BQ769xxAddr, 0x05, reg | 0x02);  // 开
IIC_WritByte(BQ769xxAddr, 0x05, reg & 0xFD);  // 关
```

### 读取电压
```c
u16 raw = (VC_HI << 8) | VC_LO;
u16 mv = (raw * VoltCellGainUV) / 1000 + VoltOffset;
```

### 读取电流
```c
i16 raw = (CC_HI << 8) | CC_LO;
if(raw & 0x8000) raw = 0x10000 - raw;  // 负电流
u16 ma = (raw * 8.44 * 1000) / ShuntSpec;
```

## 阈值计算
```c
// 过压阈值
OVTrip = (((OV_Thresh - Offset) * 1000 / Gain - 0x2008) >> 4) & 0xFF;

// 欠压阈值
UVTrip = (((UV_Thresh - Offset) * 1000 / Gain - 0x1000) >> 4) & 0xFF;
```
