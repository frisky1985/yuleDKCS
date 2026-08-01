# ICCOA DK 规范合规审计报告

> 审计范围: ICCOA/T 002-2024 (数字车钥匙3.0) vs yuleDKCS Hub 当前实现
> 审计日期: 2026-07-30
> 复审日期: 2026-08-01 — S2S 生产接入 ✅ + 钥匙状态模型补齐（SUSPENDED/TERMINATED）✅, 本报告相应结论已更新

## 1. 版本对应关系

| 规范来源 | 技术版本 | 代码协议名 |
|:---------|:--------|:-----------|
| PDF 文件名: "ICCOA DK 4.0" | 规范内容写的是**数字车钥匙3.0** | `iccoa_dk40` |

**问题**: 三处版本号不一致。文件名说 4.0，规范内容说 3.0，代码说是 dk40。需要确认这份 PDF 对应的到底是 ICCOA DK 3.0 还是 DK 4.0。

## 2. 架构级问题（🔴 P0）

### 2.1 KeyShareService 分享路由（已按协议分流 — 2026-08-01 复审确认 ✅）

`internal/service/key_share.go`（CreateShare/AcceptShare）当前按目标厂商委托对应适配器：

```go
// 查找目标厂商适配器 (CCC → Mailbox 中继, ICCOA/ICCE → S2S)
a, ok := s.registry.GetByVendor(req.ToVendor.String())
...
adapterResp, err := a.ShareKey(ctx, req)
```

- CCC 协议: 适配器内部走 Mailbox（返回 Mailbox sharing URL）
- ICCOA/ICCE: 适配器内部走 S2S 客户端（GenSession/Sign 等）

**原审计结论（统一走 CCC Mailbox）已过时，本项已修复。**

### 2.2 S2S 客户端已接入生产流（2026-08-01 复审确认 ✅）

| 文件 | 状态 | 说明 |
|:----|:----:|:-----|
| `s2s/iccoa_client.go` (348行) | ✅ 完成 | 12 个 API 方法全部实现 |
| `s2s/iccoa_types.go` (220行) | ✅ 完成 | 数据结构完整 |
| `s2s/iccoa_client_test.go` (34测试) | ✅ 完成 | mock 测试覆盖全 |
| `iccoa_adapter.go` | ✅ 已接入 | `NewICCOAAdapterWithClient()` 由 main.go 环境变量驱动调用; adapter 已调用 TrackKey/ManageKey/GenSession/Sign/NotifyKeyEvent/HealthCheck |
| `hub/main.go` | ✅ 已接入 | `registerICCOAAdapter`/`registerICCEAdapter`: 配置 `ICCOA_{VENDOR}_BASE_URL`（或 `ICCE_{VENDOR}_BASE_URL`）即启用真 S2S 客户端, 未配置回退 stub |

**影响（原结论已过时）**：S2S 客户端已写入生产路径。配置环境变量后 ICCOA/ICCE 通信走真 S2S API；未配置时仍为 stub（本地/开发默认）。

## 3. 实施差距（🔴 P1）

### 3.1 分享流程与 ICCOA §7.6（2026-08-01 复审: 主链路已走通）

| ICCOA 规范要求（§7.6.1） | 当前实现 |
|:-------------------------|:---------|
| 车 App → 车服务器: 分享请求 | ✅ Hub CreateShare → adapter.ShareKey → S2S genSession |
| 车服务器生成 ownerSessionId | ✅ `ICCOAAdapter.ShareKey` 调 `GenSession` |
| 中间分享证书 CSR (getMidCsr) | ❌ 未接入（S2S 客户端未实现/未调用） |
| 签发中间证书 (putMidCert) | ❌ 未接入 |
| 签发好友钥匙 (share/sign) | ✅ `ICCOAAdapter.AcceptShare` 调 `Sign` |
| 注册钥匙 (trackKey) | ✅ `ICCOAAdapter.BindKey` 调 `TrackKey` |
| 钥匙事件通知 (notifyKeyEvent) | ✅ Bind/Unbind/Share/Accept 后均调用 `NotifyKeyEvent` |

**复审说明**: genSession/sign/trackKey/notifyKeyEvent/manageKey 主链路已通过 adapter 接入 S2S 客户端；getMidCsr/putMidCert 中间证书链路仍为缺口（P2）。

