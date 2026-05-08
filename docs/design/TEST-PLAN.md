# 数字钥匙项目 — 三端测试计划

> **版本**：v1.0  
> **日期**：2026-04-26  
> **作者**：test-agent  
> **状态**：测试计划  
> **关联阶段**：阶段 6 — 三端测试  
> **前置条件**：代码审查 V2 已通过（三端安全修复完成）

---

## 1. 测试范围与目标

### 1.1 总体目标

| 目标 | 说明 |
|------|------|
| **质量保障** | 验证三端（嵌入式 / App / 云端）实现满足架构设计、API 契约和 PRD 需求 |
| **安全验证** | 确保所有安全修复（代码审查 V2 阶段）有效，防护中继攻击、签名伪造、重放攻击 |
| **协议合规** | ICCE 和 CCC 双协议栈行为符合各自规范（CC3.0 / T/CA 110-2020） |
| **集成可靠** | 端到端配对、钥匙下发、解锁流程在正常和异常路径下均正确 |

### 1.2 三端测试覆盖矩阵

| 测试层 | 嵌入式端 | App 端 | 云端 |
|-------|---------|--------|------|
| 单元测试 | ✅ HKDF / SM2 / 防中继状态机 | ✅ ECDSA / AES-GCM / RSSI 估算 | ✅ JWT / SM2 / SM4 / HSM Mock |
| 集成测试 | ✅ 安全通道 / NFC 交易 / BLE 数据 | ✅ App ↔ 嵌入式模拟器 | ✅ REST API 端到端 |
| Widget 测试 | N/A | ✅ 钥匙分享 UI / 配对流程 UI | N/A |
| 安全测试 | ✅ SCP03 / 中继攻击模拟 | ✅ 防中继 RSSI / 签名伪造 | ✅ HSM 隔离 / JWT 重放 |
| **涉及文件路径** | `embedded/src/` `embedded/tests/` | `app/` `app/test/` | `cloud/` `cloud/pkg/` |

### 1.3 测试环境概要

| 环境 | 说明 |
|------|------|
| **嵌入式** | ARM Cortex-M7 模拟器（QEMU）或 NXP S32G2 EVB；工具链 `arm-none-eabi-gcc` |
| **App** | Flutter 3.x；iOS 模拟器 / Android 模拟器 |
| **云端** | Go 1.21+；Docker / Kubernetes（测试环境） |
| **通用** | Mock HSM（开发测试）、真实 HSM（生产隔离验证） |

---

## 2. 嵌入式端测试

> **测试文件**：  
> `embedded/tests/test_anti_relay.c` · `embedded/tests/test_icce_sm.c` · `embedded/tests/test_ccc_sm.c`  
> **构建配置**：`embedded/CMakeLists.txt` — `ENABLE_TESTS=ON`

### 2.1 单元测试

#### 2.1.1 HKDF 密钥派生

| 用例编号 | 描述 | 输入 | 预期输出 | 覆盖代码 |
|---------|------|------|---------|---------|
| EU-HKDF-01 | 标准 HKDF-Expand | IKM=32B, info="session\_key", L=32 | 32B 伪随机输出 | `embedded/src/security/secure_channel.c` |
| EU-HKDF-02 | HKDF 派生会话密钥（两轮 Expand） | 主密钥+盐+info | 2 组 32B 子密钥 | 同上 |
| EU-HKDF-03 | 相同输入产生不同输出（盐随机性） | 两次相同 IKM，不同盐 | 两个不同的输出 | 同上 |
| EU-HKDF-04 | 空 / 非法输入处理 | 空 IKM、超长 info | 返回 `ERR_CRYPTO` | 同上 |

**测试命令**：
```bash
cd embedded && cmake -B build -DENABLE_TESTS=ON && cmake --build build --target test_anti_relay
ctest --test-dir build
```

#### 2.1.2 防中继决策逻辑

