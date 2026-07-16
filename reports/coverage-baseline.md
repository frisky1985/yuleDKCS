# 覆盖率基线报告

**日期**: 2026-07-16  
**模块**: `backend/dkcs`  
**命令**: `go test ./... -coverprofile=coverage.out`

---

## 总体覆盖率: **80.0%**

## 包级覆盖率

| 包 | 耗时 | 覆盖率 |
|---|---|---|
| `cmd/dkcs` | — | 0.0% |
| `internal/cache` | 1.820s | **100.0%** |
| `internal/config` | 1.244s | **100.0%** |
| `internal/device` | 0.784s | **100.0%** |
| `internal/keymgmt` | 2.394s | **100.0%** |
| `internal/middleware` | 2.954s | 96.0% |
| `internal/mq` | (cached) | 72.7% |
| `internal/repository` | (cached) | 84.9% |
| `internal/service` | 4.030s | 80.4% |
| `internal/tsp` | 34.597s | **100.0%** |
| `pkg/logger` | 4.491s | 100.0% |
| `pkg/telemetry` | 4.958s | 100.0% |

## 函数级覆盖率摘要

### 完全覆盖 (100%) 的包
- **internal/cache** — Redis 缓存所有方法
- **internal/config** — 配置加载与环境变量
- **internal/device** — 设备服务
- **internal/keymgmt** — 密钥管理（Bind/Unbind/Suspend/Resume/Revoke/Renew）
- **internal/tsp** — TSP 服务（SendCommand/StreamStatus）
- **pkg/logger** — 日志初始化
- **pkg/telemetry** — 遥测初始化

### 覆盖不足 (< 50%) 的函数

| 函数 | 覆盖率 | 备注 |
|---|---|---|
| `cmd/dkcs/main` | 0.0% | 入口函数，需集成测试 |
| `cmd/dkcs/initDatabase` | 0.0% | DB 初始化 |
| `cmd/dkcs/initRedis` | 0.0% | Redis 初始化 |
| `cmd/dkcs/PublishKeyEvent` | 0.0% | 事件发布（main 包） |
| `internal/mq/types.PublishKeyEvent` | 0.0% | MQ 类型方法 |
| `internal/mq/types.keyEventTypeToMsgType` | 0.0% | 类型转换 |
| `internal/repository/vehicle_repo.Create` | 0.0% | Vehicle 创建 |
| `internal/service/command_service.publishCommand` | 0.0% | 命令发布 |
| `internal/service/event_service.RecordEvent` | 0.0% | 事件记录 |
| `internal/service/event_service.ListEvents` | 0.0% | 事件列表 |
| `internal/service/event_service.StreamEvents` | 0.0% | 事件流 |
| `internal/service/event_service.GetEventStats` | 0.0% | 事件统计 |
| `internal/service/key_service.WithEventBus` | 0.0% | 事件总线注入 |
| `internal/service/key_service.emitKeyEvent` | 22.2% | 事件发布 |
| `internal/service/command_service.sendCommand` | 69.0% | 命令发送（部分覆盖） |
| `pkg/logger.(Info/Error/Debug/Warn/Fatal)` | 0.0% | 日志级别方法 |
| `pkg/telemetry.(IncCounter/RecordDuration/RecordGRPCRequest)` | 0.0% | 遥测记录方法 |

## 低覆盖区域分析

1. **cmd/dkcs (0.0%)** — main 包和初始化逻辑完全没有测试覆盖，入口函数通常需要集成/端到端测试
2. **internal/mq (72.7%)** — Kafka Producer/Consumer 的 Publish/Start 路径有缺口
3. **internal/service (80.4%)** — event_service、command_service.publishCommand、key_service.emitKeyEvent 等事件/命令发布路径是低覆盖的主要贡献者
4. **pkg/logger (100% 初始化, 0% 调用)** — 日志级别方法 (Info/Error/Debug/Warn/Fatal) 覆盖率为 0%，因为它们是对 zap 的简单委托
5. **internal/repository (84.9%)** — vehicle_repo.Create 未测试

## 基线对比建议

- 当前基线 **80.0%**，目标建议 ≥ 85%
- 优先补齐: cmd/dkcs 初始化路径、event_service 事件流、command_service.publishCommand
- 低优先级: logger 级别方法、telemetry 记录方法（简单委托，实际被上层间接调用）
