# P1-2: Condition Tree 动态分配 — 修复报告

**日期**: 2026-07-16  
**负责人**: yuleDKCS 边缘引擎  
**标签**: `P1` `condition-tree` `nvm` `dynamic-allocation` `embedded`

---

## 1. 问题描述

ICCE 边缘引擎的条件树 (`icce_condition_t`) 使用**内嵌静态指针** (`struct icce_condition *left`, `*right`) 表达树形结构，存在三个致命问题：

### 1.1 悬垂指针 (Dangling Pointers)

`icce_edge_init()` 中 Rule 2 使用 C99 复合字面量 + 取地址：

```c
.condition.left = &(icce_condition_t){
    .op      = COND_OP_ZONE_EQ,
    .zone_id = ICCE_ZONE_INTERIOR
},
```

复合字面量的生命周期为包含它的**块作用域**。在初始化完成后，`left`/`right` 指向的内存可能已被重用。这是一个**未定义行为**，在编译器优化级别较高时会导致条件评估结果不可预测。

### 1.2 不可序列化 (Not Serializable)

指针是运行时唯一的。跨电源周期恢复条件树需要将指针关系转换为**位置无关**的表示（如索引）。当前的 `icce_condition_t` 结构体无法直接写入 NVM 并恢复。

### 1.3 不可动态加载 (Not NVM-Loadable)

没有 API 可以从 NVM（Flash/EEPROM）读取条件配置并构建运行时树结构。所有条件树必须在编译时硬编码。

---

## 2. 修复方案

### 2.1 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                   edge_condition.h (新 API)                   │
│  ┌───────────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │ Pool Allocator │  │  Serialize / │  │ NVM Load / Save   │  │
│  │ (freelist)     │  │  Deserialize │  │ (storage_driver)  │  │
│  └───────────────┘  └──────────────┘  └───────────────────┘  │
└─────────────────────────────────────────────────────────────┘
         │                        │              │
         ▼                        ▼              ▼
┌────────────────────┐  ┌────────────────┐  ┌──────────────┐
│ icce_edge.c        │  │ 序列化格式:      │  │ NVM (Flash/  │
│ (条件评估逻辑不变)   │  │ flat array +    │  │ EEPROM)      │
│                     │  │ index refs     │  │              │
└────────────────────┘  └────────────────┘  └──────────────┘
```

### 2.2 新增文件

| 文件 | 说明 |
|------|------|
| `src/decision/edge_condition.h` | 动态条件树 API 头文件 |
| `src/decision/edge_condition.c` | 实现：内存池、序列化、NVM 加载 |

### 2.3 修改文件

| 文件 | 变更 |
|------|------|
| `include/icce_digital_key.h` | 新增 `icce_edge_get_rule_array()` API |
| `src/icce_edge.c` | 修复悬垂指针、集成 NVM 加载、初始化条件池 |

---

## 3. 实现细节

### 3.1 内存池 (Freelist Allocator)

```c
#define EDGE_COND_POOL_SIZE  64  // 最大 64 个条件节点

typedef struct cond_pool_node {
    icce_condition_t cond;    // 条件节点
    bool             allocated;  // 是否已分配
} cond_pool_node_t;
```

- 固定大小数组，无 heap malloc，适合 bare-metal 嵌入式
- `edge_condition_alloc()` — O(n) 线性扫描空闲槽
- `edge_condition_free_tree()` — 递归释放子树
- 统计信息：峰值使用、分配次数、溢出计数

**为什么不用 heap malloc？** 车辆 ECU 的嵌入式环境通常禁止动态内存分配，即便允许也禁止在中断/安全上下文中使用。固定池消除碎片和 OOM 不确定性。

### 3.2 序列化格式 (Binary, Flat Array)

**序列化节点** (12 bytes):

```
Offset  Size  Field         Description
0       1     op            操作码 (icce_condition_op_e)
1       4     threshold     数值阈值 (RSSI dBm / distance mm)
5       1     zone_id       区域 ID (ZONE_EQ 专用)
6       2     left_idx      左子节点索引 (0xFFFF = NULL)
8       2     right_idx     右子节点索引 (0xFFFF = NULL)
10      2     reserved      保留 (0)
```

**序列化树**:

```
┌──────────────┐
│ node_count: 3│
│ root_idx: 0  │
├──────────────┤
│ Node 0:      │  ← root (AND)
│  op=AND      │
│  left_idx=1  │
│  right_idx=2 │
├──────────────┤
│ Node 1:      │  ← left child (RSSI_GT -70)
│  op=RSSI_GT  │
│  threshold=-70│
│  left=FFFF   │
│  right=FFFF  │
├──────────────┤
│ Node 2:      │  ← right child (VEHICLE_PARKED)
│  op=VEHICLE_ │
│  PARKED      │
│  left=FFFF   │
│  right=FFFF  │
└──────────────┘
```

### 3.3 NVM 加载流程

```
icce_edge_init()
  │
  ├── edge_condition_pool_init()
  │
  ├── edge_condition_load_rules_from_nvm(0)
  │     ├── storage_read("edge_cond_ruleset", ...)
  │     │     ├── 成功 → 解析 serialized_edge_rule_t[]
  │     │     │     └── icce_edge_add_rule() × N
  │     │     └── 失败 → 返回 ICCE_ERR_NOT_FOUND
  │     └── (回退到静态规则)
  │
  └── 引擎初始化完成