| 用例编号 | 描述 | 输入 | 预期输出 | 覆盖代码 |
|---------|------|------|---------|---------|
| EU-AR-01 | 正常距离内解锁请求 | distance=1.5m, counter=递增 | `DECISION_ALLOW` | `embedded/src/security/anti_relay.c` |
| EU-AR-02 | 超出解锁阈值 | distance=3.5m (>2m 阈值) | `DECISION_DENY` | 同上 |
| EU-AR-03 | 计数器回退（重放攻击） | new_counter < stored_counter | `DECISION_DENY` + 告警 | 同上 |
| EU-AR-04 | 挑战超时 | challenge_age > 100ms | `DECISION_DENY_TIMEOUT` | 同上 |
| EU-AR-05 | 多因子交叉验证通过 | UWB 距离✓ + BLE RSSI 一致✓ | `DECISION_ALLOW` | 同上 |
| EU-AR-06 | 多因子交叉验证失败 | UWB 距离✓ + BLE RSSI 异常✓ | `DECISION_DENY` | 同上 |
| EU-AR-07 | 随机数Nonce不重复检测 | 重复的 nonce 值 | `DECISION_DENY_REPLAY` | 同上 |

**测试命令**：
```bash
cd embedded && cmake -B build -DENABLE_TESTS=ON && cmake --build build --target test_anti_relay
./build/test_anti_relay
```

#### 2.1.3 SE050 驱动接口

| 用例编号 | 描述 | 预期行为 |
|---------|------|---------|
| EU-SE-01 | `SE050_Sign()` 正常签名 | 调用 `crypto_engine.c` → SE050 返回 64B SM2 签名 |
| EU-SE-02 | SE050 未连接时调用 | 返回 `ERR_SE_NOT_READY`，不崩溃 |
| EU-SE-03 | 密钥不存在时验签 | 返回 `ERR_KEY_NOT_FOUND` |
| EU-SE-04 | `SE050_GetRandom(32)` | 返回 32B 密码学安全随机数 |
| EU-SE-05 | 并发多线程调用 SE050 | 无竞争条件，返回正确结果 |

### 2.2 集成测试

#### 2.2.1 安全通道建链流程

| 用例编号 | 描述 | 步骤 |
|---------|------|------|
| EI-SC-01 | ICCE 协议安全通道建链 | 1. BLE 连接建立 → 2. 公钥交换 → 3. SM2 ECDH → 4. HKDF 派生会话密钥 → 5. 双向挑战应答 → 6. 通道加密 |
| EI-SC-02 | CCC 协议安全通道建链 | 1. BLE 连接 → 2. ECDH P-256 密钥协商 → 3. HKDF → 4. ECDSA 签名验证 → 5. AES-256-GCM 加密通道 |
| EI-SC-03 | 安全通道建链失败（无效证书） | 证书链验证失败 → 安全通道拒绝建立 → 返回错误码 |
| EI-SC-04 | 安全通道超时 | 挑战应答超 100ms → 通道建立超时 → 复位 |

#### 2.2.2 NFC 交易完整流程

| 用例编号 | 描述 | APDU 序列 |
|---------|------|-----------|
| EI-NFC-01 | CCC NFC 完整解锁流程 | SELECT(AID) → GET CHALLENGE → INTERNAL AUTHENTICATE → UNLOCK → STATUS |
| EI-NFC-02 | ICCE NFC 完整解锁流程 | SELECT(AID) → GET CHALLENGE → ICCE 认证 → 解锁命令 |
| EI-NFC-03 | NFC 交易超时 | 超时不响应 → 交易中止 → 状态回滚 |
| EI-NFC-04 | NFC 刷卡时电量耗尽 | 手机断电 → NFC 被动模式仍可刷卡（独立供电） |

#### 2.2.3 BLE 数据传输

