# yuleDKCS SDK Phase 2d — MailboxClient 实现计划

> **平台**: iOS (Swift) + Android (Kotlin) + Backend (Go)  
> **依赖**: Phase B (Relay Server) + Phase 2a (HubClient)  
> **工时**: ~1.5 天

---

## 架构

```
发送方手机                          Hub REST Gateway(:8080)             接收方手机
  SDK.MailboxClient                    │                              SDK.MailboxClient
       │                               │                                   │
       ├──POST /api/v1/mailbox─────→   │  (CreateMailbox)                  │
       │                            ←──┼── sharing_url                     │
       │                               │                                   │
       │  (分享 URL 给好友)             │                                   │
       │══════════════════════分享═══════════════════════════               │
       │                               │                                   │
       │                               │   GET /api/v1/mailbox/:id/display │
       │                               │ ←─────────────────────────        │
       │                               │   PUT /api/v1/mailbox/:id         │ (KeySigning)
       │                               │ ←─────────────────────────        │
       │                               │                                   │
       │   GET /api/v1/mailbox/:id/content                                 │
       │ ←─────────────────────────────│                                   │
       │   PUT /api/v1/mailbox/:id     │  (Import)                         │
       │ ─────────────────────────────→│                                   │
```

### 安全模型

- **Mailbox 公开 API**（无 JWT auth）— 安全依赖：
  1. mailbox_id 是随机 UUID（不可猜测）
  2. Payload 端到端加密（密钥来自 URL fragment `#secret`）
  3. SDK 确保 fragment 永不发送到服务器

### Sharing URL 格式

```
https://hub.yuletech.com:8080/api/v1/mailbox/{mailbox_id}#{secret}
```

---

## Backend: Mailbox REST 路由

在 Hub REST Gateway 新增（无需 auth 中间件）：

| 方法 | 路径 | gRPC 调用 | 说明 |
|:----|:-----|:----------|:-----|
| POST | `/api/v1/mailbox` | CreateMailbox | 发送方创建邮箱 |
| GET | `/api/v1/mailbox/:id/display` | ReadDisplayInformationFromMailbox | 读取展示信息 |
| GET | `/api/v1/mailbox/:id/content` | ReadSecureContentFromMailbox | 读取加密内容 |
| PUT | `/api/v1/mailbox/:id` | UpdateMailbox | 更新邮箱 |
| DELETE | `/api/v1/mailbox/:id` | DeleteMailbox | 删除邮箱 |
| POST | `/api/v1/mailbox/:id/relinquish` | RelinquishMailbox | 转移邮箱 |

---

## iOS MailboxClient

```swift
public class YDKMailboxClient {
    // 从 sharing URL 解析 mailbox_id 和 secret
    public static func parseSharingURL(_ url: String) -> MailboxInfo?

    // 读取展示信息（接收方）
    public func readDisplayInfo(mailboxId: String) async throws -> Data

    // 读取加密内容（接收方）
    public func readSecureContent(mailboxId: String) async throws -> MailboxContent

    // 更新邮箱（KeySigning / Import / Cancel）
    public func updateMailbox(mailboxId: String, dataType: MailboxDataType, payload: Data) async throws -> MailboxStatus

    // 删除邮箱
    public func deleteMailbox(mailboxId: String, reason: String) async throws
}

public struct MailboxInfo {
    public let mailboxId: String
    public let secret: String  // URL fragment
}

public enum MailboxDataType: Int {
    case keyCreation = 1
    case keySigning = 2
    case `import` = 3
    case senderCancel = 4
    case receiverCancel = 5
}
```

---

## Android MailboxClient

```kotlin
class MailboxClient(private val hubEndpoint: String) {
    companion object {
        fun parseSharingURL(url: String): MailboxInfo?
    }

    suspend fun readDisplayInfo(mailboxId: String): ByteArray
    suspend fun readSecureContent(mailboxId: String): MailboxContent
    suspend fun updateMailbox(mailboxId: String, dataType: MailboxDataType, payload: ByteArray): MailboxStatus
    suspend fun deleteMailbox(mailboxId: String, reason: String)
}
```
