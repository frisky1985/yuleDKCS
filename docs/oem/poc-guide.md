# POC 联调指南 — yuleDKCS × OEM 数字钥匙

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **适用场景**: OEM POC 联调（侧重华为 ICCE / CCC Digital Key 3.0）  
> **系统版本**: yuleDKCS v1.0.0  
> **专家评审评分**: 4.5 / 5.0  

---

## 1. POC 目标与范围

### 1.1 核心目标

通过端到端联调验证 yuleDKCS 数字钥匙平台与 OEM TSP（Telematics Service Provider）的集成能力，涵盖：

| 目标 | 优先级 | 成功标准 |
|:-----|:------:|:---------|
| TSP API 对接贯通 | P0 | 6 个核心接口（getVehicles / requestKeys / revokeKeys / bindKey / unbindKey / getKeyStatus）全部联调通过 |
| 端到端钥匙生命周期 | P0 | 从钥匙创建 → 设备绑定 → 近场解锁 → 钥匙吊销全链路跑通 |
| ICCE 国密合规验证 | P0 | SM2 签名验签链路在 Java Adapter 层验证通过 |
| CCC 证书链验证 | P0 | CCC Root CA → OEM 子 CA → 设备证书链得到正确验证 |
| 性能基准 | P1 | 钥匙绑定 < 3s，解锁指令 < 500ms（UWB）/ < 1s（BLE） |
| 安全审计 | P1 | 通信信道（mTLS）、密钥隔离（SE050/SE）、防中继机制通过审查 |

### 1.2 范围定义

**范围内（Scope In）：**

- ICCE T/CA 110-2020 协议兼容性 6 项核心接口
- CCC Digital Key 3.0 Release 1 协议兼容性 8 项核心接口
- 云端 → 云端对接（yuleDKCS Hub ↔ OEM TSP）
- 移动端 → 云端对接（手机 App ↔ yuleDKCS Hub）
- 移动端 → 车端近场通信（BLE / NFC / UWB）
- Java TSP Adapter 热部署与配置（Spring Boot gRPC Server）
- mTLS 双向证书认证链验证

**范围外（Scope Out）：**

- OEM 自有车端固件集成（POC 期间使用 yuleDKCS 车端模拟器）
- OEM TSP 业务系统定制开发（如用户管理、支付）
- Apple Wallet / Google Wallet 白标签适配
- 多租户隔离策略
- 灰度发布能力

### 1.3 POC 参与角色

| 角色 | 责任 | 人员要求 |
|:-----|:-----|:---------|
| **OEM 接口人** | 协调 TSP API 凭据、证书、测试车辆 | 1 人（技术 PM） |
| **OEM 开发工程师** | TSP API 联调对接、车端配置 | 1–2 人（后端/嵌入式） |
| **yuleDKCS 技术接口** | Adapter 配置、联调支持、问题排查 | 1 人 |
| **yuleDKCS 架构师** | 协议层解读、安全审计、方案评审 | 0.5 人 |

---

## 2. 双方准备工作清单

### 2.1 OEM 侧准备（A 角）

#### TSP API 端点与凭据

| 协议 | 必需项 | 优先级 | 备注 |
|:-----|:-------|:------:|:-----|
| **ICCE** | `adapter.icce.api-url`（HTTPS 端点） | P0 | 基础 URL，如 `https://icce-tsp.oem.com` |
| ICCE | `adapter.icce.api-key` | P0 | 静态 API Key |
| ICCE | `adapter.icce.tenant-id` | P0 | 租户标识 |
| ICCE | ICCE API 接口文档 | P0 | 接口签名的 SM2 规范 |
| **CCC** | `adapter.ccc.api-url`（HTTPS 端点） | P0 | 基础 URL |
| CCC | `adapter.ccc.client-id` / `client-secret` | P0 | OAuth2 Client Credentials |
| CCC | CCC OAuth2 Token 端点 | P0 | Token 获取 URL |

#### 网络与基础设施

| 项目 | 要求 | 确认方法 |
|:-----|:-----|:---------|
| TSP API 公网可达 | HTTPS 443 端口开放 | `curl -I https://<api-url>` |
| TLS 版本 | ≥ 1.2（推荐 1.3） | `openssl s_client` 验证 |
| 防火墙规则 | 将 yuleDKCS 云服务 IP 加入白名单 | 预发布时提供 CIDR |
| API 速率限制 | 了解限流策略（如 1000 req/min） | 确认 TSP 文档 |
| 测试车辆 VIN | 至少提供 2 台测试车辆 VIN | 含 ICCE 与 CCC 各至少 1 台 |
| 测试手机设备 ID | 至少提供 1 台 Android + 1 台 iOS | 用于钥匙绑定测试 |

#### 证书与密钥材料