| 用例编号 | 描述 | 验证点 |
|---------|------|--------|
| EI-BLE-01 | GATT MTU 协商 | MTU ≥ 512 bytes |
| EI-BLE-02 | 大数据分包传输 | 钥匙数据 > MTU 时自动分包重组 |
| EI-BLE-03 | BLE 连接断开重连 | 意外断开 → 自动重连 ≤ 3 次 |
| EI-BLE-04 | 多设备同时连接 | 最多 8 台设备同时保持 BLE 连接 |

### 2.3 安全测试

#### 2.3.1 SCP03 认证

| 用例编号 | 描述 | 预期 |
|---------|------|------|
| ES-SCP-01 | 正确的 SCP03 密钥进行认证 | 认证成功，建立安全通道 |
| ES-SCP-02 | 错误的 SCP03 密钥 | 认证失败，返回 `SW69 84`（引用数据无效）|
| ES-SCP-03 | SCP03 会话密钥重放（相同密钥重连） | 每次会话密钥全新生成，旧密钥无效 |

#### 2.3.2 中继攻击模拟

| 用例编号 | 描述 | 工具/方法 | 预期结果 |
|---------|------|---------|---------|
| ES-RA-01 | 固定距离信号放大 | 使用信号放大器延长 BLE 范围 | UWB 测距结果不变（物理距离不变），解锁允许 |
| ES-RA-02 | BLE + UWB 同步中继 | 延迟注入 50ms | 挑战超时 → 解锁拒绝 |
| ES-RA-03 | 纯 BLE 中继（无 UWB） | 禁用手机 UWB，仅 BLE 连接 | 系统拒绝（要求 UWB 距离验证） |
| ES-RA-04 | 位置跳跃攻击 | 模拟手机从 5m 瞬移至 1m | 移动模式检测 → 告警 + 拒绝 |

#### 2.3.3 安全回归测试（对应代码审查 V2 修复）

| 修复编号 | 描述 | 验证方法 |
|---------|------|---------|
| FIX S6 | MockHSMClient 生产环境拒绝使用 | 设置 `ENV=production` → 所有 HSM 调用返回错误 |
| FIX S7 | 嵌入式随机数熵源验证 | 连续生成 1000 次随机数 → 无重复 |
| FIX S8 | 防中继计数器溢出处理 | 计数器达到 MAX → 安全复位 |

---

## 3. App 端测试

> **测试文件**：`app/test/` 目录下 8 个测试文件  
> **框架**：Flutter Test (Dart)  
> **依赖**：`app/pubspec.yaml`

### 3.1 单元测试

#### 3.1.1 密码学运算（ECDSA / AES-GCM）

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| AU-CRY-01 | ECDSA P-256 签名生成 | `app/lib/security/crypto_helper.dart` |
| AU-CRY-02 | ECDSA P-256 签名验证 | 同上 |
| AU-CRY-03 | ECDSA 签名篡改检测 | 修改签名末位 → 验签失败 |
| AU-CRY-04 | AES-256-GCM 加密 / 解密 | 同上 |
| AU-CRY-05 | AES-GCM 密文篡改检测 | 修改密文 → 解密失败或返回错误 |
| AU-CRY-06 | SM2 签名 / 验签（国密） | `app/lib/security/crypto_helper.dart` |
| AU-CRY-07 | SM4-GCM 加密 / 解密 | 同上 |

#### 3.1.2 防中继 RSSI 距离估算

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| AU-AR-01 | 正常距离 RSSI 在范围内 | `app/lib/security/anti_relay.dart` |
| AU-AR-02 | RSSI 异常（BLE 信号强度与 UWB 距离不匹配） | 同上 |
| AU-AR-03 | 快速移动中的 RSSI 抖动处理 | 同上 |
| AU-AR-04 | RSSI 采样平滑算法（滑动窗口） | 同上 |

**防中继距离估算算法验证**（`app/lib/security/anti_relay.dart`）：

```
输入: RSSI₁ (1m参考) = -40 dBm, 实测 RSSI = -65 dBm
计算: distance = 10 ^ ((RSSI₁ - RSSI) / (10 * n))
     n = 环境因子 (室内 2.0 ~ 4.0)
验证: 计算距离 1.5m ~ 2.0m 与 UWB 测距结果一致性
```

