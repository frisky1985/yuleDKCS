# yuleDKCS 端到端 Demo 剧本

> **版本**: v1.0 | **日期**: 2026-07-08
> **作者**: yuleDKCS Team
> **目标受众**: 内部评审 / 潜在客户 / 投资人
> **预计时长**: 12 分钟

---

## 前提条件

- [x] yuleHUB 服务运行中（gRPC :8080，REST API :8080）
- [x] PostgreSQL 15+ 已连接（`docker compose up -d db`）
- [x] Redis 7+ 已连接（`docker compose up -d redis`）
- [x] Kafka 3.6+ 已就绪（`docker compose up -d zookeeper kafka`）
- [x] 5 家适配器已注册（Apple CCC、Samsung CCC、小米 ICCOA、OPPO ICCOA、vivo ICCOA）
- [x] 测试车辆模拟器在线（VH-REMOTE-001、VH-BIND-001、VH-PROV-001）
- [x] 演示用手机号/设备预配置

### 环境检查命令

```bash
# 1. 启动依赖服务
cd ~/yuleDKCS && docker compose up -d db redis

# 2. 启动 yuleHUB
cd ~/yuleDKCS/backend/hub && go run ./cmd/hub/ \
  --log-level info \
  --log-file /tmp/hub-demo.log &
sleep 2

# 3. 验证服务健康
curl -s http://localhost:8080/api/v1/health
# 预期: {"status":"ok"}

# 4. 验证适配器状态
TOKEN=$(curl -s -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"user_id":"admin","password":"admin123"}' | jq -r '.token')
curl -s http://localhost:8080/api/v1/hub/adapters \
  -H "Authorization: Bearer $TOKEN"
# 预期: 5 家适配器全部 healthy
```

---

## Demo 流程

### 场景 1: 数字钥匙全生命周期 (5 分钟)

> **核心展示**: 从车主注册 → 绑定车辆 → 无感解锁 → 启动引擎 → 钥匙分享 → 吊销 → 验证失效
> **展示方式**: Speaker 口头讲解 + curl 演示 + App 模拟器屏幕投屏

#### 1.1 车主注册账号

```bash
# 1. 发送验证码
curl -s -X POST http://localhost:8080/api/v1/auth/send-code \
  -H "Content-Type: application/json" \
  -d '{"phone":"+8613912340001","type":"REGISTER"}'
# 预期: codeId 返回 (实际演示通过后台直接创建)

# 2. 注册并登录
curl -s -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "phone":"+8613912340001",
    "code":"666666",
    "deviceInfo":{
      "deviceId":"dev-iphone-01",
      "deviceType":"iOS",
      "osVersion":"18.0",
      "appVersion":"1.0.0",
      "deviceModel":"iPhone 15 Pro"
    }
  }'
```

**保存 Token**（后续所有请求使用）:

```bash
export TOKEN="<上一步返回的 accessToken>"
export USER_ID="usr_owner_001"
```

#### 1.2 绑定车辆

```bash
# 注册设备（手机上 yuleDKCS App 调用）
curl -s -X POST http://localhost:8080/api/v1/devices \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "platform":"ios",
    "model":"iPhone 15 Pro",
    "os_version":"18.0",
    "app_version":"1.2.3",
    "ble":true,
    "uwb":true,
    "nfc":true,
    "secure_element":true
  }'
# 预期: 返回 device_id

# 绑定车辆（扫码/PIN 码）
curl -s -X POST http://localhost:8080/api/v1/vehicles/bind \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "vin":"LSVXXXXXXXDEMO0001",
    "proofType":"QR_CODE",
    "proofData":"demo-pairing-code-001",
    "nickname":"我的汉 EV"
  }'
# 预期: status=PENDING → 中控确认后变为 ACTIVE
```

**🎤 Speaker 话术**: *"车主扫码绑定车辆，yuleHUB 将绑定指令通过适配器转发到车厂 DK Server。车端 SE050 生成密钥对，公钥返回云端完成配对。"*

#### 1.3 手机生成密钥对 → 公钥上传

