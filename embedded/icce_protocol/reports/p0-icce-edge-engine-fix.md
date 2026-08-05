# P0-2: ICCE 边缘计算引擎 — 真实触发逻辑实现

## 概览

| 项目 | 值 |
|------|------|
| **目标文件** | `src/icce_edge.c`, `include/icce_digital_key.h` |
| **P0 优先级** | P0-2 (P0 = 专家审阅通过的阻塞级) |
| **状态** | ✅ 完成 |

## 问题分析

### 发现的问题

1. **`icce_digital_key.h` 仅有 stub**
   - 头文件只声明了 3 个函数和 1 个宏（`ICCE_OK`）
   - 所有 `.c` 文件使用的类型（`icce_edge_rule_t`, `icce_trigger_e`, `icce_zone_e`, `icce_action_e`, `icce_vehicle_status_t`, `icce_uwb_session_t` 等）**从来没被定义**过
   - 至少 10 个枚举值、8 个结构体、12 个约束常量缺失

2. **`icce_edge.c` 是空壳**
   - `icce_edge_process_trigger()`: 只找了最高优先级规则，**没检查条件**（RSSI、车辆状态、时间窗口）
   - `icce_edge_evaluate()`: 只做了简单的距离阈值比较；RSSI 部分完全 `(void)zone` 无操作
   - **没有状态机**: 没有 IDLE/MONITORING/TRIGGERED/ACTIVE/FALLBACK 状态管理
   - **没有时间触发**: 没有定时器轮询或周期性评估
   - **没有复合条件**: 不支持 `RSSI > -70dBm && 车辆静止` 这类组合条件

## 改动内容

### 文件 1: `include/icce_digital_key.h` (完全重写)

**之前**: 592 字节，仅含 3 个 stub 函数声明

**之后**: ~13 KB 完整类型定义，包含：

| 类型 | 内容 |
|------|------|
| **错误码** | `ICCE_OK`, `ICCE_ERR_PARAM`, `ICCE_ERR_NOT_FOUND`, `ICCE_ERR_BUSY`, `ICCE_ERR_NO_MEM`, `ICCE_ERR_SECURITY` |
| **Zone 枚举** | `icce_zone_e` — NONE, FAR, MID, NEAR, VICINITY, INTERIOR, MAX |
| **Zone 结构体** | `icce_zone_def_t` — zone + inner_mm + outer_mm + actions_mask |
| **Trigger 枚举** | `icce_trigger_e` — 8 种 (ZONE_ENTER, ZONE_EXIT, DISTANCE, BLE_RSSI, UWB_RANGE, VEHICLE_STATE, TIME_INTERVAL, COMPOUND) |
| **Action 枚举** | `icce_action_e` — 8 种 (UNLOCK, LOCK, START, STOP, CLIMATE, LIGHTS, HORN, CUSTOM) |
| **条件运算符** | `icce_condition_op_e` — 13 种 (AND, OR, NOT, RSSI_GT/LT, DIST_GT/LT, ZONE_EQ, VEHICLE_STOPPED/ LOCKED/PARKED, TIME_IN_WINDOW) |
| **条件节点** | `icce_condition_t` — 递归树结构 (op + threshold + zone_id + left/right) |
| **规则结构体** | `icce_edge_rule_t` — 完整字段 (trigger, zone_id, thresholds, time_mask, interval_ms, actions, priority, cooldown, condition tree) |
| **状态机枚举** | `icce_edge_state_e` — IDLE, MONITORING, TRIGGERED, ACTIVE, FALLBACK |
| **车辆状态** | `icce_vehicle_status_t` — 12 个字段 (含 gear_position, speed_kmh) |
| **UWB 会话** | `icce_uwb_session_t` — session_id, channel, role, mac_mode, distance_mm, quality |
| **公共 API** | 所有模块的完整函数声明（edge, zone, vehicle, security, uwb, ble, core） |

### 文件 2: `src/icce_edge.c` (完全重写)

**之前**: ~135 行 stub

**之后**: ~560 行真实实现

---

## 实现细节

### 1. 状态机

