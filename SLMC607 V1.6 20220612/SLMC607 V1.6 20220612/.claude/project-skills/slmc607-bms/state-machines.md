# SLMC607 状态机设计详解

## 1. 系统模式状态机 (TaskFun.c:40-74)

### 状态定义
```c
#define SYS_MODE_SLEEP      0  // 睡眠模式
#define SYS_MODE_STANDBY    1  // 待机模式
#define SYS_MODE_DISCHARGE  2  // 放电模式
#define SYS_MODE_CHARGE     3  // 充电模式
```

### 状态切换图
```
                    电流≥20mA
    ┌─────────────┐
    │   STANDBY   │────────────→ CHARGE
    │  (待机)     │←─────────────┘
    └──────┬──────┘
           │
           │ 电流≤-20mA
           ↓
        DISCHARGE
        (放电)
           │
           │ 电流>-20mA
           └──────→ STANDBY

    静置2小时 → SLEEP (睡眠)
    电流>20mA → 唤醒到STANDBY
```

### 切换条件
| 当前状态 | 切换条件 | 目标状态 |
|----------|----------|----------|
| STANDBY | 电流≥20mA | CHARGE |
| STANDBY | 电流≤-20mA | DISCHARGE |
| STANDBY | 静置2小时 | SLEEP |
| CHARGE | \|电流\|<20mA | STANDBY |
| DISCHARGE | \|电流\|<20mA | STANDBY |
| SLEEP | \|电流\|>20mA | STANDBY |

---

## 2. 均衡状态机 (bq769xx.c:492-611)

### 状态定义
```c
typedef enum _ePCB_SEQ {
    PCB_SEQ_INIT = 0,   // 初始化
    PCB_SEQ_WAIT,       // 等待稳定
    PCB_SEQ_CHK,        // 检查条件
    PCB_SEQ_SELECT,     // 选择均衡电芯
    PCB_SEQ_MAP,        // 映射寄存器位
    PCB_SEQ_SET,        // 写入均衡寄存器
    PCB_SEQ_BALA_TIM,   // 均衡计时
} ePCB_SEQ;
```

### 状态流程图
```
    ┌────────┐
    │  INIT  │ 关闭所有均衡，清零标志
    └───┬────┘
        ↓
    ┌────────┐
    │  WAIT  │ 等待10个周期(2s)稳定
    └───┬────┘
        ↓
    ┌────────┐
    │  CHK   │ 检查：待机模式？压差够？电压够？
    └───┬────┘
        │ 是
        ↓
    ┌──────────┐
    │  SELECT  │ 冒泡排序，选电压最高的N串
    └───┬──────┘
        ↓
    ┌────────┐
    │  MAP   │ 逻辑电芯号 → BQ769xx寄存器位
    └───┬────┘
        ↓
    ┌────────┐
    │  SET   │ 写入CellBal1/2/3寄存器
    └───┬────┘
        ↓
    ┌────────────┐
    │ BALA_TIM   │ 均衡180秒(900×200ms)
    └─────┬──────┘
          │ 时间到
          └──────→ INIT (循环)
```

### 均衡策略参数 (ConfigPara.c:42-45)
```c
balanc_start_volt = 3000;    // 启动电压 3.0V
balanc_diffe_volt = 100;     // 压差阈值 100mV
balanc_number_max = 3;       // 最多3串同时均衡
balanc_oneall_time = 900;    // 单次均衡180秒
```

---

## 3. 通信操作状态机 (bq769xx.c:662-697)

### 操作类型
```c
COMM_DSG_ON      // 放电MOS开
COMM_DSG_OFF     // 放电MOS关
COMM_CHG_ON      // 充电MOS开
COMM_CHG_OFF     // 充电MOS关
COMM_CLEAR_ALERT // 清除告警
COMM_ENTER_SHIP  // 进入关机模式
```

### 执行流程
```
外部指令 → 设置 Bq769xx_Oper_Type
           ↓
主循环检测 Bq769xx_Oper_EN
           ↓
执行对应操作 (DSGSET/CHGSET)
           ↓
清零标志，完成
```

---

## 状态机设计最佳实践

### 1. 单一状态变量
```c
static u8 stup_balan = 0;  // 均衡状态机变量
```

### 2. switch-case结构
```c
switch(stup_balan) {
    case PCB_SEQ_INIT:
        // 初始化逻辑
        stup_balan = PCB_SEQ_WAIT;
        break;
    case PCB_SEQ_WAIT:
        // 等待逻辑
        if(condition) stup_balan = PCB_SEQ_CHK;
        break;
    // ...
}
```

### 3. 时间片驱动
```c
if(TaskTimePare.Tim200ms_flag == 1) {
    // 状态机处理
}
```

### 4. 非阻塞设计
- 不使用delay等待
- 使用计数器记录时间
- 每次调用推进一个状态

---

## AI助手参考规范
当用户询问状态机相关问题时：
1. 先画出状态切换图
2. 说明每个状态的进入/退出条件
3. 给出代码实现示例
4. 标注关键代码位置 (文件名:行号)
