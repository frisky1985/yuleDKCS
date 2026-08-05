# CHANGELOG

> yuleDKCS — 数字钥匙系统 (Digital Key System)
> 项目起始于 digital-key-project 架构阶段，当前仓库为合并重构后的 yuleDKCS。

---

## [1.0.0] — 2026-05-06

### 新增
- 系统架构设计文档发布（SYSTEM_ARCHITECTURE.md v1.0.0）
- API 参考文档发布（REST + gRPC + MQTT + SDK API）
- 部署指南发布（Docker Compose / Kubernetes 生产部署）
- 安全指南发布（认证、加密、SE050、防中继攻击）
- 权限模型参考文档发布（8 位权限位定义）

### 架构
- 三端架构（嵌入式 / App / 云端）设计完成
- ICCE / CCC / ICCOA 三协议栈架构设计完成
- 密钥层级体系设计（Root → Master → Device → Session）
- 端到端数据流定义（密钥绑定 / 无钥匙解锁 / 远程控车）

### 安全
- JWT 认证框架 + ECDSA P-256 / RSA 密钥结构
- 安全启动链定义（Boot ROM → BootLoader → TFM → Application）
- 防中继攻击机制（UWB Secure Ranging + 时间窗口校验）
- 应用层 + 通信层 + 数据层 + 硬件层四层安全模型

---

## [1.0.0-rc.1] — 2026-07-07

### 新增
- 量产就绪审计报告（三端全栈 + 文档合规）
- Spec 契约层文档（SHALL/SHALL NOT 定义）
- 追踪矩阵文档（traceability-matrix.md）
- 安全概念文档（safety-concept.md）
- Spec 多设备文档（spec-multi-device.md）

### 修复 (P0)
- **EMB-P0-01~09**: 嵌入式全部 P0 缺陷修复完成
  - CCC sec_verify() 真实 ECDSA P-256 验签
  - ICCE security_auth/bind 真实签名验证循环
  - CAN 自旋循环改为超时 + 错误返回
  - ECDH 错误路径私钥清零
  - 解锁阈值 3000mm → 2000mm
  - UWB 测距挑战超时检测
  - 自动上锁前车内钥匙检测
  - 全部占位 TODO 实现
- **GO-P0-01**: 日志系统修复 — 生产日志使用 `go.uber.org/zap`，新增 `--log-level`/`--log-file` CLI 参数

### 新增文档
- CHANGELOG.md（本文档）
- RELEASE_NOTES.md
- docs/INTEGRATION_GUIDE.md — 第三方集成指南
- docs/RUNBOOK.md — 运维手册
- docs/FAQ.md — 常见问题
- .yuleosh/compatibility-matrix.md — 版本兼容性矩阵

---

## [0.9.0] — 2026-04-26

### 新增
- 产品需求规格说明书（PRD）V1.0 — 26,000 字
- 项目计划书 — 44 任务 / 40 周 / 7520 工时
- 任务分解文档（DEV-TASKS.md）
- API 契约文档（API-CONTRACT.md）
- 三端开发指南（嵌入式 / App / 云端）
- 测试计划（TEST-PLAN.md）
- 代码审查报告 V2 — 全部通过
- 交付报告（DELIVERY-REPORT.md）

### 代码完成
- 嵌入式三协议栈开发完成（ICCE / CCC / ICCOA DK 3.0 & 4.0）
- 原生 Android/iOS SDK 开发完成
- 云端服务开发完成（Go Hub + DKCS + Java Adapters）
- 安全设计文档完成
- 测试用例设计完成 — ~16,800 字符
- 总计 ~24,500 行代码，165 个文件

---

## 备注

- `legacy-digital-key-project` 分支保留原始 digital-key-project 归档（Flutter + Go 单体）
- 缺失文档审计（DOC-P1-03）已在 v1.0.0-rc.1 中补充完成
- 已知未完成项：集成测试、CI/CD 流水线、Kafka 消息队列集成（GO-P0-02）
