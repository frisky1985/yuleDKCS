# 用户体系对接方案（P1-2）

> 状态: 📋 待决策 · 关联: SDK 架构决策（三层模型，SDK 不做用户登录）

## 问题

Hub 当前鉴权是**单一 admin 账号**（`ADMIN_USERNAME`/`ADMIN_PASSWORD`，默认 `admin/admin123`），
JWT 用 HS256 共享密钥签发。这不满足量产：

1. 车厂 App 用户（车主）无法登录 Hub —— SDK 嵌入车厂 App，用户账号在车厂侧
2. 钥匙归属/分享（`bindKey`/Mailbox）需要**用户级**身份，不是 admin
3. 硬编码默认密码 `admin/admin123` 是上线即被攻破的漏洞
4. 多厂商部署（CCC/ICCOA/ICCE 多 OEM）下 user_id 会跨厂商冲突

## 方案对比

### 方案 A：OEM 签发 JWT，Hub 用 JWKS 验签（推荐）

```
车主 App ←登录→ OEM 后端 (OIDC/OAuth2)
车主 App --Authorization: Bearer <OEM JWT>--> Hub REST Gateway
                                            │ JWKS 拉取公钥验签 (RS256/ES256)
                                            ▼
                                  user_id = oem:<oem_id>:<user_id>
```

| 维度 | 说明 |
|:-----|:-----|
| 身份来源 | OEM 后端是唯一认证方（登录、会话、吊销全在车厂侧）|
| Hub 职责 | 只验签，不存密码/用户数据 |
| 合规 | Hub 无用户数据存储负担（个保法/GDPR）|
| 吊销 | OEM 侧随时吊销（JWT 短时效 + 车厂会话失效）|
| 多厂商 | `iss` 区分租户，`oem:<id>:<uid>` 命名空间隔离 |
| 工作量 | 需 OEM 提供 JWKS endpoint（标准 OIDC 能力，多数车厂已有）|
| 风险 | 首次对接依赖车厂侧配合 |

### 方案 B：Hub 自建用户体系（用户名密码 / 手机号验证码）

| 维度 | 说明 |
|:-----|:-----|
| 优点 | Hub 自主可控，不依赖车厂 |
| 缺点 | 与"SDK 不做用户登录"架构决策冲突；重复造轮子 |
| 缺点 | 用户数据合规负担（存储、脱敏、删除权）|
| 缺点 | 车厂不会接受把账号体系放到第三方 Hub |

**结论：不推荐。** 违背既定架构决策，且合规成本高。

### 方案 C：信任代理模式（API Key + 车厂传 user_id）

```
车主 App ←→ OEM 后端 (唯一入口)
OEM 后端 --API Key/mTLS + user_id--> Hub
```

| 维度 | 说明 |
|:-----|:-----|
| 优点 | 实现最简单；OEM 后端完全代理 |
| 缺点 | 信任边界移到 OEM 后端；用户级吊销粒度粗 |
| 缺点 | 审计弱（Hub 看到的是代理身份）|
| 适用 | **服务间调用**（Relay、OEM 后端 → Hub），不作为移动端主路径 |

**结论：作为方案 A 的补充**，用于车厂后端服务到 Hub 的内部通道（mTLS + 固定凭据），
移动端 SDK 走方案 A。

## 推荐：A 为主 + C 补充

| 场景 | 通道 | 认证 |
|:-----|:-----|:-----|
| 移动 SDK → Hub REST | 方案 A | OEM JWT（JWKS 验签）|
| 车厂后端 → Hub gRPC/REST | 方案 C | mTLS / API Key |
| 运维 → Hub REST | 保留 admin JWT | HS256 + 强随机 secret |

## 实施步骤（方案 A）

1. **Gateway 新增 JWKS 验证中间件**
   - 支持多租户：`OEM_JWKS_URL`（逗号分隔，多 OEM 部署）
   - 按 `iss` 路由到对应租户公钥；缓存 JWKS（TTL 1h）
2. **令牌双轨**
   - 保留现有 HS256 admin JWT（运维通道，`iss=dkcs-admin`）
   - 新增 RS256/ES256 OEM JWT（用户通道，`iss=<oem>`）
3. **user_id 命名空间**
   - 统一为 `oem:<oem_id>:<user_id>`，钥匙归属/Mailbox 分享按命名空间隔离
4. **P0 安全修复（必须与方案 A 一起上）**
   - 删除 `admin/admin123` 硬编码默认值 → 未配置 `ADMIN_PASSWORD` 时拒绝启动
5. **K8s secrets**
   - `secrets.yaml` 增加 `admin-password`；`OEM_JWKS_URL` 进 configmap

## 验收标准

- [ ] 无 `ADMIN_PASSWORD` 配置时 Hub 拒绝启动（fail closed）
- [ ] OEM JWT 验签通过后，`bindKey`/Mailbox 操作归属到 `oem:<id>:<uid>`
- [ ] 多租户下 A 厂商 token 不能访问 B 厂商数据
- [ ] JWKS 拉取失败时降级为 401（不放过请求）