| 用例编号 | RSSI₁ | 实测 RSSI | n | 计算距离 | 判定 |
|---------|-------|----------|---|---------|------|
| AU-AR-05 | -40 dBm | -40 dBm | 3.0 | ~1.0m | ✅ 允许解锁 |
| AU-AR-06 | -40 dBm | -70 dBm | 3.0 | ~4.0m | ✅ 允许解锁（接近阈值） |
| AU-AR-07 | -40 dBm | -80 dBm | 3.0 | ~10m | ❌ 拒绝解锁 |

#### 3.1.3 协议状态机

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| AU-PROTO-01 | ICCE 配对状态机转换 | `app/lib/protocol/icce/icce_pairing.dart` |
| AU-PROTO-02 | CCC 配对状态机转换 | `app/lib/protocol/ccc/ccc_pairing.dart` |
| AU-PROTO-03 | 协议协商：自动选择 ICCE vs CCC | `app/lib/protocol/protocol_factory.dart` |
| AU-PROTO-04 | 协议版本不匹配时优雅降级 | 同上 |

**测试命令**：
```bash
cd app && flutter test test/protocol/icce_protocol_test.dart
cd app && flutter test test/protocol/ccc_protocol_test.dart
```

### 3.2 Widget 测试

> 使用 Flutter Widget Tester，验证 UI 组件在各种状态下的行为

#### 3.2.1 钥匙分享 UI

| 用例编号 | 描述 | Widget 文件 |
|---------|------|----------|
| AW-SHARE-01 | 正常展示分享弹窗 | `app/lib/ui/widgets/share_permission.dart` |
| AW-SHARE-02 | 无网络时展示离线提示 | 同上 |
| AW-SHARE-03 | 分享权限选择器多选 | 同上 |
| AW-SHARE-04 | 时间限制设置正确保存 | 同上 |
| AW-SHARE-05 | 超出权限范围时 UI 禁用 | 同上 |

#### 3.2.2 配对流程 UI

| 用例编号 | 描述 | Widget 文件 |
|---------|------|----------|
| AW-PAIR-01 | 配对中显示加载动画 | `app/lib/ui/pages/pairing_page.dart` |
| AW-PAIR-02 | 配对成功展示车辆信息 | 同上 |
| AW-PAIR-03 | 配对失败展示错误原因 | 同上 |
| AW-PAIR-04 | 配对码输入验证 | 同上 |
| AW-PAIR-05 | 重复配对操作防抖 | 同上 |

#### 3.2.3 车辆控制 UI

| 用例编号 | 描述 | Widget 文件 |
|---------|------|----------|
| AW-CTRL-01 | 解锁按钮点击 → 反馈动画 | `app/lib/ui/pages/vehicle_control_page.dart` |
| AW-CTRL-02 | 空调控制滑块交互 | 同上 |
| AW-CTRL-03 | 远程控制超时提示 | 同上 |
| AW-CTRL-04 | 车辆离线状态 UI 灰化 | 同上 |

**测试命令**：
```bash
cd app && flutter test test/services/key_management_test.dart
cd app && flutter test test/services/auth_test.dart
cd app && flutter test test/sdk/digital_key_sdk_test.dart
```

### 3.3 集成测试

#### 3.3.1 App ↔ 嵌入式模拟器（E2E 配对）

> 注：需启动嵌入式模拟器（QEMU 模拟 S32G2）或使用 mock BLE/GATT server