```bash
# 配钥到设备（自动协商协议）
curl -s -X POST http://localhost:8080/api/v1/devices/dev-iphone-01/provision \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "vehicle_id":"VH-PROV-001"
  }'
# 预期: key_id + status=ACTIVE

# 验证钥匙已创建
curl -s http://localhost:8080/api/v1/keys \
  -H "Authorization: Bearer $TOKEN" | jq .
# 预期: 返回钥匙列表，1 把主钥匙 ACTIVE
```

#### 1.4 无感解锁（靠近车辆 → BLE → UWB → 解锁）

> *Speaker 拿起手机，走向演示车辆模型*

**模拟 BLE 连接 + UWB 测距 + 解锁**:

```bash
# 查看车辆当前状态
curl -s http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/status \
  -H "Authorization: Bearer $TOKEN" | jq .
# 预期: doors=全部 LOCKED

# BLE 连接 → UWB 测距 → PKE 解锁（模拟 App 调用）
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-ccc-apple-001",
    "params":{"method":"uwb_pke","distance_mm":800},
    "source":3
  }'
# 预期: result_code=0, 车端门锁已开

# 验证解锁结果
curl -s http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/status \
  -H "Authorization: Bearer $TOKEN" | jq '.status.doors'
# 预期: driver=UNLOCKED
```

**🎤 Speaker 话术**: *"手机靠近车辆，BLE 广播发现 → 建立安全通道 → UWB 精确测距（< 2米）→ 自动解锁。全程 < 300ms，无需掏出手机。"*

#### 1.5 启动引擎

```bash
# 车内 BLE 安全会话 → 启动授权
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"engine_on",
    "key_id":"key-ccc-apple-001",
    "params":{"method":"ble_inside"},
    "source":2
  }'
# 预期: result_code=0, engine=ON

# 停止引擎
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"engine_off",
    "key_id":"key-ccc-apple-001",
    "params":{},
    "source":2
  }'
```

#### 1.6 钥匙分享（车主 → 家人）

```bash
# 车主创建分享（指定家人手机号，仅限解锁+启动，24小时有效）
curl -s -X POST http://localhost:8080/api/v1/shares \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "key_id":"key-ccc-apple-001",
    "to_user_id":"usr_family_001",
    "access_level":{
      "lock":true,
      "unlock":true,
      "engine":true,
      "trunk":false,
      "window":false,
      "climate":false,
      "find":true,
      "seat":false
    },
    "valid_from":1715000000,
    "valid_until":1715086400,
    "max_uses":50,
    "trace_id":"share-demo-001"
  }'
# 预期: share_id + share_code（如 "382914"）

# 家人接受分享（使用分享码）
curl -s -X POST http://localhost:8080/api/v1/shares/accept \
  -H "Authorization: Bearer $FAMILY_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "share_code":"382914",
    "device_id":"dev-xiaomi-01",
    "vendor":"XIAOMI",
    "device_pubkey":"<base64-encoded-public-key>"
  }'
# 预期: 返回新的 key_id, status=ACTIVE

# 家人使用分享钥匙解锁车辆
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $FAMILY_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-shared-001",
    "params":{},
    "source":3
  }'
# 预期: result_code=0, 解锁成功 ✅
```

**🎤 Speaker 话术**: *"钥匙分享是 yuleDKCS 的核心卖点。车主可以精确控制权限（谁、什么时候、能做什么）、设定期限和次数。被分享人拿到的是独立钥匙，不是车主钥匙的拷贝。"*

#### 1.7 钥匙吊销

```bash
# 车主吊销分享钥匙
curl -s -X DELETE http://localhost:8080/api/v1/keys/key-shared-001 \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"reason":"家人不再需要使用","notifyUser":true}'
# 预期: status=REVOKED

# 验证：被吊销钥匙无法解锁
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $FAMILY_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-shared-001",
    "params":{},
    "source":3
  }'
# 预期: 返回错误码 GRPC_PERMISSION_DENIED, 解锁失败 ❌
```

**🎤 Speaker 话术**: *"吊销立即生效。即使用人拿着钥匙站在车旁边，也无法解锁。车端云端同步，无缝安全。"*

---

### 场景 2: 三协议切换 (3 分钟)

