# 软件集成策略 (Integration Strategy)

> **项目**: yuleDKCS 数字钥匙系统
> **文档编号**: yuleDKCS-IS-001
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: ACTIVE
> **过程域**: ASPICE SWE.5.BP1 (Develop integration strategy)
> **关联**: `docs/architecture.md`（组件/接口）、`tests/integration/README.md`

---

## 1. 集成序列（自底向上）

| 阶段 | 集成内容 | 验证 | 对应需求域 |
|:-----|:---------|:-----|:-----------|
| **S1 单元级** | 各组件内函数/方法 | CI L1 (go test + coverage 门禁) | REQ-019~023 |
| **S2 协议层** | BERTLV 编解码 + 消息信封 + 签名完整性 | CI L2 契约层 (`tests/integration/`) | REQ-024~027, REQ-018 |
| **S3 组件级** | Hub 内部组件（Gateway↔Token↔Service↔Adapter） | Go 集成套件 (backend/cloud/hub/tests/integration, 16 用例/15 编号) | REQ-010~015, REQ-019~020 |
| **S4 系统级** | 手机↔车模拟器↔云 端到端 | `tests/e2e/` (carsim, 11 场景) | REQ-001~009 |
| **S5 目标环境** | 车端嵌入式 + 硬件 | HIL (S32K312 EVB) | REQ-028~035, REQ-006, REQ-016 |

**集成理由**: 协议契约（S2）先行固化接口，组件（S3）验证接口实现，系统（S4/S5）验证跨端数据流 — 每层通过才进入上层，符合自底向上策略。

## 2. 桩/驱动 (Stubs & Drivers)

| 桩/驱动 | 用途 | 位置 |
|:--------|:-----|:-----|
| carsim 车模拟器 | 模拟 TCU/车端（BLE/UWB/NFC 握手、状态机） | `tests/e2e/carsim/` |
| MobileClient 驱动 | 模拟手机端（连接/配对/控车） | `tests/e2e/client/` |
| MCAL stubs | 车端 MCAL 驱动桩（Mcu/Dio/Gpt/…） | `embedded/mcal_stubs/` |
| MockHSM | 生产环境 HSM 隔离验证 | `backend/cloud/pkg/crypto/hsm_mock_test.go` |
| HIL HardwareInterface | HIL 硬件接口封装（env check/power/报告） | `tests/hil/hil_runner.py` |

## 3. 集成测试通过标准

- 契约层（pytest）: 100% 通过（`yuleosh ci run 2` 门禁）。
- Go 组件套件: 全场景通过（build tag=integration，需 hub 二进制或运行实例）。
- 系统 E2E: 全场景通过（carsim 可达；不可达时 skip 不阻塞）。

## 4. 回归策略

- 每次合并前: CI L1（必过）+ L2（含契约层集成测试）。
- 协议/接口契约变更: 必须同步更新 `include/` 契约头 + 契约层测试（防漂移校验强制）。
- 固件版本发布前: HIL 全量回归（37 用例，P0 100%）。
- 失败处理: 分析→修复→回归记录（见 `docs/hil/HIL-TEST-RESULTS.md` §3 范例）。