| 用例编号 | 描述 | 数据流 |
|---------|------|--------|
| AI-E2E-01 | 完整 ICCE 配对流程 | App 扫码 → BLE 连接 → 公钥交换 → SM2 签名验证 → 钥匙安装 → 激活 |
| AI-E2E-02 | 完整 CCC 配对流程 | App 发现 BLE → 设备认证 → ECDH → 钥匙配置 → 云端注册 |
| AI-E2E-03 | 钥匙分享 E2E | 车主创建分享 → 云端生成副钥匙 → 推送至车端 → 受邀者领取 |
| AI-E2E-04 | 远程解锁 E2E | App 发送指令 → 云端鉴权 → 下发至车端 → 执行解锁 → 状态反馈 |
| AI-E2E-05 | 配对过程中断恢复 | 配对中断 → 重连后从断点继续 |
| AI-E2E-06 | 云端网络波动时 App 行为 | 网络超时 → 重试机制 → 用户提示 |

**测试命令**：
```bash
cd app && flutter test test/communication/ble_manager_test.dart
cd app && flutter test test/communication/uwb_manager_test.dart
```

### 3.4 App 安全测试

| 用例编号 | 描述 | 验证点 |
|---------|------|--------|
| AS-SEC-01 | Secure Enclave / TEE 密钥存储 | 私钥无法被提取，只能在安全环境中使用 |
| AS-SEC-02 | App 被 root/越狱后行为 | 检测 root/越狱 → 拒绝使用敏感功能 |
| AS-SEC-03 | 签名伪造尝试 | 篡改解锁请求 → 验签失败 → 拒绝 |
| AS-SEC-04 | 内存中敏感数据清理 | 签名运算后 → 敏感内存区域清零 |

---

## 4. 云端测试

> **测试文件**：`cloud/pkg/crypto/*.go` 中的 `_test.go` 文件  
> **框架**：Go testing (`testing.T`)  
> **执行命令**：`cd cloud && go test ./...`

### 4.1 单元测试

#### 4.1.1 JWT 验证

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| CU-JWT-01 | 有效 JWT 验证通过 | `cloud/pkg/utils/jwt.go` |
| CU-JWT-02 | JWT 过期（exp 已过） | 同上 |
| CU-JWT-03 | JWT 签名伪造（修改 payload） | 同上 |
| CU-JWT-04 | JWT 使用错误的 secret | 同上 |
| CU-JWT-05 | JWT claim 缺失（missing kid） | 同上 |
| CU-JWT-06 | Refresh Token 刷新 | 同上 |
| CU-JWT-07 | Token 重放（相同 jti 重复使用） | 同上 |

#### 4.1.2 SM2 签名 / 验签

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| CU-SM2-01 | SM2 签名生成 | `cloud/pkg/crypto/sm2.go` |
| CU-SM2-02 | SM2 签名验证 | 同上 |
| CU-SM2-03 | SM2 签名篡改检测 | 同上 |
| CU-SM2-04 | SM2 公钥导出格式验证（DER 编码） | 同上 |
| CU-SM2-05 | SM2 密钥对从种子恢复确定性 | 同上 |

#### 4.1.3 SM4 加密 / 解密

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| CU-SM4-01 | SM4-GCM 加密 → 解密数据一致 | `cloud/pkg/crypto/sm4.go` |
| CU-SM4-02 | SM4-GCM 密文篡改 → 解密失败 | 同上 |
| CU-SM4-03 | SM4 不同 IV 产生不同密文 | 同上 |
| CU-SM4-04 | SM4-GCM 认证标签验证 | 同上 |

#### 4.1.4 ECDSA / AES（国际算法通道）

| 用例编号 | 描述 | 覆盖文件 |
|---------|------|---------|
| CU-ECDSA-01 | P-256 ECDSA 签名 / 验签 | `cloud/pkg/crypto/ecdsa.go` |
| CU-AES-01 | AES-256-GCM 加密 / 解密 roundtrip | `cloud/pkg/crypto/aes.go` |

#### 4.1.5 HSM Mock 隔离验证