> **核心展示**: 同一辆车，三种手机品牌，三种协议协议都能解锁
> **展示方式**: Speaker 依次拿起三部手机演示

| 手机 | 协议 | 适配器 |
|:-----|:-----|:-------|
| iPhone 15 Pro | CCC DK 3.0 | Apple CCC Adapter |
| 小米 14 Pro | ICCOA DK 4.0 | Xiaomi ICCOA Adapter |
| OPPO Find X7 | ICCOA DK 4.0 | OPPO ICCOA Adapter |

```bash
# ── iPhone (CCC 协议) ──
echo "=== iPhone via CCC Protocol ==="
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-ccc-apple-001",
    "params":{"protocol":"CCC_DK3"},
    "source":3
  }' | jq '.result_code'
# 预期: 0 (解锁成功)

# ── 小米 (ICCOA 协议) ──
echo "=== Xiaomi via ICCOA Protocol ==="
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $XIAOMI_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-iccoa-xiaomi-001",
    "params":{"protocol":"ICCOA_DK40"},
    "source":3
  }' | jq '.result_code'
# 预期: 0

# ── OPPO (ICCOA 协议) ──
echo "=== OPPO via ICCOA Protocol ==="
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $OPPO_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-iccoa-oppo-001",
    "params":{"protocol":"ICCOA_DK40"},
    "source":2
  }' | jq '.result_code'
# 预期: 0
```

**🎤 Speaker 话术**: *"yuleDKCS 的协议桥接层 (Protocol Bridge) 自动将各厂商的协议统一为内部模型。对上层应用来说，不管手机用什么协议，都是一样的钥匙模型。"*

**查看适配器状态**:

```bash
curl -s http://localhost:8080/api/v1/hub/adapters \
  -H "Authorization: Bearer $TOKEN" | jq '.adapters[] | {vendor, protocol, healthy}'
```

---

### 场景 3: yulePIN 安全防护 (2 分钟)

> **核心展示**: yulePIN 实时检测安全威胁、拦截攻击、可视化告警
> **展示方式**: 终端实时输出 + 仪表盘截图展示

#### 3.1 防中继攻击

```bash
# 模拟中继攻击（伪造近距离信号）
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-ccc-apple-001",
    "params":{
      "method":"relay_attack_sim",
      "distance_mm":5000,
      "uwb_confidence":0.15
    },
    "source":3,
    "trace_id":"security-demo-relay-001"
  }'
# 预期: yulePIN 检测到距离异常 + confidence 过低 → 拒绝解锁

# 查看 yulePIN 安全日志
curl -s http://localhost:8080/api/v1/hub/health \
  -H "Authorization: Bearer $TOKEN" | jq .
```

**yulePIN 检测逻辑**（后端实时输出）:

```
[pin] ⚠️ 安全事件: RELAY_ATTACK_DETECTED
[pin]   ├─ vehicle: VH-REMOTE-001
[pin]   ├─ key_id: key-ccc-apple-001
[pin]   ├─ distance: 5000mm (阈值: 2000mm)
[pin]   ├─ uwb_confidence: 0.15 (阈值: 0.70)
[pin]   ├─ action_taken: BLOCKED
[pin]   └─ alert_level: HIGH
```

#### 3.2 伪造签名攻击

```bash
# 模拟伪造签名请求
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"unlock",
    "key_id":"key-ccc-apple-001",
    "params":{
      "method":"forged_signature_sim",
      "fake_signature":"AAAA..."
    },
    "source":3,
    "trace_id":"security-demo-forgery-001"
  }'
# 预期: yulePIN 签名验证失败 → 拒绝
```

**yulePIN 检测输出**:

```
[pin] ⚠️ 安全事件: FORGED_SIGNATURE
[pin]   ├─ key_id: key-ccc-apple-001
[pin]   ├─ verification: FAILED
[pin]   ├─ action_taken: BLOCKED + NOTIFIED
[pin]   └─ alert_level: CRITICAL
```

#### 3.3 安全事件可视化

```bash
# 全链路追踪
curl -s http://localhost:8080/api/v1/hub/health \
  -H "Authorization: Bearer $TOKEN" | jq '.adapters[] | select(.healthy == false)'
```

