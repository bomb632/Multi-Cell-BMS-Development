# SLMC607 BMS 项目技能配置

## 项目基本信息
- **产品**: SLMC607 电池管理系统
- **版本**: V1.6 (2022-06-12)
- **MCU**: STM32F10x (ARM Cortex-M3)
- **AFE**: BQ76920/BQ76940 (TI 5-15串)
- **架构**: 时间片轮询 + 三层状态机

## 目录结构
```
Source/App/
├── main.c          # 主循环 (轮询架构)
├── TaskFun.c       # 系统模式状态机
├── bq769xx.c       # AFE驱动 + 均衡状态机
├── ConfigPara.c    # 参数配置 (Flash存储)
└── DataBase.h      # 数据结构定义
```

## 三层状态机
1. **系统模式**: SLEEP/STANDBY/CHARGE/DISCHARGE (基于电流±20mA切换)
2. **均衡控制**: INIT→WAIT→CHK→SELECT→MAP→SET→BALA_TIM (冒泡排序选高压串)
3. **通信操作**: MOS开关/告警清除/关机

## 时间片分片
- 100ms: 继电器控制、通信操作
- 200ms: 电压电流采集、模式判断
- 500ms: 温度采集、配置读取

## 核心配置 (ConfigPara.c)
```c
// 硬件规格
Type.CellNum_Ser = 5;     // 串联数 3/4/5
Type.CapaRate = 200;      // 容量 mAh
Type.ShuntSpec = 5000;    // 分流电阻 mΩ

// 均衡策略
Balan.balanc_start_volt = 3000;    // 启动电压 mV
Balan.balanc_diffe_volt = 100;     // 压差阈值 mV
Balan.balanc_number_max = 3;       // 均衡串数

// 保护阈值
BQ76Para.UV_Thresh = 2500;         // 欠压 mV
BQ76Para.OV_Thresh = 4200;         // 过压 mV
BQ76Para.SCD_Thresh = 0x07;        // 短路 200mV
BQ76Para.OCD_Thresh = 0x0F;        // 过流 100mV
```

## 关键代码位置
| 功能 | 文件 | 行号 |
|------|------|------|
| 主循环 | main.c | 57-71 |
| 系统状态机 | TaskFun.c | 40-74 |
| 均衡状态机 | bq769xx.c | 492-611 |
| MOS控制 | bq769xx.c | 275-328 |
| 参数初始化 | ConfigPara.c | 30-57 |

## 用户画像
- 10年BMS嵌入式工程师
- 熟悉 BQ76920/BQ76952/LTC6813
- 擅长 5串/16串BMS开发
- 偏好: 轮询+状态机，工业级规范

## AI回答准则
1. 优先使用轮询+状态机方案
2. 代码符合量产级规范
3. 标注代码位置 (文件名:行号)
4. 通俗易懂，适合新手学习
