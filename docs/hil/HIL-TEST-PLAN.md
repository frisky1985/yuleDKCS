# HIL 测试计划 (HIL Test Plan)

> **项目**: yuleDKCS 数字钥匙系统
> **版本**: 1.0.0 | **日期**: 2026-08-06 | **状态**: ACTIVE
> **关联**: `docs/hil/hil-test-spec.md`（测试规格）、`docs/hil/acceptance-criteria.md`（P0/P1 验收标准）、`tests/hil/hil_runner.py`（执行器）
> **过程域**: ASPICE SWE.6.BP2 (Perform qualification tests) — 目标环境证据

---

## 1. 目标与范围

HIL（Hardware-in-the-Loop）在 **S32K312 EVB 目标硬件**上验证车端嵌入式软件（ICCE/CCC/ICCOA 协议栈 + HAL + SE050 + 电源/唤醒），覆盖嵌入式域需求 REQ-028~035 与系统级 REQ-006/009/016。

## 2. 测试环境

| 项 | 配置 |
|:---|:-----|
| 主控板 | NXP S32K312 EVB |
| 通信模组 | KW47A (BLE) · NCJ29D6 (UWB) · ST25R501 (NFC) |
| 安全芯片 | NXP SE050 (SCP03) |
| 执行器 | `tests/hil/hil_runner.py`（--domain / --test / --flash / --power-on） |
| 输出 | `tests/hil/reports/hil-report-{ts}.json` |

## 3. 测试域与用例

| 域 | 用例 | 验收标准来源 | 需求 |
|:---|:-----|:-------------|:-----|
| BLE | HIL-BLE-01~05（连接/RSSI/重连/并发/MTU） | acceptance-criteria §2.1 | REQ-029 |
| SE050 | HIL-SE-01~05（SCP03 建链/失败/注入/更新/删除） | §2.2 | REQ-033 |
| UNLOCK | HIL-UL-01~04（BLE/NFC/UWB 解锁/重试） | §2.3 | REQ-004, REQ-009 |
| NFC | HIL-NFC-01~04（刷卡/多卡/超时/场强） | §2.4 | REQ-028 |
| UWB | HIL-UWB-01~04（1m/5m/10m/20m 测距） | §3.2 | REQ-030 |
| PM | HIL-PM-01~03（休眠电流/唤醒延迟/低电量） | §3.4 | REQ-034 |
| WAKEUP | HIL-WK-01~03（BLE/NFC/定时唤醒） | §3.7 | REQ-034 |
| FI | HIL-FI-01~06（BLE/SE050/NFC 故障、掉电恢复、非法状态机、签名绕过） | §3.6 | REQ-006 |
| VS | HIL-VS-01~03（状态推送/离线缓冲/频控） | §3.5 | REQ-016 |

## 4. 执行与门禁

```bash
python3 hil_runner.py --check-env   # 环境自检
python3 hil_runner.py --all         # 全量回归（固件版本发布前必跑）
python3 hil_runner.py --domain BLE,NFC
```

- P0 用例（acceptance-criteria §2）: **必须 100% 通过**，任一失败阻塞固件发布。
- P1 用例（§3）: ≥90% 通过。
- 每次固件版本发布前执行全量回归，结果归档 `tests/hil/reports/` + `docs/hil/evidence/`。

## 5. 结果归档

- 原始 JSON: `tests/hil/reports/hil-report-*.json`（副本: `docs/hil/evidence/`）
- 汇总证据: `.osh/ci/sil-hil-results.json`
- 执行摘要与失败分析: `docs/hil/HIL-TEST-RESULTS.md`
