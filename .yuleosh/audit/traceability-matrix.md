# Traceability Matrix

> Generated: 2026-08-06T11:18:27.242552
> Version: 0.1.0

## Requirements → Implementation → Tests

### KL-SHALL-01
- Req ID: KL-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持数字钥匙的完整生命周期：创建(Created) → 预配对(Pre-Paired) → 配对完成(Paired) → 激活(Active)...

### KL-SHALL-02
- Req ID: KL-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在钥匙创建时使用非对称密钥对，私钥在手机 SE/TEE 中生成，公钥上传云端

### KL-SHALL-03
- Req ID: KL-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在配对过程中完成双向身份认证：车端验证手机签名，手机验证车端签名

### KL-SHALL-04
- Req ID: KL-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 限制同一车辆绑定的有效数字钥匙数量 ≤ 10 把

### KL-SHALL-05
- Req ID: KL-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在钥匙状态变更（激活/暂停/吊销/过期）时通过 MQTT 推送实时同步至车端

### KL-SHALL-06
- Req ID: KL-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 允许车主暂停、恢复和吊销其名下车辆的任意钥匙

### KL-SHALL-07
- Req ID: KL-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 确保钥匙更新（权限变更/有效期延长/密钥轮换）经密码学签名验证后才生效

### KL-SHALL-08
- Req ID: KL-SHALL-08
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在钥匙创建和配对过程中，通过安全通道传输所有密钥材料，不允许明文密钥暴露于网络

### KL-SHALL-NOT-01
- Req ID: KL-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许越权用户（非钥匙持有者/非车主）执行钥匙创建、更新或吊销操作

### KL-SHALL-NOT-02
- Req ID: KL-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在未经配对确认的情况下激活任何数字钥匙

### PE-SHALL-01
- Req ID: PE-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在手机靠近车辆 ≤ 2m 时自动完成 BLE 连接 + UWB 测距 + 双向认证，总延迟 ≤ 1 秒

### PE-SHALL-02
- Req ID: PE-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在车主离开车辆 ≥ 5m 且超过 30 秒后自动上锁所有车门

### PE-SHALL-03
- Req ID: PE-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在上锁前确认车内无有效数字钥匙（防止钥匙锁在车内）

### PE-SHALL-04
- Req ID: PE-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在解锁指令执行前，完成 UWB 距离验证（距离 < 2m 阈值）和 BLE RSSI 交叉校验

### PE-SHALL-05
- Req ID: PE-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 每次解锁/上锁操作使用新的一次性随机数 (Nonce)，防止重放攻击

### PE-SHALL-06
- Req ID: PE-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在解锁/上锁成功后通过 CAN FD 向 BCM/GW 发送对应指令

### PE-SHALL-07
- Req ID: PE-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在解锁/上锁时提供视觉/声音反馈（车灯闪烁/鸣笛）

### PE-SHALL-08
- Req ID: PE-SHALL-08
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持 BLE Central (Peripheral) 角色，保持与最多 8 台设备的并发 BLE 连接

### PE-SHALL-NOT-01
- Req ID: PE-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在 UWB 测距距离 > 2m 时执行解锁指令

### PE-SHALL-NOT-02
- Req ID: PE-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在无有效经过认证的数字钥匙时执行任何车辆解锁/车门打开操作

### NF-SHALL-01
- Req ID: NF-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持通过 NFC 被动刷卡方式解锁车门（手机电量耗尽/离线时仍可用）

### NF-SHALL-02
- Req ID: NF-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在 NFC 交互中使用 ISO/IEC 7816-4 APDU 协议完成 SELECT → GET CHALLENGE → INTERNAL ...

### NF-SHALL-03
- Req ID: NF-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持 CCC Digital Key AID (`A000000F5A3`) 和 ICCE AID (`A000000F5A3ICCE`) 两...

### NF-SHALL-04
- Req ID: NF-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在 NFC 刷卡解锁时完成芯片级安全认证（签名验证），认证失败则拒绝解锁

### NF-SHALL-05
- Req ID: NF-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ NFC 刷卡解锁响应时间 SHALL ≤ 500ms（从手机触碰 NFC 读卡器到解锁成功反馈）

### NF-SHALL-NOT-01
- Req ID: NF-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在 NFC 交互超时或卡片会话异常中断后，残留任何未提交的解锁状态