```
                   ┌─────────────────────────────────────┐
                   │          ┌──────────┐               │
                   │    init  │  IDLE    │               │
                   │          └──────────┘               │
                   │              │                      │
                   │         start│                      │
                   │              ▼                      │
                   │    ┌──────────────────┐             │
                   │    │   MONITORING     │ ◄──────────┐│
                   │    │  (持续评估触发器) │            ││
                   │    └───────┬──────────┘            ││
                   │            │ trigger fired          ││
                   │            ▼                       ││
                   │    ┌──────────────────┐   timeout  ││
                   │    │   TRIGGERED      │────────────┘│
                   │    │  (执行动作序列)   │            ││
                   │    └───────┬──────────┘            ││
                   │       ┌───┴───┐                    ││
                   │       ▼       ▼                    ││
                   │  ┌────────┐ ┌──────────┐          ││
                   │  │ ACTIVE │ │ FALLBACK │──────────┘│
                   │  │        │ │ (重试)   │           │
                   │  └────────┘ └──────────┘           │
                   └─────────────────────────────────────┘
```

- **IDLE → MONITORING**: 引擎初始化后进入监控状态
- **MONITORING**: 评估所有启用的规则，响应任何触发器
- **MONITORING → TRIGGERED**: 规则匹配 → 执行动作
- **TRIGGERED → ACTIVE**: 动作执行成功
- **ACTIVE → MONITORING**: 超时（默认 30 秒）后自动回退
- **TRIGGERED → FALLBACK**: 动作执行失败
- **FALLBACK → MONITORING**: 重试耗尽（默认 3 次）或规则不再有效
- **FALLBACK 内**: 每 5 秒重试一次

### 2. 事件触发

#### BLE RSSI 触发
```c
// 规则 3: RSSI > -70dBm 触发车灯提示
g_engine.rules[3] = (icce_edge_rule_t){
    .trigger = ICCE_TRIGGER_BLE_RSSI,
    .threshold_rssi = -70,
    .actions = { ICCE_ACTION_LIGHTS },
    .priority = 100,
    .enabled = true
};
```

- 使用 **5 点滑动平均** 滤波，防止 RSSI 抖动误触发
- `icce_edge_update_rssi()` 供 BLE 栈回调调用
- 同时检查瞬时值和移动平均值，减少漏报

#### UWB 测距触发
- **3 点滑动平均** 平滑距离数据
- 通过 `icce_edge_evaluate()` 连续评估
- 支持复合条件组合（如距离 + 车辆状态）

#### 车辆状态变更触发
```c
// 检测引擎熄火 + 挂 P 挡 → 自动上锁
// 通过 icce_edge_update_vehicle_state() 触发
```
- 检测 `engine_status`, `lock_status`, `door_status`, `gear_position` 变化
- 支持 `COND_OP_VEHICLE_STOPPED`, `COND_OP_VEHICLE_LOCKED`, `COND_OP_VEHICLE_PARKED`

### 3. 时间触发

```c
// 规则 4: 每 60 秒执行状态同步
g_engine.rules[4] = (icce_edge_rule_t){
    .trigger = ICCE_TRIGGER_TIME_INTERVAL,
    .interval_ms = 60000,
    .actions = { ICCE_ACTION_CUSTOM },
    .priority = 50,
    .enabled = true
};
```

- `icce_edge_timer_tick(elapsed_ms)` 需从主循环定期调用
- 引擎内部追踪 `last_tick`，累积 elapsed_ms
- 时间间隔到期 + 不处于冷却期 → 执行动作

### 4. 复合条件触发

条件表达式树支持：

