# Demo 脚本 — yuleDKCS × OEM 数字钥匙 POC 演示

> **文档版本**: 1.0.0  
> **创建日期**: 2026-07-16  
> **演示场景**: 手机 App ↔ 云端 ↔ 车端（ICCE / CCC 双协议）  
> **演示时长**: 约 25–30 分钟  

---

## 1. 演示概览

### 1.1 演示角色

| 角色 | 人员 | 职责 |
|:-----|:-----|:------|
| 解说 | yuleDKCS 架构师 | 讲解流程和技术要点 |
| 操作 | yuleDKCS 工程师 | 手机端操作、观察日志 |
| 见证 | OEM 侧团队 | 观察验证点、提问 |

### 1.2 演示环境

| 组件 | 部署方式 | 备注 |
|:-----|:---------|:-----|
| yuleDKCS Hub + DKCS | Docker Compose（本机/云 VM） | `docker-compose-poc.yml` |
| ICCE Adapter | Docker Compose（Java gRPC Server） | `adapter-icce` |
| CCC Adapter | Docker Compose（Java gRPC Server） | `adapter-ccc` |
| OEM TSP Mock | Docker Compose（可选） | 真实 TSP 端点可用时替换 |
| Mobile Demo App | Android APK / iOS IPA | 安装于测试手机 |
| TCU 模拟器 | Docker Compose（`tcu-simulator`） | 模拟 BLE/NFC/UWB 通信 |

### 1.3 硬件准备

| 物品 | 数量 | 用途 |
|:-----|:----:|:-----|
| Android 测试手机（12+） | 1 台 | ICCE/CCC 钥匙创建与绑定 |
| iOS 测试手机（15+） | 1 台 | CCC 钥匙（Apple Wallet）演示 |
| 笔记本电脑（演示机） | 1 台 | 部署环境 + 观察日志 |
| 大屏/投屏 | 1 台 | 展示操作和日志 |
| 备选手机 | 2 台 | 钥匙分享演示 |

### 1.4 前置条件检查

- [ ] 所有 Docker 服务运行正常（`docker ps` 确认）
- [ ] Adapter 健康检查通过（`curl localhost:8080/actuator/health` → UP）
- [ ] OEM TSP Mock 端点可用（`curl https://mock-tsp/health` → 200）
- [ ] Demo App 安装完成并配置连接端点
- [ ] TCU 模拟器 BLE 广播正常
- [ ] 备选网络连接（手机 4G/5G → yuleDKCS 云端）

---

## 2. 演示流程（6 个场景，约 25 分钟）

### 场景 1：环境健康检查（3 分钟）

**目的：** 证明系统可用，建立信心。

**操作步骤：**

```
1. 打开终端，展示 Docker 容器状态
   $ docker ps
   → 显示 hub / dkcs / adapter-icce / adapter-ccc / tcu-simulator 等容器状态均为 Up

2. 调用健康检查 API
   $ curl http://localhost:8080/v1/health
   → {
       "status": "healthy",
       "version": "1.0.0",
       "adapters": {"icce": "UP", "ccc": "UP"},
       "database": "connected",
       "mqtt": "connected"
     }

3. 展示 Grafana 看板（可选）
   → 打开 Grafana Dashboard，显示系统指标正常
```

**解说词：**
> "各位领导/同事，这是 yuleDKCS 数字钥匙平台的 POC 环境。目前 ICCE 和 CCC 两个适配器均处于正常运行状态，数据库和消息队列也连接正常。系统版本为 1.0.0，已通过专家评审 4.5/5.0。"

### 场景 2：用户注册与车辆发现（3 分钟）

**目的：** 验证 TSP API 对接正确，用户车辆信息同步。

**操作步骤：**

```
1. 打开 Demo App → 用户登录界面
2. 输入测试用户凭据 → 点击登录
3. App 自动调用 getVehicles 接口拉取车辆列表
4. 展示车辆列表界面

   演示 App 显示：
   ┌──────────────────────────┐
   │  我的车辆                  │
   │                          │
   │  ├─ OEM Model X          │
   │  │  VIN: WBA3A5C5X...   │
   │  │  支持: BLE NFC UWB    │
   │  │  [ICCE] [CCC]         │
   │  │                       │
   │  └─ 数字钥匙: 0 把        │
   └──────────────────────────┘
```

**验证点：**
- [ ] `GET /api/v1/vehicles` 返回正确车辆信息（ICCE 路径）
- [ ] `GET /api/v1/users/{uid}/vehicles` 返回正确车辆信息（CCC 路径）
- [ ] App 正确显示车辆品牌/型号/VIN
- [ ] 协议支持标识正确（ICCE/CCC 标签）

**备选：** 若 TSP 不可用 → 切换到 MockAdapter 模式，展示预设车辆数据。

