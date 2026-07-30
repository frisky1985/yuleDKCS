# ICCOA DK 规范合规审计报告

> 审计范围: ICCOA/T 002-2024 (数字车钥匙3.0) vs yuleDKCS Hub 当前实现
> 审计日期: 2026-07-30

## 1. 版本对应关系

| 规范来源 | 技术版本 | 代码协议名 |
|:---------|:--------|:-----------|
| PDF 文件名: "ICCOA DK 4.0" | 规范内容写的是**数字车钥匙3.0** | `iccoa_dk40` |

**问题**: 三处版本号不一致。文件名说 4.0，规范内容说 3.0，代码说是 dk40。需要确认这份 PDF 对应的到底是 ICCOA DK 3.0 还是 DK 4.0。

## 2. 架构级问题（🔴 P0）

### 2.1 KeyShareService 对所有协议统一走 CCC Mailbox

`internal/service/key_share.go` 第 52-78 行：

```go
// 2. 创建 Mailbox 作为底层传输通道（CCC §11.3.4）
mbReq := &pb_relay.CreateMailboxRequest{...}
mb, err := s.mailboxController.Create(ctx, mbReq)
```

**这是错误的。** ICCOA 分享走的是**车服务器 ↔ 设备服务器 S2S** 架构，不需要 CCC Mailbox 这个中继层。

**影响**：当前所有的 ICCOA 分享请求都走了一个不必要的 CCC Mailbox 中转，与 ICCOA 规范 §7.6 定义的分享流程不符。

**修正建议**：KeyShareService 需要按协议分流：
- CCC 协议: 继续走 Mailbox
- ICCOA/ICCE: 直接走 Adapter S2S，跳过 Mailbox

### 2.2 S2S 客户端代码存在但未接入生产流

| 文件 | 状态 | 说明 |
|:----|:----:|:-----|
| `s2s/iccoa_client.go` (348行) | ✅ 完成 | 12 个 API 方法全部实现 |
| `s2s/iccoa_types.go` (220行) | ✅ 完成 | 数据结构完整 |
| `s2s/iccoa_client_test.go` (34测试) | ✅ 完成 | mock 测试覆盖全 |
| `iccoa_adapter.go` | ⚠️ 半完成 | `NewICCOAAdapterWithClient()` 存在但**未在 main.go 中调用** |
| `hub/main.go` | ❌ 未接入 | 仍然只用 `NewICCOAAdapter(vendor, logger)`（stub 模式） |

**影响**：S2S 客户端代码已写好、已测试，但生产路径上 ICCOA 通信仍然是 stub。

## 3. 实施差距（🔴 P1）

### 3.1 分享流程与 ICCOA §7.6 不一致

| ICCOA 规范要求（§7.6.1） | 当前实现 |
|:-------------------------|:---------|
| 车 App → 车服务器: 分享请求 | ❌ 没有车 App 入口 |
| 车服务器生成 ownerSessionId | ❌ stub（S2S client 的 GenSession 已实现但未走通路） |
| 中间分享证书 CSR (getMidCsr) | ❌ stub |
| 签发中间证书 (putMidCert) | ❌ stub |
| 签发好友钥匙 (share/sign) | ❌ stub |
| 注册钥匙 (trackKey) | ❌ stub |
| 钥匙事件通知 (notifyKeyEvent) | ❌ stub |

所有 S2S 客户端方法都已实现，但没有被任何业务流程调用。

### 3.2 钥匙状态与 §3.12 不一致

| ICCOA 钥匙状态 | 当前代码 pb.KeyStatus |
|:---------------|:--------------------|
| 未激活 (INACTIVE) | `KEY_STATUS_UNSPECIFIED` — 无对应值 |
| 已激活 (ACTIVE) | `ACTIVE` ✅ |
| 已冻结 (SUSPENDED) | ❌ 无 `SUSPENDED` 状态 |
| 已删除 (TERMINATED) | ❌ 无 `TERMINATED`/`DELETED` 状态 |

### 3.3 车服务器 API 覆盖（§13.5）

