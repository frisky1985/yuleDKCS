# 用户/设备身份对接方案（P1-2，修订版）

> 状态: 📋 待决策 · 关联: SDK 架构决策（三层模型，SDK 不做用户登录）
> 修订说明: v2 依据 CCC-TS-101 v4.0.0 原文 §11.3.4 修正认证模型。

## 规范依据（CCC-TS-101 v4.0.0，§11.3.4 General API Parameter Definitions, p.172）

> "The deviceAttestation should be used to authenticate the sender device if the
> Relay Server is not hosted by the owner device OEM. In other cases, the device
> OEM might use proprietary methods to authenticate the device. **The receiver
> device does not need to present a deviceAttestation.**"

规范定义的 Mailbox 认证模型：
- **URL Secret**：sender 生成，追加为 URL fragment，经任意渠道（微信/WhatsApp）发给接收方；
  Relay Server 零知识（HTTP fragment 不上行），**URL 即访问凭据**
- **deviceAttestation**：仅跨 OEM 时发送方出示，relay 存储但**不校验**（§11.3.5，
  由接收方设备 OEM 校验）；接收方不需要
- **deviceClaim**：sender/receiver 双方提供
- **notificationToken**：Push 通知通道（无 token 时按规范轮询间隔降级）

## 结论：Hub 是"两个身份域"的组合，不能用一套用户 JWT 覆盖

| 接口面 | 认证模型 | 依据 |
|:-----|:-----|:-----|
| **Mailbox (Relay) API** | URL Secret 访问控制 + deviceAttestation 透传存储 + deviceClaim + notificationToken | CCC-TS-101 §11.3.4（现状已实现，不加用户 JWT）|
| **Hub 管理 API**（bindKey/devices/车辆）| 设备厂 Server 签发 token，Hub JWKS 验签（`iss=设备厂`）| 架构原则：开通 = 设备厂 Server → Hub → 车厂 Server |
| **服务间**（设备厂/车厂 Server ↔ Hub）| mTLS / API Key | 服务间通道 |

### 为什么 Mailbox 接口不能加用户 JWT

1. **破坏跨 OEM 分享**：三星用户分享给苹果用户时，接收设备不持有发送方 OEM 的 token
2. **规范无此要求**：规范明确 URL 即凭据，relay 零知识；强制用户身份偏离规范
3. **接收方连 attestation 都不需要**（原文），加用户级认证是过度设计

### 管理 API 的身份语义（修正 v1 的错误）

v1 把方案 A 表述为"验证用户身份"。修正：Hub **不存人车关系**（架构原则），
管理 API 的 token 来自**设备厂 Server**（SDK `setToken("session-token-from-oem-server")`），
Hub 用 JWKS 验证的是**设备厂 Server 的授权**，不是终端用户身份。
`user_id` 命名空间：`oem:<oem_id>:<device_id>`（设备维度，与人车关系无关）。

## 实施步骤

1. **Mailbox 接口**：保持规范模型，不引入用户 JWT（现状已满足）
   - 确认 deviceAttestation 字段存储、不校验（✅ 已实现）
   - URL Secret 访问控制按规范（relay 不校验 fragment，✅ 已对齐）
2. **管理 API 增加 JWKS 验证中间件**（`OEM_JWKS_URL`，多租户逗号分隔）
   - 令牌双轨：运维 admin JWT（HS256, `iss=dkcs-admin`）+ 设备厂 token（RS256/ES256, `iss=<设备厂>`）
   - 未配置 JWKS 时拒绝启动（fail closed）
3. **P0 安全修复（必须）**
   - 删除 `admin/admin123` 硬编码默认值 → 未配置 `ADMIN_PASSWORD` 拒绝启动
4. **K8s secrets**：增加 `admin-password`；`OEM_JWKS_URL` 进 configmap

## 验收标准

- [ ] Mailbox API 无用户 JWT 依赖，跨 OEM 分享链路可用（E2E 验证）
- [ ] 管理 API 无 `ADMIN_PASSWORD`/`OEM_JWKS_URL` 时拒绝启动（fail closed）
- [ ] 多租户下 A 设备厂 token 不能访问 B 设备厂数据
- [ ] JWKS 拉取失败时 401（不放过请求）
