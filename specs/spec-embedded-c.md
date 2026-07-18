# Embedded C Unit Test Infrastructure — OpenSpec

| Field | Value |
|---|---|
| Spec ID | SPEC-EMBEDDED-C-001 |
| Status | Draft |
| Author | Claude |
| Date | 2026-07-18 |
| Review | Pending (@ponytail) |

---

## 1. Overview

yuleDKCS 嵌入式端包含 37 个 .c 源文件 + 17 个 .h 头文件，涵盖 ICCE、ICCOA、CCC 三大数字钥匙协议栈及 Unified 抽象层。截至目前，**零单元测试覆盖、零 MISRA 静态分析**。本项目旨在从零搭建 C 单元测试基础设施，使后续协议修改和移植具备回归验证能力。

### 1.1 测试策略

```
协议层测试: 编译真实协议源文件 + 桩化 HAL → 验证 API 行为
基础设施: Unity (vendor/unity/) 单头文件测试框架
CI 集成: 每个 PR 在 L2 阶段自动编译并运行 C 单元测试
```

---

## 2. Requirements

### SWR-001: C 单元测试框架引入

**SHALL** 在 `embedded/tests/vendor/unity/` 下引入 Unity 测试框架（自定义极小版本）。

**Rationale**: Unity 是嵌入式 C 单元测试的行业标准框架之一，单文件架构、零依赖、可交叉编译，适合嵌入式项目。

**Verification**: `make test_icce` 在 x86 宿主机上编译并运行通过。

---

### SWR-002: ICCE 协议栈单元测试

**SHALL** 为 ICCE 协议栈提供独立的单元测试文件 `embedded/tests/test/test_icce.c`，覆盖：

- `ICCE_CORE_001..003`: 初始化/反初始化/主循环生命周期
- `ICCE_ZONE_001..004`: 区域分类（远/中/近/附近/车内）及定义查询
- `ICCE_UWB_001..003`: UWB 会话创建/启动/停止/多会话上限检查
- `ICCE_SEC_001..004`: 安全绑定、认证挑战-响应、会话验证
- `ICCE_VEH_001..004`: 车辆初始化/状态查询/控制命令/回调注册
- `ICCE_EDGE_001..006`: 边缘规则引擎初始化/添加/移除/优先级/距离评估

**SHALL** 以上 24 个测试用例在 `make test_icce` 下全部通过（0 failures）。

**Rationale**: ICCE 是高价值业务模块（数字钥匙），测试覆盖率不足会引入功能回归风险。

---

### SWR-003: CCC 协议栈单元测试

**SHALL** 维护并扩展 `embedded/tests/test/test_ccc_dk_core.c`，覆盖 CCC 协议栈核心 API：

- 初始化/反初始化
- NFC 场检测/监听
- BLE 连接/断开
- UWB 会话管理/区域分类
- 安全模块（密钥存储/签名验证）

**SHALL** 在 `make test_ccc` 下编译并运行（已知行为差异测试标记为已知问题）。

**Rationale**: CCC 是核心协议栈，与 ICCOA 共享部分 HAL，需要持续回归验证。

---

### SWR-004: ICCOA 协议栈单元测试

**SHALL** 维护 `embedded/tests/test/test_iccoa_dk_core.c` 和 `test_iccoa_ble.c`，覆盖：

- ICCOA 核心初始化/认证请求/认证验证
- BLE 广播/数据收发/回调
- DK30 协议（校验和/响应）
- 车辆控制/状态
- DK40 初始化

**SHALL** 在 `make test_iccoa` 下全部通过（0 failures）。

**Rationale**: ICCOA 是对 CCC 的扩展，测试需跟随协议演进。

---

### SWR-005: CI 中集成 C 单元测试

**SHALL** 在 `.github/workflows/ci.yml` 的 `l2-integration-and-sast` job 中增加 Embedded C 单元测试步骤：

```yaml
- name: Embedded C Unit Tests (ICCOA/CCC/ICCE/Unified)
  working-directory: embedded/tests
  run: |
    make clean
    make test_iccoa test_ccc test_icce 2>&1
    echo "✅ C unit tests complete"
```

**SHALL** 在 `ubuntu-latest` runner 上使用系统默认 GCC 编译，不依赖交叉工具链。

**SHALL** 上报测试结果到 GitHub Actions Summary 并上传 `embedded/tests/build/` 作为构建产物。

**Rationale**: CI 集成确保每次 PR 自动执行嵌入式 C 回归测试，防止协议修改引入破坏性变更。

---

## 3. Known Limitations

| Limitation | Impact | Mitigation |
|---|---|---|
| CCC 测试 2 个失败（key_store_load, key_sharing） | 已记录，不影响 ICCOA/ICCE | 标记为已知行为差异 |
| CCC real sources 需 25+ 个 board pin `-D` 宏 | 维护成本低 | 在 Makefile 中集中管理 |
| 无 MISRA 静态分析 | 长期风险 | 后续引入 cppcheck |
| `embedded/tests/support/forward_decls.h` | uwb_ncj29d6.c 的前向声明补丁 | 仅在 uwb 编译时 |-include|

---

## 4. 文件清单

| 文件 | 说明 |
|---|---|
| `specs/spec-embedded-c.md` | 本规约 |
| `reports/embedded-c-progress.md` | 执行进度报告 |
| `embedded/tests/test/test_icce.c` | ICCE 单元测试（24 用例） |
| `embedded/tests/support/stubs_hal.c` | 桩化 HAL（spi, gpio, sys_tick 等） |
| `embedded/tests/support/icce/*.h` | ICCE 内部接口桩头文件（8 个） |
| `embedded/tests/Makefile` | 更新后的 Makefile（含 ICCE + CCC 编译） |
| `.github/workflows/ci.yml` | 更新后的 CI（含 C 测试步骤） |

---

*End of spec-embedded-c.md*
