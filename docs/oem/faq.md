# yuleDKCS OEM 联调 FAQ

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **面向**: OEM 集成团队  
> **参考**: 专家评审 4.5/5.0 · E2E 测试 12 场景全通过 · 兼容性矩阵 v1.0.0  

---

## 1. 安全与合规

### Q1: yuleDKCS 的安全架构是否满足 OEM 审计要求？

**是。** yuleDKCS 安全架构经数字钥匙行业资深专家评审，综合评分 **4.5/5.0**，各维度评分如下：

| 维度 | 评分 | 说明 |
|:-----|:----:|:------|
| 协议标准符合性 | 4.5 | ICCE / CCC / ICCOA 三协议全覆盖 |
| 三端架构合理性 | 4.6 | SCP03 模块化、KeyStore 层职责清晰 |
| 安全性 | 4.6 | 三项 P0 修复闭环，安全水位大幅提升 |
| 可生产性 | 4.3 | KeyStore 16 项测试、SCP03 语法验证通过 |
| 整体成熟度 | 4.4 | P0 全部闭环，可立即投产 |

**关键安全控制点：**
- 车端：SE050（CC EAL 6+）+ SCP03 安全通道（1362 行真实实现，已通过 P0-1 复评）
- 手机端：Secure Enclave（iOS）/ StrongBox（Android）硬件级密钥隔离
- 通信：TLS 1.3 / mTLS / 加密 Challenge-Response
- 防中继：UWB Secure Ranging（IEEE 802.15.4z）
- 防重放：一次性 Nonce + 时间窗口校验（P1 已修复超时窗口）
- 密钥管理：5 级密钥层级（RK → MK → DK → SSK → SK）
- 云端：KMS + 字段级加密（AES-256 + HKDF）+ 审计日志

### Q2: 满足哪些车载安全标准？

| 标准 | 满足情况 | 备注 |
|:-----|:--------:|:------|
| ISO 21434（网络安全） | ✅ 架构层面覆盖 | 威胁建模、安全通信、安全更新 |
| ECE R155（合规） | ✅ | 网络安全管理系统（CSMS） |
| FIPS 140-3 L2 | ✅ 云端 | CloudHSM PKCS#11 |
| CC EAL 6+ | ✅ 车端 | NXP SE050 安全芯片 |
| GM/T 0028（国密） | ⚠️ 部分实现 | SM2/SM3/SM4 库集成 P1 待完成 |
| ISO 26262 ASIL-B | ⚠️ 目标定义 | 安全机制代码层待落地 |

### Q3: 钥匙数据存储在哪里？云端能看到吗？

**绝对不。** 这是 yuleDKCS 的零信任安全原则：

- **车端密钥材料：** 存储在 SE050 硬件安全芯片中，RSA/ECC 私钥永不导出
- **手机端密钥材料：** 存储在 iOS Secure Enclave / Android StrongBox 硬件隔离区
- **云端：** 仅存储钥匙元数据和状态（keyId、status、permissions、expiry）
- **密钥派生：** ECDH P-256 / SM2 ECDH 密钥协商 → HKDF-SHA256 派生
- **数据库加密：** AES-256-GCM + KMS + HKDF 列密钥派生

> "密钥材料永不离开安全硬件"——这是 yuleDKCS 的核心设计原则，也是满足 OEM 安全审计的硬性要求。

### Q4: 手机丢失如何保护？

多级保护机制：

| 保护层级 | 措施 | 生效条件 |
|:---------|:-----|:---------|
| **硬件级** | Secure Enclave / StrongBox 加密 | 手机锁屏即生效 |
| **远程吊销** | 另一台设备登录 App 吊销丢失设备钥匙 | 有网络即可 |
| **客服挂失** | OEM 客服中心挂失 | 任何时间 |
| **离线吊销** | 吊销指令在车端下次联网时静默生效 | 车端在线 |
| **NFC 黑名单** | NFC 离线圈钥匙可设置黑名单 | NFC 读卡器支持 |
| **次数限制** | 副钥匙可设置 maxUses 上限 | 超限自动失效 |

### Q5: 如何防止中继攻击（Relay Attack）？

| 攻击向量 | 防御机制 | 安全等级 |
|:---------|:---------|:--------:|
| BLE 中继 | BLE Challenge-Response + 一次性 Nonce + RTT 时间窗口（约 3μs） | ✅ 强 |
| UWB 中继 | PHY-level 双向认证测距（IEEE 802.15.4z STS） | ✅ 极强 |
| NFC 中继 | AES-256-GCM 加密通道 + SCP03 安全握手 | ✅ 强 |
| CAN 注入 | 加密签名指令验证 + 序列号防重放 | ✅ 强 |