| 用例编号 | 描述 | 覆盖文件 | **安全关键** |
|---------|------|---------|-----------|
| CU-HSM-01 | `MockHSMClient` 生成密钥（开发环境） | `cloud/pkg/crypto/hsm_mock_test.go` |  |
| CU-HSM-02 | `ENV=production` → 拒绝 Mock HSM | 同上 | ⚠️ FIX S6 |
| CU-HSM-03 | `MockHSMClient.Sign()` 生产环境拒绝 | 同上 | ⚠️ FIX S6 |
| CU-HSM-04 | `MockHSMClient.Verify()` 生产环境拒绝 | 同上 | ⚠️ FIX S6（原来恒返 true，已修复）|
| CU-HSM-05 | `MockHSMClient.GenerateRandom()` 生产环境拒绝 | 同上 | ⚠️ FIX S6 |

**测试命令**：
```bash
cd cloud && go test ./pkg/crypto/... -v
cd cloud && ENV=production go test ./pkg/crypto/... -v
```

### 4.2 API 测试

> **测试范围**：所有 REST API 端点（对应 `docs/API-CONTRACT.md`）

#### 4.2.1 用户认证 API

| 用例编号 | API | 方法 | 描述 |
|---------|-----|------|------|
| CA-AUTH-01 | `/auth/send-code` | POST | 发送验证码 |
| CA-AUTH-02 | `/auth/login` | POST | 有效凭据登录 |
| CA-AUTH-03 | `/auth/login` | POST | 无效验证码登录失败 |
| CA-AUTH-04 | `/auth/token/refresh` | POST | Token 刷新 |
| CA-AUTH-05 | `/auth/token/refresh` | POST | 使用过期 refresh token |
| CA-AUTH-06 | `/auth/logout` | POST | 登出（Token 失效） |
| CA-AUTH-07 | `/auth/profile` | GET | 获取用户信息（有效 Token）|
| CA-AUTH-08 | `/auth/profile` | GET | 无 Token 访问 → 401 |
| CA-AUTH-09 | `/auth/profile` | GET | 过期 Token 访问 → 401 |

**curl 示例**：
```bash
# 登录
curl -X POST https://api-test.digitalkey.example.com/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"phone":"+8613912345678","code":"123456","deviceInfo":{}}'

# 获取钥匙列表
curl https://api-test.digitalkey.example.com/api/v1/keys \
  -H "Authorization: Bearer $ACCESS_TOKEN"
```

#### 4.2.2 钥匙管理 API

| 用例编号 | API | 方法 | 描述 |
|---------|-----|------|------|
| CA-KEY-01 | `/keys` | POST | 创建钥匙（有效请求） |
| CA-KEY-02 | `/keys` | POST | 重复创建钥匙 → 409 Conflict |
| CA-KEY-03 | `/keys` | GET | 列表钥匙（分页） |
| CA-KEY-04 | `/keys/{keyId}` | GET | 获取钥匙详情 |
| CA-KEY-05 | `/keys/{keyId}` | GET | 访问不存在的 keyId → 404 |
| CA-KEY-06 | `/keys/{keyId}` | DELETE | 吊销钥匙（车主）|
| CA-KEY-07 | `/keys/{keyId}` | DELETE | 副钥匙用户吊销主钥匙 → 403 |
| CA-KEY-08 | `/keys/{keyId}/suspend` | POST | 暂停钥匙 |
| CA-KEY-09 | `/keys/{keyId}/resume` | POST | 恢复钥匙 |
| CA-KEY-10 | `/keys/{keyId}/shares` | POST | 创建分享 |
| CA-KEY-11 | `/keys/{keyId}/shares/{shareId}` | DELETE | 撤销分享 |

#### 4.2.3 车辆管理 API