### RC-SHALL-01
- Req ID: RC-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 要求所有远程控车指令携带 JWT 签名和密钥签名双重认证

### RC-SHALL-02
- Req ID: RC-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持以下远程控车动作：解锁/上锁、启动/停止发动机、闪灯鸣笛、空调控制、车窗控制

### RC-SHALL-03
- Req ID: RC-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 远程控车指令从 App 发出到车端执行的端到端响应时间 SHALL ≤ 3s

### RC-SHALL-04
- Req ID: RC-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在远程控车指令经过云端时记录完整审计日志（用户ID、钥匙ID、操作类型、时间戳、结果）

### RC-SHALL-05
- Req ID: RC-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 远程控车指令 SHALL 通过 MQTT over TLS 1.3 通道下发至车端

### RC-SHALL-06
- Req ID: RC-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在车端离线时返回明确的"车辆离线"状态给 App 端

### RC-SHALL-07
- Req ID: RC-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持远程控车指令的状态查询（PENDING/EXECUTING/EXECUTED/FAILED/TIMEOUT）

### RC-SHALL-NOT-01
- Req ID: RC-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 执行缺少有效密钥签名的远程控车指令

### RC-SHALL-NOT-02
- Req ID: RC-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许临时钥匙或权限不足的钥匙执行 START_ENGINE 等受限操作

### ES-SHALL-01
- Req ID: ES-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 仅在车内检测到有效经认证的数字钥匙时授权发动机启动

### ES-SHALL-02
- Req ID: ES-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在启动授权前建立 BLE 安全会话并完成双向签名验证

### ES-SHALL-03
- Req ID: ES-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 确认车内测距结果（UWB 检出手机位于驾驶舱内）后才发送启动授权

### ES-SHALL-04
- Req ID: ES-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 发动机启动授权响应时间 SHALL ≤ 500ms

### ES-SHALL-NOT-01
- Req ID: ES-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在没有有效经认证数字钥匙的情况下授权发动机启动

### ES-SHALL-NOT-02
- Req ID: ES-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许已吊销/暂停/过期的钥匙授权发动机启动

