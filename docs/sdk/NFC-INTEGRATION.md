# NFC 备用解锁集成指南 (2b-H)

> 配套裁决文档: `docs/sdk/PHASE2B-GH-P1-CONTRACT.md` 工作流 2 (2b-H)。
> 双端实现: iOS `YDKCoreNFCManager` (CoreNFC) / Android `AndroidNfcManager` (NfcAdapter + ISO-DEP)。
> 双端命令映射一致契约见文末「APDU 命令映射表」; 纯逻辑部分有模拟测 (不依赖硬件)。

---

## 1. 架构与流程

NFC 备用解锁 = 手机无电/无网络时的最后一道解锁通道（对标 Apple CarKey / 数字车钥匙 NFC 方案）:

```
用户贴卡 → 读卡会话 (iOS NFCTagReaderSession / Android Tag dispatch)
        → 读取车辆标签 (tagId + NDEF/记录区 vehicleId)
        → 发送控制指令 (ISO 7816-4 APDU, SW1SW2=0x9000 成功)
        → 车辆执行 unlock/lock/startEngine
```

- **iOS**: `NFCTagReaderSession` (pollingOption `.iso14443`) 检测 ISO 14443 系列标签
  (`NFCMiFareTag` / `NFCISO7816Tag`), `connect` 后经 `sendCommand(apdu:)` / `sendMiFareCommand` 收发指令。
- **Android**: `NfcAdapter.getDefaultAdapter` 判硬件; 标签经 **Reader 模式回调** 或 **前台调度 intent** 到达;
  指令走 `IsoDep.transceive(apdu)`, MiFare Classic 兜底; NDEF 经 `Ndef.get(tag).ndefMessage` 读取。

---

## 2. iOS 配置（宿主 App）

### 2.1 Info.plist

```xml
<key>NFCReaderUsageDescription</key>
<string>用于在手机无网络/无电场景下通过车辆 NFC 标签执行备用解锁</string>
```

### 2.2 Entitlement（真机必需）

在宿主 App 的 entitlements 文件（或 Xcode Signing & Capabilities → Near Field Communication Tag Reading）:

```xml
<key>com.apple.developer.nfc.readersession.formats</key>
<array>
    <string>NDEF</string>
    <string>TAG</string>
</array>
```

> ⚠️ 缺少 entitlement 时 `NFCTagReaderSession(pollingOption:delegate:queue:)` 返回 nil /
> session 立即失效（对应 SDK 错误 `YDKNFCError.sessionCreationFailed`）。
> 模拟器与无 NFC 硬件设备上 `NFCTagReaderSession.readingAvailable == false`
> （SDK 抛 `YDKNFCError.hardwareUnavailable`）。

### 2.3 平台限制

- 支持设备: iPhone 7 及以上（iOS 11+; SDK 最低 iOS 15）。
- NFC 会话必须在前台/活跃状态启动, 后台无法启动新会话; 贴卡时 App 需在前台。
- 系统对 session 有约 60s 超时, 超时回调错误码 `readerSessionInvalidationErrorSessionTimeout`
  （SDK 映射为 `YDKNFCError.timeout`）。

### 2.4 SDK 用法

```swift
let nfc = YDKCoreNFCManager(expectedTagId: "04A1B2C3D4E5F6") // 可选: 绑定标签防错贴

// 读取车辆标签（返回 vehicleId + tagId）
let info = try await nfc.readVehicleTag()

// 发送指令
try await nfc.sendCommandViaNFC(command: .unlock)   // .lock / .startEngine

// 错误处理: YDKNFCError 覆盖 无硬件/无CoreNFC/未检测到/标签不匹配/指令被拒/超时/用户取消
do { try await nfc.sendCommandViaNFC(command: .lock) }
catch let e as YDKNFCError { /* 按 errorDescription 提示用户 */ }
```

---

## 3. Android 配置（宿主 App）

### 3.1 AndroidManifest.xml

```xml
<uses-permission android:name="android.permission.NFC" />   <!-- 普通权限, 无需运行时申请 -->

<uses-feature android:name="android.hardware.nfc" android:required="false" />

<application>
    <!-- 可选: 前台调度场景的 tech-filter（Reader 模式无需此声明） -->
    <activity android:name=".MainActivity">
        <intent-filter>
            <action android:name="android.nfc.action.TECH_DISCOVERED" />
        </intent-filter>
        <meta-data
            android:name="android.nfc.action.TECH_DISCOVERED"
            android:resource="@xml/nfc_tech_filter" />
    </activity>
</application>
```

`res/xml/nfc_tech_filter.xml`（tech-list, 与 `NfcCommandBuilder.TECH_LIST` 一致）:

```xml
<resources xmlns:xliff="http://schemas.android.com/apk/res/android">
    <tech-list>
        <tech>android.nfc.tech.IsoDep</tech>
    </tech-list>
    <tech-list>
        <tech>android.nfc.tech.Ndef</tech>
    </tech-list>
    <tech-list>
        <tech>android.nfc.tech.MifareClassic</tech>
    </tech-list>
    <tech-list>
        <tech>android.nfc.tech.NfcA</tech>
    </tech-list>
</resources>
```

### 3.2 宿主集成（两种标签获取方式, 推荐 Reader 模式）

**方式 A — Reader 模式（推荐, 回调直达 Tag）:**

```kotlin
val nfc = AndroidNfcManager(context)

override fun onResume() {
    super.onResume()
    nfc.enableReaderMode(this)          // 默认 FLAG_READER_NFC_A|B|F|V
}
override fun onPause() {
    super.onPause()
    nfc.disableReaderMode(this)
}

// 读取 + 指令（awaitTag 内部挂起, 贴卡即继续）
lifecycleScope.launch {
    val info = nfc.readVehicleTag()
    nfc.sendCommandViaNfc(NfcCommandType.UNLOCK)
}
```

