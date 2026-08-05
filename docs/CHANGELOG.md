# yuleDKCS 版本变更日志 (Changelog)

> **文档类型**: 项目级变更日志
> **覆盖范围**: 三端全栈 (Embedded / Android / iOS / Go Hub / Go DKCS / Java Adapters)

---

## [1.0.0] — 2026-07-07

### ⚠️ 量产就绪审计
- **三端全栈量产就绪审计** 完成，覆盖 Go/Embedded/App/Docs/Spec
- **整体评分**: ⚠️ 3.7/5（量产受阻→已降级为 27 P1 + 24 P2）

### 🔴 P0 安全/功能修复 (10/11 已修复)

| ID | 模块 | 修复内容 |
|:---|:-----|:---------|
| EMB-P0-01 | CCC 协议 | `sec_verify()` 实现真实 ECDSA P-256 验签 |
| EMB-P0-02 | ICCE 协议 | `security_auth/bind` 实现真实签名验证循环 |
| EMB-P0-03 | Unified HAL | CAN 自旋循环改为超时 + 错误返回 |
| EMB-P0-04 | ICCE Crypto | ECDH 错误路径添加 `memset` 清零 private_key |
| EMB-P0-05 | ICCOA/Unified | 交叉编译配置添加 |
| EMB-P0-06 | ICCE | 解锁阈值 3000mm → 2000mm（3 处代码 + 头文件宏） |
| EMB-P0-07 | ICCE UWB | UWB 测距挑战超时检测实现 |
| EMB-P0-08 | ICCE | 自动上锁前车内钥匙检测实现 |
| EMB-P0-09 | ALL | 29 个 TODO 已评估/实现 |
| GO-P0-01 | Hub | 日志系统增强（实际 zap 正常，internal/logger 降级 P2） |

### 🟡 P1 缺陷摘要

- **Go 后端 (12)**: 竞态条件、Redis 缓存死代码、Service 层边界模糊、路由冗余、CVE 依赖、API v1 覆盖率 2.1%、Repository 零测试等
- **嵌入式 C (11)**: ISR volatile/不可重入/malloc 空检查、Nonce 去重缺失、引擎启动权限检查、KDF 未验证、TLV EOF 截断等
- **Spec/文档 (3)**: ASIL 等级冲突已修复 (EAL6+)、FTTI 冲突已修复 (500ms)、6 份缺失文档（本批次补全）

### ✨ 新功能

- **Kafka 事件总线集成**: `internal/mq` 包 + KeyEvent 发布 → `key_shared/revoked/activated/updated` 事件
- **CI/CD 基础设施**:
  - `.github/workflows/android-ci.yml` — Android 端 lint + test + coverage + build
  - `.github/workflows/ios-ci.yml` — iOS 端 lint + test + build
  - `.github/workflows/ci-java.yml` — Java Adapter 端 checkstyle + test + coverage
  - `.github/workflows/ci.yml` — Go 端 build + test + coverage
  - `.github/workflows/yuleosh-ci.yml` — yuleOSH 证据链对接
- **Docker 集成测试环境**: PostgreSQL 16 + Redis 7 + Kafka 7.6 Docker Compose
- **yuleOSH 证据链**: `ci-pipeline.yaml` 三层结构 (dev → integration → evidence-pack)
- **架构重构方案**: yuleHUB 四层 ASPICE 分层规划

### 📚 文档

- `docs/design/PRD.md` — 产品需求文档 v2
- `docs/design/DEV-TASKS.md` — 开发任务分解
- `docs/aspice/SWE.4`/`SWE.5`/`SWE.6`/`SYS.5` — ASPICE 合规文档
- `docs/spec/spec-multi-device.md` — 多设备共享 Spec
- `.yuleosh/spec-contract.md` — 142 条 SHALL/SHALL_NOT 约束
- `.yuleosh/acceptance-matrix.md` — 验收判定矩阵
- 文档缺陷修复: ASIL 统一 EAL6+、FTTI 统一 <500ms、Spec ID 前缀冲突修复
- 本批次补全: CHANGELOG / RELEASE_NOTES / 集成指南 / 运维手册 / FAQ / 兼容性矩阵

### 🔧 代码重构

- Go `hub/gateway`: 路由标准化
- Go `dkcs/keymgmt`: 并发安全增强 (RWMutex)
- Embedded ICCE: 所有协议栈签名验证从占位符升级为真实实现
- Embedded CCC/ICCOA: 交叉编译配置对齐

### 📊 性能/测试数据

- **Go 代码**: ~64,500 行, 23 包全部编译通过, 零竞态
- **C 代码**: ~32,700 行, ICCE/CCC/ICCOA 全部交叉编译通过
- **Go 测试**: 23 包全通过, `go vet` 零问题
- **覆盖率**: Hub gateway 76.7%, API v1 2.1% [待提升], DKCS 待统计
- **国密算法 (SM2/SM3/SM4)**: 架构完成, 库集成待完成
- **对标银基 Ingeek**: 代码完成度相近, 量产经验差距大 (0 vs 260+ 车型)

---

## [1.0.0-rc.1] — 2026-07-05

### 初始发布候选版

- 三端代码架构完成: Embedded (ICCE/CCC/ICCOA) + Android/iOS SDK + Go Hub/DKCS + Java Adapters
- 文档覆盖: PRD / 架构设计 / API 契约 / 测试计划 / 安全指南 / 部署指南 / 开发指南
- CI: Go 基础 CI 工作流 (ci.yml) + Lint 独立工作流 (lint.yml)
- 协议支持: ICCE / CCC DK 3.0 / ICCOA DK 3.0 & 4.0 协议栈全部就位
- BLE/UWB/NFC 三模通信架构
- 密钥生命周期管理 (创建/分享/激活/吊销)
- 多租户设备管理
- 安全芯片 (SE050) EAL5+ 集成

---

## 版本兼容规则

- **主版本号**: 三端必须一致 (v1.x.x)
- **次版本号**: rc → 正式发布 → 功能迭代
- **修订号**: 仅修复补丁

> 各端版本对应关系详见 [compatibility-matrix.md](compatibility-matrix.md)
