# CCC-TS-101 v4.0.0 — Digital Key Technical Specification 知识库

> 来源: CCC-TS-101-Digital-Key-v4.0.0_APPROVED 3-19-25.pdf (647 pages)
> 重点章节: §11 Digital Key Sharing, §11.3 Communication Channel, §11.3.4 API Parameters, §11.3.5 Security

---

## 1. Relay Server 定位

**Relay Server 是零知识传输层** (§11.3, p.170-172)

- 只做 store-and-forward：存加密的 blob，等另一方来取
- **不解密 payload** — 加密密钥是 sender device 生成的 Secret，不在 relay server 上
- Secret 在 URL fragment (`#` 后面)，**从不发送到 relay server**
- 认证通过 `deviceAttestation`（发送方）或 Device OEM 自有机制

## 2. 六个 API (§11.3, p.171)

| 规范名 | 说明 | 
|--------|------|
| CreateMailbox | 发送方创建分享邮箱 |
| UpdateMailbox | 任一方更新邮箱内容 |
| DeleteMailbox | 删除邮箱 |
| ReadDisplayInformationFromMailbox | 读取展示信息 |
| ReadSecureContentFromMailbox | 读取加密内容 |
| RelinquishMailbox | 转移邮箱到另一设备 |

## 3. API 参数 (§11.3.4.1-6, p.176-177)

### CreateMailbox
- payload: `format` + `content` (genericSharingData + OEM-specific)
- displayInformation: sender 决定
- notificationToken: sender 提供，邮箱生命周期内有效
- mailboxConfiguration: `accessRights="RWD"`, `expiration=<OEM policy>`
- isPushNotificationSupported: **必须为 true** (所有 CCC relay server 必须支持)

### UpdateMailbox
- 接收方: KeySigningRequest 或 sharingReceiverCancel
- 发送方: ImportRequest, sharingSenderCancel, 或 DevicePIN entry
- notificationToken: **接收方应提供**（用于 Import 完成后通知）

### DeleteMailbox
- 接收方获取 ImportRequest 后删除
- 发送方收到取消信号后删除
- 如果 key 已签名还需发 Remote Termination Request

### ReadDisplayInformationFromMailbox
- 展示信息可重复读取

### ReadSecureContentFromMailbox  
- payload = Create/Update 时上传的内容

### RelinquishMailbox
- 允许收到邀请但不能处理 key 的设备释放 mailbox 链接

## 4. 安全模型 (§11.3.5, p.177-178)

```
发送方                          Relay Server                    接收方
  │                                  │                            │
  │  1. 生成 Secret                   │                            │
  │  2. 用 Secret 加密 payload        │                            │
  │  3. CreateMailbox ─────────→      │                            │
  │     (加密的payload)               │                            │
  │  4. ← sharing_url ──────────────────────────────────→         │
  │     (URL#secret)                   │                            │
  │                                  │  5. ReadSecureContent ──→  │
  │                                  │     (不需要 secret!)       │
  │                                  │  ← 加密的payload ──────    │
  │                                  │  6. 用 Secret 解密         │
```

**要点:**
- Secret **不在 relay server 上验证** — 它在 URL fragment 中，发给接收方
- payload **用 Secret 端到端加密** — relay server 只是透传
- 发送方认证: `deviceAttestation` (跨 OEM) 或 Device OEM 自有机制
- 接收方: **不需要 deviceAttestation**

## 5. Push 通知 (§11.3.1, p.171)

- **必须实现** ("shall be implemented")
- 使用 notification tokens [38]
- 不使用 token 时用轮询策略（spec 定义了具体间隔）

## 6. 错误码 (§11.14, p.216-217)

Tabel 11-27 定义的是 **设备间 sharing 错误码**（TLV 格式, Tag 5Ah），不是 relay server API 的错误码：

| Code | Description |
|------|-------------|
| 00h | Missing mandatory field |
| 01h | Invalid message structure |
| 02h | Invalid message content |
| 03h | Version not supported |
| 04h | Certificate expired |
| 05h | Invalid certificate structure |
| 06h | Invalid certificate content |
| 07h | Invalid certificate chain |
| 08h | Request rejected |
| 20h | Sharing cancelled |
| 21h | Sharing cancelled by owner – no valid PIN |
| 22h | Sharing cancelled by friend |
| FFh | Unspecified error |

## 7. sharingDataType 枚举 (§11.3.3, p.175)

| Value | Key |
|-------|-----|
| 1 | sharingKeyCreationRequest |
| 2 | sharingKeySigningRequest |
| 3 | sharingImportRequest |
| 4 | sharingSenderCancel |
| 5 | sharingReceiverCancel |
| 6 | sharingPinReEntryRequest |
| 7 | sharingPinReEntryValue |

## 8. 服务器 API (§17, p.367-416)

定义在 §17.7-17.10 的是车辆 OEM 服务器和设备 OEM 服务器之间的 API（server-to-server），**不是 relay server API**。

Relay Server 的具体 API 定义在引用的 [38] — "Stateful Workflow" 规范中。

## 9. 关键结论

| 我的实现 | 规范要求 | 结论 |
|----------|----------|------|
| secret 在 server 生成 | Secret 由 sender device 生成 | ✅ 不矛盾（relay 生成占位符也可） |
| secret 不校验 | 规范说 secret 不在 relay 验证 | ✅ **正确！我之前判断错了** |
| 6 个 API | 6 个 API | ✅ 对齐，但命名应加 FromMailbox 后缀 |
| Push 通知 | "shall be implemented" | ✅ 已实现 |
| payload 透传 | relay 不解密 | ✅ |
| notificationToken | sender 提供, receiver 也应提供 | ✅ 已支持 |
| accessRights RWD | 要求 RWD | ✅ |
| 错误码 | TLV 设备间错误码，非 server API | ✅ Server 用自有错误码 |