| 用例编号 | API | 方法 | 描述 |
|---------|-----|------|------|
| CA-VEH-01 | `/vehicles` | GET | 获取车辆列表 |
| CA-VEH-02 | `/vehicles/bind` | POST | 绑定车辆 |
| CA-VEH-03 | `/vehicles/{vehicleId}/status` | GET | 获取车辆状态（在线）|
| CA-VEH-04 | `/vehicles/{vehicleId}/status` | GET | 获取车辆状态（离线）→ 503 |
| CA-VEH-05 | `/vehicles/{vehicleId}/controls` | POST | 发送远程控制指令 |
| CA-VEH-06 | `/vehicles/{vehicleId}/controls/{requestId}` | GET | 查询控制状态 |
| CA-VEH-07 | `/vehicles/{vehicleId}/ota/check` | GET | 检查 OTA 更新 |
| CA-VEH-08 | `/vehicles/{vehicleId}/ota/confirm` | POST | 确认 OTA 升级 |

#### 4.2.4 分享权限验证

| 用例编号 | 描述 | 预期行为 |
|---------|------|---------|
| CA-PERM-01 | 临时钥匙用户尝试 START_ENGINE | 403 Forbidden |
| CA-PERM-02 | 超期临时钥匙尝试解锁 | 403 Token 已过期 |
| CA-PERM-03 | 地理围栏外尝试解锁 | 403 超出允许范围 |
| CA-PERM-04 | 达到最大使用次数后尝试解锁 | 403 使用次数已用尽 |

### 4.3 安全测试

#### 4.3.1 JWT 安全

| 用例编号 | 描述 | 工具 |
|---------|------|------|
| CS-JWT-01 | JWT 重放攻击 | Burp Suite Repeater 重复发送同一 Token |
| CS-JWT-02 | JWT 签名算法切换（alg: none）| 修改 Token header alg=none |
| CS-JWT-03 | JWT 密钥暴力破解 | 使用 jwt_tool.py |
| CS-JWT-04 | JWT Kid 注入（SQL/NoSQL 注入）| 修改 kid claim |

#### 4.3.2 API 安全

| 用例编号 | 描述 | 工具 |
|---------|------|------|
| CS-API-01 | SQL/NoSQL 注入 | Burp Suite Intruder |
| CS-API-02 | 参数污染 | 同一参数多次提交 |
| CS-API-03 | 限流测试（Rate Limiting） | 超过 100 req/min → 429 |
| CS-API-04 | CORS 配置验证 | 跨域请求处理正确 |
| CS-API-05 | 敏感数据泄露检查 | 响应中无明文密码/私钥 |

#### 4.3.3 HSM 隔离验证

| 用例编号 | 描述 | 验证方式 |
|---------|------|---------|
| CS-HSM-01 | 生产环境必须使用真实 HSM | `ENV=production` 测试套件全部失败或报错 |
| CS-HSM-02 | 签名运算在 HSM 内部完成 | 日志中无明文私钥 |
| CS-HSM-03 | HSM 连接失败时系统降级 | HSM 不可用 → API 返回 503 |

---

## 5. 测试环境要求

### 5.1 嵌入式测试环境

| 组件 | 最低要求 | 推荐 |
|------|---------|------|
| 硬件 | NXP S32G2 EVB 或 QEMU ARM 模拟器 | S32G3 EVB |
| 调试器 | J-Link / Lauterbach Trace32 | 同左 |
| BLE 仿真 | BlueZ hcitool / nRF Connect | nRF52840 DK |
| UWB 仿真 | Qorvo DWM3000 或软件仿真 | 真实 SR250 模组 |
| 构建工具 | ARM GCC 10.x | ARM GCC 13.x |
| 单元测试框架 | Unity Test Framework | Google Test (C++) |

### 5.2 App 测试环境

| 组件 | iOS | Android |
|------|-----|---------|
| 最低版本 | iOS 15.0 | Android 10 (API 29) |
| 设备 | iPhone 13+ 模拟器 | Pixel 6 / 小米 13 模拟器 |
| 测试框架 | flutter_test | flutter_test |
| BLE 测试 | BlueCat iOS | nRF Connect Android |
| 安全测试 | Frida + Objection | 同左 |

### 5.3 云端测试环境

