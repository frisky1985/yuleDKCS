# Push 通知配置模板

## 概述

YuleDKCS Hub 支持通过 **FCM (Firebase Cloud Messaging)** 和 **APNs (Apple Push Notification service)** 向手机设备发送推送通知。

推送流程：
1. 用户手机通过 Relay Server 注册设备推送 Token
2. Vehicle 状态变化时，Relay Server 通过 PushNotifier 发送通知
3. 支持同时配置 FCM + APNs（CompositePusher），自动广播到所有渠道
4. 无配置时自动降级为 NoopPusher（空操作）

## 环境变量列表

| 环境变量 | 是否必需 | 说明 |
|---------|---------|------|
| `FCM_PROJECT_ID` | 否 | Firebase 项目 ID，设置后启用 FCM 推送 |
| `APNS_KEY_ID` | 否 | APNs 认证密钥 ID，设置后启用 APNs 推送 |
| `APNS_TEAM_ID` | 否 | Apple Developer Team ID |
| `APNS_BUNDLE_ID` | 否 | App Bundle ID，如 `com.yuletech.digitalkey` |
| `APNS_AUTH_KEY` | 否 | APNs 认证密钥内容（.p8 PEM 文本），不是文件路径 |
| `APNS_ENV` | 否 | 设为 `production` 使用 APNs 生产环境，否则使用开发沙箱 |

> **注意：** 仅需至少配置 FCM 或 APNs 中任意一个。两个都配置时，通知会同时发送给 Android 和 iOS 设备。

## FCM 配置 (Android)

### 获取方式

