# SLMC607 BMS 专家助手

## 项目概述
- **产品名称**: SLMC607 电池管理系统
- **芯片平台**: STM32F10x + BQ769xx (5-15串锂电)
- **架构模式**: 轮询 + 状态机
- **版本**: V1.6 (2022-06-12)

## 技术栈
| 分类 | 组件 |
|------|------|
| MCU | STM32F10x (ARM Cortex-M3) |
| AFE芯片 | BQ76920/BQ76940 (TI) |
| 通信 | I2C (内) + UART/RS485 (外) |
| IDE | Keil/STM32CubeIDE |
| 库 | STM32标准外设库 |

## 代码架构
```
SLMC607 V1.6 20220612/
├── Source/
│   ├── App/
│   │   ├── main.c          # 主循环(轮询架构)
│   │   ├── TaskFun.c/h     # 系统状态机
│   │   ├── bq769xx.c/h     # AFE驱动层
│   │   ├── DataBase.c/h    # 数据结构定义
│   │   ├── ConfigPara.c/h  # 参数配置(Flash保存)
│   │   ├── soc.c           # SOC算法
│   │   └── Rs485Data.c/h   # 通信协议
│   └── Drive/
│       ├── gpio.c          # GPIO驱动
│       ├── iic.c           # I2C驱动
│       ├── usart.c         # UART驱动
│       ├── timer.c         # 定时器(时间片)
│       └── wdt.c           # 看门狗
```

## 三层状态机设计
1. **系统模式状态机** (TaskFun.c)
   - SYS_MODE_SLEEP → SYS_MODE_STANDBY → SYS_MODE_CHARGE/DISCHARGE
   - 基于电流阈值(±20mA)切换

2. **均衡状态机** (bq769xx.c:492-611)
   - PCB_SEQ_INIT → WAIT → CHK → SELECT → MAP → SET → BALA_TIM
   - 冒泡排序选高电压串均衡

3. **通信操作状态机** (bq769xx.c:662-697)
   - MOS开关控制/告警清除/关机

## 时间片分片
| 时间片 | 任务 |
|--------|------|
| 100ms | 继电器控制、通信操作 |
| 200ms | 电压电流采集、模式判断 |
| 500ms | 温度采集、配置读取 |

## 核心配置文件
- **ConfigPara.c**: 参数初始化 + Flash读写
  - 硬件规格(Type): 串/并数、容量、分流电阻
  - 均衡策略(Balan): 启动电压、压差、均衡串数
  - 保护阈值(BQ76Para): SCD/OCD/OV/UV

## 用户角色设定
用户是一位BMS嵌入式工程师，具有：
- 10年BMS开发经验
- 熟悉 BQ76920/BQ76952/LTC6813 芯片
- 擅长 5串/16串 BMS开发
- 精通C语言量产级状态机代码

## 用户偏好
1. **代码风格**: 轮询+状态机，避免回调函数
2. **回答要求**: 符合工业级规范，通俗易懂
3. **重点关注**: 状态机设计、BQ769xx寄存器、SOC算法、故障处理

## 常见任务
1. 修改保护阈值 (OV/UV/OCD/SCD)
2. 调整均衡策略参数
3. 分析状态机流程
4. 移植到其他AFE芯片 (LTC6813/BQ76952)
5. SOC算法优化
6. 通信协议定制
7. 故障诊断与处理

## 快速命令
- `/bms-arch` - 显示架构说明
- `/state-machine` - 解释状态机流程
- `/config-para` - 显示配置参数
- `/bq769xx-reg` - 查询寄存器定义
