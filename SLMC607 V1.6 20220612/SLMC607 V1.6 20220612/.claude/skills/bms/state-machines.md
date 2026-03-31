# SLMC607 状态机设计参考

## 系统模式状态机 (TaskFun.c:40-74)

```
电流≥20mA ──────→ CHARGE
    ↑              ↓
    │             ←┘
    │   电流<20mA
    │
┌───────┐
│STANDBY│──→ 电流≤-20mA → DISCHARGE
└───────┘         ↑
    │             │
    │ 静置2h      │ 电流>-20mA
    ↓             │
  SLEEP ←─────────┘
    │
    │ 电流>20mA
    └────→ 唤醒到STANDBY
```

### 切换条件
| 条件 | 目标状态 |
|------|----------|
| 电流≥20mA | CHARGE |
| 电流≤-20mA | DISCHARGE |
| \|电流\|<20mA | STANDBY |
| STANDBY静置2h | SLEEP |
| SLEEP检测到电流 | STANDBY |

---

## 均衡状态机 (bq769xx.c:492-611)

```
INIT → WAIT(2s) → CHK → SELECT → MAP → SET → BALA_TIM(180s)
  ↑                                                    ↓
  └────────────────────────────────────────────────────┘
```

### 状态说明
- **INIT**: 关闭所有均衡，清零标志
- **WAIT**: 等待10×200ms=2s稳定
- **CHK**: 检查待机模式？压差≥100mV？电压≥3.0V？
- **SELECT**: 冒泡排序选电压最高的3串
- **MAP**: 逻辑电芯号 → BQ769xx寄存器位
- **SET**: 写入CellBal1/2/3寄存器
- **BALA_TIM**: 均衡900×200ms=180s

---

## 代码模板

### 简单状态机
```c
typedef enum {
    STATE_INIT,
    STATE_WAIT,
    STATE_RUN,
    STATE_MAX
} State_t;

static State_t state = STATE_INIT;
static u16 wait_cnt = 0;

void state_machine(void) {
    if(!time_flag) return;  // 时间片检查

    switch(state) {
        case STATE_INIT:
            // 初始化
            wait_cnt = 0;
            state = STATE_WAIT;
            break;

        case STATE_WAIT:
            if(++wait_cnt > 10) {  // 等待10个周期
                state = STATE_RUN;
            }
            break;

        case STATE_RUN:
            // 运行逻辑
            if(done) {
                state = STATE_INIT;  // 循环
            }
            break;
    }
}
```
