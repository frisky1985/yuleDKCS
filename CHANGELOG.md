# CHANGELOG

> yuleDKCS — 数字钥匙系统 (Digital Key System)
> 项目起始于 digital-key-project 架构阶段，当前仓库为合并重构后的 yuleDKCS。

---

## [2.2.0] — 2026-08-10

### 新增
- TCU 车端 MQTT 通道（backend/dkcs）：配置 + paho 客户端 + 8 用例（含并发 80 消息/TLS/重连），topic 约定 `dkcs/tcu/{commands,telemetry,events}`
- HIL 软件在环（SIL）：transport 抽象（QEMU/串口/J-Link）+ 固件 HIL 命令通道 + 状态机注入（FI-05 转真实），46 用例 10 PASSED/36 SKIPPED（无硬件诚实标记，拒绝假数据）
- 生产烧录工具链（embedded/firmware_toolchain）：
  - B1 固件签名加密：.ydk 包格式（116B 头部）+ ECDSA P-256 + AES-256-GCM，17 测试（6 种篡改检测）
  - B2 烧录脚本生成器：验签解密 → J-Link 脚本 + 批次 manifest + 烧录日志 CSV，9 测试
  - B3 批次管理：工厂侧 SQLite（哈希链防篡改/良率/设备状态机/密钥审计）+ 云端 batch-api（Go REST，存储可插拔 file/PostgreSQL）+ MES 对接文档
- 依赖安全升级：Spring Boot 3.2.12 / grpc-java 1.68.2（CVE-2024-47535）/ protobuf 3.25.5（CVE-2024-35255）

### 修复
- **SM2 密码实现 6 处致命缺陷**（测试驱动发现）：SM2_P/A 常数颠倒（基点 G 不在曲线上）、fp_mul 蒙哥马利错配、fn_mul_reduce 截断、fp_inv/fp_exp 指数位序、ec_point_dbl 别名 bug、crypto_sm2_key_exchange NULL 崩溃 → 修复后 GB/T 32918.2 A.2 标准验签通过
- ccc security.c 越权 deinit 共享 crypto_engine（unified 多协议共存时 ICCE 密码路径失效）
- ICCE zone 分类 2m 边界（P0-06 语义）
- QEMU 6.2 SysTick 外部时钟源（32768Hz）导致 tick 卡死 → 内核时钟修复
- 补缺失 freertos_stubs.c / se050_scp03.c 编译错误 / crypto_random stdio

### 测试
- 嵌入式 C 测试：97 → **196 tests / 0 failures**（icce_edge 99.2%、sm2 95.4%、sm4 95.6%、SE050 SCP03 77.3%、edge_condition 70.85% 覆盖率）
- HIL：46 用例（9 真实 SIL 验证 + 37 硬件域诚实 SKIP），报告含 skipped 统计
- batch-api：11 Go 测试 + PG 集成测试（build tag）
- 工具链：17 + 9 + 13 单测全绿

### 架构
- 目录四类重组：backend/frontend/embedded/mobile + docs/tests
- 新增 backend/cloud/batch-api（生产批次管理）、embedded/firmware_toolchain（生产烧录工具链）
- go.work 纳入 batch-api 模块

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
