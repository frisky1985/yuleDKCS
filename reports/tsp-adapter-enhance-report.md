# TSP 适配器增强报告

**项目**: yuleDKCS P1-6  
**日期**: 2026-07-16  
**作者**: AI 评审助手  

---

## 1. 背景

专家评审指出三个 TSP 适配器（CCCAdapter / ICCEAdapter / ICCOAAdapter）存在以下问题：

1. **BindKey 返回空 sharedSecret** — 适配器未实现真正的 TSP bindKey 调用，sharedSecret 为空
2. **ICCE 适配器为骨架实现** — 仅包含模拟响应，无真实 HTTP 客户端
3. **缺乏重试逻辑** — API 调用失败时不会自动重试
4. **缺乏响应校验** — 返回结构无一致性验证
5. **缺乏单元测试** — 适配器层无任何测试覆盖

---

## 2. 增强内容

### 2.1 接口定义增强（adapter-core）

**文件: `TspAdapter.java`**

新增的方法和 DTO：

| 新增项 | 类型 | 说明 |
|--------|------|------|
| `BindKeyRequest` | Record | 包含 userId, vehicleId, vin, deviceId, devicePublicKey, attestationToken, options |
| `BindKeyResponse` | Record | 包含 keyId, sharedSecret (base64), tspPublicKey (base64), sessionId, keySlot, keyData |
| `UnbindKeyRequest` | Record | 包含 userId, keyId, vehicleId, reason |
| `KeyStatusResponse` | Record | 包含 status, createdAtEpochMs, expiresAtEpochMs, boundDeviceId, metadata |
| `bindKey(BindKeyRequest)` | 方法 | 绑定钥匙到设备，返回包含 sharedSecret 的响应 |
| `unbindKey(UnbindKeyRequest)` | 方法 | 解除钥匙绑定 |
| `getKeyStatus(String keyId)` | 方法 | 查询钥匙当前状态 |

### 2.2 重试逻辑（adapter-core）

**新增文件: `RetryUtil.java`**

实现了通用指数退避重试工具：

- **最大重试次数**: 3（可配置）
- **初始延迟**: 500ms（可配置）
- **退避因子**: 2（每次翻倍）
- **最大总超时**: 30s（可配置）
- **抖动**: ±25%（均匀分布）
- **可重试条件**: timeouts、connection refused/reset、5xx 服务端错误
- **不重试条件**: 4xx 客户端错误
- **同步/异步**: 提供 `executeWithRetry`（阻塞）和 `executeAsync`（CompletableFuture）双接口

**文件: `AbstractTspAdapter.java`**

- 所有 `do*()` 调用自动包装 `RetryUtil.executeAsync`
- 子类通过 `configureRetry()` 自定义重试参数

### 2.3 响应 Schema 校验（adapter-core）

**新增文件: `ResponseValidator.java`**

为所有 TspAdapter DTO 提供结构校验：

| 验证器 | 检查内容 |
|--------|----------|
| `validate(VehicleListResponse)` | vehicles 非空, vehicleId 非空, VIN 格式 |
| `validate(KeyResponse)` | success=true 时 keyId 非空 |
| `validate(BindKeyResponse)` | **CRITICAL: success=true 时 sharedSecret 必须非空** |
| `validate(KeyStatusResponse)` | status 必须是 ACTIVE/SUSPENDED/REVOKED/EXPIRED |
| `validate(BindKeyRequest)` | 预检: userId, vehicleId, deviceId, devicePublicKey 必填 |

### 2.4 适配器增强

| 适配器 | 变更 | 说明 |
|--------|------|------|
| **CccAdapter** | 新增 doBindKey / doUnbindKey / doGetKeyStatus | 委派到 CccClient 新方法 |
| **CccClient** | 新增 bindKey / unbindKey / getKeyStatus | 完整 HTTP 实现 + JSON 解析 |
| **CccClient** | 全方法外包 RetryUtil | 所有对外调用有指数退避 |
| **IccoaAdapter** | 新增 doBindKey / doUnbindKey / doGetKeyStatus | 委派到 IccoaClient 新方法 |
| **IccoaClient** | 新增 bindKey / unbindKey / getKeyStatus | 完整 HTTP 实现 + JSON 解析 |
| **IccoaClient** | 全方法外包 RetryUtil | 所有对外调用有指数退避 |
| **IcceAdapter** | 新增 real HTTP client (IcceClient) | 从骨架实现升级为完整 HTTP 客户端 |
| **IcceClient** | 初次创建 | ICCE 协议的完整 HTTP 客户端，含 RetryUtil |
| **IcceAdapter** | 新增 doBindKey / doUnbindKey / doGetKeyStatus | 委派到 IcceClient 新方法 |