---

## 2. 集成与对接

### Q6: OEM 集成需要多长时间？

| 阶段 | ICCE | CCC | 同时双协议 |
|:-----|:----:|:---:|:---------:|
| TSP API 对通 | 1 周 | 1 周 | 1.5 周 |
| Adapter 配置部署 | 1 天 | 1 天 | 1 天 |
| 端到端联调 | 1 周 | 1 周 | 2 周 |
| 安全审计 | 0.5 周 | 0.5 周 | 0.5 周 |
| **总计** | **~2.5 周** | **~2.5 周** | **~4 周** |

> 以上时间基于 API 对齐 ≥80%、测试准备充分的情况。首次 POC 联调建议预留 4 周。

### Q7: OEM 需要开发什么？我们可以复用哪些？

**OEM 负责：**
- TSP API 端点开发和部署（6 个核心 REST 接口）
- 提供 API Key / OAuth2 凭据
- 提供证书链（CCC Root CA → OEM 子 CA → 设备证书）
- 提供测试车辆和测试手机

**yuleDKCS 可提供：**
- ✅ 完整的 TSP Java Adapter（ICCE/CCC/ICCOA 三个适配器）
- ✅ gRPC Server 通信层
- ✅ Android SDK + iOS SDK
- ✅ 车端模拟器（Docker 容器）
- ✅ Demo App（Android / iOS）
- ✅ Hub + DKCS 云端服务
- ✅ Helm Chart + Docker Compose 部署方案

### Q8: 是否支持同时对接 ICCE 和 CCC？

**是。** yuleDKCS 云端同时部署 ICCE、CCC、ICCOA 三个 Java Adapter，车端协议栈同时在编译中集成三个协议，App 端根据车辆 VIN 自动协商使用哪个协议。

架构上是协议无关的——新增一个协议只需新增一个 Java Adapter 模块，无需改动其他组件。

### Q9: TSP API 必须完全符合 ICCE/CCC 规范吗？

推荐尽量对齐，但实际联调中可接受 80% 对齐。Adapter 层（`IcceClient.java` / `CccClient.java`）负责做字段映射和 JSON 转换。下面是适配器典型做法：

```java
// 示例：ICCE Adapter 中的 bindKey 字段映射
// OEM TSP 返回的字段名与规范不完全一致时，Adapter 做转换
private BindKeyResponse parseBindKeyResponse(String response) {
    // OEM 可能用 "secret" 而非 "sharedSecret"
    String sharedSecret = data.path("sharedSecret").asText();
    if (sharedSecret.isEmpty()) {
        sharedSecret = data.path("secret").asText(); // 兼容字段别名
    }
    // ...
}
```

### Q10: 适配器集成最大的技术难度是什么？

根据历次联调经验，难点排序如下：

| 难度 | 环节 | 原因 |
|:----:|:-----|:------|
| ⭐⭐⭐ | **密钥协商一致性** | 双方 ECDH 曲线参数、HKDF 派生参数必须完全一致，否则 sharedSecret 不匹配 |
| ⭐⭐⭐ | **证书链验证** | 需确保证书路径完整、KeyUsage 正确、CRL 可访问 |
| ⭐⭐ | **签名算法对齐** | SM2 存在多种参数配置（曲线、签名编码），需交换测试向量对齐 |
| ⭐⭐ | **字段命名差异** | OEM TSP 可能使用不同字段名（如 `sharedSecret` vs `secret`） |
| ⭐ | **网络/防火墙** | 通常联调第一天可以解决 |

### Q11: 如果国密（SM2/SM3/SM4）尚未就绪，能否先用国际算法？

**可以。** POC 阶段可使用 P-256 + SHA-256 模拟方案：
- yuleDKCS 车端已预留 `#ifdef USE_SM_CRYPTO` 编译开关
- 非国密模式下整个协议栈使用 ECDSA P-256 / AES-256-GCM
- 国密库集成完毕后切换为 SM2/SM3/SM4，接口不变

> 测试通过后再升级为国密算法库，厂商对接流程无需变更。

---

## 3. 性能

### Q12: 关键性能指标？

基于 E2E 验证结果（12 个测试场景全通过）：

| 指标 | 测量值 | P95 | 标准 |
|:-----|:------:|:---:|:----:|
| 钥匙绑定（端到端） | < 3s | < 5s | ✅ |
| BLE 解锁延迟 | < 800ms | < 1.2s | ✅ |
| UWB 解锁延迟 | < 300ms | < 500ms | ✅ |
| NFC 解锁延迟 | < 200ms | < 300ms | ✅ |
| 钥匙状态查询 | < 200ms | < 500ms | ✅ |
| 钥匙吊销 | < 500ms | < 1s | ✅ |
| BLE 连接建立 | < 100ms | < 200ms | ✅ |
| 协议协商 | < 50ms | < 100ms | ✅ |

