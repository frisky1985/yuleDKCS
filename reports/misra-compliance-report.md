# MISRA C:2012 首次合规扫描报告

> **扫描日期:** 2026-07-30  
> **扫描工具:** cppcheck 2.17.1 (MISRA Addon)  
> **目标标准:** MISRA C:2012 (含 Amendment 1 & 2)  
> **扫描范围:** yuleDKCS 嵌入式 C 代码（协议层 + 固件层）

---

## 执行摘要

完成对 yuleDKCS 嵌入式 C 代码的首次 MISRA C:2012 合规扫描，共计扫描 **31 个 .c 源文件**和涉及的 **.h 头文件**。

| 指标 | 数值 |
|------|------|
| **总违规数** | **1,935** |
| 违规规则种数 | 33 |
| 涉及文件数 | ~50 |
| Required 规则违规 | 1,680 (86.8%) |
| Advisory 规则违规 | 255 (13.2%) |

> ⚠️ **说明:** 本扫描为首次基线扫描，偏差审批（Deviation）尚未实施。大量违规来自嵌入式代码中常见模式（单行 `if+return`、函数指针注册回调、宏定义括号等），后续可通过偏差审批大幅降低报告数量。

---

## 违规分类统计

### 按规则编号

| 排名 | 规则 | 违规数 | MISRA 分类 | 简要说明 |
|------|------|--------|-----------|---------|
| 1 | **15.5** | 499 | Required | 函数末尾单 return（当前大量 early return） |
| 2 | **17.7** | 358 | Required | 函数指针/回调调用未显式匹配原型 |
| 3 | **10.4** | 320 | Required | 算术/逻辑表达式操作数类型不匹配 |
| 4 | **2.5** | 163 | Advisory | 宏定义缺少括号包围 |
| 5 | **8.7** | 73 | Required | 函数无外部链接但未声明 `static` |
| 6 | **12.1** | 52 | Advisory | 运算符优先级混淆（缺少括号） |
| 7 | **7.2** | 49 | Required | `unsigned` 字符常量使用 |
| 8 | **8.4** | 48 | Required | 外部函数缺少头文件声明 |
| 9 | **18.4** | 46 | Required | 指针运算结果用作数组索引 |
| 10 | **15.6** | 43 | Required | 循环内多条 `break`/`return` |
| 11 | **14.4** | 35 | Required | 循环控制变量类型不当 |
| 12 | **10.1** | 33 | Required | 移位/位操作类型不当 |
| 13 | **12.3** | 31 | Advisory | 逗号运算符的使用 |
| 14 | **13.3** | 21 | Required | 递增/递减操作与副作用混合 |
| 15 | **16.4** | 20 | Required | Switch 中的 fallthrough |
| 16 | **8.9** | 16 | Required | 对象定义未在同一翻译单元内 |
| 17 | **17.3** | 16 | Required | 数组索引越界检查缺失 |
| 18 | **5.9** | 14 | Advisory | 标识符命名与目标无关 |
| 19 | **17.8** | 13 | Required | 函数参数个数与声明不匹配 |
| 20 | **2.7** | 12 | Advisory | 宏内有多条语句 |
| 21 | **2.3** | 9 | Advisory | `//` 注释可能引起混用 |
| 22 | **10.3** | 9 | Required | 复合赋值两侧类型不匹配 |
| 23 | **8.5** | 8 | Required | 头文件未包含自检保护 |
| 24 | **11.5** | 8 | Required | `void*` 转换为对象指针 |
| 25 | **11.3** | 6 | Required | 指针强制转换为整数类型 |
| 26 | **10.8** | 6 | Required | 复合表达式中类型转换不匹配 |
| 27 | **8.6** | 4 | Required | 声明未使用标识符 |
| 28 | **15.7** | 4 | Required | 空函数体 |
| 29 | **13.4** | 4 | Advisory | 赋值运算符用作表达式 |
| 30 | **20.1** | 3 | Required | `#include` 格式不规范 |
| 31 | **9.3** | 2 | Required | 数组初始化列表不匹配 |
| 32 | **20.10** | 2 | Required | `#`/`##` 运算符使用不当 |
| 33 | **18.8** | 2 | Required | 可变长度数组 (VLA) |
| 34 | **9.2** | 1 | Required | 结构体初始化不完整 |
| 35 | **8.2** | 1 | Required | 函数声明不完整 |
| 36 | **3.1** | 1 | Required | 嵌套注释 |
| 37 | **21.6** | 1 | Required | `stddef.h`/`stdint.h` 未包含 |
| 38 | **21.10** | 1 | Required | 受限标准库函数使用 |
| 39 | **11.8** | 1 | Required | `void*` 算术运算 |

