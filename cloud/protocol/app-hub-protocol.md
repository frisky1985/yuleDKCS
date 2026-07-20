# App ↔ HUB 私有协议规范 (BERTLV)

**版本**: 1.0.0
**日期**: 2026-05-06
**传输**: HTTPS REST + BERTLV
**安全**: OAuth2.0 + 数据签名

---

## 1. 协议概述

### 1.1 API设计原则

1. **外层**: REST API (HTTP/JSON) - 便于调试和抓包
2. **内容层**: BERTLV编码 - 统一数据结构
3. **传输层**: HTTPS + OAuth2.0 Token认证

### 1.2 API基础URL

```
生产环境: https://api.digitalkey.cn/v1
测试环境: https://api-test.digitalkey.cn/v1
```

### 1.3 请求格式

```
POST /{resource}
Content-Type: application/json
Authorization: Bearer {token}
X-Device-Id: {device_id}
X-App-Version: {version}
X-Request-Id: {uuid}

{
  "header": { /* BERTLV Hex */ },
  "body": { /* BERTLV Hex */ },
  "signature": { /* BERTLV Hex */ }
}
```

### 1.4 响应格式

```json
{
  "code": 0,
  "message": "success",
  "header": "E10401...",
  "body": "A00102...",
  "signature": "E1FF01..."
}
```

---

## 2. API列表

### 2.1 密钥管理

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /keys/bind | 绑定密钥 |
| DELETE | /keys/{keyId} | 解绑密钥 |
| PUT | /keys/{keyId}/suspend | 挂起密钥 |
| PUT | /keys/{keyId}/resume | 恢复密钥 |
| PUT | /keys/{keyId}/revoke | 撤销密钥 |
| GET | /keys | 获取密钥列表 |
| GET | /keys/{keyId} | 获取密钥详情 |
| POST | /keys/{keyId}/refresh | 刷新密钥 |

### 2.2 密钥分享

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /shares | 创建分享 |
| POST | /shares/accept | 接受分享 |
| DELETE | /shares/{shareId} | 取消分享 |
| GET | /shares | 获取分享列表 |
| GET | /shares/{shareId} | 获取分享详情 |

### 2.3 车辆控制

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /vehicles/{vehicleId}/commands | 发送控制指令 |
| GET | /vehicles/{vehicleId}/commands/{cmdId} | 查询指令状态 |
| GET | /vehicles/{vehicleId}/status | 获取车辆状态 |

### 2.4 用户

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /users/register | 用户注册 |
| POST | /users/login | 用户登录 |
| POST | /users/logout | 用户登出 |
| GET | /users/profile | 获取用户信息 |
| PUT | /users/profile | 更新用户信息 |
| POST | /users/devices | 绑定设备 |
| DELETE | /users/devices/{deviceId} | 解绑设备 |

### 2.5 车辆

| 方法 | 路径 | 功能 |
|------|------|------|
| POST | /vehicles | 添加车辆 |
| DELETE | /vehicles/{vehicleId} | 删除车辆 |
| GET | /vehicles | 获取车辆列表 |
| GET | /vehicles/{vehicleId} | 获取车辆详情 |

---

## 3. 消息类型定义

### 3.1 消息类型 (MessageType)

| 类型码 | 名称 | 说明 |
|--------|------|------|
| 0101 | UserRegister | 用户注册 |
| 0102 | UserLogin | 用户登录 |
| 0103 | UserProfile | 用户信息 |
| 0201 | VehicleAdd | 添加车辆 |
| 0202 | VehicleList | 车辆列表 |
| 0203 | VehicleDetail | 车辆详情 |
| 0301 | KeyBind | 密钥绑定 |
| 0302 | KeyUnbind | 密钥解绑 |
| 0303 | KeySuspend | 密钥挂起 |
| 0304 | KeyResume | 密钥恢复 |
| 0305 | KeyRevoke | 密钥撤销 |
| 0306 | KeyList | 密钥列表 |
| 0307 | KeyDetail | 密钥详情 |
| 0401 | ShareCreate | 创建分享 |
| 0402 | ShareAccept | 接受分享 |
| 0403 | ShareCancel | 取消分享 |
| 0404 | ShareList | 分享列表 |
| 0501 | CommandSend | 发送指令 |
| 0502 | CommandQuery | 查询指令 |
| 0503 | VehicleStatus | 车辆状态 |