### KS-SHALL-01
- Req ID: KS-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持车主创建三种级别钥匙分享：主钥匙(Full Access)、副钥匙(Limited Admin)、临时钥匙(Time/Location R...

### KS-SHALL-02
- Req ID: KS-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持四种分享方式：二维码分享、链接分享、NFC 碰一碰分享、手机号直接推送

### KS-SHALL-03
- Req ID: KS-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持以下分享约束设置：时间窗口（开始/结束时间）、使用次数上限、地理围栏

### KS-SHALL-04
- Req ID: KS-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在车主撤销分享后，被分享方的钥匙在 < 10s 内失效

### KS-SHALL-05
- Req ID: KS-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 分享创建请求 SHALL 在 30 秒内完成云端处理和分享链接/码生成

### KS-SHALL-06
- Req ID: KS-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 完整记录每次钥匙分享的创建、接受、使用和撤销事件

### KS-SHALL-07
- Req ID: KS-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在受邀者首次接受分享钥匙时，要求受邀者注册/登录并通过身份认证

### KS-SHALL-NOT-01
- Req ID: KS-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许非车主用户创建或撤销钥匙分享

### KS-SHALL-NOT-02
- Req ID: KS-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许被分享钥匙超出其权限约束（时间/次数/地理范围）执行操作

### KR-SHALL-01
- Req ID: KR-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持云端即时吊销数字钥匙，云端吊销列表 TTL ≤ 10s

### KR-SHALL-02
- Req ID: KR-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在车端维护本地吊销缓存，车辆下次联网时同步更新

### KR-SHALL-03
- Req ID: KR-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在被吊销钥匙尝试操作时，车端本地缓存能独立判定吊销状态并拒绝

### KR-SHALL-04
- Req ID: KR-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在钥匙吊销后通过推送通知告知钥匙持有者

### KR-SHALL-05
- Req ID: KR-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 吊销操作 SHALL 记录完整的审计日志（操作人、时间、原因、关联钥匙）

### KR-SHALL-NOT-01
- Req ID: KR-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许被吊销的钥匙在吊销操作完成后继续执行任何车辆操作

### RA-SHALL-01
- Req ID: RA-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 使用 UWB 物理层 ToF (Time of Flight) 测距测量手机与车辆的真实距离

### RA-SHALL-02
- Req ID: RA-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 对每次测距使用一次性随机数 (Nonce)，防止测距结果重放

### RA-SHALL-03
- Req ID: RA-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 验证测距结果的签名，确保测距数据未被中间人篡改

### RA-SHALL-04
- Req ID: RA-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在解锁指令的响应时间超出协议规定阈值（~3μs）时拒绝执行

### RA-SHALL-05
- Req ID: RA-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 实现 BLE RSSI + UWB 距离多因子交叉验证：若 BLE RSSI 估算距离与 UWB 测距结果显著不一致，拒绝解锁

### RA-SHALL-06
- Req ID: RA-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 实现防重放计数器，拒绝计数器值小于或等于上次已接收值的消息

### RA-SHALL-07
- Req ID: RA-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在检测到疑似中继攻击时记录安全事件并推送告警至车主 App

### RA-SHALL-NOT-01
- Req ID: RA-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在未完成 UWB 安全测距的情况下仅凭 BLE 连接执行解锁

### RA-SHALL-NOT-02
- Req ID: RA-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许同一 Nonce 值被使用两次（重放攻击）

### KSS-SHALL-01
- Req ID: KSS-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 车端私钥 SHALL 存储于 SE050 安全芯片内，任何软件层无法以明文形式读取私钥

### KSS-SHALL-02
- Req ID: KSS-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 手机端私钥 SHALL 存储于 SE/TEE 安全区域（iOS Keychain / Android KeyStore）内

### KSS-SHALL-03
- Req ID: KSS-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 所有密码学运算（签名/解密/密钥派生）SHALL 在 SE/TEE 安全环境中执行，而非在通用 CPU/内存中

### KSS-SHALL-04
- Req ID: KSS-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ SE050 SHALL 满足 EAL6+（安全芯片认证等级）及以上

### KSS-SHALL-05
- Req ID: KSS-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持多重密钥层级：Root Key → Master Key → Device Key → Session Key，各级密钥通过 HKDF 派...

### KSS-SHALL-06
- Req ID: KSS-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 会话密钥 (Session Key) SHALL 每次连接通过 ECDH (P-256/SM2) 密钥协商生成，用完即销毁

### KSS-SHALL-07
- Req ID: KSS-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持 ICCE 模式下的国密算法 (SM2/SM3/SM4) 和 CCC 模式下的国际算法 (ECDSA/AES-256-GCM)

### KSS-SHALL-08
- Req ID: KSS-SHALL-08
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持安全启动链：Boot ROM → BootLoader(SE050验签) → TFM → Application 逐级校验

### KSS-SHALL-NOT-01
- Req ID: KSS-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 密钥材料 SHALL NOT 以明文形式离开安全环境 (SE/TEE/HSM)

### KSS-SHALL-NOT-02
- Req ID: KSS-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 在生产环境中使用 Mock HSM 或模拟安全元件执行密码学操作

### CM-SHALL-01
- Req ID: CM-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 手机与云端之间所有通信 SHALL 使用 TLS 1.3 加密

### CM-SHALL-02
- Req ID: CM-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 云端与车端之间所有通信 SHALL 使用 MQTT over TLS 1.3 或 gRPC over TLS

### CM-SHALL-03
- Req ID: CM-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 手机与车端 BLE 通信 SHALL 使用 LE Secure Connections (LE SC) 建立安全认证链路

### CM-SHALL-04
- Req ID: CM-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ BLE GATT 连接参数 SHALL 满足：连接间隔 30ms~50ms、MTU ≥ 512 bytes

### CM-SHALL-05
- Req ID: CM-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 内部协议消息 SHALL 使用 BER-TLV 编码（紧凑、标准化）

### CM-SHALL-06
- Req ID: CM-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 所有远程控车接口（REST API）SHALL 使用 Bearer Token (JWT) 鉴权

### CM-SHALL-07
- Req ID: CM-SHALL-07
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ JWT Access Token 有效期 SHALL ≤ 1 小时，Refresh Token SHALL ≤ 7 天

### CM-SHALL-08
- Req ID: CM-SHALL-08
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在云端 REST API 对所有关键操作（钥匙创建/吊销/分享、远程控车）进行细粒度权限校验

### CM-SHALL-NOT-01
- Req ID: CM-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 使用未加密的明文信道传输任何密钥材料、认证令牌或车辆控制指令

### CM-SHALL-NOT-02
- Req ID: CM-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许缺少有效 JWT Token 或 Token 已过期的 API 请求通过

### OT-SHALL-01
- Req ID: OT-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持通过 OTA 方式升级车端固件

### OT-SHALL-02
- Req ID: OT-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ OTA 升级包 SHALL 经过数字签名，车端在安装前 SHALL 验证签名完整性

### OT-SHALL-03
- Req ID: OT-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持 OTA 升级状态追踪（DOWNLOAD_PENDING → DOWNLOADING → VERIFYING → INSTALLING →...

### OT-SHALL-NOT-01
- Req ID: OT-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 安装签名校验失败的 OTA 升级包

### UA-SHALL-01
- Req ID: UA-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持手机号+验证码、第三方 OAuth（微信/Apple ID/Google）等多种登录方式

### UA-SHALL-02
- Req ID: UA-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 基于 OAuth 2.0 / OpenID Connect 协议实现用户认证

### UA-SHALL-03
- Req ID: UA-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在车主证明环节要求用户通过 VIN 码 + 购车证明或车厂 API 验证车辆所有权

### UA-SHALL-04
- Req ID: UA-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 支持多因子认证 (MFA)：短信验证码、生物识别等

### UA-SHALL-05
- Req ID: UA-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 基于 RBAC + ABAC 实现细粒度权限控制

### UA-SHALL-NOT-01
- Req ID: UA-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许未通过车辆所有权验证的用户创建该车辆的主钥匙

### AL-SHALL-01
- Req ID: AL-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 记录所有钥匙生命周期变更（创建、激活、暂停、恢复、吊销、过期）的审计日志

### AL-SHALL-02
- Req ID: AL-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 记录所有车辆控制和车门操作（解锁、上锁、启动、寻车等）的审计日志

### AL-SHALL-03
- Req ID: AL-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 审计日志 SHALL 包含：操作人、关联钥匙、操作类型、时间戳、设备信息、地理位置、操作结果

### AL-SHALL-04
- Req ID: AL-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 审计日志保留期限 SHALL ≥ 3 年

### AL-SHALL-05
- Req ID: AL-SHALL-05
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 记录安全事件（认证失败、中继攻击检测、异常权限使用）到独立的安全事件日志

### AL-SHALL-06
- Req ID: AL-SHALL-06
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 审计日志 SHALL 通过消息队列 (Kafka Topic: `digitalkey.audit`) 异步写入

### AL-SHALL-NOT-01
- Req ID: AL-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL NOT 允许非授权用户删除或篡改审计日志

### DP-SHALL-01
- Req ID: DP-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 同时支持 ICCE、(T/CA 110-2020) 和 CCC (Digital Key 3.0) 两种数字钥匙协议

### DP-SHALL-02
- Req ID: DP-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 车端固件 SHALL 同时包含 ICCE 和 CCC 协议栈，配对时自动协商协议类型

### DP-SHALL-03
- Req ID: DP-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 云端 SHALL 同时管理 ICCE 国密证书和 CCC X.509 证书

### DP-SHALL-04
- Req ID: DP-SHALL-04
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ App SHALL 根据车辆 VIN 自动识别并选用对应协议，用户无感知

### DP-SHALL-NOT-01
- Req ID: DP-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ ICCE 模式 SHALL NOT 使用国际算法（ECDSA/AES）替代国密算法（SM2/SM3/SM4）

### DP-SHALL-NOT-02
- Req ID: DP-SHALL-NOT-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ CCC 模式 SHALL NOT 使用国密算法替代国际算法（ECDSA/P-256/AES-256-GCM）

### OM-SHALL-01
- Req ID: OM-SHALL-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 系统 SHALL 在手机无网络连接时，通过 NFC 刷卡方式仍然可执行车辆解锁

### OM-SHALL-02
- Req ID: OM-SHALL-02
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 预下载的离线钥匙授权数据在有效期内 SHALL 可在无网络环境下使用

### OM-SHALL-03
- Req ID: OM-SHALL-03
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 离线期间的操作记录（解锁/上锁等）在网络恢复后 SHALL 自动同步至云端

### OM-SHALL-NOT-01
- Req ID: OM-SHALL-NOT-01
- SHALL statements: 1
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ 离线钥匙 SHALL NOT 在过期后仍可用于车辆解锁或启动

### 用户设备注册
- Req ID: RS-001
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 多设备配钥
- Req ID: RS-002
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 多设备管理
- Req ID: RS-003
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 性能指标
- Req ID: RS-004
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 可用性需求
- Req ID: RS-005
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 安全性需求
- Req ID: RS-006
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 离线能力
- Req ID: RS-007
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 协议兼容性
- Req ID: RS-008
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 用户体验
- Req ID: RS-009
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### dkcs 入口测试
- Req ID: SWR-001
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### hub 入口测试
- Req ID: SWR-002
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### yuledkcs 统一入口测试
- Req ID: SWR-003
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### C 单元测试框架引入
- Req ID: SWR-001
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### ICCE 协议栈单元测试
- Req ID: SWR-002
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### CCC 协议栈单元测试
- Req ID: SWR-003
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### ICCOA 协议栈单元测试
- Req ID: SWR-004
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### CI 中集成 C 单元测试
- Req ID: SWR-005
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### ICCOACodec.Encode nil pointer dereference（P0 🔴）→ SWR-HUB-001, SWR-HUB-002
- Req ID: KNI-001
- SHALL statements: 2
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ lowercase vendor and protocol strings before registry lookup
  ❌ add nil safety check before accessing RemoteControl field

### strings.ToLower 定义了但未调用（P1 🟡）→ SWR-HUB-002
- Req ID: KNI-002
- SHALL statements: 2
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ actually call strings.ToLower() on the vendor variable
  ❌ normalize vendor/protocol to lowercase at lookup points

### Registry 大小写不敏感匹配缺失（P1 🟡）→ SWR-HUB-001
- Req ID: KNI-003
- SHALL statements: 3
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ normalize keys to lowercase in Registry.Register()
  ❌ normalize keys to lowercase in Registry.Get()
  ❌ NOT break existing matching behavior for lowercase keys

### hub/service 补测试 → SWR-HUB-003
- Req ID: FIX-001
- SHALL statements: 4
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ add unit tests for `backend/cloud/hub/internal/service/` covering all 7 source f...
  ❌ reach at least 80% test coverage for the service package
  ❌ use Go standard testing package
  ❌ NOT modify production code logic

### hub/logger 补测试 → SWR-HUB-003
- Req ID: FIX-002
- SHALL statements: 2
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ add unit tests for `backend/cloud/hub/internal/logger/`
  ❌ reach at least 85% test coverage for the logger package

### 覆盖率门禁 → SWR-HUB-004
- Req ID: FIX-003
- SHALL statements: 4
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ enforce a coverage gate at fail-under=60 in CI
  ❌ fail the CI run when coverage drops below 60%
  ❌ apply the gate to both backend/dkcs and backend/cloud/hub
  ❌ implement the gate via go test -coverprofile plus custom shell check

### 集成测试 CI 化 → SWR-HUB-005
- Req ID: FIX-004
- SHALL statements: 3
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ run integration tests in backend/cloud/hub/tests/integration as a CI step
  ❌ run integration tests separately from unit tests
  ❌ NOT block unit test results on integration test outcome

### SAST 安全扫描 → SWR-HUB-005
- Req ID: FIX-005
- SHALL statements: 3
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ run gosec on all Go code in CI
  ❌ run security scan as a separate CI step
  ❌ report all findings in CI output

### CI 分层 L1/L2/L3 → SWR-HUB-005
- Req ID: FIX-006
- SHALL statements: 6
- Status: ✅ Covered
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:
  ❌ restructure CI into 3 layers
  ❌ include unit tests coverage gate and go vet in L1
  ❌ include integration tests and SAST scan in L2
  ❌ include full build and docker build in L3
  ❌ require L1 for merge
  ❌ run L2 and L3 only after L1 passes

### Android SDK API 单元测试
- Req ID: SWR-001
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### iOS SDK API 单元测试
- Req ID: SWR-002
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 测试编译 CI 集成
- Req ID: SWR-003
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 设备注册 → RS-001
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 多设备按需配钥 → RS-002
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

### 多设备管理 → RS-003
- SHALL statements: 0
- Status: ❌ No implementation
- Scenarios: 0 ⚠️
- Test files: 0 ❌ Not covered by any test
- SHALL details:

## Summary
- Total Requirements: 144
- Requirements with implementation: 121 (84%)
- Requirements with test coverage: 0 (0%)
- Uncovered SHALLs: 141
- Scenarios: 7
- Reviews: 0
- CI Runs: 5