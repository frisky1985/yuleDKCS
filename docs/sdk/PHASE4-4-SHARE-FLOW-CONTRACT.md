# Sprint Contract: 4.4 CCC 分享全链路 — SDK 高层编排层（分享部分先行）

> 状态: ✅ 已批准
> 日期: 2026-07-31
> 关联: `docs/sdk/SDK-TASKS.md` Phase 4.4；规范 CCC-TS-101 §11.4 Inter-Account Key Sharing Flow
> 原则: 分享部分先行（不依赖 BLE）；BLE 解锁部分等 2b 稳定

---

## Scope

**What**: SDK 高层分享编排层，把已有 MailboxClient 6 API 串成完整 CCC 分享流，双端（iOS/Android）对齐规范 §11.4。

**In Scope**:
- **Sender 分享流**（§11.4.1 Steps 1-5）: `shareKey(to:displayInfo:payload:)` → CreateMailbox → 生成分享 URL（`https://host/api/v1/mailbox/{id}#{secret}`）
- **Receiver 接受流**（§11.4.2-11.4.4）: `acceptSharedKey(url:)` → parseSharingURL → readDisplayInfo → readSecureContent → updateMailbox(KEY_SIGNING) → updateMailbox(IMPORT) → trackKey
- **取消流**: senderCancel / receiverCancel（SENDER_CANCEL=4 / RECEIVER_CANCEL=5）
- **分享 URL 生成与解析**（双端已有 parse，补 build）
- 双端单元测试：流程编排（mock MailboxClient）+ URL 形状 + 错误路径

**Out of Scope**:
- 物理机两台手机 E2E（需真机 + 真实 Relay 环境，单列）
- 钱包层（Apple/Samsung/小米各写各的）
- 密钥材料生成（Endpoint Creation / Key Signing 属 KeyManager/钱包层，SDK 编排层接收 Data 参数）

---

## 现状核对（2026-07-31）

| 组件 | iOS | Android | 缺口 |
|:-----|:----|:--------|:-----|
| MailboxClient 6 API | ✅ `YDKMailboxClient.swift` 261L | ✅ `MailboxClient.kt` 230L | 无 |
| parseSharingURL | ✅ | ✅ | 补 `buildSharingURL` |
| HubClient createShare/acceptShare | ✅ (REST /shares) | ✅ | 分享码模式（非 Mailbox） |
| **高层分享编排** | ❌ | ❌ | **本次核心** |
| Hub e2e_14 跨厂商 Mailbox | ✅ 已通过 | — | 服务端验证已有 |

## Testable Behaviors

### B1. Sender 分享流
- [ ] B1.1: `shareKey` 调 CreateMailbox 并返回分享 URL（含 mailbox_id + fragment secret）| Owner: W1/W2
- [ ] B1.2: URL 格式 = `https://{host}/api/v1/mailbox/{mailbox_id}#{secret}` | Owner: W1/W2
- [ ] B1.3: CreateMailbox 失败 → 抛错误，不生成 URL | Owner: W1/W2

### B2. Receiver 接受流
- [ ] B2.1: `acceptSharedKey(url:)` 解析 URL → readDisplayInfo → readSecureContent 顺序正确 | Owner: W1/W2
- [ ] B2.2: 拿到 content 后 updateMailbox(KEY_SIGNING) → updateMailbox(IMPORT) 顺序正确 | Owner: W1/W2
- [ ] B2.3: 非法 URL（无 mailbox_id / 无 secret）→ 抛错 | Owner: W1/W2
- [ ] B2.4: readSecureContent 失败 → 流程中止 | Owner: W1/W2

### B3. 取消流
- [ ] B3.1: senderCancel → updateMailbox(SENDER_CANCEL) | Owner: W1/W2
- [ ] B3.2: receiverCancel → updateMailbox(RECEIVER_CANCEL) | Owner: W1/W2

### B4. 数据模型对齐
- [ ] B4.1: MailboxDataType 枚举值与 relay.proto/规范一致（KEY_CREATION=1, KEY_SIGNING=2, IMPORT=3, SENDER_CANCEL=4, RECEIVER_CANCEL=5）| Owner: W1/W2
- [ ] B4.2: 请求形状与 Hub 契约一致（camelCase + base64 payload, 见 memory 契约）| Owner: W1/W2

## Acceptance Criteria

| ID | Criterion | Pass Condition | Fail Condition | Priority | Owner |
|----|-----------|----------------|----------------|----------|-------|
| AC1 | 双端高层编排 API | shareKey/acceptSharedKey/senderCancel/receiverCancel 双端齐 | 缺任一 | P0 | W1/W2 |
| AC2 | URL 生成 | buildSharingURL 双端实现, 格式规范 | 格式不符 | P0 | W1/W2 |
| AC3 | 流程顺序 | 编排层 API 调用顺序 = 规范 §11.4 | 顺序错 | P0 | W1/W2 |
| AC4 | 错误路径 | URL 非法/网络失败抛错且流程中止 | 静默失败 | P0 | W1/W2 |
| AC5 | 本机验证 | iOS 独立 harness 可执行验证通过; Android 测试就位 CI 执行 | 语法/逻辑错误 | P1 | W1/W2 |

## Responsibility Matrix

| Criterion | Responsible | Fallback |
|-----------|-------------|----------|
| AC1-AC5 (iOS) | W1 | orchestrator |
| AC1-AC5 (Android) | W2 | orchestrator |

## Negotiation Log

| Round | Party | Action | Notes |
|-------|-------|--------|-------|
| 1 | orchestrator | 提出 contract | 基于 MailboxClient 完成度 + e2e_14 服务端验证 + 规范 §11.4 流程 |
| 2 | orchestrator | 批准 | 缺口 = 高层编排层；mock 单测即可验证流程顺序 |