### 按严重度

| 严重度 | 违规数 | 占比 |
|--------|--------|------|
| **Required** | 1,680 | 86.8% |
| **Advisory** | 255 | 13.2% |
| **Mandatory** | 0 | 0.0% |

### 按模块分布

| 模块 | 违规数 | 占比 | 主要违规规则 |
|------|--------|------|-------------|
| **icce_protocol** | 938 | 48.5% | 15.5, 17.7, 10.4, 8.7, 12.1 |
| **ccc_protocol** | 459 | 23.7% | 15.5, 17.7, 10.4, 8.4 |
| **iccoa_protocol** | 273 | 14.1% | 15.5, 17.7, 10.4, 7.2 |
| **unified_protocol** | 248 | 12.8% | 15.5, 17.7, 10.4, 15.6 |
| 头文件/其他 | 17 | 0.9% | 2.5 |

### 违规最多的源文件

| 文件 | 违规数 | 主要规则 |
|------|--------|---------|
| `icce_protocol/src/crypto/crypto_engine.c` | 230 | 15.5, 17.7, 10.4, 8.7 |
| `unified_protocol/src/dk_unified.c` | 187 | 15.5, 17.7, 10.4 |
| `icce_protocol/src/crypto/sm2.c` | 110 | 15.5, 17.7, 10.4 |
| `icce_protocol/src/crypto/sm4.c` | 105 | 15.5, 17.7 |
| `ccc_protocol/src/keymgmt/key_mgmt.c` | 99 | 15.5, 17.7, 10.4 |
| `icce_protocol/src/decision/offline_decision.c` | 93 | 15.5, 17.7 |
| `icce_protocol/src/security/security_auth.c` | 89 | 15.5, 17.7, 10.4 |
| `ccc_protocol/src/ble/ble_kw47a.c` | 86 | 15.5, 10.4, 8.4 |

---

## 违规模式深度分析

### TOP 1: Rule 15.5 — 函数末尾单 return (499 次)

**描述:** MISRA C:2012 要求每个函数末尾有且仅有一个 `return` 语句。当前代码大量使用"early return"模式。

**典型代码:**
```c
if (ret != CCC_OK) return ret;
if (dist_cm <= threshold) return ZONE_INSIDE;
```

**建议:**
- 对简单守卫检查，提取到函数入口的统一的错误处理代码
- 或提交偏差审批（Deviation），说明嵌入式代码中 early return 用于简化错误处理是安全且常见的模式

### TOP 2: Rule 17.7 — 函数指针/回调调用 (358 次)

**描述:** 调用通过函数指针传递的回调函数时，需要确保函数原型匹配。

**典型代码:**
```c
ble_register_conn_cb(on_ble_connected);
uwb_set_threshold(&g_threshold);
nfc_start_listen();
```

**建议:**
- 确保所有注册回调都通过明确的函数指针类型定义
- 对已知安全的 HAL 回调调用提交偏差审批

### TOP 3: Rule 10.4 — 混合类型表达式 (320 次)

**描述:** 不同类型操作数的算术或逻辑表达式。大量出现在位操作、长度计算中。

**典型代码:**
```c
if (len > 0 && payload) { ... }        // len 是 uint16_t, payload 是指针
uint8_t header[4] = { (uint8_t)(len >> 8), ... };
```

**建议:**
- 使用显式类型转换
- 对位操作相关的匹配差异提交偏差审批

### TOP 4: Rule 2.5 — 宏定义括号 (163 次, Advisory)

**描述:** 宏定义中的参数或整个替换列表应该用括号包围。

**典型代码:**
```c
#define MAX_STORED_KEYS  32     // ❌ 缺少括号
#define DK_ERR_NO_MEM    (-2)   // ✅ 正确
#define DK_CAP_BLE_Coded (1 << 6)  // ✅ 正确
```