1. 访问 [Firebase Console](https://console.firebase.google.com/)
2. 创建或选择已有项目
3. 项目设置 → 服务账号 → 生成新的私钥（JSON 文件）
4. 从生成的 JSON 文件中获取 `project_id` 字段值

### 认证方式

FCM 使用 Google Application Default Credentials (ADC) 进行认证，按以下顺序查找凭证：

1. `GOOGLE_APPLICATION_CREDENTIALS` 环境变量指向的服务账号 JSON 文件路径
2. GCP 元数据服务（在 GCP/GKE 上运行时自动获取）
3. 本地 gcloud CLI 配置的凭证

### 环境变量设置

```bash
# 方式一：使用 ADC（推荐）
export GOOGLE_APPLICATION_CREDENTIALS="/path/to/service-account.json"
export FCM_PROJECT_ID="your-firebase-project-id"

# 方式二：在 GKE/GCP 上运行时，自动使用元数据服务
export FCM_PROJECT_ID="your-firebase-project-id"
```

### FCM 推送 Token 注册流程

```go
// 客户端注册示例（由 Relay Server 协议完成）
// 设备在 Relay 注册时上报 FCM Token:
//   - Token 由 Firebase SDK (Android) 或相关库获取
//   - 通过 Relay Polling 接口上报给 Hub
//   - Hub 存储 Token 并在需要时通过 FCMPusher 发送
```

### 验证 FCM 配置

使用 curl 测试（需先获取 OAuth2 令牌）：

```bash
# 获取 Access Token（使用服务账号）
gcloud auth application-default print-access-token

# 手动发送测试通知
ACCESS_TOKEN="$(gcloud auth application-default print-access-token)"
PROJECT_ID="your-firebase-project-id"
DEVICE_TOKEN="your-device-fcm-token"

curl -X POST "https://fcm.googleapis.com/v1/projects/${PROJECT_ID}/messages:send" \
  -H "Authorization: Bearer ${ACCESS_TOKEN}" \
  -H "Content-Type: application/json" \
  -d '{
    "message": {
      "token": "'"${DEVICE_TOKEN}"'",
      "notification": {
        "title": "Test",
        "body": "Hello from YuleDKCS"
      }
    }
  }'
```

---

## APNs 配置 (iOS)

### 获取方式

1. 访问 [Apple Developer Portal](https://developer.apple.com/)
2. Certificates, Identifiers & Profiles → Keys → 创建新密钥
3. 勾选 "Apple Push Notifications service (APNs)"
4. 下载 `.p8` 文件（仅下载一次！）
5. 记录 Key ID（在密钥详情页面）

### 需要的配置项

| 配置项 | 获取途径 |
|-------|---------|
| `APNS_KEY_ID` | 密钥详情页面的 Key ID |
| `APNS_TEAM_ID` | Apple Developer 会员资格页面的 Team ID |
| `APNS_BUNDLE_ID` | App 的 Bundle Identifier |
| `APNS_AUTH_KEY` | `.p8` 文件的完整 PEM 内容 |
| `APNS_ENV` | `production` 或留空（开发沙箱） |

### 环境变量设置

```bash
# APNs 认证密钥内容（.p8 文件文本）
export APNS_AUTH_KEY="-----BEGIN PRIVATE KEY-----
MIGTAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBHkwdwIBAQQg...
...
-----END PRIVATE KEY-----"

export APNS_KEY_ID="ABC123DEFG"
export APNS_TEAM_ID="TEAM123456"
export APNS_BUNDLE_ID="com.yuletech.digitalkey"

# 生产环境（正式发布）
export APNS_ENV="production"

# 开发环境（测试）
# 不设置 APNS_ENV 或设为空，默认使用开发沙箱
```

### APNs Push Token 注册流程

```go
// 客户端注册示例（由 Relay Server 协议完成）
// 设备在 Relay 注册时上报 APNs Device Token:
//   - Token 由 iOS App 通过 UIApplication.registerForRemoteNotifications() 获取
//   - 通过 Relay Polling 接口上报给 Hub
//   - Hub 存储 Token 并在需要时通过 APNsPusher 发送
```

### 验证 APNs 配置

使用 `npx apns` 或直接发送 HTTP/2 请求：

```bash
# 方式一：使用 apns2 命令行工具
npm install -g @parse/node-apn
apns send \
  --token <device-token> \
  --key <path-to-p8-file> \
  --key-id <key-id> \
  --team-id <team-id> \
  --topic <bundle-id> \
  --alert '{"title":"Test","body":"Hello from YuleDKCS"}'

# 方式二：使用 curl (需要 HTTP/2 支持)
# 注意：curl 需要编译时开启 http2 支持
curl -v --http2 \
  -H "apns-topic: com.yuletech.digitalkey" \
  -H "apns-push-type: alert" \
  -H "apns-priority: 10" \
  -H "authorization: bearer <jwt-token>" \
  -d '{"aps":{"alert":{"title":"Test","body":"Hello"},"content-available":1}}' \
  "https://api.development.push.apple.com/3/device/<device-token>"
```

---

## 本地测试

### 使用 NoopPusher（默认行为）

不设置任何 FCM/APNs 环境变量，Push 自动降级为 NoopPusher：

```bash
# 启动 Hub（无 Push 配置）
go run ./cmd/hub/
# 输出: 正常启动，Push 功能不启用
```

### 使用 MockPusher（单元测试）

在单元测试中使用 `MockPusher` 验证推送行为：

```go
func TestPushNotification(t *testing.T) {
    pusher := relay.NewMockPusher()
    
    err := pusher.Notify(context.Background(), relay.PushMessage{
        Title:     "Test",
        Body:      "Door unlocked",
        Token:     "device-token-123",
        MailboxID: "mbox-001",
    })
    if err != nil {
        t.Fatalf("Notify failed: %v", err)
    }
    
    msgs := pusher.Messages()
    if len(msgs) != 1 {
        t.Fatalf("expected 1 message, got %d", len(msgs))
    }
    if msgs[0].Title != "Test" {
        t.Errorf("expected 'Test', got %s", msgs[0].Title)
    }
}
```

### 端到端 Push 测试

```bash
# 1. 配置 FCM（可选）
export GOOGLE_APPLICATION_CREDENTIALS="./service-account.json"
export FCM_PROJECT_ID="my-test-project"

# 2. 配置 APNs（可选）
export APNS_KEY_ID="ABC123"
export APNS_TEAM_ID="TEAM123"
export APNS_BUNDLE_ID="com.test.app"
export APNS_AUTH_KEY="$(cat /path/to/AuthKey_ABC123.p8)"

# 3. 启动 Hub
go run ./cmd/hub/

# 4. 通过 Relay API 触发推送
#    详见 Relay Server API 文档
```

---

## 生产部署检查清单

- [ ] FCM 服务账号已创建，JSON 密钥已轮换
- [ ] `GOOGLE_APPLICATION_CREDENTIALS` 指向正确的服务账号 JSON 文件
- [ ] APNs `.p8` 密钥未过期（Apple 密钥不过期，但可撤销）
- [ ] APNs Bundle ID 与 App Store 上架的应用匹配
- [ ] APNs 环境设置正确：正式版用 `production`，TestFlight/开发用沙箱
- [ ] 推送证书/密钥已轮换（建议每 90 天轮换一次）
- [ ] 监控 FCM/APNs 错误率和成功率
- [ ] 确认 NoopPusher 降级路径在无配置时可正常工作

---

## 代码参考

- 推送接口定义: `internal/relay/notifier.go`
- FCM 推送实现: `internal/relay/pusher_fcm.go`
- APNs 推送实现: `internal/relay/pusher_apns.go`
- Mock 测试实现: `internal/relay/pusher_mock.go`
- Push 集成初始化: `cmd/hub/main.go` (第 102-136 行)
