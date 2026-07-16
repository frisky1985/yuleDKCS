# yuleDKCS yuleHUB — 安全层 + 诊断层 + ASPICE 标记 重构报告

**日期**: 2026-07-07 | **生成**: Claude Agent (SubAgent)

---

## 执行摘要

在 yuleDKCS yuleHUB 架构下完成了三项增强：

1. **安全监控层** (`hub/security/`) — 对标银基 VSoC 威胁事件模型
2. **yulePIN诊断埋点层** (`hub/diagnostics/`) — 对标银基 yulePIN 追踪/采集/健康平台
3. **ASPICE 层标记** — 所有 Go/C 包完成 SWE.4–SWE.6 层映射 + LST

---

## 1. 安全监控层 (Task A)

### 路径: `backend/cloud/hub/internal/security/`

| 文件 | 行数 | 职责 |
|------|------|------|
| `types.go` | 80 | ThreatEvent / Alert / SecurityStats 类型定义 |
| `monitor.go` | 147 | Monitor 接口 + DefaultMonitor 实现 |
| `store.go` | 21 | EventStore / AlertStore 持久化存储接口 |

### 关键设计决策

- **ThreatEvent 类型**: `auth_failure`, `relay_attack`, `key_tamper`, `replay_attack`, `device_anomaly`, `brute_force`
- **严重级别**: low → medium → high → critical（四级，critical 触发自动阻断）
- **Monitor 接口**: ReportEvent / GetEvents / GetStats / CreateAlert / GetAlerts / AcknowledgeAlert
- **存储**: 接口隔离 (EventStore/AlertStore) — 默认内存实现，生产可切换 PostgreSQL/Elasticsearch

### 对标银基 VSoC

| 银基 VSoC 能力 | yuleDKCS 实现 |
|----------------|---------------|
| 安全事件采集 | `Monitor.ReportEvent()` |
| 告警规则引擎 | `Monitor.CreateAlert()` + 主动告警条件 |
| 安全状态大盘 | `Monitor.GetStats()` → Grafana 数据源 |
| 事件溯源 | `Monitor.GetEvents(filter)` 支持时间/类型/设备筛选 |

---

## 2. yulePIN诊断埋点层 (Task B)

### 路径: `backend/cloud/hub/internal/diagnostics/`

| 文件 | 行数 | 职责 |
|------|------|------|
| `types.go` | 64 | TraceSpan / LogEntry / HealthStatus 类型定义 |
| `tracer.go` | 180 | Tracer 接口 + DefaultTracer (完整 Span 树) |
| `collector.go` | 177 | Collector 接口 + DefaultCollector (缓冲 + 异步刷写) |
| `health.go` | 249 | HealthChecker (DB/HTTP 检查 + 周期性 + HTTP handler) |

### 关键设计决策

- **全链路追踪**: Trace → Span Tree 模型，通过 `context.Context` 跨服务传播
- **日志关联**: LogEntry 内嵌 TraceID/SpanID，全链路可追溯
- **健康检查**: 插件式 CheckFunc、支持 DB ping / HTTP / 自定义、周期性自检

### 对标银基 yulePIN

| 银基 yulePIN 能力 | yuleDKCS 实现 |
|----------------|---------------|
| 全链路追踪 | `Tracer.StartSpan()` / `EndSpan()` / Span Tree |
| 日志采集 | `Collector.Collect()` / 关联 Trace |
| 服务健康监控 | `HealthChecker.RunChecks()` / `HTTPHealthHandler()` |
| 聚合查询 | `Tracer.QuerySpans()` / `Collector.Query()` |

---

## 3. ASPICE 层标记 (Task C)

### C1: Go 包 Layer 注释 (`doc.go` 共 22 个包)

| ASPICE 层 | 包数量 | 包含 |
|-----------|--------|------|
| **SWC (SWE.6)** | 7 | hub/service, hub/unified, dkcs/service, dkcs/device, dkcs/tsp |
| **RTE (SWE.5)** | 3 | hub/gateway, hub/diagnostics, dkcs/middleware |
| **BSW (SWE.4)** | 12+ | hub/security, hub/token, hub/telemetry, dkcs/keymgmt, dkcs/repository... |
| **MCAL (SWE.4)** | 3+ | hub/adapter, adapter-ccc/iccoa/icce (Java) |

### C2: Embedded C 头文件 (9 个文件已标记)

| 文件 | 模块 ID | ASPICE 层 |
|------|---------|-----------|
| `icce_protocol/include/icce_digital_key.h` | EMB-SWC-ICCE | SWC (SWE.6) |
| `ccc_protocol/include/ccc_digital_key.h` | EMB-SWC-CCC | SWC (SWE.6) |
| `iccoa_protocol/include/iccoa_digital_key.h` | EMB-SWC-ICCOA | SWC (SWE.6) |
| `unified_protocol/include/dk_unified.h` | EMB-SWC-UNIFIED | SWC (SWE.6) |
| `icce_protocol/include/security_auth.h` | EMB-BSW-SEC | BSW (SWE.4) |
| `ccc_protocol/include/hardware_abstraction.h` | EMB-MCAL-HW | MCAL (SWE.4) |
| `icce_protocol/src/crypto/crypto_engine.h` | EMB-BSW-CRYPTO | BSW (SWE.4) |
| `icce_protocol/src/crypto/crypto_types.h` | EMB-BSW-CRYPTO-TYPES | BSW (SWE.4) |
| `icce_protocol/src/ble/ble_adapter.h` | EMB-BSW-BLE | BSW (SWE.4) |
| `icce_protocol/src/security/hsm_interface.h` | EMB-BSW-HSM | BSW (SWE.4) |
| `ccc_protocol/src/security/security.c` | EMB-BSW-SE050 | BSW (SWE.4) |

### C3: LST.md 已创建

**路径**: `docs/architecture/LST.md`

内容：
- 云端 22 个 Go 包的层映射
- 嵌入式 C 各模块的层映射
- 层间依赖规则
- 接口清单

---

## 4. 验证状态

```bash
# Hub 新模块编译 (security + diagnostics)
backend/cloud/hub → go build ./...
```

> **结果**: 新模块依赖 `zap` (已在 go.mod 中), 编译通过。

---

## 5. 后续建议

| 优先级 | 事项 | 描述 |
|--------|------|------|
| P0 | 安全事件持久化 | 实现 PostgreSQL/Elasticsearch 版 EventStore |
| P0 | 诊断数据汇入 Kafka | Collector.Flush() 接入 Kafka Sink |
| P1 | 告警规则引擎 | 基于阈值/频率/时间窗的告警规则配置 |
| P1 | 轨迹可视化 | Tracer → Jaeger/Zipkin 导出 |
| P2 | 嵌入式 MCAL 增强 | BLE/NFC/UWB 驱动添加完整 ASPICE 文档注释 |
| P2 | Java 适配器 ASPICE 标记 | adapter-ccc/iccoa/icce 添加 package-info.java |

---

*报告结束 — 共创建/修改 37 个文件*
