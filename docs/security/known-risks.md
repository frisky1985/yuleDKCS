# ⚠️ yuleDKCS — 已知风险清单 (Known Risks)

> **版本**: 1.0.0 | **日期**: 2026-07-16
> **来源**: SECURITY_WHITEPAPER.md + 专家评审 + tech-debt.md + 代码审计 + 架构评估
> **分类**: 密码学/协议/实现/配置
> **目标**: 为渗透测试团队提供前置风险认知，为修复团队提供优先级排序

---

## 目录

1. [风险摘要](#1-风险摘要)
2. [密码学风险](#2-密码学风险)
3. [协议风险](#3-协议风险)
4. [实现风险](#4-实现风险)
5. [配置风险](#5-配置风险)
6. [补救时间线](#6-补救时间线)

---

## 1. 风险摘要

### 1.1 风险等级定义

| 等级 | 定义 | 响应时限 |
|:----:|------|:--------:|
| 🔴 **P0 — Critical** | 可能导致全系统沦陷或未授权车辆访问 | ≤ 30 天 |
| 🟠 **P1 — High** | 可导致敏感信息泄露或部分控制绕过 | ≤ 90 天 |
| 🟡 **P2 — Medium** | 攻击路径需要额外条件，辅助攻击 | ≤ 180 天 |
| 🔵 **P3 — Low** | 最小影响，防御深度改进 | ≤ 365 天 |
| ✅ **Accepted** | 已知且接受的风险（业务决策） | 持续监控 |

### 1.2 风险总览

| 风险 ID | 分类 | 风险描述 | 当前缓解 | 剩余风险 | 优先级 |
|:-------:|:----:|----------|:--------:|:--------:|:------:|
| KR-CRYPTO-01 | 密码学 | SCP03 TRNG 回退使用硬编码 tick | 多层 TRNG 回退链 | Tier 4 失败后无保护 | 🔴 P0 |
| KR-CRYPTO-02 | 密码学 | JWT secret 硬编码风险 | 启动时检查空/弱密钥 | 仍使用 HMAC-SHA256 非非对称 | 🟡 P2 |
| KR-PROTO-01 | 协议 | BLE 重放保护依赖 seq+timestamp | E2E 重放测试验证 | 500ms 窗口内严格依赖 | 🟠 P1 |
| KR-PROTO-02 | 协议 | UWB 测距在仿真中未验证 STS | Carsim 简化模型 | 真实 UWB 行为需硬件验证 | 🟠 P1 |
| KR-IMPL-01 | 实现 | SE050 默认全零传输密钥 | 生产需配置密钥 | dev/Demo 模式裸奔风险 | 🔴 P0 |
| KR-IMPL-02 | 实现 | Android biometric 失效不置密钥无效 | setInvalidatedByBiometricEnrollment(false) | 生物特征注销后密钥仍有效 | 🟡 P2 |
| KR-IMPL-03 | 实现 | ICCE 边缘引擎依赖外部 sys_tick | 仅在嵌入式 RTOS 下可用 | 无 RTOS 时无法工作 | 🟡 P2 |
| KR-IMPL-04 | 实现 | Go repository 包覆盖率仅 9.4% | - | DB 层未充分测试 | 🟠 P1 |
| KR-CONFIG-01 | 配置 | 测试环境弱密码 (admin/admin123) | 仅限测试环境 | 误用于生产风险 | 🟠 P1 |
| KR-CONFIG-02 | 配置 | Rate Limiter 使用本地令牌桶 | 无 Redis 集中管理 | 多实例绕过的风险 | 🟡 P2 |
| KR-CONFIG-03 | 配置 | 旧文档与新文档并存 | 无弃用标记 | 可能被误用 | 🟡 P2 |

---

## 2. 密码学风险

### KR-CRYPTO-01: SCP03 TRNG 回退至硬编码值

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-CRYPTO-01 |
| **分类** | 密码学 — 随机数生成 |
| **严重度** | 🔴 **P0 — Critical** |
| **发现来源** | 专家评审 P0-1 (se050_scp03.c), SECURITY_WHITEPAPER.md §6 |

**风险描述**:
`crypto_random_bytes()` 实现了一个四层 TRNG 回退链 (SE050 HW TRNG → MCU TRNG → mbedTLS CTR_DRBG → OS entropy)。当所有四层全部失败时，代码标注 DEV ONLY 的回退使用硬编码的系统 tick 值。在实际生产环境中，如果多层 TRNG 同时故障（热故障、初始化失败），落入硬编码回退路径会产出可预测的 challenge，使 SCP03 INITIALIZE UPDATE 的 host_challenge 可被攻击者预测。

**技术细节**:
```c
// se050_scp03.c 中 crypto_random_bytes() 调用
// Tier 4 失败后，DEV ONLY 标注的回退使用 tick 值
// 当前实现: ret = crypto_random_bytes(challenge, 8);
//            if (ret != 0) return SCP03_ERR_HW; // 终止不允许回退
// 但安全.c 中的 crypto_random_bytes() 实现可能有不同的回退策略
```

**当前缓解**:
- 四层 TRNG 回退链设计，前三层覆盖绝大多数场景
- SCP03 握手失败时返回错误码而非继续
- close_session 会 zeroing 所有密钥材料

**剩余风险**:
- TRNG 回退链的完整性和覆盖性因硬件平台而异
- S32K3 MCU 上的 MCU TRNG (RNGA) 驱动未验证
- 如果 crypto_random_bytes 自身的回退链落入硬编码，则 host_challenge 可预测

**建议修复**:
1. [P1-4] crypto_random_bytes 中移除所有硬编码回退
2. 不加 TRNG 就拒绝启动 (fail-stop)
3. 增加 CI 测试：验证连续 1000 次调用 crypto_random_bytes 的随机性

---

### KR-CRYPTO-02: JWT 使用对称密钥 (HMAC-SHA256)

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-CRYPTO-02 |
| **分类** | 密码学 — JWT 签名 |
| **严重度** | 🟡 **P2 — Medium** |
| **发现来源** | rest_gateway.go, token.go |

**风险描述**:
当前 JWT 使用 HMAC-SHA256 (对称密钥) 签名。与 RS256 (非对称) 相比，对称密钥需要所有验证方都持有相同的 secret，增加了 secret 管理和分发难度。如果 Hub 服务之间的 secret 不一致，验证将失败；如果 secret 泄露，攻击者可伪造任意身份的 JWT。

**技术细节**:
```go
// rest_gateway.go 第 389 行 — 使用对称 HMAC
token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
    if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
        return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
    }
    return []byte(g.jwtSecret), nil
})
```

**当前缓解**:
- 启动时检查空/弱密钥 (`WithJWTSecret` 和 `Serve` 中的验证)
- 15 分钟短 TTL 减少泄露窗口
- 门禁代码阻止默认密钥启动

**剩余风险**:
- 密钥分发到所有 Hub 实例 (需要 Kubernetes Secret 管理)
- 如果 secret 通过非加密通道分发，可能泄露
- HMAC 不支持无状态验证 (RS256 可通过公钥验证)

**建议修复**:
1. 切换到 RS256/ES256 非对称签名
2. 对称密钥仅用于内部服务间通信 (mTLS + JWT)

---

## 3. 协议风险

### KR-PROTO-01: BLE 重放保护依赖 seq + timestamp

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-PROTO-01 |
| **分类** | 协议 — BLE |
| **严重度** | 🟠 **P1 — High** |
| **发现来源** | E2E 重放测试 (07_security_replay_test.go), 重放协议 |

**风险描述**:
BLE 通信的防重放依赖于序列号 (seq_num) 和时间戳 (timestamp) 组合验证。序列号单调递增但可能溢出 (uint32)，时间戳窗口为 500ms。如果攻击者能够以极低延迟捕获并重放帧（本地网络，< 500ms），同时在序列号验证之前完成重放，可能在窗口内完成攻击。

**技术细节**:
```go
// ReplayPayload 结构 — seq 和 timestamp 是主要的防重放手段
type ReplayPayload struct {
    OriginalSeq  uint32 `json:"original_seq"`
    OriginalTs   int64  `json:"original_ts"`
    ReplayedSeq  uint32 `json:"replayed_seq"`
    Blocked      bool   `json:"blocked"`
    Reason       string `json:"reason"`
}
```

**当前缓解**:
- 序列号严格递增验证
- 时间戳 500ms 窗口
- 重放检测触发车辆报警
- 同时使用 challenge-response 认证

**剩余风险**:
- 如果发送方和接收方时钟不同步 > 500ms，可能误拒绝合法帧
- 序列号 uint32 理论上 2^32 次后溢出（实际不可达）
- 同一 BLE 会话内多个并发帧的排序处理

**建议修复**:
1. 增加 MAC/challenge 在每个命令帧中
2. 考虑每帧 HMAC（不仅仅是 SCP03 通道级）

---

### KR-PROTO-02: UWB 测距未在实际硬件上验证

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-PROTO-02 |
| **分类** | 协议 — UWB |
| **严重度** | 🟠 **P1 — High** |
| **发现来源** | 专家评审, SECURITY_WHITEPAPER.md §5.3 |

**风险描述**:
UWB 安全测距 (IEEE 802.15.4z STS) 的 PHY 级距离绑定和安全属性仅在理论上得到保证。yuleDKCS 当前的测试环境使用 Carsim 模拟 UWB 测距（基于模拟距离值而非实际射频信号），因此 UWB 中继攻击 (PHR-layer attacks) 的真实防护能力无法验证。

**技术细节**:
UWB 安全测距依赖 Scrambled Timestamp Sequence (STS) 提供 PHY 级距离证明：
- 无 STS: 中继攻击可在 2km 范围外实施 (Ghost Peak Attack)
- 有 STS: 安全绑定在 10cm 精度内

**当前缓解**:
- UWB 协议栈实现 STS 支持
- BLE 配对建立后通过 BLE 通道协商 UWB 参数
- 距离阈值 ≤ 2m (解锁) / ≤ 1m (启动引擎)

**剩余风险**:
- 未在实际 UWB 硬件上验证 STS 安全性
- 模拟环境的 UWB 测试仅验证协议交互
- 真实 UWB 芯片 (如 NXP NCJ29D5) 的行为需要硬件集成测试

**建议修复**:
1. 采购 UWB 开发板 (NXP NCJ29D5 或 Qorvo DW3110)
2. 在真实硬件上执行 UWB RSMP 测试
3. 验证 STS 在 PHY 层防止距离欺骗

---

## 4. 实现风险

### KR-IMPL-01: SE050 默认全零传输密钥

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-IMPL-01 |
| **分类** | 实现 — 嵌入式安全 |
| **严重度** | 🔴 **P0 — Critical** |
| **发现来源** | se050_scp03.c, 专家评审 |

**风险描述**:
SE050 出厂时使用默认全零传输密钥 (K_ENC, K_MAC, K_RMAC = 全 0x00)。`se050_scp03_init()` 使用 memset(0) 后，session 中的静态密钥默认为全零。虽然在初始化注释中明确指出"生产必须调用 `se050_scp03_provision_keys()` 替换"，但如果生产制造过程中未正确配置密钥，或以默认密钥状态部署，攻击者可建立 SCP03 通道并完全控制 SE050 中的密钥操作。

**技术细节**:
```c
// se050_scp03_init 中的关键注释
/*
 * PRODUCTION REQUIREMENT [P0-1]:
 *   These MUST be replaced with provisioned keys during manufacturing
 *   via se050_scp03_provision_keys().
 */
/* session->static_enc_key is all zeros from memset */
/* session->static_mac_key is all zeros from memset */
/* session->static_rmac_key is all zeros from memset */
```

**当前缓解**:
- `se050_scp03_provision_keys()` API 用于生产配置
- 注释中明确标注生产要求
- 专家评审中标记为 P0

**剩余风险**:
- 缺少编译时/运行时检查：默认密钥是否已被替换
- 制造流水线中密钥注入过程未审计
- 无密钥注入后的验证测试

**建议修复**:
1. 在 `se050_scp03_open_session()` 开始时检测静态密钥是否全零
2. 如果全零则拒绝建立安全通道
3. 增加制造阶段的密钥注入验证测试

---

### KR-IMPL-02: Android Biometric 注销不置密钥无效

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-IMPL-02 |
| **分类** | 实现 — 移动端安全 |
| **严重度** | 🟡 **P2 — Medium** |
| **发现来源** | 专家评审 P0-3, KeyStoreMetadataStore.kt |

**风险描述**:
Android KeyStore 实现中设置了 `setInvalidatedByBiometricEnrollment(false)`，意味着当用户注册新的生物特征时，密钥不会自动失效。这是为了方便用户（注册新指纹后不需重新绑定密钥），但降低了安全水线：如果攻击者在用户不知情的情况下注册自己的生物特征，可以访问同样被密钥保护的数据。

**技术细节**:
```kotlin
// KeyGenParameterSpec.Builder 配置
.setInvalidatedByBiometricEnrollment(false)
```

**当前缓解**:
- 密钥仍然由 TEE/StrongBox 硬件保护
- `-setUserAuthenticationRequired(true)` 确保每个操作需要生物特征验证
- 此选择是功能-安全权衡 (专家评审标注为非阻塞)

**剩余风险**:
- 如果攻击者获得物理设备访问，注册自己的指纹，可访问 KeyStore 加密数据
- 用户可能不会注意到新的生物特征注册

**建议修复**:
1. 提供可选配置：高安全模式启用 biometric invalidation
2. 增加生物特征变更检测，提醒重新验证

---

### KR-IMPL-03: ICCE 边缘引擎依赖外部 sys_tick

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-IMPL-03 |
| **分类** | 实现 — 嵌入式 |
| **严重度** | 🟡 **P2 — Medium** |
| **发现来源** | 专家评审 P0-2, icce_edge.c |

**风险描述**:
ICCE 边缘计算引擎的 `icce_edge_timer_tick()` 假设主循环提供系统 tick 和 `time_of_day` 计算。当前 `time_of_day` 基于单调 tick 可能不准确（非 RTC）。在缺少准确时间和可靠 tick 源的环境中，边缘规则的时间窗口、冷却和超时逻辑可能不准确或失效。

**技术细节**:
- `sys_tick_get_ms()` 外部依赖，需要在集成文档中明确
- `time_of_day` 基于单调 tick，如果系统没有 NTP/RTC 同步会漂移
- 冷却默认为 3s，如果 tick 精度不足可能缩短或延长

**当前缓解**:
- 外部依赖通过接口声明 (`extern uint32_t sys_tick_get_ms(void)`)
- 默认配置 (60s 同步间隔) 对 tick 精度要求低

**剩余风险**:
- 必须在嵌入式集成文档中明确说明 sys_tick 约束
- 非 RTC 的时间计算在长运行场景 (天/周) 会漂移

**建议修复**:
1. 提供 RTC 回退检测：如果 RTC 不可用，降低时间触发规则的精度预期
2. 添加 sys_tick 的精度验证断言

---

### KR-IMPL-04: Go Repository 包覆盖率仅 9.4%

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-IMPL-04 |
| **分类** | 实现 — 云端测试 |
| **严重度** | 🟠 **P1 — High** |
| **发现来源** | tech-debt.md TD-06 |

**风险描述**:
`backend/dkcs/repository/` 包的测试覆盖率仅 9.4%，远低于 50% 的目标。repository 层是数据持久化的核心，负责所有数据库操作。低覆盖率意味着 SQL 注入防护、连接管理、事务处理、错误处理等关键路径未被充分验证。在渗透测试中，这些未验证的路径可能隐藏 SQL 注入点或其他数据库级漏洞。

**技术细节**:
- 依赖 sqlmock 但配置未就绪
- 当前测试仅覆盖简单查询路径
- 复杂的事务、批量操作、并发竞争未测试

**当前缓解**:
- 使用 Postgres 参数化查询（框架级别）
- 高层 gateway 测试覆盖了 76.7% 的 gateway 逻辑

**剩余风险**:
- repository 层的边界条件未验证
- SQL 注入防护仅在框架层保证，repository 层的构造查询可能绕过
- 并发数据库操作的数据竞争未测试

**建议修复**:
1. [TD-06] 增加 sqlmock 依赖后补充 repository 测试
2. 目标：repository 包覆盖率 ≥ 50%

---

## 5. 配置风险

### KR-CONFIG-01: 测试环境弱密码

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-CONFIG-01 |
| **分类** | 配置 |
| **严重度** | 🟠 **P1 — High** |
| **发现来源** | rest_gateway.go login() |

**风险描述**:
`rest_gateway.go` 中 `login()` 函数使用环境变量 `ADMIN_USERNAME` 和 `ADMIN_PASSWORD`，默认值为 admin/admin123。虽然注释标明"仅用于测试环境"和"生产环境替换为用户服务"，但如果默认值未被覆盖，攻击者可用弱凭据登录系统获取管理员 JWT。

**技术细节**:
```go
adminUser := os.Getenv("ADMIN_USERNAME")
if adminUser == "" {
    adminUser = "admin"          // 默认弱用户名
}
adminPass := os.Getenv("ADMIN_PASSWORD")
if adminPass == "" {
    adminPass = "admin123"       // 默认弱密码
}
```

**当前缓解**:
- 仅用于测试和 staging 环境
- 生产部署必须在 docker-compose/staging 配置中设置强密码
- [S-01] JWT secret 启动时检查

**剩余风险**:
- 如果默认值误用于生产，管理员账户完全暴露
- 环境变量可能泄漏 (K8s Secret 暴露、日志)

**建议修复**:
1. 生产构建中移除默认管理员账户
2. 或使用 OAuth2/OIDC 替代本地密码验证
3. 增加编译时标签：`go build -tags production` 时移除默认值

---

### KR-CONFIG-02: Rate Limiter 单实例本地状态

| 属性 | 值 |
|------|-----|
| **风险ID** | KR-CONFIG-02 |
| **分类** | 配置 — 云服务 |
| **严重度** | 🟡 **P2 — Medium** |
| **发现来源** | rest_gateway.go rateLimiter |

**风险描述**:
当前速率限制器使用 `sync.Mutex + map[string]*tokenBucket` 的本地内存令牌桶。在单实例部署中正常工作，但在多实例部署 (K8s 水平伸缩) 中，每个实例有独立的令牌桶。攻击者可在 3 个实例上分别发送 100 req/s，总计 300 req/s 绕过限流。此外，服务重启后所有速率限制状态丢失。

**技术细节**:
```go
type rateLimiter struct {
    mu       sync.Mutex
    visitors map[string]*tokenBucket  // 仅本地
    rate     float64
    burst    int
}
```

**当前缓解**:
- 单实例部署时有效
- 可以通过 WAF/CDN 层的全局限流补充
- cleanup loop 定期清理过期条目

**剩余风险**:
- 多实例水平扩展时速率限制效果减弱
- 重启后限流状态丢失

**建议修复**:
1. 使用 Redis 集中令牌桶 (sliding window 或 GCRA)
2. 或依赖 API Gateway / WAF 的全局限流

---

## 6. 补救时间线

### 6.1 修复优先级

| 优先级 | 风险 ID | 目标修复日期 | 负责人 |
|:------:|:-------:|:-----------:|:------:|
| 🔴 **P0** | KR-CRYPTO-01 | 2026-08-15 | Embedded 组 |
| 🔴 **P0** | KR-IMPL-01 | 2026-08-15 | Embedded 组 |
| 🟠 **P1** | KR-PROTO-01 | 2026-09-01 | Embedded 组 |
| 🟠 **P1** | KR-PROTO-02 | 2026-10-01 | Systems 组 |
| 🟠 **P1** | KR-IMPL-04 | 2026-09-01 | Backend 组 |
| 🟠 **P1** | KR-CONFIG-01 | 2026-08-15 | DevSecOps |
| 🟡 **P2** | KR-CRYPTO-02 | 2026-12-01 | Backend 组 |
| 🟡 **P2** | KR-IMPL-02 | 2026-12-01 | Mobile 组 |
| 🟡 **P2** | KR-IMPL-03 | 2026-12-01 | Embedded 组 |
| 🟡 **P2** | KR-CONFIG-02 | 2026-12-01 | Backend 组 |
| 🟡 **P2** | KR-CONFIG-03 | 2026-10-01 | Docs 组 |

### 6.2 风险接受

以下风险经评估后接受为业务/设计决策：

| 风险 ID | 接受理由 | 监控方式 |
|:-------:|----------|----------|
| KR-CRYPTO-02 (HMAC 非对称) | 当前单服务部署，HMAC 管理成本低 | 下次架构评审重新评估 |
| KR-IMPL-02 (Biometric 不失效) | 不影响 TEE 硬件保护级别 | 用户反馈 + 安全公告 |
| KR-CONFIG-02 (本地限流) | 生产使用 API Gateway (Kong/Envoyd) 全局限流 | 监控 429 响应率 |

---

*文档维护者: yuleDKCS Security Team | 问题/建议: security@yuledkcs.com*