### 3.2 钥匙状态与 §3.12（2026-08-01 复审: 已补齐 ✅）

| ICCOA 钥匙状态 | 当前代码 pb.KeyStatus | 说明 |
|:---------------|:--------------------|:-----|
| 未激活 (INACTIVE) | `KEY_STATUS_UNSPECIFIED` | 无独立枚举值, store 层用 `pending` 表示, 映射 UNSPECIFIED |
| 已激活 (ACTIVE) | `ACTIVE` ✅ | store: `active` |
| 已冻结 (SUSPENDED) | `SUSPENDED` ✅ | store: `suspended`; SuspendKey/ResumeKey 流转 |
| 已删除 (TERMINATED) | `TERMINATED` ✅ | 2026-08-01 新增枚举值 (=5); store: `terminated`; RevokeKey 流转 |

**补充**: `REVOKED`(=3)/`EXPIRED`(=4) 为扩展状态, 旧数据 `revoked` 仍映射 `REVOKED`（向后兼容）。状态流转单测: active→suspended→active、active→terminated 均绿。

### 3.3 车服务器 API 覆盖（§13.5, 2026-08-01 复审）

| API | 客户端实现 | 业务调用 | 状态 |
|:----|:---------:|:--------:|:----:|
| op/sign | ❌ | ❌ | P2: 未实现 |
| share/genSession | ✅ | ✅ adapter.ShareKey | ✅ 已接入 |
| share/getMidCsr | ✅ | ❌ | P1: 已实现未接入 |
| share/putMidCert | ✅ | ❌ | P1: 已实现未接入 |
| share/sign | ✅ | ✅ adapter.AcceptShare | ✅ 已接入 |
| share/cancel | ✅ | ❌ | P1: 已实现未接入 |
| trackKey | ✅ | ✅ adapter.BindKey | ✅ 已接入 |
| manageKey | ✅ | ✅ adapter.UnbindKey/RevokeNotify | ✅ 已接入 |
| notifyKeyEvent | ✅ | ✅ Bind/Unbind/Share/Accept 后调用 | ✅ 已接入 |
| getVehicleProfile | ✅ | ❌ | P2: 已实现未接入 |
| healthCheck | ✅ | ✅ adapter.HealthCheck | ✅ 已接入 |
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

## 5. 综合评级（2026-08-01 复审更新）

| 维度 | 评级 | 说明 |
|:----|:----:|:-----|
| 架构一致性 | 🟢 | KeyShareService 已按协议分流（CCC→Mailbox, ICCOA/ICCE→S2S） |
| S2S 客户端代码 | 🟢 | 设计良好，类型完整，mock 测试覆盖 34 个场景 |
| S2S 客户端接入 | 🟢 | main.go 环境变量驱动（`ICCOA_{VENDOR}_BASE_URL`）, 未配置回退 stub |
| 分享流程 | 🟡 | 主链路已走通（genSession/sign/trackKey/notifyKeyEvent）; getMidCsr/putMidCert 中间证书链路未接入 |
| 钥匙状态 | 🟢 | SUSPENDED/TERMINATED 已补齐, 状态流转单测绿 |
| 错误码 | 🟢 | §13.4 完整支持 |
| 合规测试 | 🟡 | 20 个测试但全是 mock，不暴露实施问题 |
| ICCE | 🟡 | S2S 客户端已接入（env 驱动）; 无规范 PDF 参考 |

## 6. 优先级修复建议（2026-08-01 复审更新）

```
P1 ─ 送测前
├─ getMidCsr / putMidCert 中间证书链路接入（分享流程剩余缺口）
├─ share/cancel 业务接入
└─ 合规测试从 mock 升级为 API 级测试

P2 ─ 量产完善
├─ KeyShareService 业务流里收 ShareID
├─ syncKeyInfo 设备服务器 API
└─ 未激活 (INACTIVE) 独立枚举值（当前用 pending→UNSPECIFIED 表示）
```

**已完成（2026-08-01）**: KeyShareService 按协议分流 ✅ · hub/main.go 接入 ICCOA/ICCE S2S 客户端（env 驱动）✅ · 钥匙状态模型补齐（SUSPENDED/TERMINATED）✅