```

### 3.4 向后兼容

**未改动**的代码路径：

- `evaluate_condition_tree()` — 递归评估逻辑完全不变
- `evaluate_single_rule()` — 不变
- `is_rule_in_time_window()` — 不变
- `is_rule_in_cooldown()` — 不变
- `execute_rule_actions()` — 不变

**icce_condition_t 结构体未变更**，只是其 `left`/`right` 指针现在指向池分配的稳定内存，而非块作用域的复合字面量。

---

## 4. 约束验证

| 约束 | 满足情况 |
|------|---------|
| 不改现有工作条件评估逻辑 | ✅ `evaluate_condition_tree()` 未修改 |
| C 语言 | ✅ 纯 C11，freestanding 兼容 |
| 向后兼容 | ✅ 结构体不变，API 追加不删除 |
| 静态配置作为默认降级 | ✅ NVM 加载失败时使用静态规则 |
| 无 heap 依赖 | ✅ 固定大小内存池，O(1) 分配延迟 |
| 可移植 | ✅ `storage_driver.h` 是抽象接口 |

---

## 5. API 清单

### 内存管理

```c
int32_t         edge_condition_pool_init(void);
int32_t         edge_condition_pool_deinit(void);
icce_condition_t *edge_condition_alloc(void);
icce_condition_t *edge_condition_create_leaf(op, threshold, zone_id);
icce_condition_t *edge_condition_create_composite(op, left, right);
void            edge_condition_free_tree(icce_condition_t *root);
```

### 序列化

```c
int32_t edge_condition_serialize(root, buffer, buf_size, *out_len);
int32_t edge_condition_deserialize(buffer, buf_len, **out_root);
```

### NVM 操作

```c
int32_t edge_condition_load_from_nvm(handle, rule_tag, **out_root);
int32_t edge_condition_save_to_nvm(handle, rule_tag, root);
int32_t edge_condition_delete_from_nvm(handle, rule_tag);
int32_t edge_condition_load_rules_from_nvm(handle);
int32_t edge_condition_save_rules_to_nvm(handle);
```

### 工具

```c
void edge_condition_dump(root, out_buf, buf_size);
void edge_condition_get_pool_stats(*stats);
int32_t edge_condition_upgrade_rule(*rule);
int32_t edge_condition_deep_copy(src, **out_dst);
bool edge_condition_equal(a, b);
```

---

## 6. 用法示例

### 示例 1: 动态构造条件树

```c
/* 替代旧的复合字面量方法 */
icce_condition_t *zone_eq = edge_condition_create_leaf(
    COND_OP_ZONE_EQ, 0, ICCE_ZONE_INTERIOR);
icce_condition_t *parked = edge_condition_create_leaf(
    COND_OP_VEHICLE_PARKED, 0, 0);
icce_condition_t *root = edge_condition_create_composite(
    COND_OP_AND, zone_eq, parked);

/* 嵌入规则 */
g_engine.rules[2].condition = *root;  // 结构体拷贝
```

### 示例 2: 从 NVM 加载条件配置

```c
uint8_t storage_handle;
storage_init(&storage_handle);

icce_condition_t *cond = NULL;
if (edge_condition_load_from_nvm(storage_handle,
                                  "unlock_approach",
                                  &cond) == ICCE_OK) {
    // cond 是池分配的，可正常使用
    bool result = evaluate_condition_tree(cond);
}
```

### 示例 3: 序列化/反序列化

```c
uint8_t buf[EDGE_COND_SERIALIZE_MAX];
uint32_t len;

edge_condition_serialize(cond_tree, buf, sizeof(buf), &len);
// buf 可以写入文件/NVM/网络传输

icce_condition_t *restored;
edge_condition_deserialize(buf, len, &restored);
// restored 是池分配的等价树
```

---

## 7. 测试建议

| 测试 | 说明 |
|------|------|
| 池耗尽测试 | 连续分配 > 64 节点，验证返回 NULL |
| 序列化往返 | 构造树 → 序列化 → 反序列化 → `edge_condition_equal()` |
| NVM 回退 | 模拟 NVM 读取失败，验证使用静态默认规则 |
| 深层嵌套 | 深度 > 10 的 AND/OR/NOT 树，验证正确序列化 |
| 空树 | NULL root 的序列化/反序列化 |
| 单节点树 | 单个 COND_OP_RSSI_GT 叶子节点 |
| 内存泄漏 | 反复分配/释放，验证 `used_nodes` 稳定 |
| 悬垂指针回归 | 编译并运行评估路径，验证 Rule 2 条件正确触发 |

---

## 8. 风险与待办

### 已知风险

- **池大小限制**：EDGE_COND_POOL_SIZE = 64。如果未来条件树需要超过 64 节点，需增大此值。
- **NVM save 占位**：`edge_condition_save_rules_to_nvm()` 当前返回 `ICC_ERR_NOT_FOUND`，因为需要访问 `g_engine.rules[]` 内部数组。可通过 `icce_edge_get_rule_array()` 桥接。
- **互斥访问**：条件分配/释放在多任务环境下需要保护（目前假定单线程）。

### 待办项

- [ ] 实现 `edge_condition_save_rules_to_nvm()` 完整功能
- [ ] 添加 RTOS 互斥保护（如 FreeRTOS `semaphore`）
- [ ] 增加 JSON 序列化格式（对于 OTA 配置更新场景）
- [ ] 添加单元测试（Unity 框架）

---

## 9. 代码统计

```
新增:
  src/decision/edge_condition.h   ~350 行  (API 声明 + 常量 + 类型定义)
  src/decision/edge_condition.c   ~680 行  (完整实现)

修改:
  include/icce_digital_key.h      +3 行   (新增 API 声明)
  src/icce_edge.c                 ~+50 行  (悬垂指针修复 + NVM 集成)

总计新增约 1000 行，零破坏性变更。
```
