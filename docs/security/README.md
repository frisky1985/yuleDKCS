# 🔐 yuleDKCS — 第三方安全渗透测试指南

> **版本**: 1.0.0 | **日期**: 2026-07-16
> **目标**: 为渗透测试团队提供完整的测试模板、预备条件和流程

---

## 目录

1. [测试预备条件](#1-测试预备条件)
2. [测试流程](#2-测试流程)
3. [测试报告模板](#3-测试报告模板)
4. [支持与联络](#4-支持与联络)

---

## 1. 测试预备条件

### 1.1 硬件要求

```
┌─────────────────────────────────────────────┐
│  测试人员设备                  yuleDKCS 环境  │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   │
│  │Pentest  │   │Android  │   │ Car     │   │
│  │Laptop   │───│ Device  │───│Simulator│   │
│  │(Burp)   │   │(Rooted) │   │(Docker) │   │
│  └─────────┘   └─────────┘   └─────────┘   │
│       │            │                        │
│       │    ┌───────▼───────┐               │
│       └────┤  iOS Device   │               │
│            │  (Jailbroken) │               │
│            └───────────────┘               │
└─────────────────────────────────────────────┘
```

**必备设备**:
| 设备 | 数量 | 用途 | 备注 |
|------|:----:|------|------|
| 渗透测试笔记本 | 1 | Burp Suite, Wireshark, 自定义工具 | macOS/Linux 推荐 |
| Android 设备 | 1 | KeyStore 测试、API 测试 | API 34+, Root 可选 |
| iOS 设备 | 1 | Keychain 测试、证书固定测试 | iOS 17+, 越狱可选 |
| BLE Sniffer | 1 | BLE 协议抓包 | nRF52840 Dongle 推荐 |

### 1.2 软件要求

| 软件 | 版本 | 用途 |
|------|:----:|------|
| Burp Suite Professional | 2024+ | HTTP(S) 流量拦截和修改 |
| OWASP ZAP | 2.14+ | API 自动化扫描 |
| Wireshark | 4.0+ | 网络协议分析 |
| nRF Sniffer for Wireshark | v4+ | BLE 抓包分析 |
| Go | 1.22+ | 运行 E2E 测试和 Fuzz |
| Docker | 24+ | 启动测试基础设施 |
| Python | 3.11+ | 自定义攻击脚本 |
| hashcat / john | latest | JWT 密钥爆破 |
| jwt-cli | latest | JWT 编解码 |
| Frida | 16+ | 移动端 hook (SSL pinning bypass) |
| objection | latest | 移动端安全评估 |
| Proxmark3 | RDV4 | NFC 协议分析 (可选) |

### 1.3 测试环境搭建

#### 步骤 1: 获取代码和凭据

```bash
# 克隆代码库 (从内部 Git)
git clone git@github.com:frisky1985/yuleDKCS.git
cd yuleDKCS

# 获取测试凭据 (联系 security@yuledkcs.com)
# 凭据包括:
#   - JWT secret: test-jwt-secret-for-pentest-2026 (测试专用)
#   - mTLS 客户端证书: client.p12
#   - Hub gRPC 端点信息
#   - Carsim 测试密钥对
```

#### 步骤 2: 启动测试基础设施

```bash
# 使用 Docker Compose 启动所有测试服务
make e2e-up

# 等待所有容器就绪 (约 30s)
docker ps

# 验证服务健康状态
curl -s http://localhost:8080/health | jq .
```

预期输出:
```json
{"status": "ok"}
```

#### 步骤 3: 验证 E2E 测试环境

```bash
# 运行基础 E2E 测试确认环境正常
cd tests/e2e
go test ./scenarios/ -run TestKeyBinding -v
# 预期: ❄️✅ PASS

# 运行安全重放测试
go test ./scenarios/ -run TestSecurityReplay -v
# 预期: 重放检测 + 报警触发
```

#### 步骤 4: 配置 Burp Suite

1. 启动 Burp Suite → Proxy → Options
2. 设置代理监听: `127.0.0.1:8081`
3. 安装 Burp CA 证书到:
   - Android 设备: 设置 → 安全 → 安装证书
   - iOS 设备: Safari 访问 `http://burpsuite` → 安装描述文件
4. 配置 Android 设备代理: 设置 → Wi-Fi → 高级 → 代理 = 手动 → `burp-ip:8081`
5. 验证: Android 浏览器访问 `http://burp` → 显示 Burp Suite 页面

> **注意**: Android 7+ 默认不信任用户安装的 CA。如果使用 Root 设备，需要将 Burp CA 安装到系统信任存储区：
> ```bash
> adb root
> adb remount
> openssl x509 -inform der -in burp-ca.der -out /tmp/cacert.pem
> adb push /tmp/cacert.pem /system/etc/security/cacerts/
> adb shell chmod 644 /system/etc/security/cacerts/9a5ba580.0
> adb reboot
> ```

### 1.4 测试数据准备

```bash
# 1. 生成测试密钥对 (若没有)
cd certs
openssl ecparam -genkey -name prime256v1 -out test_ecdsa_p256.key
openssl ec -in test_ecdsa_p256.key -pubout -out test_ecdsa_p256.pub

# 2. 生成测试 JWT
JWT_SECRET="test-jwt-secret-for-pentest-2026"
JWT_TOKEN=$(jwt encode \
  --alg HS256 \
  --secret "$JWT_SECRET" \
  '{"user_id":"admin","role":"admin","exp":'$(date -d "+15 min" +%s)'}')
echo "JWT: $JWT_TOKEN"

# 3. 验证 JWT
curl -s -H "Authorization: Bearer $JWT_TOKEN" \
  http://localhost:8080/api/v1/keys | jq .
```

---

## 2. 测试流程

### 2.1 测试阶段

```
Phase 1 ─── 信息收集     (Day 1, 4h)
  ├── 代码审计 (SAST)
  ├── API 文档分析
  └── 端点发现

Phase 2 ─── 云端 API     (Day 1-2, 12h)
  ├── JWT 安全测试
  ├── 速率限制测试
  ├── 参数注入测试
  └── RBAC 提权测试

Phase 3 ─── 嵌入式协议   (Day 2-3, 16h)
  ├── SCP03 握手安全
  ├── BLE 重放/畸形帧
  ├── BER-TLV Fuzzing
  └── UWB/NFC 协议测试

Phase 4 ─── 移动端       (Day 3-4, 12h)
  ├── KeyStore 导出测试
  ├── Keychain 泄露测试
  ├── 证书固定绕过
  └── Frida hook 分析

Phase 5 ─── 端到端       (Day 4-5, 8h)
  ├── 完整攻击链
  ├── 密钥分享/提权
  └── 综合报告编写
```

### 2.2 每日流程模板

```yaml
每日开始:
  - 确认测试环境健康 (docker ps, curl health)
  - 生成新的 JWT token (15min)
  - 同步测试进度到看板

每日结束:
  - 截图所有发现
  - 记录测试日志到 `pentest-logs/day-{n}.md`
  - 备份 Burp Suite 项目文件
  - 确认所有测试环境已关闭 (no lingering containers)
```

### 2.3 已知限制

| 项目 | 限制 | 影响 |
|------|------|------|
| UWB | 仅在 Carsim 模拟 | 真实 UWB PHY 安全测试无法执行 |
| SCP03 | 使用 mock SE050 | 真实 SE050 的物理安全无法测试 |
| 移动端 | Debug 构建 (无混淆) | 发布构建可能更难分析 |
| BLE | 通过 TCP 模拟 | BLE PHY 层 (重传、jamming) 无法测试 |

---

## 3. 测试报告模板

### 3.1 报告结构

```
┌─────────────────────────────────────────────┐
│  yuleDKCS 渗透测试报告                        │
│  v1.0.0                                      │
├─────────────────────────────────────────────┤
│  目录                                         │
│  1. 执行摘要                                  │
│  2. 测试范围与目标                             │
│  3. 测试方法论                                │
│  4. 发现总览                                  │
│  5. Critical 发现详细                         │
│  6. High 发现详细                             │
│  7. Medium 发现详细                           │
│  8. Low 发现详细                              │
│  9. 修复建议                                  │
│  10. 附录                                     │
└─────────────────────────────────────────────┘
```

### 3.2 执行摘要模板

```markdown
# 1. 执行摘要

**测试目标**: yuleDKCS v0.9.0 — 数字钥匙平台安全渗透测试
**测试期间**: YYYY-MM-DD → YYYY-MM-DD
**测试团队**: [渗透测试团队名称]
**评估类型**: 黑盒 + 灰盒 渗透测试

## 总体评级: [PASS / PASS WITH WEAKNESS / FAIL]

## 发现统计

| 严重度 | 数量 | 已确认 | 已修复 | 已接受 |
|:------:|:----:|:------:|:------:|:------:|
| 🔴 Critical | N | N | N | N |
| 🟠 High     | N | N | N | N |
| 🟡 Medium   | N | N | N | N |
| 🔵 Low      | N | N | N | N |
| **总计**   | N | N | N | N |

## 关键发现

1. **[CRITICAL] 标题**: 简要描述
2. **[HIGH] 标题**: 简要描述
3. **[MEDIUM] 标题**: 简要描述

## 预期修复时间线

| 优先级 | 时间线 | 备注 |
|:------:|:------:|------|
| 🔴 Critical | ≤30 天 | 阻塞发布 |
| 🟠 High     | ≤90 天 | 发布前必须修复 |
| 🟡 Medium   | ≤180 天 | 规划迭代修复 |
| 🔵 Low      | ≤365 天 | 记录 + 排期 |

## 风险接受

- 已接受的 Risk: [列出风险评估中的接受项]
```

### 3.3 发现的详细模板

```markdown
## [CRITICAL] 发现 #1: [标题]

### 描述
[发现的详细描述，包括攻击场景]

### 影响
[漏洞被利用后的影响]

### 受影响的端点
- [端点 URL / 服务名称]
- [具体 API/协议]

### 复现步骤

1. [步骤 1]
2. [步骤 2]
3. [步骤 3]

### 概念验证 (PoC)

```bash
# PoC 命令或脚本
```

### 截图/证据

[截图或日志输出]

### 修复建议

```diff
[建议的代码修复 diff]
```

### 状态
- [✅ 已修复 / 🔄 修复中 / ⏳ 已排期 / ❌ 未修复]
- **CVSS 3.1**: X.X (AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H)
- **CWE**: CWE-NNN
- **分类**: [密码学/协议/实现/配置]
```

### 3.4 CVSS 评分参考

| 评分 | 等级 | 颜色 |
|:----:|:----:|:----:|
| 9.0–10.0 | Critical | 🔴 Red |
| 7.0–8.9 | High | 🟠 Orange |
| 4.0–6.9 | Medium | 🟡 Yellow |
| 0.1–3.9 | Low | 🔵 Blue |

---

## 4. 支持与联络

### 技术联络
| 角色 | 联系人 | 联系方式 |
|:----:|:------:|:---------|
| 安全主管 | security@yuledkcs.com | 邮件 / PGP |
| 嵌入式安全 | embedded-sec@yuledkcs.com | 即时消息 |
| 云端安全 | cloud-sec@yuledkcs.com | 即时消息 |
| 移动端安全 | mobile-sec@yuledkcs.com | 即时消息 |

### 测试过程中

| 事项 | 联系方式 | 响应时间 |
|:----|:---------|:--------:|
| 环境问题 | #yuledkcs-pentest Slack | ≤ 30min |
| 发现敏感数据 | security@yuledkcs.com (PGP) | ≤ 15min |
| 需要凭据 | security@yuledkcs.com | ≤ 1h |
| 服务意外中断 | #yuledkcs-incident Slack | ≤ 5min |

### 安全漏洞披露

| 项目 | 信息 |
|:-----|:-----|
| 负责披露 | security@yuledkcs.com |
| PGP 公钥 | `0x12345678` (keys.openpgp.org) |
| 披露时限 | 修复后 90 天 |
| Bug Bounty | https://hackerone.com/yuledkcs |

---

*© 2026 yuleDKCS. 此文档供授权的第三方安全实验室使用。*