| 项目 | ICCE | CCC |
|:-----|:-----|:-----|
| 根证书 | ICCE Root CA 证书（SM2 签发） | CCC Root CA（ECC P-256） |
| OEM 子 CA 证书 | ICCE OEM CA 证书 | CCC OEM CA 证书 |
| OEM 签名私钥 | SM2 私钥（用于绑定签名） | ECDSA P-256 私钥 |
| 证书撤销列表（CRL） | ICCE CRL 端点 | CCC CRL 端点（按规范要求） |
| 密钥派生参数 | ICCE 密钥派生常数 | CCC KDF 参数 |

#### 文档

- [ ] ICCE TSP API 接口定义文档（OpenAPI / Swagger）
- [ ] CCC TSP API 接口定义文档
- [ ] ICCE 国密算法对接说明（SM2/SM3/SM4 参数）
- [ ] CCC 证书链说明（Root CA → 子 CA）
- [ ] OEM 测试环境网络拓扑图
- [ ] OEM TSP 故障码与错误码说明

### 2.2 yuleDKCS 侧准备（B 角）

| 项目 | 说明 | 交付物 |
|:-----|:-----|:-------|
| ICCE Java Adapter | 预配置集成 Spring Boot 服务 | `adapter-icce` Docker Image |
| CCC Java Adapter | 预配置集成 Spring Boot 服务 | `adapter-ccc` Docker Image |
| Adapter gRPC Server | 统一 gRPC 通信层 | `adapter-grpc-server` Docker Image |
| 适配器配置模板 | 可替换的 application.yml | 配置模板文件 |
| Hub + DKCS 部署 | Docker Compose 全套部署 | `docker-compose-poc.yml` |
| 联调测试脚本 | ICCE / CCC 接口一致性测试 | `scripts/poc-test/` |
| 联调监控面板 | 适配器健康与错误率指标 | Grafana Dashboard JSON |
| 移动端 Demo App | Android/iOS Demo App（含 ICCE/CCC 协议支持） | 构建 APK / IPA |

---

## 3. 联调时间线建议

### 3.1 总体计划（4 周）

```
周次 │  阶段         │  主要活动
─────┼───────────────┼────────────────────────────────────────────────
  W1 │  环境搭建     │  TSP API 凭据发放、网络连通性验证、Docker 部署
  W1 │  接口对通     │  6 个核心接口 curl 验证通过
  W2 │  ICCE 联调    │  ICCE 适配器集成、bindKey + unlock 联调
  W2 │  CCC 联调     │  CCC 适配器集成、bindKey + unlock 联调
  W3 │  端到端验证   │  手机 App ↔ 云端 ↔ TSP 全链路跑通
  W3 │  安全审计     │  mTLS、SM2 签名、证书链验证
  W4 │  回归与收尾   │  BUG 修复、性能压测、POC 报告输出
```

### 3.2 详细里程碑

| 里程碑 | 时间 | 验收标准 |
|:-------|:---:|:---------|
| **M1: 环境就绪** | W1 D3 | OEM TSP 端点可通过 curl 访问，凭据验证通过，yuleDKCS 全套服务运行正常 |
| **M2: ICCE 贯通** | W2 D3 | ICCE 6 个核心接口对接完成，bindKey 返回非空 sharedSecret；SM2 签名验证通过 |
| **M3: CCC 贯通** | W2 D5 | CCC 6 个核心接口对接完成，OAuth2 Token 获取正常；X.509 证书链验证通过 |
| **M4: 端到端** | W3 D4 | 手机 App 完成钥匙创建→绑定→BLE解锁→吊销全流程 |
| **M5: POC 完成** | W4 D4 | 联调报告签署，问题清单闭环 |

### 3.3 每日联调流程建议

```
9:00  站会 — 同步昨日进展、今日计划、阻塞项
9:30  联调执行 — ICCE / CCC 交替进行
12:00 午休
14:00 联调执行 — 端到端测试
16:00 问题复现与排查
17:00 日总结 — 输出联调日志、关闭已确认问题
```

---

## 4. 风险与假设

### 4.1 风险清单

| 编号 | 风险描述 | 概率 | 影响 | 缓解措施 |
|:-----|:---------|:----:|:----:|:---------|
| R1 | OEM TSP API 文档与实际实现不一致 | 中 | 高 | 优先通过 curl 验证每个接口行为；逐接口对通后再集成 Adapter |
| R2 | ICCE SM2 签名算法参数差异（如曲线参数 vs. 国标） | 中 | 高 | POC 开始前交换 SM2 测试向量（明文→签名→验签）对齐算法参数 |
| R3 | OEM TSP 存在不明速率限制 | 低 | 中 | 联调前确认限流策略；Adapter 已内置指数退避重试 |
| R4 | 跨地域网络延迟导致绑定超时 | 低 | 中 | 确认 TSP 端点部署地域；Adapter 超时 30s 可配置 |
| R5 | 测试车辆/手机临时不可用 | 低 | 中 | 备选测试车辆和手机各 +2 |
| R6 | OEM 证书链未预置到 yuleDKCS 信任存储 | 低 | 高 | 联调开始前交换证书；mTLS 配置可先降级为单向 TLS |
| R7 | CCC Root CA 证书交叉认证问题 | 低 | 中 | 确认 CCC Root CA 是否为规范指定 CA |

