# yuleDKCS 架构设计文档

## 总体架构

```
┌────────────────────────────────────────────────┐
│            应用层 (Applications)              │
│  - 数字钥匙管理    - 车辆控制    - OTA 更新     │
├────────────────────────────────────────────────┤
│            统一 API 层 (Unified API)          │
│  dkcs.h - 协议无关的通用接口                    │
├────────────────────────────────────────────────┤
│            协议层 (Protocol Stack)            │
│  ┌────────┐  ┌────────┐  ┌────────┐        │
│  │  CCC   │  │  ICCE  │  │ ICCOA │        │
│  │ R3 API │  │  API   │  │  API  │        │
│  └────────┘  └────────┘  └────────┘        │
├────────────────────────────────────────────────┤
│            安全层 (Security Layer)            │
│  - 密钥管理    - 加解密    - 签名验证     │
├────────────────────────────────────────────────┤
│            传输层 (Transport Layer)          │
│  - BLE        - NFC        - UWB            │
├────────────────────────────────────────────────┤
│            硬件抽象层 (HAL)                  │
│  - SE 驱动    - BLE 控制器    - NFC 控制器    │
└────────────────────────────────────────────────┘
```

## 模块设计

### 1. 统一 API 层 (Unified API)

目标: 提供协议无关的统一接口

核心接口:
- `dkcs_init()` / `dkcs_deinit()`
- `dkcs_pairing_start()`
- `dkcs_vehicle_unlock()` / `dkcs_vehicle_lock()`
- `dkcs_engine_start()`
- `dkcs_key_share()` / `dkcs_key_revoke()`

设计原则:
1. 协议透明: 应用无需关心底层协议细节
2. 类型安全: 编译时检查协议类型匹配
3. 错误统一: 统一的错误码空间

### 2. 协议层 (Protocol Stack)

每个协议独立实现，通过统一的内部接口对接。

#### CCC 模块

主要组件:
- `ccc_core.c`: 核心状态机
- `ccc_crypto.c`: 加密/解密操作
- `ccc_session.c`: 会话管理
- `ccc_transport_ble.c`: BLE 传输
- `ccc_transport_uwb.c`: UWB 测距

状态机:
```
IDLE -> PAIRING -> AUTHENTICATING -> ESTABLISHING -> ACTIVE
                         ↓               ↓
                      FAILED         TERMINATING
```

#### ICCE 模块

主要组件:
- `icce_core.c`: APDU 处理
- `icce_apdu.c`: 命令构建/解析
- `icce_crypto.c`: 国密算法

APDU 命令流程:
```
SELECT -> VERIFY_PIN -> COMMAND -> RESPONSE
```

#### ICCOA 模块

主要组件:
- `iccoa_core.c`: 消息处理
- `iccoa_tlv.c`: TLV 编码/解码
- `iccoa_session.c`: 会话管理

TLV 格式:
```
[Type(1)] [Length(2)] [Value(n)]
```

### 3. 安全层 (Security Layer)

通用安全服务:
- 密钥生成和存储
- 加解密操作
- 签名和验证
- 随机数生成
- 安全清除

安全原则:
1. 敏感数据内存零化
2. 密钥从不离开 SE
3. 所有加密操作通过 SE 执行

### 4. 传输层 (Transport Layer)

#### BLE 传输
- GATT 服务定义
- 广播数据格式
- 连接管理
- 功耗优化

#### NFC 传输
- ISO-DEP 协议
- APDU 交互
- 读取范围控制

#### UWB 传输 (CCC 专有)
- IEEE 802.15.4z
- 距离测量
- 角度估算

### 5. 硬件抽象层 (HAL)

SE 接口:
```c
typedef struct {
    error_t (*init)(void);
    error_t (*generate_key_pair)(uint8_t *pub, uint8_t *priv);
    error_t (*sign)(const uint8_t *data, size_t len, uint8_t *sig);
    error_t (*derive_shared_secret)(const uint8_t *priv, const uint8_t *pub, uint8_t *secret);
    // ...
} se_interface_t;
```

支持的 SE 类型:
- eSE (嵌入式 SE)
- SIM (UICC-based SE)
- TEE (可信执行环境)
- Software (仅用于测试)

## 数据流

### 配对流程

```
Device                          Vehicle
  |                                |
  |---- 1. Pairing Request ------->|
  |   [Ephemeral PubKey + Challenge]
  |                                |
  |<--- 2. Pairing Response --------|
  |   [Vehicle PubKey + Challenge + Sig]
  |                                |
  |---- 3. Pairing Confirm ------->|
  |   [Device Cert + Sig(Challenge)]
  |                                |
  |<--- 4. Vehicle Cert -----------|
  |                                |
  |==== Session Established =======|
```

### 解锁流程

```
Device                          Vehicle
  |                                |
  |==== Secure Session ============|
  |                                |
  |---- 1. Unlock Command -------->|
  |   [Encrypted + MAC]            |
  |                                |
  |<--- 2. Status Response ---------|
  |   [Success + New Counter]      |
  |                                |
  |==== Doors Unlocked ============|
```

## 错误处理

### 错误分类

1. 协议错误: 版本不匹配、消息格式错误
2. 安全错误: 认证失败、签名无效
3. 运输错误: 超时、断连、并发冲突
4. 硬件错误: SE 通信失败、存储满

### 恢复策略

- 会话超时: 重新鉴权
- 验证失败: 返回初始状态
- 传输失败: 重试机制
- SE 故障: 降级到软实现 (仅测试)

## 性能优化

### 启动时间
- 目标: < 100ms
- 优化: SE 预初始化、延迟加载

### 配对时间
- 目标: < 5s
- 优化: 快速配对模式、预计算

### 解锁响应
- 目标: < 200ms
- 优化: 会话缓存、预连接

## 安全考量

### 威胁模型

1. **重放攻击**: 使用计数器/序列号防护
2. **中间人攻击**: 密钥协商 + 证书验证
3. **侧信道攻击**: 消息认证 + MAC
4. **恶意软件**: SE 隔离、代码签名

### 安全测试

- 渗透测试
- 模糊测试
- 代码审计
- 配置审查