| 运算符 | 含义 | 示例 |
|--------|------|------|
| `COND_OP_AND` | 所有子条件为真 | `RSSI > -70 AND 车辆静止` |
| `COND_OP_OR` | 任一子条件为真 | `UWB距离 < 2m OR RSSI > -50` |
| `COND_OP_NOT` | 取反 | `NOT 车门已锁` |
| `COND_OP_RSSI_GT/LT` | RSSI 比较 | `RSSI_GT(-70)` |
| `COND_OP_DIST_GT/LT` | 距离比较 | `DIST_LT(2000)` |
| `COND_OP_ZONE_EQ` | 区域相等 | `ZONE_EQ(INTERIOR)` |
| `COND_OP_VEHICLE_STOPPED` | 引擎关 + 速度 0 | — |
| `COND_OP_VEHICLE_LOCKED` | 门锁状态 == 锁定 | — |
| `COND_OP_VEHICLE_PARKED` | 挡位 == P | — |
| `COND_OP_TIME_IN_WINDOW` | 当前小时在 time_mask 内 | — |

**实际规则示例 — 进入车内且已挂 P 挡时启动引擎:**
```c
g_engine.rules[2].condition = (icce_condition_t){
    .op   = COND_OP_AND,
    .left = &(icce_condition_t){
        .op      = COND_OP_ZONE_EQ,
        .zone_id = ICCE_ZONE_INTERIOR
    },
    .right = &(icce_condition_t){
        .op = COND_OP_VEHICLE_PARKED
    }
};
```

### 5. 冷却与时间窗口

- **冷却期**: 每个规则有独立 `cooldown_ms`，默认 3 秒，防止快速重复触发
- **时间窗口**: `time_mask` 是 24 位位图（bit 0 = 00:00–01:00），`0xFFFFFF` = 全天候
- **动作重试**: FALLBACK 状态下最多重试 3 次，间隔 5 秒

### 6. 向后兼容

| 原有函数 | 兼容性 |
|----------|--------|
| `icce_edge_init()` | 参数不变，初始化 5 条默认规则（含原有 3 条 + 新 RSSI 和定时） |
| `icce_edge_deinit()` | 不变 |
| `icce_edge_add_rule()` | 不变，新 rule 结构体向后兼容（新增字段默认 0） |
| `icce_edge_remove_rule()` | 不变 |
| `icce_edge_enable_rule()` | 不变 |
| `icce_edge_process_trigger()` | 签名不变，行为增强: 检查时间窗口、冷却期、复合条件 |
| `icce_edge_evaluate()` | 签名不变，行为增强: 评估距离/RSSI/UWB/车辆状态规则，含滑动平均滤波 |

**新增函数**（不冲突）:
- `icce_edge_get_state()` — 获取当前状态机状态
- `icce_edge_timer_tick()` — 周期调用，处理时间触发
- `icce_edge_update_rssi()` — BLE RSSI 更新入口
- `icce_edge_update_vehicle_state()` — 车辆状态变更入口

## 编译验证

```bash
cd /Users/stefan/yuleDKCS/embedded/icce_protocol
cmake -S . -B build-edge-test \
  -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake \
  -DCMAKE_C_FLAGS="-fsyntax-only"
cmake --build build-edge-test --check  # 语法检查
```

## 测试建议

| 测试场景 | 预期行为 |
|----------|----------|
| UWB 距离 < 2m 且 P 挡 | ON: 解锁；OFF: 无动作 |
| BLE RSSI > -70dBm 且车辆熄火 | 闪烁车灯 |
| BLE RSSI > -70dBm 但车辆正在行驶 | 无动作（复合条件不满足） |
| 60 秒定时器到期 | 执行自定义状态同步动作 |
| 10 秒内连续触发同一规则 | 冷却期内不重复执行 |
| 动作执行失败 | FALLBACK 状态，5 秒后重试，最多 3 次 |
| ACTIVE 状态超时 30 秒 | 自动返回 MONITORING |

## 文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `include/icce_digital_key.h` | 更新 | 完整类型定义 + 公共 API |
| `src/icce_edge.c` | 重写 | 真实边缘计算引擎 |
| `reports/p0-icce-edge-engine-fix.md` | 新增 | 本文档 |

## 开放问题

1. 规则持久化: 当前规则是静态初始化的，生产环境需要从 NVM 加载自定义规则
2. 诊断上报: FALLBACK 统计应通过 DLT/Log 上报
3. 条件树内存是静态嵌入的，动态条件树需要堆分配