**方式 B — 前台调度（intent 路由）:**

```kotlin
override fun onResume()  { super.onResume();  nfc.enableForegroundDispatch(this) }
override fun onPause()   { super.onPause();   nfc.disableForegroundDispatch(this) }

override fun onNewIntent(intent: Intent) {
    super.onNewIntent(intent)
    setIntent(intent)
}

// onResume 中把 intent 交给 SDK（有等待方则消费, 否则同步解析返回）
override fun onResume() {
    super.onResume()
    nfc.enableForegroundDispatch(this)
    nfc.onTagDispatched(intent)
}
```

### 3.3 平台限制

- NFC 为普通权限（`android.permission.NFC`）, 安装即授权, 无需运行时申请。
- 无 NFC 硬件: `NfcAdapter.getDefaultAdapter` 返回 null → SDK 抛 `NfcUnavailableException`;
  NFC 关闭 → 抛 `NfcDisabledException`。
- `enableForegroundDispatch` / `enableReaderMode` 必须在 `onResume` 期间有效, `onPause` 必须配对注销,
  否则系统抛 `SecurityException` 或行为异常。
- `IsoDep.transceive` 为阻塞调用, SDK 内部已在 `Dispatchers.IO` 执行。

---

## 4. APDU 命令映射表（双端一致, 有模拟测 + Python 交叉验证）

| 操作 | APDU 字节 (hex) | 说明 |
|:-----|:----------------|:-----|
| 解锁 unlock (0x01) | `80 D2 01 00 00 00` | CLA=0x80 专有类, INS=0xD2 车辆指令, P1=指令码 |
| 上锁 lock (0x02) | `80 D2 02 00 00 00` | 同上 |
| 启动 startEngine (0x03) | `80 D2 03 00 00 00` | 同上 |
| 读车辆记录 | `00 B0 00 00 40` | ISO 7816-4 READ BINARY, 读 64 字节 |
| MiFare 读块 | `30 04` | READ block 4 (NDEF 起始块) |

- 响应: 末两字节 SW1SW2, `90 00` = 成功; 其余为失败（SDK 抛 `NfcCommandException` / `YDKNFCError.commandFailed`）。
- vehicleId 解析: 响应去 SW 后按 UTF-8 解码、去尾零/空白; NDEF 文本记录按 NFC Forum Text RTD 解码;
  均失败时兜底使用 tagId（大写十六进制, 无分隔符）。
- iOS 侧实现: `YDKNFCApdu`; Android 侧实现: `NfcCommandBuilder`。

---

## 5. 模拟测覆盖（不依赖硬件）

| 测试文件 | 覆盖 |
|:---------|:-----|
| `mobile/ios/Tests/YDKBLEManagerTests/YDKNFCManagerTests.swift` | 指令 APDU 映射 / 读 APDU / tagIdHex / SW 校验 / vehicleId 解析 / 接口契约 / 无 CoreNFC 降级错误路径 |
| `mobile/android/.../src/test/kotlin/com/yuledkcs/sdk/ble/NfcManagerTest.kt` | 同上纯逻辑部分（NfcCommandBuilder, JVM 可跑）/ tech-list / 接口契约反射复核 |

> iOS 验证: `swiftc -parse YDKNFCManager.swift`（macOS 宿主无 CoreNFC → 走降级分支）;
> 用 CoreNFC 桩模块 `-I` 可对真实分支做 parse/typecheck。
> Android 验证: 静态审查 NfcAdapter/Tag/IsoDep/Ndef API 正确性 + Python 交叉验证命令映射（禁止 gradle/kotlinc）。

---

## 6. 真机联调清单

- [ ] iOS: Info.plist `NFCReaderUsageDescription` + entitlement `com.apple.developer.nfc.readersession.formats=["NDEF","TAG"]` 已配置
- [ ] iOS: 真机（iPhone 7+）验证 `NFCTagReaderSession.readingAvailable == true`
- [ ] Android: 真机验证 `NfcAdapter` 非 null 且 `isEnabled == true`
- [ ] 确认车辆标签类型: ISO 14443-4 (ISO-DEP) 还是 MiFare Classic（决定 ISO7816 vs MiFare 分支）
- [ ] 贴卡读取: tagId 格式与车辆绑定记录一致; 绑定 `expectedTagId` 后错贴标签被拒
- [ ] 指令链路: unlock/lock/startEngine 三指令均返回 SW=9000; 车辆实际执行动作正确
- [ ] 异常路径: NFC 关闭 / 无标签 / 标签不支持 / 指令被拒 / 超时 / 用户取消 的错误提示
- [ ] 前后台: 前台贴卡成功; 后台/锁屏贴卡行为符合产品预期（iOS 后台不可启动 session）
- [ ] 安全: 当前 APDU 为固定字节（无 challenge）, 生产前需与车厂安全模块约定
      nonce/防重放/会话密钥方案（见 §7）

---

## 7. 安全提示

当前实现为通道打通: 指令 APDU 不含随机 challenge, vehicleId 明文存于标签记录区。
生产发布前需与车厂安全模块对齐:

1. **防重放**: 指令 APDU 增加 nonce/challenge（P2/Data 区携带 4 字节随机数, 车辆侧校验）。
2. **车辆身份**: tagId + vehicleId 双因子绑定（SDK 已支持 `expectedTagId` 校验）。
3. **敏感数据**: 标签记录区若含密钥材料, 需确认标签安全模块访问控制（DESFire EV2+ 等）。
