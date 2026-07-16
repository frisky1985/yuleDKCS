# yuleRUN (Device Under Test) 搭建报告

## 概述

本报告记录 yuleRUN 自动化压测平台 MVP 的搭建过程。yuleRUN 对标银基 yuleRUN，专为数字钥匙场景设计，支持多手机型号的自动化压测。

## 文件结构

```
backend/hub/dut/
├── types.go       (111行)  — 核心类型定义
├── executor.go    (150行)  — DeviceProvider 接口 + Mock 实现
├── runner.go      (232行)  — 压测执行器（串行/并行/停止）
├── scenario.go    (228行)  — 6 个预置测试场景
├── report.go      (189行)  — ReportGenerator + 对比报告
├── store.go       (172行)  — ResultStore 接口 + 内存实现
└── doc.go          (34行)  — ASPICE 注释 + 架构文档
```

**总计: 1116 行 Go 代码，6 个预置场景，7 个文件**

## 核心接口

### DeviceProvider
- `ListDevices` — 列出可用设备
- `ConnectDevice` / `DisconnectDevice` — 设备生命周期
- `ExecuteStep` — 单步执行

### ScenarioRunner
- `RunScenario` — 串行执行场景
- `RunParallel` — 多设备并行执行
- `StopRun` — 强制中止

### ReportGenerator
- `GenerateReport` — 单次压测报告
- `CompareReports` — 多设备对比报告

### ResultStore
- `SaveRun` / `GetRun` / `ListRuns` / `DeleteRun` — 结果持久化

## 预置场景

| ID | 名称 | 协议 | 步骤数 | 说明 |
|----|------|------|--------|------|
| pke_basic_001 | 基本PKE解锁/上锁 | BLE | 4 | BLE连接→解锁→上锁→断连 |
| nfc_tap_001 | NFC刷卡解锁 | NFC | 3 | NFC握手→解锁→验证 |
| remote_ctrl_001 | 远程控车 | MQTT | 4 | 连接→解锁→关窗→启动 |
| sharing_001 | 钥匙分享全流程 | BLE | 5 | 创建→接收→使用→吊销→验证吊销 |
| stress_001 | 压力测试(100次) | BLE | 100 | 连续解锁100次，评估稳定性 |
| concurrent_001 | 并发测试(3设备) | BLE | 12 | 多设备同时解锁/锁车/启动 |

## 关键设计决策

1. **接口优先**: 所有核心能力（设备提供、场景编排、报告生成、持久化）均为接口
2. **Mock 实现**: `MockDeviceProvider` 支持随机失败率和延时模拟，便于集成测试
3. **不要第三方依赖**: UUID 使用 `crypto/rand` 自实现，零外部依赖
4. **可配置场景**: Scenario 定义与 Runner 分离，支持 JSON/YAML 配置驱动
5. **P95 延时分析**: ReportGenerator 自动计算 avg/max/P95 百分位延时
6. **并发安全**: 所有共享状态使用 `sync.RWMutex` 保护

## ASPICE 对齐

- **SWE.1** — Requirements Analysis: 接口定义对标银基 yuleRUN 需求
- **SWE.4** — Unit Verification: MockDeviceProvider 可验证每步执行
- **SWE.5** — Integration: Runner 编排 Provider + Scenario + Store
- **SWE.6** — Qualification: 6 个预置场景组成资格测试套件

## 构建验证

```bash
$ cd ~/yuleDKCS/backend/hub && go build ./...
# 构建通过，零错误
```

## 后续规划

1. 添加 gRPC API 暴露压测接口
2. 实现蓝牙/UWB 真实设备提供者
3. 集成时序数据库（如 InfluxDB）存储延时指标
4. 添加 Dashboard 可视化面板
5. 支持远程设备池管理