| API | 客户端实现 | 业务调用 | 状态 |
|:----|:---------:|:--------:|:----:|
| op/sign | ❌ | ❌ | P2: 未实现 |
| share/genSession | ✅ | ❌ | **P0: 已实现未接入** |
| share/getMidCsr | ✅ | ❌ | P1: 已实现未接入 |
| share/putMidCert | ✅ | ❌ | P1: 已实现未接入 |
| share/sign | ✅ | ❌ | P1: 已实现未接入 |
| share/cancel | ✅ | ❌ | P1: 已实现未接入 |
| trackKey | ✅ | ❌ | **P0: 已实现未接入** |
| manageKey | ✅ | ❌ | P1: 已实现未接入 |
| notifyKeyEvent | ✅ | ❌ | **P0: 已实现未接入** |
| getVehicleProfile | ✅ | ❌ | P2: 已实现未接入 |
| healthCheck | ✅ | ❌ | P2: 已实现未接入 |
| share/getSar | ✅ | ❌ | P2: 已实现未接入 |
| share/putSharingAttestation | ✅ | ❌ | P2: 已实现未接入 |
| notifySyncKeyPropBatch | ❌ | ❌ | P3: 未实现 |

### 3.4 设备服务器 API 覆盖（§13.6）

| API | 实现 | 状态 |
|:----|:----:|:----:|
| manageKey | ✅（client 已实现） | P1 |
| syncKeyInfo | ❌ | P2 |
| healthCheck | ✅ | ✅ |
| syncKeyPropDataBatch | ❌ | P3 |

### 3.5 错误码（§13.4）

| 规范错误码 | 当前 S2S 客户端 |
|:----------|:---------------|
| 40000~50001 共 14 个 | ✅ `ICCOAAPIError` 结构体已定义 |
| HTTP Status 表示结果 | ✅ `doRequest` 中处理 |
| 缺少标准 Body 的 fallback | ✅ 兼容代码已实现 |

**错误码无差距。**

## 4. 合规测试问题（🟡 P2）

### 4.1 合规测试全是 mock

`tests/compliance/iccoa/iccoa_bind_test.go` 中的测试：
- 使用 `common.DefaultICCOADevice()` mock 对象
- 模拟 ECDH、证书交换等流程
- 测试验证的是**概念正确性**而非**API 端点正确性**
- 任何未对接真实厂商 API 的实现都会通过这些测试

**影响**：20 个合规测试可能全部通过了，但它们不暴露任何实施差距。

## 5. 综合评级

| 维度 | 评级 | 说明 |
|:----|:----:|:-----|
| 架构一致性 | 🔴 | KeyShareService 强行使用 Mailbox，与 ICCOA 架构冲突 |
| S2S 客户端代码 | 🟢 | 设计良好，类型完整，mock 测试覆盖 34 个场景 |
| S2S 客户端接入 | 🔴 | 代码存在但生产路径未使用 |
| 分享流程 | 🔴 | §7.6 定义的 16 步流程全部走不通 |
| 钥匙状态 | 🟡 | 缺少 SUSPENDED/TERMINATED 状态 |
| 错误码 | 🟢 | §13.4 完整支持 |
| 合规测试 | 🟡 | 20 个测试但全是 mock，不暴露实施问题 |
| ICCE | 🔴 | 与 ICCOA 同理，但 ICCE 还没有规范 PDF 参考 |

## 6. 优先级修复建议

```
P0 ─ 必须现在修
├─ KeyShareService 按协议分流（ICCOA/ICCE 不走 Mailbox）
├─ hub/main.go 接入 ICCOA S2S 客户端（环境变量 → 配置 → 真机 API）
└─ hub/main.go 接入 ICCE S2S 客户端

P1 ─ 分享流程走通
├─ ShareKey → genSession → getMidCsr → putMidCert → sign 流程
├─ trackKey 钥匙注册
├─ notifyKeyEvent 事件回传
└─ 钥匙状态模型补齐（SUSPENDED/TERMINATED）

P2 ─ 量产完善
├─ KeyShareService 业务流里收 ShareID
├─ syncKeyInfo 设备服务器 API
└─ 合规测试从 mock 升级为 API 级测试
```
