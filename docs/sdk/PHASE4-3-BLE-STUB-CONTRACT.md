# Sprint Contract: 4.3 BLE 桩测试（模拟车辆广播）

> 状态: ✅ 已批准（Generator/Evaluator 视角由 orchestrator 一人担任，contract 即 done 标准）
> 日期: 2026-07-31
> 关联: `docs/sdk/SDK-TASKS.md` Phase 4.3，依赖 2b-A/B/C/D（已完成真实化）

---

## Scope

**What**: 用模拟车辆广播（BLE 桩）驱动 SDK 全链路：扫描发现 → 广告解析 → 连接 → GATT 服务/特征 → 指令构建 → 响应解析。

**In Scope**:
- iOS: `FakeCentral`（实现 `YDKCentralManaging`）+ `FakePeripheral`（实现 `YDKPeripheralManaging`）桩，注入 `YDKBLEManager.init(central:)`
- Android: `FakeBleScanEngine`（实现 `BleScanEngine`）桩，注入 `BleManager(context, scanEngine)`
- 三种协议广播样本: CCC (0xFFF5) / ICCOA (0xFEF5) / ICCE (0xFEFA)
- **存量测试同步**: 2b-E 改造导致的旧断言修复（FFD1→FFF5、旧帧格式→新帧格式）
- 指令构建 wire 级断言（对照规范帧格式）

**Out of Scope**:
- 真机 BLE 联调（模拟器无法真实射频，真机测试单列）
- UWB/NFC（2b-G/H，硬件项）
- 蓝牙配对/加密协商（LE Secure Connections 属系统层）

---

## 测试设施现状（2026-07-31 核对）

| 设施 | iOS | Android |
|:-----|:----|:--------|
| 扫描抽象 | `YDKCentralManaging` + `init(central:)` ✅ | `BleScanEngine` + `BleManager(context, scanEngine)` ✅ |
| 外设抽象 | `YDKPeripheralManaging` ✅ | GATT 经 `BleScanEngine` 回调链路（桩到扫描层） |
| 广告解析 | `YDKAdvertisementParser` ✅ | `BleAdvertiseParser` ✅ |
| 帧格式 | `CCCCommandFrame` (4B) ✅ | `CccFrame` (4B) ✅ |
| 加密 | `CCCSecureChannel` ✅ (16/16 测试) | `CccSecureChannel` ✅ (测试就位) |

## 存量测试破坏面（2b-E 改造导致，必须先修）

| 文件 | 断言 | 需改 |
|:-----|:-----|:-----|
| iOS `YDKBLEManagerTests.swift:43` | `serviceUUID(for: .ccc) == "FFD1"` | → `"FFF5"` |
| iOS `YDKBLEManagerTests.swift:26-27` | `command[0] == 0x01; command.count == 8` | → 4B 帧头 + SE 消息；count 随加密变化 |
| iOS `YDKAdvertisementParserTests.swift:102,117,129` | 广播含 `FFD1` | → `FFF5` |
| Android `BleAdvertiseParserTest.kt:72-74,130` | `hasServiceUuid16(0xFFD1)` | → `0xFFF5` |
| Android `BleProtocolAdapterTest.kt:339` | `CCC_SERVICE == 0000FFD1...` | → `0000FFF5...` |
| Android `ScanResultProcessorTest.kt:44` | CCC 广播 FFD1 | → FFF5 |
| Android `BleProtocolAdapterTest.kt:128` | `command[0] == 0x01` | → 校验 4B 帧头 SE+0x0B |

## Testable Behaviors

### B1. 存量测试同步（前置，必须全绿）
- [ ] B1.1: iOS 全部 `YDKBLEManagerTests` 编译 + 通过 | Owner: W1
- [ ] B1.2: Android 全部 `BleProtocolAdapterTest` 等编译 + 通过 | Owner: W2

### B2. 扫描发现（桩）
- [ ] B2.1: iOS FakeCentral 注入 → `scanVehicles` 发现 CCC 桩广播 → vehicleId 正确 | Owner: W1
- [ ] B2.2: iOS 非本协议广播（无 0xFFF5）→ 过滤不返回 | Owner: W1
- [ ] B2.3: Android FakeBleScanEngine 注入 → `scanVehicles` 发现 CCC 桩 → vehicleId 正确 | Owner: W2
- [ ] B2.4: Android 非本协议广播 → 过滤不返回 | Owner: W2

### B3. 连接 + GATT 流程（桩）
- [ ] B3.1: iOS FakePeripheral 发现 0xFFF5 服务 + SPSM 特征 → 读取 SPSM 成功 | Owner: W1
- [ ] B3.2: iOS 写控制指令 → FakePeripheral 收到帧 = 规范 wire 格式 | Owner: W1
- [ ] B3.3: Android 连接流程经桩驱动 → 指令构建 wire 断言 | Owner: W2

### B4. 指令构建 wire 级（对照规范）
- [ ] B4.1: iOS unlock 指令 = `0x010B + len(BE) + payload`（SE + DK_APDU_RQ）| Owner: W1
- [ ] B4.2: Android unlock 指令同格式 | Owner: W2

## Acceptance Criteria

| ID | Criterion | Pass Condition | Fail Condition | Priority | Owner |
|----|-----------|----------------|----------------|----------|-------|
| AC1 | 存量测试修复 | 所有旧断言更新为规范值, 测试编译通过 | 任何旧 FFD1/旧帧断言残留 | P0 | W1/W2 |
| AC2 | 扫描桩发现 | 注入桩后能发现 3 协议广播样本 | 发现不到或 vehicleId 错误 | P0 | W1/W2 |
| AC3 | 过滤非目标 | 无本协议 service 的广播被过滤 | 误返回 | P0 | W1/W2 |
| AC4 | wire 格式 | 指令帧头 4B + MessageType/ID 正确 | 帧头不符规范 | P0 | W1/W2 |
| AC5 | 本机验证 | iOS swiftc 语法 + 独立 harness 可执行验证; Android 测试就位 CI 执行 | 语法错误 | P1 | W1/W2 |

## Responsibility Matrix

| Criterion | Responsible | Fallback |
|-----------|-------------|----------|
| AC1-AC5 (iOS) | W1 | orchestrator |
| AC1-AC5 (Android) | W2 | orchestrator |

## 拆解（并行派工）

| Worker | 范围 | 文件 |
|:------:|:-----|:-----|
| W1 | iOS 4.3 + 存量修复 | `Tests/YDKBLEManagerTests/` (FakeCentral/FakePeripheral/新测试) + 3 个旧测试文件 |
| W2 | Android 4.3 + 存量修复 | `sdk/src/test/.../ble/` (FakeBleScanEngine/新测试) + 3 个旧测试文件 |

## Negotiation Log

| Round | Party | Action | Notes |
|-------|-------|--------|-------|
| 1 | orchestrator | 提出 contract | 基于 2b-E 完成 + 存量破坏面核对 |
| 2 | orchestrator | 批准 | 测试设施注入点已确认（init(central:)/scanEngine），无阻塞 |