### 4.2 假设

| 编号 | 假设条件 |
|:-----|:---------|
| A1 | OEM TSP 接口符合 ICCE T/CA 110-2020 或 CCC DK 3.0 规范要求 |
| A2 | OEM TSP 支持 HTTPS TLS 1.2+ 加密通信 |
| A3 | OEM TSP 提供的 API Key / OAuth2 凭据在 POC 期间有效 |
| A4 | OEM 测试车辆支持 BLE + NFC 近场通信（ICCE/CCC 最低要求） |
| A5 | OEM 测试手机（Android 12+ / iOS 15+）已安装 Demo App |
| A6 | POC 期间 yuleDKCS 端使用非生产密钥材料进行测试 |
| A7 | 车端 POC 使用 yuleDKCS 车端模拟器而非真实 OEM TCU |

### 4.3 降级与备选方案

| 场景 | 降级方案 |
|:-----|:---------|
| OEM TSP 端点未就绪 | 使用 yuleDKCS 内置 MockAdapter（模拟 TSP 行为），绑定 / 解锁返回预设成功 |
| ICCE SM2 算法未对接 | 回退到 P-256 + SHA-256 模拟方案（测试通过后再升级为国密） |
| CCC OAuth2 不可用 | 临时开放 IP 白名单 + API Key 认证 |
| 车端模拟器不可用 | 使用 yuleDKCS 云端解锁模拟（不经过 BLE/UWB，直接通过 Hub MQTT 下发指令） |
| 网络连通性故障 | 搭建 VPN 隧道或专线中转 |

---

## 5. 联调环境架构图

```
                        ┌──────────────────────┐
                        │    OEM TSP API       │
                        │   (HTTPS / mTLS)     │
                        └─────────┬────────────┘
                                  │
                        ┌─────────▼────────────┐
                        │   Java Adapter(s)     │
                        │  ┌────┐ ┌────┐ ┌───┐ │
                        │  │ICCE│ │CCC │ │...│ │
                        │  └────┘ └────┘ └───┘ │
                        │   gRPC Server :50051  │
                        └─────────┬────────────┘
                                  │ gRPC
                        ┌─────────▼────────────┐
                        │    yuleDKCS Hub       │
                        │  (Go, HTTP/gRPC)      │
                        └─────────┬────────────┘
                                  │ HTTPS
                        ┌─────────▼────────────┐
                        │  Mobile Demo App      │
                        │  Android / iOS SDK    │
                        │  BLE/UWB/NFC ↗ TCU   │
                        └──────────────────────┘
```

---

## 6. 联调成功标准

| 维度 | 具体指标 | 最低要求 | 目标值 |
|:-----|:---------|:--------:|:------:|
| 接口通过率 | ICCE 6 项核心接口 | 100% | 100% |
| 接口通过率 | CCC 6 项核心接口 | 100% | 100% |
| 端到端场景 | ICCE 钥匙创建→绑定→解锁→吊销 | 6/12 场景 | 12/12 场景 |
| 端到端场景 | CCC 钥匙创建→绑定→解锁→吊销 | 6/12 场景 | 12/12 场景 |
| 绑定性能 | 全链路绑定时间 P95 | < 5s | < 3s |
| 解锁性能（BLE） | 指令发送到车门响应的延迟 P95 | < 2s | < 1s |
| 解锁性能（UWB） | 指令发送到车门响应的延迟 P95 | < 1s | < 500ms |
| 安全审计项 | mTLS、证书链、SM2/ECC 签名 | 全部通过 | 全部通过 |
| 重试稳定性 | TSP 503 场景下自动重试 3 次 | 通过 | 通过 |

---

## 7. 联调产出物清单

| 产出物 | 负责方 | 交付时间 |
|:-------|:------:|:---------|
| 联调环境搭建记录 | yuleDKCS | W1 D5 |
| ICCE 接口对通确认表 | OEM + yuleDKCS | W2 D3 |
| CCC 接口对通确认表 | OEM + yuleDKCS | W2 D5 |
| 端到端测试报告（12 场景） | yuleDKCS | W3 D5 |
| 性能测试报告 | yuleDKCS | W4 D2 |
| POC 联调总结报告 | 双方 | W4 D5 |

---

*本文档应与 [ICCE 对接文档](icce-integration.md)、[CCC 对接文档](ccc-integration.md)、[Demo 脚本](demo-script.md) 配套使用。*