### 场景 3：钥匙创建与绑定（5 分钟）

**目的：** 端到端钥匙生命周期核心流程，展示密钥协商。

**操作步骤：**

```
1. 在车辆详情页，点击 "创建数字钥匙"
2. 选择钥匙类型：Owner Key（车主钥匙）
3. App 请求创建钥匙
   → POST /api/v1/keys (ICCE) 或 POST /api/v1/keys/request (CCC)
4. TSP 返回 keyId + keyData
5. 点击 "绑定到本设备"
6. App 启动 NFC/APDU 握手（或 BLE 密钥交换）
7. 后台执行 ECDH 密钥协商
   [展示日志窗口]
   
   Hub 日志输出示例：
   [INFO] Adapter transaction: POST /api/v1/keys/bind
   [INFO] bindKey success: keyId=icce-xxxx-xxxx, sessionId=session-xxx
   [INFO] ECDH sharedSecret derived successfully
   [INFO] Key bound to device: device-xxx
   
8. 绑定完成，App 显示 "数字钥匙已就绪"
```

**验证点：**
- [ ] `requestKeys` 返回 `keyId` 和 `keyData` 非空
- [ ] `bindKey` 返回 `sharedSecret` 非空且正确派生
- [ ] ECDH/SM2 密钥协商日志无错误
- [ ] App 显示钥匙绑定成功状态
- [ ] 钥匙使用 ICCE SM2 或 CCC ECDSA 签名验证通过

**解说词：**
> "这是数字钥匙的核心流程——钥匙创建和绑定。关键在于底层的密钥协商（ECDH P-256 / SM2 ECDH），通过非对称密码学确保只有持有合法设备钥匙的用户可以绑定。全程密钥材料不离开安全硬件——车端存放在 SE050 CC EAL 6+ 安全芯片中，手机端存放在 Secure Enclave / StrongBox 中，云端只管理钥匙状态元数据，看不到实际密钥。"

### 场景 4：近场解锁（5 分钟）

**目的：** 演示最核心的用户场景——手机解锁车辆。

**操作步骤（ICCEE/CCC BLE 解锁）：**

```
1. 演示人员持已绑定钥匙的手机靠近 TCU 模拟器（或真实车辆）
2. 手机 App 自动检测到车辆 BLE 广播
3. App → BLE GATT 建立连接
4. ICCE: Control Point (0xFEFB) Write → BER-TLV 解锁指令
   CCC: Control Point (0xFFD2) Write → 加密 APDU 数据包
5. BLE 传输 → TCU 收到指令 → 验证签名 → 执行解锁

   [展示日志：车端收到解锁指令]
   [INFO] BLE connected: device=test-phone-01, rssi=-65dBm
   [INFO] ICCE/CCC vehicle control: UNLOCK
   [INFO] Signature verification: PASSED
   [INFO] Challenge-response: PASSED
   [INFO] CAN command: UNLOCK (door_status=UNLOCKED)
```

**操作步骤（CCC NFC 解锁备选）：**

```
1. 将已绑定钥匙的手机贴近 NFC 阅读器（ST25R501 模拟器）
2. NFC APDU 交换 → SCP03 安全通道 → 解锁指令
3. 体验：即贴即开，< 500ms
```

**验证点：**
- [ ] BLE 连接建立成功（RSSI 信号正常）
- [ ] ICCE/CCC 协议层解锁指令发送成功
- [ ] 车端签名验证通过
- [ ] Challenge-Response 超时窗口校验通过
- [ ] CAN 总线解锁指令发出
- [ ] UWB 距离检查（若支持）通过
- [ ] 全链路延迟 < 1s（BLE）/ < 500ms（UWB）
- [ ] App 显示 "车辆已解锁" 状态

### 场景 5：钥匙分享（3 分钟）

**目的：** 展示钥匙分享能力（ICCE/CCC 共享钥匙）。

**操作步骤：**

```
1. 在 App 中选择已绑定的 Owner Key
2. 点击 "分享钥匙"
3. 输入接收方手机号/用户 ID
4. 设置权限：解锁+锁定（仅限）
5. 设置有效期：24 小时
6. 设置使用次数上限：10 次
7. 点击 "发送邀请"
8. 接收方手机收到通知
9. 接收方 App 中创建 Sub Key 完成

   [展示日志]
   [INFO] Share key created: keyId=shared-key-xxx
   [INFO] Permission: [LOCK, UNLOCK], maxUses=10, expires=...
   [INFO] Shared key bound to recipient device: device-yyy
```

**验证点：**
- [ ] 副钥匙权限配置正确（无 START 等更高级权限）
- [ ] 副钥匙有效期设置正确
- [ ] 接收方成功绑定副钥匙
- [ ] 副钥匙使用次数计数器初始化正确