**🎤 Speaker 话术**: *"yulePIN 是嵌入在 yuleHUB 中的安全引擎。它实时分析每一次交互的 UWB 测距数据、签名有效性、行为模式。中继攻击、伪造签名、重放攻击，在毫秒级被识别和拦截。"*

---

### 场景 4: OMS 运维监控 (2 分钟)

> **核心展示**: 钥匙生命周期可视化 + 全链路追踪 + 服务健康
> **展示方式**: 仪表盘截图 + curl 实时查询

```bash
# ── OMS: 所有钥匙一览 ──
curl -s http://localhost:8080/api/v1/keys \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" | jq '.keys[] | {key_id, key_type, status, vehicle_id}'

# ── OMS: 服务健康状态 ──
curl -s http://localhost:8080/api/v1/hub/health \
  -H "Authorization: Bearer $TOKEN"
# 预期: healthy=true, 所有适配器 green

# ── yulePIN 全链路追踪（trace_id 查询）──
curl -s -X POST http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/command \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "action":"find",
    "key_id":"key-ccc-apple-001",
    "params":{},
    "source":4,
    "trace_id":"trace-demo-001"
  }'
# OMS 后台展示:
# trace-demo-001 → Hub receive → Adapter route → OEM forward → Vehicle execute → Hub respond
# 完整链路: 12ms total

# ── OMS: 生命周期状态概览 ──
echo "=== 钥匙生命周期统计 ==="
echo "  车主钥匙: key-ccc-apple-001 (ACTIVE)"
echo "  分享钥匙: key-shared-001 (REVOKED) <- 刚刚吊销"
echo "  小米钥匙: key-iccoa-xiaomi-001 (ACTIVE)"
echo "  OPPO钥匙: key-iccoa-oppo-001 (ACTIVE)"
```

**🎤 Speaker 话术**: *"OMS 让运维人员一目了然——哪把钥匙在用、哪把已吊销、哪个适配器健康状态如何。yulePIN 的全链路追踪从 App 请求到车端执行再到响应返回，每个环节的耗时都可视化。"*

---

## 整场 Demo 时间线

| 时间 | 内容 | 展示方式 |
|:----|:-----|:---------|
| 0:00-0:30 | 开场: 问题背景 + yuleDKCS 架构概览 | Slide |
| 0:30-2:00 | 环境检查 + 服务健康 | 终端 |
| 2:00-7:00 | **场景1**: 全生命周期（绑车→解锁→启动→分享→吊销） | 终端 + 手机屏投 |
| 7:00-10:00 | **场景2**: 三协议切换（iPhone→小米→OPPO） | 三部手机 + 终端 |
| 10:00-12:00 | **场景3**: yulePIN 安全防护 + **场景4**: OMS 监控 | 终端 + 仪表盘 |
| 12:00-15:00 | Q&A | 互动 |

---

## 故障排查

### 常见错误及恢复

| 症状 | 原因 | 恢复步骤 |
|:-----|:-----|:---------|
| `health` 返回 502 | HUB 服务未启动 | `cd ~/yuleDKCS/backend/hub && go run ./cmd/hub/` |
| 登录返回 401 | 数据库未初始化 | `docker compose restart db && sleep 3` |
| 适配器 unhealthy | gRPC 连接断开 | 查看 HUB 日志 `tail -f /tmp/hub-demo.log` |
| 指令超时 | 车辆模拟器离线 | `curl -s http://localhost:8080/api/v1/vehicles/VH-REMOTE-001/status` |
| Token 过期 | 3600s 超时 | 重新登录获取新 Token |

### 快速恢复脚本

```bash
# 一键重置 Demo 环境
function demo_reset() {
  echo ">>> 重置 Demo 环境..."
  docker compose restart db redis
  sleep 3
  pkill -f "hub" 2>/dev/null
  sleep 1
  cd ~/yuleDKCS/backend/hub && go run ./cmd/hub/ &
  sleep 2
  curl -s http://localhost:8080/api/v1/health
  echo "<<< 重置完成"
}
```