---

## 4. 请求响应详解

### 4.1 密钥绑定 (POST /keys/bind)

**请求 Body BERTLV**:

| 标签 | 名称 | 格式 | 说明 |
|------|------|------|------|
| A0 01 | VehicleId | AN32 | 车辆ID |
| A0 02 | Vendor | AN16 | 手机厂商 |
| A0 03 | Protocol | AN16 | 协议类型 |
| A0 04 | KeyType | N2 | 密钥类型 |
| A0 05 | AccessLevel | B4 | 权限 |
| A0 06 | DevicePubkey | B64 | 设备公钥 |
| A0 07 | ValidFrom | N14 | 生效时间 |
| A0 08 | ValidUntil | N14 | 失效时间 |
| A0 09 | MaxUses | N6 | 最大次数 |

**响应 Body BERTLV**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | Status | N2 |
| A0 03 | VehiclePubkey | B64 |
| A0 04 | SharedSecret | B32 |
| A0 05 | PairingInfo | 结构 |
| A0 06 | ResultCode | N4 |

**PairingInfo结构**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 05 01 | BleMac | AN12 |
| A0 05 02 | BleServiceUuid | AN36 |
| A0 05 03 | NfcAid | AN32 |
| A0 05 04 | UwbChannel | N2 |

### 4.2 发送控制指令 (POST /vehicles/{vehicleId}/commands)

**请求 Body BERTLV**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | Action | N2 |
| A0 03 | Params | 结构 |
| A0 04 | WaitResponse | N1 |

**响应 Body BERTLV**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | CmdId | AN32 |
| A0 02 | Status | N2 |
| A0 03 | EstimatedTime | N4 |

### 4.3 创建分享 (POST /shares)

**请求 Body BERTLV**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | KeyId | AN32 |
| A0 02 | AccessLevel | B4 |
| A0 03 | ValidFrom | N14 |
| A0 04 | ValidUntil | N14 |
| A0 05 | MaxUses | N6 |
| A0 06 | ToUserId | AN32 |

**响应 Body BERTLV**:

| 标签 | 名称 | 格式 |
|------|------|------|
| A0 01 | ShareId | AN32 |
| A0 02 | ShareCode | N6 |
| A0 03 | ShareUrl | AN128 |

---

## 5. 认证与安全

### 5.1 OAuth2.0认证流程

```
1. 用户打开App → App请求授权码(code)
2. App用code换AccessToken + RefreshToken
3. 每次API请求携带AccessToken
4. Token过期时用RefreshToken刷新
```

### 5.2 Token格式

| 字段 | 说明 |
|------|------|
| AccessToken | JWT格式，有效期1小时 |
| RefreshToken | Opaque格式，有效期30天 |

### 5.3 请求签名

每个请求需要携带签名:

```
X-Signature: {algorithm}={signature}
Algorithm: HMAC-SHA256
Signature = HMAC-SHA256(
  SecretKey,
  HTTP_METHOD + "\n" +
  REQUEST_PATH + "\n" +
  TIMESTAMP + "\n" +
  BODY_SHA256
)
```

---

## 6. 错误码

| 错误码 | HTTP状态码 | 说明 |
|--------|------------|------|
| 0 | 200 | 成功 |
| 1001 | 400 | 参数错误 |
| 1002 | 401 | 签名无效 |
| 1003 | 401 | Token无效 |
| 1004 | 401 | Token过期 |
| 1005 | 403 | 权限不足 |
| 1006 | 429 | 请求过于频繁 |
| 2001 | 404 | 密钥不存在 |
| 2002 | 409 | 密钥已存在 |
| 3001 | 404 | 车辆不存在 |
| 4001 | 404 | 用户不存在 |
| 4002 | 409 | 用户已存在 |
| 5001 | 400 | 分享已过期 |
| 5002 | 409 | 分享已接受 |
| 9001 | 500 | 服务器内部错误 |
| 9002 | 503 | 服务不可用 |

---

## 7. 版本演进

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-05-06 | 初始版本 |
