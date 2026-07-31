# Relay Server — PICS/PIXIT 认证文档

> **文档版本**: v1.0  
> **对应规范**: CCC-TS-101 v4.0 §11.3.4 — Mailbox API  
> **测试日期**: 2026-07-31  
> **实现**: yuleDKCS Hub Relay Service (internal/relay/)

---

## PICS (Protocol Implementation Conformance Statement)

### 基本信息

| 项目 | 内容 |
|:-----|:------|
| 产品名称 | yuleDKCS Relay Server |
| 产品版本 | v1.0 |
| 实现协议 | CCC Digital Key v4.0 — Mailbox API (§11.3.4) |
| 通信方式 | gRPC (内部) / HTTP REST Gateway (外部 SDK) |

### 必选功能 (Mandatory)

| # | 功能 | 规范章节 | 状态 | 备注 |
|:-:|:-----|:---------|:----:|:-----|
| M1 | CreateMailbox | §11.3.4.1 | ✅ | 发送方创建分享邮箱 |
| M2 | UpdateMailbox | §11.3.4.2 | ✅ | 任一方更新邮箱内容 |
| M3 | DeleteMailbox | §11.3.4.3 | ✅ | 删除邮箱 |
| M4 | ReadDisplayInfo | §11.3.4.4 | ✅ | 读取展示信息 |
| M5 | ReadSecureContent | §11.3.4.5 | ✅ | 读取加密内容 |
| M6 | RelinquishMailbox | §11.3.4.6 | ✅ | 转移邮箱到新设备 |

### 邮箱状态机

| 状态 | 规范定义 | 实现状态 | 转换路径 |
|:-----|:---------|:--------:|:---------|
| CREATED (1) | 邮箱已创建 | ✅ | → UPDATED_BY_SENDER / UPDATED_BY_RECEIVER |
| UPDATED_BY_SENDER (2) | 发送方已更新 | ✅ | → COMPLETED / CANCELLED |
| UPDATED_BY_RECEIVER (3) | 接收方已更新 | ✅ | → COMPLETED / CANCELLED |
| COMPLETED (4) | 完成状态 | ✅ | 终止态 |
| CANCELLED (5) | 任一方取消 | ✅ | 终止态 |
| EXPIRED (6) | TTL 超时 | ✅ | 终止态（GC 自动处理） |

### Push 通知

| 功能 | 状态 | 备注 |
|:-----|:----:|:-----|
| 发送方 Push (FCM) | ✅ | `FCM_PROJECT_ID` 环境变量配置 |
| 发送方 Push (APNs) | ✅ | `APNS_KEY_ID` + `APNS_TEAM_ID` + ... |
| 无配置时降级 | ✅ | NoopPusher 静默模式 |

---

## PIXIT (Protocol Implementation eXtra Information for Testing)

### 测试环境

| 项目 | 值 |
|:-----|:----|
| Relay Server gRPC 端口 | 9090 (通过 Hub) |
| REST Gateway 端口 | 8080 |
| SDK 通信协议 | HTTP/JSON (iOS: URLSession, Android: OkHttp) |
| Sharing URL 格式 | `https://hub.yuletech.com:8080/api/v1/mailbox/{id}#{secret}` |
| Secret 位置 | URL fragment (`#`)，永不发送到服务器 |
| Mailbox ID 格式 | UUID v4 |
| Payload 加密 | 端到端 (客户端加密/解密，Relay Server 不感知内容) |

### 测试用例映射

| 测试用例 | 覆盖 RPC | 规范章节 | 文件 |
|:---------|:---------|:---------|:-----|
| E2E-11 | 全部 6 个 RPC | §11.3.4 | `e2e_11_relay_mailbox_test.go` |
| E2E-12 | Share (ICCOA S2S) | ICCOA.DK.TS.002 §7.6 | `e2e_12_iccoa_full_share_via_s2s_test.go` |
| E2E-13 | Share (ICCE) | ICCE spec | `e2e_13_icce_full_share_test.go` |
| E2E-14 | 跨厂商 Mailbox 分享 | §11.3.4 + 跨厂商 | `e2e_14_cross_vendor_mailbox_share_test.go` |

### 已知限制

| # | 限制 | 原因 | 计划修复 |
|:-:|:-----|:-----|:---------|
| 1 | Mailbox REST API 需完整 HTTP 测试 | REST Gateway 需要启动完整 gin engine | 下个迭代 |
| 2 | 跨厂商 Push 通知仅支持 FCM/APNs | 厂商 Push 通道需各自对接 | TBD |