### 场景 6：钥匙吊销（3 分钟）

**目的：** 展示安全吊销能力。

**操作步骤：**

```
1. 使用车主 App，在钥匙管理列表中找到已分享的副钥匙
2. 点击 "吊销钥匙"
3. 确认吊销
4. 接收方 App 实时收到吊销通知 → 钥匙图标变灰
5. 演示使用副钥匙靠近车辆 → 解锁失败

   [展示日志]
   [INFO] Revoke key request: keyId=shared-key-xxx
   [INFO] Key status changed to REVOKED
   [INFO] Unlock attempt with revoked key: REJECTED (status=REVOKED)
```

**验证点：**
- [ ] 吊销 API 调用成功
- [ ] `getKeyStatus` 确认状态已变更
- [ ] 吊销后解锁被拒绝
- [ ] 吊销指令离线生效（下次联网同步）

### 备选场景：Mock 环境演示（3 分钟）

**适用于：** 真实 TSP 端点不可用时

```
1. 启动 MockAdapter
   docker compose -f docker-compose-poc.yml up mock-adapter

2. 验证 MockAdapter 健康状态
   curl http://localhost:8081/actuator/health → UP

3. 重复场景 2–5，所有接口返回预设模拟数据

4. MockAdapter 模拟的边界情况：
   ┌──────────────────────────────────────────────────┐
   │  模拟场景          │  响应行为                     │
   ├──────────────────────────────────────────────────┤
   │  TSP 503 错误      │  前 2 次返回 503，第 3 次成功 │
   │  sharedSecret 为空 │  ResponseValidator 告警      │
   │  吊销同步延迟      │  状态变更异步延迟返回          │
   └──────────────────────────────────────────────────┘
```

---

## 3. 预期效果与验证点汇总

| 场景 | 最低通过条件 | 目标通过条件 |
|:-----|:-----------:|:-----------:|
| 环境健康 | 全部 4 项 | 全部 4 项 |
| 车辆发现 | ICCE/CCC 至少 1 项通过 | 全部通过 |
| 钥匙绑定 | 绑定流程完成，key 状态 ACTIVE | 附带 sharedSecret 验证 |
| 近场解锁 | BLE 解锁成功（ICCE/CCC 任选） | BLE + NFC 双模 |
| 钥匙分享 | 副钥匙创建成功 | 附带权限验证 |
| 钥匙吊销 | 吊销成功 + 解锁被拒 | 离线生效确认 |

---

## 4. 故障应急方案

### 4.1 常见故障及处理

| 故障现象 | 可能原因 | 处理方式 |
|:---------|:---------|:---------|
| Adapter 健康检查 DOWN | TSP 端点不可达 | 切换至 MockAdapter |
| BLE 扫描不到车辆 | 手机蓝牙未开 / TCU 模拟器未启动 | 检查蓝牙权限，重启 tcu-simulator |
| 钥匙绑定失败 | ECDH 密钥协商失败 | 检查设备时间和曲线参数 |
| 解锁无响应 | MQTT 链路中断 | 验证 EMQX 状态，重启 hub |
| 签名验证失败 | 证书链不完整 | 重新导入 CA 证书链 |
| Demo App 闪退 | SDK 版本不匹配 | 检查 App SDK 版本号，确认与云端版本一致 |

### 4.2 紧急备选方案

若全套端到端演示不可用，准备以下备选方案：

**方案 A：云端演示（5 分钟）**
- 通过 `curl` 命令展示所有 API 调用全流程
- 展示 `getVehicles → requestKeys → bindKey → getKeyStatus → revokeKeys` 的完整日志输出

**方案 B：录播演示（提前录制 10 分钟视频）**
- 录制完整的端到端演示视频（连接真实设备）
- 现场播放视频 + 嵌入式讲解

---

## 5. 演示后 Q&A 准备

演示结束后预留 10 分钟 Q&A，建议准备以下问题的回答：

| 问题 | 回答要点 |
|:-----|:---------|
| 集成需要多长时间？ | ICCE/CCC 各约 1–2 周完成接口对接，4 周完成端到端 |
| 是否支持其他手机厂商？ | 架构支持热插拔 Adapter，新增厂商仅需新增 Java 模块 |
| 密钥安全性如何？ | SE050（EAL6+）+ Secure Enclave/StrongBox，密钥永不离安全硬件 |
| 国密合规情况？ | 架构就绪，SM2/SM3/SM4 库集成 P1 待完成 |
| 性能指标？ | 绑定 < 3s，BLE 解锁 < 1s，UWB 解锁 < 500ms（P95） |

---

*本文档应与 [POC 联调指南](poc-guide.md)、[ICCE 对接文档](icce-integration.md)、[CCC 对接文档](ccc-integration.md) 配套使用。*