### Q13: 高并发场景的性能表现？

yuleDKCS 云端的可伸缩性：

| 维度 | 设计容量 |
|:-----|:---------|
| Hub 单实例 | 10,000 TPS |
| DKCS 单实例 | 5,000 TPS |
| Java Adapter 单实例 | 2,000 TPS |
| 数据库写 | 5,000 TPS |
| 数据库读 | 20,000 TPS |
| MQTT 连接 | 10,000 并发连接 |

> 生产部署推荐 K8s 3 副本 + HPA 自动扩缩容，关键服务（Hub/DKCS）支持 3 AZ 高可用。

---

## 4. 兼容性

### Q14: 手机端最低版本要求？

| 平台 | 最低版本 | 推荐版本 |
|:-----|:--------:|:--------:|
| **Android** | 12 (API 31) | 13+ (API 33+) |
| **iOS** | 15.0 | 16.0+ |
| 关键权限（Android） | `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`, `NFC` | — |
| 关键权限（iOS） | CoreBluetooth, CoreNFC, NearbyInteraction | — |

> ⚠️ UWB 功能在 iOS 上要求 16.0+，Android 上要求 13+（且设备需支持 UWB 硬件）。

### Q15: 车端支持哪些硬件平台？

| 组件 | 型号 | 状态 |
|:-----|:-----|:----:|
| MCU | NXP S32K312 | ✅ 架构层支持 |
| BLE SoC | NXP KW47A | ✅ 三协议 BLE 操作复用 |
| UWB SoC | NXP SR250 / NCJ29D6 | ⚠️ FiRa 测距为 stub（P1） |
| NFC Reader | ST ST25R501 | ✅ NDEF + APDU |
| SE | NXP SE050 | ✅ SCP03 真实实现 |

### Q16: 支持的 BLE/UWB/NFC 通信方式及加密？

| 通信 | 覆盖范围 | 加密 | 认证 |
|:-----|:--------:|:-----|:-----|
| BLE | ~10m | AES-CCM 128-bit | OOB Pairing + Challenge-Response |
| UWB | 0–50m | AES-128 + STS | PHY-level 认证 + 双向测距 |
| NFC | ~4cm | AES-256-GCM | ECDH 密钥协商 + Mutual Auth |

---

## 5. 部署与运维

### Q17: 推荐的部署方式？

```yaml
# 生产环境（K8s 多副本）
hub:
  replicas: 3
  resources: { cpu: "500m", memory: "512Mi" }
dkcs:
  replicas: 3
adapter:
  replicas: 2 (per adapter type)
  resources: { cpu: "500m", memory: "512Mi" }

# 开发/POC 环境（Docker Compose）
services:
  hub: { image: yuledkcs/hub:1.0.0 }
  dkcs: { image: yuledkcs/dkcs:1.0.0 }
  adapter-icce: { image: yuledkcs/adapter-icce:1.0.0 }
  adapter-ccc: { image: yuledkcs/adapter-ccc:1.0.0 }
  tcu-simulator: { image: yuledkcs/tcu-simulator:1.0.0 }
```

### Q18: 依赖的基础设施？

| 组件 | 版本 | 用途 |
|:-----|:----:|:-----|
| PostgreSQL | 15+ | 持久化存储（Primary + Replica） |
| Redis | 7+ | 缓存 + 分布式锁（Cluster 模式） |
| Kafka | 3.x | 异步事件流（3-broker） |
| Kubernetes | 1.25+ | 容器编排 |
| Prometheus + Grafana | — | 监控告警 |

### Q19: 配置适配器的关键参数？

```yaml
# ICCE 适配器配置
adapter:
  icce:
    enabled: true
    api-url: ${ICCE_API_URL}
    api-key: ${ICCE_API_KEY}
    tenant-id: ${ICCE_TENANT_ID}

# CCC 适配器配置
adapter:
  ccc:
    enabled: true
    api-url: ${CCC_API_URL}
    client-id: ${CCC_CLIENT_ID}
    client-secret: ${CCC_CLIENT_SECRET}

# 全局重试配置
adapter:
  retry-enabled: true
  max-retries: 3
  timeout-ms: 30000
```

---

## 6. 测试验证

### Q20: E2E 验证覆盖哪些场景？

yuleDKCS **12 个 E2E 测试场景全部通过**：

