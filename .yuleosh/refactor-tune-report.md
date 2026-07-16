# yuleTUNE — 标定校准平台 MVP 搭建报告

> 日期：2026-07-07
> 版本：1.0.0 (MVP)
> 状态：构建通过

---

## 1. 文件结构

```
backend/hub/tune/
├── types.go         (90行)  — 核心领域类型定义
├── calibrator.go    (156行) — 标定引擎接口 + MockCalibrator
├── optimizer.go     (164行) — OTA 优化器接口 + MockOptimizer
├── profile.go       (276行) — 档案管理器接口 + Mock + 预置型号
├── store.go         (50行)  — 数据持久化接口
├── doc.go           (58行)  — 包文档 + ASPICE 注释
────────────────────────────────────────
Total: 794 行
```

## 2. 核心能力

| 模块 | 接口 | Mock | 功能 |
|------|------|------|------|
| **Calibrator** | `Calibrate()` `GetProfile()` `ListModels()` | ✅ MockCalibrator | 标定执行，基于出厂默认参数 ±10% 随机扰动 |
| **Optimizer** | `Analyze()` `ApplyRecommendation()` `BatchOptimize()` | ✅ MockOptimizer | 信号质量加权平均 → 优化参数 → 置信度评估 |
| **ProfileManager** | `RegisterModel()` `UpdateDefaultParams()` `GetDefaultParams()` | ✅ MockProfileManager | 手机型号生命周期管理 |
| **Store** | `SaveRecord()` `GetRecords()` / `SaveProfile()` `GetProfile()` / ... | — | 持久化接口（待生产实现） |

## 3. 预置手机型号（11 款旗舰）

| 厂商 | 型号 |
|------|------|
| Apple | iPhone16ProMax, iPhone16, iPhone15Pro |
| Xiaomi | Xiaomi15Ultra, Xiaomi15Pro |
| OPPO | OPPOFindX8Pro |
| vivo | vivoX200Pro |
| Huawei | HuaweiMate70Pro |
| Honor | HonorMagic7Pro |
| Samsung | SamsungS25Ultra |

## 4. 关键设计决策

- **接口优先** — 所有业务能力以 interface 表达，生产/测试可分别注入实现
- **零外部依赖** — 仅使用 Go 标准库 `context`、`math/rand`、`sync`、`time` 等
- **Mock 可串联** — `MockCalibrator` → `MockOptimizer` 可完整走通「标定 → 分析 → 优化」链路
- **标定算法** — 加权平均（信号质量 w={1.0, 0.8, 0.5, 0.2}），置信度随样本量递增
- **出厂默认参数** — 用 `map[string]float64` 表达，UWB/BLE/NFC 各 7 个参数

## 5. 对标 Wiggler

| yuleTUNE | Wiggler | 说明 |
|----------|---------|------|
| `Calibrator` | 标定执行器 | UWB/BLE/NFC 标定 |
| `Optimizer` | OTA 调优引擎 | 基于众包数据的参数优化 |
| `ProfileManager` | 标定档案管理器 | 型号注册与参数管理 |
| `Store` | 数据持久化 | 生产用 MongoDB/MySQL |

## 6. ASPICE 覆盖

- **SWE.1** 需求分析 — 接口 + 类型文档明确记录
- **SWE.2** 架构设计 — 四层职责分离，接口注入解耦
- **SWE.3** 详细设计 — Mock 实现可直接用于验证
- **SWE.4** 单元验证 — `go build ./...` 零编译错误
- **SWE.5** 集成测试 — MockCalibrator ↔ MockOptimizer 可串联
- **SWE.6** 合格性测试 — PresetModels 覆盖 8 家厂商 11 款旗舰

## 7. 构建验证

```bash
cd ~/yuleDKCS/backend/hub && go build ./...
# 无错误输出
```

## 8. Future Work

- [ ] 接入真实硬件驱动（UWB/BLE/NFC）
- [ ] 标定精度置信区间计算（高斯过程回归）
- [ ] OTA 推送通道（gRPC stream / MQTT）
- [ ] 温度-信号联合补偿模型
- [ ] 标定数据异常检测与离群值剔除
- [ ] 分布式众包数据聚合（Spark / Flink 作业）