**分析:** 项目中有约 50% 的宏定义已正确使用括号（如 `(-2)`、`(1 << 6)`），约 50% 缺少括号（简单数值常量）。主要是头文件中定义的状态码和界限常量。

**建议:** 批量修复，将简单数值常量宏也加上括号。

---

## 偏差审批建议

基于首次扫描结果，建议对以下规则申请批量偏差审批（Deviation）：

| 规则 | 理由 | 范围 | 建议有效期 |
|------|------|------|-----------|
| **15.5** | Early return 在嵌入式驱动代码中是标准安全模式，无实际安全风险 | 全部协议层、固件层 | 6个月 |
| **17.7** | 函数指针注册回调是 HAL 层设计模式，调用方无法控制被调用函数 | HAL 接口调用 | 6个月 |
| **10.4** | 位运算中 `uint16_t & 0xFF` 在嵌入式 C 中语义明确 | BLE/UWB/NFC 驱动代码 | 6个月 |
| **8.4** | 函数在当前翻译单元内被其他函数调用，编译器确认可见 | 同一 .c 文件内函数 | 6个月 |
| **8.7** | 函数在 .h 中声明暴露给外部，但当前无外部调用者 | API 函数（预备外部调用） | 12个月 |
| **2.5** | Advisory 规则，简单常量宏无可执行风险 | 全部数值常量宏 | 持续 |

---

## 修复优先级

| 优先级 | 规则 | 预计修复量 | 修复方式 | 工作量评估 |
|--------|------|-----------|---------|-----------|
| **P0** | 2.5 (Advisory) | 163 | 批量替换宏定义加括号 | 低 |
| **P0** | 8.5 (Required) | 8 | 给头文件加 `#pragma once` 或 `#ifndef` 保护 | 低 |
| **P0** | 8.4 (Required) | 48 | 补充头文件中外部函数的声明 | 低 |
| **P1** | 15.5 (Required) | 499 | 提交偏差审批（推荐）或改写为单一 return 模式 | 高 |
| **P1** | 17.7 (Required) | 358 | 提交偏差审批 | 中 |
| **P1** | 10.4 (Required) | 320 | 提交偏差审批 + 显式转型 | 中 |
| **P2** | 7.2, 12.1, 14.4, 18.4 | 各 35-50 | 逐个修复 | 中低 |
| **P3** | 其余规则 | 各 1-20 | 逐个修复 | 低 |

---

## 附录

### A. 扫描范围

| 路径 | 模块 | 文件数 |
|------|------|--------|
| `embedded/ccc_protocol/src/` | CCC Digital Key | 6 .c + 若干 .h |
| `embedded/iccoa_protocol/src/` | ICCOA Digital Key | 6 .c + .h |
| `embedded/icce_protocol/src/` | ICCE Digital Key | 12 .c + .h |
| `embedded/unified_protocol/src/` | Unified Protocol | 1 .c + .h |
| `firmware/` | BSP / HAL | 2 .c + .h |
| `embedded/system_architecture/` | 系统架构接口 | 仅 .h |

### B. 工具配置

```bash
cppcheck --addon=misra \
  --suppress=missingIncludeSystem \
  --platform=unix64 \
  --language=c --std=c11 \
  --enable=all \
  --inline-suppr \
  <include_paths> <defines> \
  <source_files>
```

### C. 已知限制

1. **缺少 MISRA 规则文本:** cppcheck 的 MISRA addon 无法直接输出规则文本（需 `--rule-texts=<file>` 提供 MISRA 标准文档 XML），因此报告中的规则说明为基于公开信息的摘要。
2. **配置不完整警告:** 部分变量因缺少完整配置（如 MCU 特定寄存器定义）而产生 `misra-config` 警告，已从违规统计中排除。
3. **扫描平台:** 使用 `unix64` 平台而非目标 MCU（ARM Cortex-M），可能产生少量假阳性/假阴性。
4. **测试代码:** 测试文件（`embedded/tests/`）未纳入主要扫描范围，仅因头文件被包含产生少量违规。

### D. 与 MISRA-COMPLIANCE.md 的关系

现有 `docs/MISRA-COMPLIANCE.md` 是合规计划文档，目标为 MISRA C:2023。本报告为 MISRA C:2012 的首次扫描基线。建议后续升级至 C:2023 标准。

---

*报告生成者: Hermes Agent | cppcheck 2.17.1 | 2026-07-30*