### 2.5 单元测试

| 测试文件 | 类型 | 测试数量 |
|----------|------|----------|
| `core/RetryUtilTest.java` | 单元测试 | 8 |
| `core/ResponseValidatorTest.java` | 单元测试 | 11 |
| `ccc/CccAdapterTest.java` | 单元测试 (Mockito) | 10 |
| `ccc/CccClientIntegrationTest.java` | 集成测试 (Mockito) | 5 |
| `icce/IcceAdapterTest.java` | 单元测试 (Mockito) | 10 |
| `iccoa/IccoaAdapterTest.java` | 单元测试 (Mockito) | 10 |

### 2.6 文档

| 文件 | 说明 |
|------|------|
| `docs/tsp-integration-guide.md` | TSP 对接完整指南，含端点清单、认证方式、BindKey 流程详解、重试策略、配置文件参考、排障指南、验收 checklist |

---

## 3. 文件变更清单

### 新增文件

```
backend/adapters/adapter-core/src/main/java/com/digitalkey/adapter/core/RetryUtil.java
backend/adapters/adapter-core/src/main/java/com/digitalkey/adapter/core/ResponseValidator.java

backend/adapters/adapter-core/src/test/java/com/digitalkey/adapter/core/RetryUtilTest.java
backend/adapters/adapter-core/src/test/java/com/digitalkey/adapter/core/ResponseValidatorTest.java

backend/adapters/adapter-ccc/src/test/java/com/digitalkey/adapter/ccc/CccAdapterTest.java
backend/adapters/adapter-ccc/src/test/java/com/digitalkey/adapter/ccc/CccClientIntegrationTest.java

backend/adapters/adapter-icce/src/main/java/com/digitalkey/adapter/icce/IcceClient.java
backend/adapters/adapter-icce/src/test/java/com/digitalkey/adapter/icce/IcceAdapterTest.java

backend/adapters/adapter-iccoa/src/test/java/com/digitalkey/adapter/iccoa/IccoaAdapterTest.java

docs/tsp-integration-guide.md
reports/tsp-adapter-enhance-report.md
```

### 修改文件

```
backend/adapters/adapter-core/src/main/java/.../TspAdapter.java        (新增 BindKeyRequest/Response 等)
backend/adapters/adapter-core/src/main/java/.../AbstractTspAdapter.java (新增 retry + 新方法)
backend/adapters/adapter-ccc/src/main/java/.../CccAdapter.java         (新增 doBindKey/unbind/keyStatus)
backend/adapters/adapter-ccc/src/main/java/.../CccClient.java          (新增 bindKey/unbind/getKeyStatus + retry)
backend/adapters/adapter-icce/src/main/java/.../IcceAdapter.java       (重构: 委派到 IcceClient)
backend/adapters/adapter-iccoa/src/main/java/.../IccoaClient.java      (新增 bindKey/unbind/getKeyStatus + retry)
backend/adapters/adapter-iccoa/src/main/java/.../IccoaAdapter.java     (新增 doBindKey/unbind/keyStatus)
backend/adapters/adapter-ccc/pom.xml                                    (添加测试依赖)
backend/adapters/adapter-icce/pom.xml                                   (添加测试依赖)
backend/adapters/adapter-iccoa/pom.xml                                  (添加测试依赖)
```

---

## 4. 剩余风险

| 风险项 | 级别 | 说明 | 缓解措施 |
|--------|------|------|----------|
| 无法连接真实 TSP | 🟡 中 | 当前无真实 TSP 环境，无法端到端验证 | RetryUtil 已就绪；集成 guide 详述了对接步骤 |
| sharedSecret 仍可能为空 | 🟠 高 | 若 OEM TSP 未正确实现 ECDH，返回空值 | ResponseValidator 会记录 CRITICAL 告警 |
| ICCE 的 API 端点可能不同 | 🟢 低 | ICCE 协议无公开标准 API 文档 | 端点可在配置灵活修改；HttpClient 路径从代码提取到配置不是当前需求 |
| ICCE 保留早期备选 | 🟢 低 | 当 API 不可用时，类仍可扩展回退方案 | 设计保留了 `IcceAdapter` 扩展点 |

---

## 5. 建议后续迭代

1. **添加 TSP 集成测试** — 在获得真实 TSP 测试账号后，添加端到端集成测试
2. **融断机制** — 当 TSP 连续失败 5 次以上时自动熔断（Circuit Breaker）
3. **TSP 令牌刷新** — 自动处理 access token 过期和刷新
4. **指标增强** — 在 AdapterMetrics 中添加 per-TSP-endpoint 的响应时间和错误率指标
5. **配置热加载** — 支持运行时更新 TSP 配置（Spring Cloud Config / K8s ConfigMap）