| 组件 | 要求 |
|------|------|
| Kubernetes | 1.28+（测试集群） |
| 数据库 | TiDB Dev / MySQL 8.0 |
| 缓存 | Redis 7.0+ |
| 消息队列 | Kafka 3.5+ |
| HSM | Thales Luna / 云 HSM（生产隔离验证） |
| API 测试工具 | Postman / Insomnia / k6 |
| 安全扫描 | Burp Suite / OWASP ZAP |

---

## 6. 通过标准

### 6.1 嵌入式端通过标准

| 类别 | 通过条件 |
|------|---------|
| 单元测试 | 全部用例通过（EU-*），无 failed，无 skipped |
| 集成测试 | 安全通道建链 ≥ 95% 成功率；NFC 交易 ≥ 98% 成功率 |
| 安全测试 | 中继攻击模拟（ES-RA-*）100% 被正确拒绝 |
| 编译检查 | `arm-none-eabi-gcc` 无 Warning（安全相关） |

### 6.2 App 端通过标准

| 类别 | 通过条件 |
|------|---------|
| 单元测试 | 全部用例通过（AU-*），覆盖率 ≥ 80%（关键路径） |
| Widget 测试 | 关键 UI 流程 100% 覆盖（钥匙分享、配对流程） |
| 集成测试 | E2E 配对 ≥ 90% 成功率 |
| 安全测试 | Secure Enclave 测试通过；root 检测生效 |

### 6.3 云端通过标准

| 类别 | 通过条件 |
|------|---------|
| 单元测试 | 全部用例通过（CU-*），覆盖率 ≥ 75% |
| API 测试 | 所有端点（CA-*）返回符合契约（状态码 + 响应格式） |
| 安全测试 | JWT/CS-* 全部安全测试通过 |
| HSM 隔离 | `ENV=production` 环境下 MockHSMClient 所有方法返回错误 |

### 6.4 三端集成通过标准

| 条件 | 说明 |
|------|------|
| 端到端解锁 | App 靠近 → BLE/UWB 连接 → 身份认证 → 解锁成功 ≤ 1s |
| 钥匙分享 | 分享创建 → 云端处理 → 受邀者领取 → 使用 ≤ 30s |
| 安全验证 | 中继攻击测试 100% 被拦截 |
| 协议合规 | ICCE 和 CCC 双协议均通过协议一致性测试 |

---

## 附录 A：测试命令速查

```bash
# === 嵌入式 ===
cd embedded
cmake -B build -DENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# === App (Flutter) ===
cd app
flutter pub get
flutter test                          # 全部测试
flutter test test/protocol/            # 协议测试
flutter test test/services/            # 服务测试
flutter test --coverage                # 含覆盖率

# === 云端 (Go) ===
cd cloud
go test ./...                          # 全部测试
go test ./pkg/crypto/... -v            # 密码学测试
ENV=production go test ./...           # 生产隔离测试（应全部失败或报错）
go test -race ./...                    # 数据竞争检测
```

## 附录 B：安全关键修复清单

| 修复 ID | 描述 | 代码审查阶段 | 验证测试用例 |
|--------|------|------------|------------|
| FIX S1 | 嵌入式防中继计数器溢出处理 | V2 | EU-AR-03 |
| FIX S2 | SE050 驱动并发安全 | V2 | EU-SE-05 |
| FIX S3 | App 端 Secure Enclave 密钥提取防护 | V2 | AS-SEC-01 |
| FIX S4 | App BLE 数据传输加密 | V2 | EI-BLE-01 |
| FIX S5 | 云端 HSM 签名结果 mock 检测 | V2 | CS-HSM-01 |
| FIX S6 | MockHSMClient 生产环境全面拒绝 | V2 | CU-HSM-02/03/04/05 |
| FIX S7 | 嵌入式随机数熵源验证 | V2 | EU-SE-04 |
| FIX S8 | 防中继随机数 nonce 唯一性检测 | V2 | EU-AR-07 |

---

*测试计划版本：v1.0 | 编制日期：2026-04-26 | 状态：待执行*