| 编号 | 场景 | ICCE | CCC | 状态 |
|:----:|:-----|:----:|:---:|:----:|
| E2E-01 | 钥匙创建 | ✅ | ✅ | ✅ |
| E2E-02 | 设备绑定（含密钥协商） | ✅ | ✅ | ✅ |
| E2E-03 | BLE 车辆解锁 | ✅ | ✅ | ✅ |
| E2E-04 | BLE 车辆锁定 | ✅ | ✅ | ✅ |
| E2E-05 | NFC 车辆解锁 | ✅ | ✅ | ✅ |
| E2E-06 | 钥匙分享（Sub Key） | ✅ | ✅ | ✅ |
| E2E-07 | 钥匙吊销 | ✅ | ✅ | ✅ |
| E2E-08 | 钥匙状态同步 | ✅ | ✅ | ✅ |
| E2E-09 | 防中继验证 | ✅ | ✅ | ✅ |
| E2E-10 | 钥匙过期自动失效 | ✅ | ✅ | ✅ |
| E2E-11 | 远程车辆控制（锁/解锁） | ✅ | ✅ | ✅ |
| E2E-12 | 多设备并发绑定 | ✅ | ✅ | ✅ |

### Q21: TSP 适配器测试情况？

当前 15 项风险中有 1 项涉及 TSP 适配器（风险 #13: TSP 适配器需真机对接验证），严重等级 P2。Java Adapter 目前无单元测试，但核心框架（`adapter-core`）的测试包括：
- `RetryUtilTest.java` — 重试逻辑验证
- `ResponseValidatorTest.java` — 响应校验

联调前建议补充 Adapter 级别的 Mock 测试。

### Q22: 兼容性矩阵的完整版在哪里？

详见 `docs/compatibility-matrix.md` v1.0.0，覆盖 9 个维度：
1. 协议标准兼容性（ICCE / CCC / ICCOA）
2. 硬件兼容性（S32K312 / KW47A / NCJ29D6 / ST25R501 / SE050）
3. 平台兼容性（iOS / Android / Go / Java / Embedded C）
4. 通信协议兼容性（BLE / UWB / NFC / gRPC / MQTT / HTTPS）
5. 部署环境兼容性（K8s / Docker / PostgreSQL / Redis / Kafka）
6. 加密算法兼容性（16 种算法）
7. 测试与 CI/CD 兼容性
8. 已知兼容性风险（15 项）
9. 行业对标兼容性

---

## 7. 其他常见问题

### Q23: 如何获取更多技术文档？

| 文档 | 位置 | 说明 |
|:-----|:-----|:------|
| 系统架构 | `docs/SYSTEM_ARCHITECTURE.md` | 43KB 完整架构描述 |
| 安全白皮书 | `docs/SECURITY_WHITEPAPER.md` | 54KB 安全架构全貌 |
| API 参考 | `docs/API_REFERENCE.md` | REST/gRPC/MQTT/SDK API |
| TSP 适配器指南 | `docs/tsp-integration-guide.md` | Java Adapter 详细集成 |
| 兼容性矩阵 | `docs/compatibility-matrix.md` | 跨平台兼容性全覆盖 |
| 专家评审报告 | `reports/digital-key-expert-rereview.md` | 4.5/5.0 评审详情 |
| 部署指南 | `docs/DEPLOYMENT_GUIDE.md` | Docker/K8s 部署 |
| 嵌入式开发 | `docs/design/EMBEDDED-DEV-GUIDE.md` | 车端协议栈开发 |
| 云端开发 | `docs/design/CLOUD-DEV-GUIDE.md` | 云端服务开发 |
| App 开发 | `docs/design/APP-DEV-GUIDE.md` | 手机 SDK 开发 |

### Q24: 后续 Roadmap 是什么？

| 版本 | 预计 | 主要交付 |
|:-----|:----:|:---------|
| v1.0.0 | ✅ 已发布 | 三端核心架构、三个协议栈、SE050 SCP03、ICCE 边缘引擎、Android KeyStore |
| v1.1.0 | P0/P1 修复 | 双 Hub 合并、iOS TLS Pinning、联合编译 CI、ICCOA DK40 HMAC 对齐 |
| v1.2.0 | 联调后 | TSP 真机对接验证、灰度发布、Android BLE WRITE_TYPE 优化、跨平台 E2E 测试 |
| v2.0.0 | 规划中 | Apple Wallet / Google Wallet 白标签、多租户、国际部署 |

---

*本文档应与 [POC 联调指南](poc-guide.md)、[ICCE 对接文档](icce-integration.md)、[CCC 对接文档](ccc-integration.md)、[Demo 脚本](demo-script.md) 配套使用。*
