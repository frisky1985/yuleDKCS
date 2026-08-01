# yuleDKCS SDK 认证测试清单与厂商适配矩阵（量产准备）

> **文档版本**: v1.0 | **日期**: 2026-08-01
> **视角**: **移动端 SDK 侧**（iOS / Android）认证准备 — 与 `docs/CERTIFICATION-CHECKLIST.md`（车端嵌入式侧）互补
> **范围**: CCC (CCC-TS-101 v4.0.0) / ICCOA (ICCOA/T 002-2024 DK 3.0 + DK 4.0) / ICCE (T/CA 110-2020)
> **功能面**: BLE 指令 / 安全通道 / NFC / UWB / 后台运行 / 分享 / 远程控车
> **状态口径**: 代码就位 = SDK 代码+模拟/桩测试通过（本环境无真机）；真机待验 = 需真机+物理车联调（单列）；规范引用 = 知识库文档裁决依据

---

## 1. 审计结论（现有认证文档 vs SDK 功能面）

### 1.1 文档覆盖矩阵

| 功能面 | CCC | ICCOA | ICCE | 现状依据文档 |
|:------|:---:|:-----:|:---:|:------------|
| BLE 指令帧 | ✅ | ✅ | ✅ | `ccc-ts101-ble-secure-channel.md`（2b-E）/ `iccoa-icce-ble-command-frames.md`（2b-F）|
| 安全通道 | ✅ | ⚠️ 部分 | ✅ | CCC: v4.0.0 SCP03 风格（§18.4/§19）；ICCOA: 链路层 LE SC（无应用层加密）；ICCE: SM4-CBC + HMAC |
| NFC 备用解锁 | ✅ | ✅ | ✅ | `../sdk/NFC-INTEGRATION.md`（2b-H, 代码就位, 真机待验）|
| UWB 测距 | ✅ | ✅ | ✅ | `../sdk/PHASE2G-UWB-PLATFORM.md`（2b-G, 代码就位, 真机待验）|
| 后台运行 | ✅ | ✅ | ✅ | `ble-background-runtime.md` + `../sdk/BLE-BACKGROUND-INTEGRATION.md`（2b-I, 真机验证单列）|
| 分享 | ✅ | ⚠️ 部分 | ⚠️ 部分 | CCC: Relay/Mailbox（`relay-server-spec.md` + `PICS_PIXIT_RELAY.md`）；ICCOA/ICCE: S2S（`iccoa-spec.md` §7.6 + `iccoa-compliance-audit.md`）|
| 远程控车 | ✅ | ✅ | ✅ | HubClient（`../sdk/SDK-ARCHITECTURE.md` §3）+ 4.2 E2E |

### 1.2 缺口清单（本次补齐）

| # | 缺口 | 处置 |
|:-:|:-----|:-----|
| G-1 | **无 SDK 侧认证测试清单** — 现有 checklist（`docs/CERTIFICATION-CHECKLIST.md`）只覆盖车端嵌入式（SWR-EMB-*），未覆盖移动端 SDK 认证测试项 | 本文档 §2 |
| G-2 | **无厂商适配矩阵**（协议 × 功能面 × 状态） | 本文档 §3 |
| G-3 | `pics-ccc.md` 缺 v4.0.0 BLE 安全通道（2b-E 按 v4.0.0 实现：HKDF-SHA256 SystemKeys、SCP03 风格 AES-128+CMAC、4 字节帧头、L2CAP SPSM、0xFFF5）；PICS 仍写 R3.0 GATT 0xFFD1 | 修补 pics-ccc.md |
| G-4 | `pics-iccoa.md` 帧格式未标注端序（DK 3.0 SEQ/LEN 小端、checksum XOR 不含 SOP — 2b-F 裁决）；安全模型未澄清（应用层无加密，依赖 LE SC 链路层）；S2S 分享状态与 `iccoa-compliance-audit.md` 不一致（生产路径仍 stub） | 修补 pics-iccoa.md |
| G-5 | `pics-icce.md` 未定义 control_command_t 帧（38 B：cmd+target+user_id+HMAC-SHA256）；待确认项（hmac 覆盖范围、SM4 IV 协商、GATT UUID）未标注 | 修补 pics-icce.md |
| G-6 | 三份 PICS 均为车端视角，缺 SDK 移动端功能面声明（后台 BLE、NFC 手机侧、UWB 手机侧、分享流程移动端） | 各 PICS 增补 §"SDK 移动端补充" |

### 1.3 已知风险（真机/规范原文待确认，2b 遗留）

| # | 项 | 影响面 | 处置 |
|:-:|:---|:------|:-----|
| R-1 | UWB 真机 token 交换 / 会话参数（iOS NearbyInteraction、Android android.uwb API 34+） | CCC/ICCOA/ICCE UWB 认证测试 | 送测前真机联调，见 `../sdk/PHASE2G-UWB-PLATFORM.md` |
| R-2 | NFC entitlement / tech-list 真机联调 | CCC/ICCE NFC 认证测试 | 送测前真机验证，见 `../sdk/NFC-INTEGRATION.md` |
| R-3 | ICCE hmac 覆盖范围（当前: 命令体前 6 字节） | ICCE 指令一致性 | 待真机对照规范原文 |
| R-4 | ICCE SM4 IV 协商（当前: 未协商全零, 仅调试） | ICCE 会话加密一致性 | 待真机/规范确认 |
| R-5 | ICCOA S2S 生产路径未接入（客户端代码就位, `hub/main.go` 仍 stub） | ICCOA 分享认证 | 按 `iccoa-compliance-audit.md` P0 修复后复测 |
| R-6 | CCC 分享物理机 E2E（双手机 ↔ Hub ↔ Relay） | CCC 分享认证 | 4.4 单列, 送测前完成 |

---

## 2. 认证测试项清单（每协议 × 功能面）

> 测试项 ID 编码: `SDK-{协议}-{功能面}-{序号}`；每项标注状态（✅ 代码就位 / ⏳ 真机待验 / 📚 规范引用）。

### 2.1 CCC（CCC-TS-101 v4.0.0）

**参考文档**: [ccc-ts101-ble-secure-channel.md](./ccc-ts101-ble-secure-channel.md)（2b-E 唯一依据）· [relay-server-spec.md](./relay-server-spec.md)（分享）· [ble-background-runtime.md](./ble-background-runtime.md)（后台）· [PICS_PIXIT_RELAY.md](../compliance/PICS_PIXIT_RELAY.md)

| ID | 测试项 | 断言/验收 | 参考章节 | SDK 落点 | 状态 |
|:---|:-------|:---------|:--------|:--------|:----:|
| SDK-CCC-BLE-01 | 系统密钥派生 | HKDF-SHA256, Info="SystemKeys", 输出 Kenc/Kmac/Krmac/LTSS（flag true 时 +Kble_intro/Kble_oob_master） | §18.4.9, PDF p.429 | `CCCSecureChannel.swift/kt` | ✅ 代码就位（iOS 16/16 断言） |
| SDK-CCC-BLE-02 | 命令加密 | AES-128-CBC(ICV=counter block) + CMAC-AES-128 8B, MAC chaining（GPC_SPE_014 §6.2.6） | §18.4.12, PDF p.429-430 | `CCCCommandFrame.swift` / `CccFrame.kt` | ✅ 代码就位 |
| SDK-CCC-BLE-03 | 响应加密与 R-MAC 验证 | Krmac 验 R-MAC（GPC_SPE_014 §6.2.7） | §18.4.13, PDF p.430 | `CCCAuthResponseVerifier` | ✅ 代码就位 |
| SDK-CCC-BLE-04 | DK 消息帧头 | 4 字节: MsgHeader(1)+PayloadHeader(1)+Length BE(2)（表 19-19） | §19.3, PDF p.449 | 帧编解码 | ✅ 代码就位 |
| SDK-CCC-BLE-05 | APDU 封装 | DK_APDU_RQ=0x0B / DK_APDU_RS=0x0C; 安全消息 class byte=0x84 | §19.3.2, PDF p.459-460 | 控制指令走 SE 消息 (Type=0x01) | ✅ 代码就位 |
| SDK-CCC-BLE-06 | Wire 级测试向量 | 规范 §19.3 SELECT APDU 示例逐字节比对 | §19.3.1, PDF p.451 | iOS 9/9 + 加密 16/16 | ✅ 代码就位 |
| SDK-CCC-BLE-07 | 后台连接恢复 | RestoreIdentifier + willRestoreState; NotifyOnConnection/Disconnection 唤醒 | [ble-background-runtime.md](./ble-background-runtime.md) AD-1..AD-4 | `YDKBLEManager.swift` | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-CCC-NFC-01 | NFC OOB 配对数据 | `ccc_nfc_oob_data_t`（52 B: BLE MAC + UWB session/channel/preamble + capability） | `pixit-common.md` §3.2 | `YDKCoreNFCManager.swift` / `AndroidNfcManager.kt` | ✅ 代码就位 / ⏳ 真机待验（entitlement/tech-list） |
| SDK-CCC-NFC-02 | NFC 备用解锁（手机贴近） | ISO 14443-4 APDU 交换, 0-4 cm | `../sdk/NFC-INTEGRATION.md` | NFC 管理器 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-CCC-UWB-01 | UWB 会话参数协商 | 通过 NFC OOB / BLE UWBConfig 获取 session id/channel/preamble code | `../sdk/PHASE2G-UWB-PLATFORM.md` | `YDKNIUWBManager.swift` / `AndroidUwbManager.kt` | ✅ 代码就位 / ⏳ 真机待验（token 交换） |
| SDK-CCC-SHARE-01 | Mailbox 六 API | Create/Update/Delete/ReadDisplayInfo/ReadSecureContent/Relinquish | `relay-server-spec.md` §2-3 + [PICS_PIXIT_RELAY.md](../compliance/PICS_PIXIT_RELAY.md) | `MailboxClient` (gRPC) | ✅ 代码就位（E2E-11 6 RPC 全过） |
| SDK-CCC-SHARE-02 | Secret fragment 安全 | Secret 只在 URL `#` 后, 不进日志/请求体 | `relay-server-spec.md` §1/§4 | `ShareFlow` URL 解析 | ✅ 代码就位 |
| SDK-CCC-SHARE-03 | 分享全链路 E2E | 双手机 ↔ Hub ↔ Relay（发送→接受→Import→删除） | `../sdk/SDK-ARCHITECTURE.md` §3 分享流程 | iOS 7/7 + Android 16 wire 用例 | ✅ 分享链路就位 / ⏳ 物理机 E2E 单列 |
| SDK-CCC-REMOTE-01 | 远程控车（经 Hub） | remoteLock/Unlock/Start → SendCommand | `../sdk/SDK-ARCHITECTURE.md` §3.1 | `HubClient` | ✅ 代码就位（4.2 E2E 21 断言） |

### 2.2 ICCOA（ICCOA/T 002-2024 — 数字车钥匙3.0 / DK 4.0 扩展）

**参考文档**: [iccoa-icce-ble-command-frames.md](./iccoa-icce-ble-command-frames.md)（2b-F 唯一依据）· [iccoa-spec.md](./iccoa-spec.md)（规范知识库）· [iccoa-compliance-audit.md](./iccoa-compliance-audit.md)（S2S 审计）

| ID | 测试项 | 断言/验收 | 参考章节 | SDK 落点 | 状态 |
|:---|:-------|:---------|:--------|:--------|:----:|
| SDK-ICCOA-BLE-01 | DK 3.0 帧编解码 | SOP=0xAA / CMD / SEQ **小端** / LEN **小端** / PAYLOAD / XOR checksum（**不含 SOP**, 覆盖 CMD+SEQ+LEN+PAYLOAD）/ EOP=0x55 | §1, dk30.c:120-132 | `IcocaFrame.swift` / `IcocaFrame.kt` | ✅ 代码就位（iOS 42/42 断言） |
| SDK-ICCOA-BLE-02 | 防重放 | seq_num 单调递增（0xFFFF→0 回绕合法） | §1, dk30.c:141-151 | 帧校验层 | ✅ 代码就位 |
| SDK-ICCOA-BLE-03 | CTRL 指令负载 | payload=[cmd(1)][param(1)]; 响应=[result(1)][0x00] | §3, dk30.c:88-104 | 控制通道 | ✅ 代码就位 |
| SDK-ICCOA-BLE-04 | 枚举映射（防锁/解颠倒） | 通用 UNLOCK→wire 0x02 / LOCK→0x01（与 CCC 风格相反） | §3 通用枚举映射 | 适配器映射层 | ✅ 代码就位 |
| SDK-ICCOA-SEC-01 | 链路层加密（LE SC） | 应用层**无**加密; 配对/绑定后须链路加密（iOS CBPeripheral 加密属性 / Android GATT 加密） | §4 裁决 AD-1/AD-2 | BLE 连接层 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-ICCOA-SEC-02 | 密钥生命周期 | BIND/UNBIND（0x01-0x04）; 8 权限位逐位校验 | `pics-iccoa.md` §4 | HubClient + KeyManager | ✅ 代码就位（4.2 E2E） |
| SDK-ICCOA-NFC-01 | NFC 备用解锁 | 手机贴近 NFC 解锁（offline 场景） | `../sdk/NFC-INTEGRATION.md` | NFC 管理器 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-ICCOA-UWB-01 | DK 4.0 UWB 测距 | IEEE 802.15.4z TWR, ch5/ch9, STS | `pics-iccoa.md` §3.6 | UWB 管理器 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-ICCOA-SHARE-01 | S2S 分享（车账号体系 16 步） | genSession → getMidCsr/putMidCert → share/sign → trackKey → notifyKeyEvent | `iccoa-spec.md` §6.3 + §7.2 | `s2s/iccoa_client.go`（12 API） | ✅ 客户端代码就位（34 mock 测试）/ 🔴 生产路径未接入（`hub/main.go` 仍 stub, 见审计 P0） |
| SDK-ICCOA-SHARE-02 | S2S 分享 E2E（mock） | e2e_12 ICCOA 5/5 全 PASS | `../compliance/PICS_PIXIT_RELAY.md` 测试映射 | Hub 集成测试 | ✅ 完成（87 passed / 0 failed 全量） |
| SDK-ICCOA-SHARE-03 | 钥匙信箱（DKF 侧） | BLE APDU 读写信箱（§8.3.12）— **设备端 DKF 功能, 非 Relay** | `iccoa-spec.md` §5 | 无 SDK 落点（车厂钱包 DKF 职责） | 📚 规范引用 |
| SDK-ICCOA-REMOTE-01 | 远程控车（经 Hub） | manageKey / SendCommand 通道 | `../sdk/SDK-ARCHITECTURE.md` §3.1 | `HubClient` | ✅ 代码就位 |

### 2.3 ICCE（T/CA 110-2020）

**参考文档**: [iccoa-icce-ble-command-frames.md](./iccoa-icce-ble-command-frames.md)（2b-F 唯一依据）· `pixit-common.md` §6.1（ICCE 参数）· `../sdk/PHASE2B-F-SM4-CONTRACT.md`

| ID | 测试项 | 断言/验收 | 参考章节 | SDK 落点 | 状态 |
|:---|:-------|:---------|:--------|:--------|:----:|
| SDK-ICCE-BLE-01 | control_command 帧 | 38 B: [command_type(1)][target(1)][user_id BE u32(4)][hmac(32)] | §5, module_design.md §3.1.4 | 指令帧编解码 | ✅ 代码就位 |
| SDK-ICCE-BLE-02 | 命令完整性 HMAC | HMAC-SHA256(会话密钥, 命令体前 6 字节) — **覆盖范围待真机确认 (R-3)** | §5 | iOS CryptoKit / Android javax.crypto | ✅ 代码就位 / ⏳ 覆盖范围真机待验 |
| SDK-ICCE-BLE-03 | 会话加密 SM4-CBC | SM4-CBC + PKCS#7, 密钥=session_key 前 16 B; IV 协商待确认 (R-4) | §6, GM/T 0002-2012 附录 A 测试向量 | `Sm4.swift` / `Sm4.kt` | ✅ 代码就位（标准向量验证） |
| SDK-ICCE-SEC-01 | 国密算法一致性 | SM2 密钥交换 / SM3 摘要 / SM4 加密 — 认证测试向量逐字节比对 | `pixit-common.md` §6.1 | 加密库封装 | ✅ 代码就位（42/42 断言含向量） |
| SDK-ICCE-SEC-02 | 挑战-响应认证 | Nonce + ECDH 派生 session_key[32] | §6（security_auth.c:177-179） | 认证流程 | ✅ 代码就位 |
| SDK-ICCE-NFC-01 | NFC 离线解锁（手机断电场景） | 手机 NFC 贴近解锁 | `../sdk/NFC-INTEGRATION.md` + `pics-icce.md` §3.2 | NFC 管理器 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-ICCE-UWB-01 | UWB 边缘分区 | ch5/6/8/9, TWR, 5 分区（FAR/MID/NEAR/VICINITY/INTERIOR） | `pics-icce.md` §3.3 + §6 | UWB 管理器 | ✅ 代码就位 / ⏳ 真机待验 |
| SDK-ICCE-SHARE-01 | S2S 分享（车服务器为中心） | 与 ICCOA 同构（无 Relay）, 证书格式/算法差异（SM2） | `iccoa-spec.md` §11 对比 | S2S 适配器 | ✅ E2E 就位（e2e_13 ICCE 6/6）/ 🔴 生产接入待厂商 API |
| SDK-ICCE-REMOTE-01 | 远程控车（经 Hub） | unlock/lock/engine/status 映射（UNLOCK→0x01, 与 ICCOA 相反） | §5 通用映射 | `HubClient` | ✅ 代码就位 |

---

## 3. 厂商适配矩阵（协议 × 功能面 × 状态）

> 状态图例: ✅ 代码就位（模拟/桩测试通过）· ⏳ 真机待验（单列联调项）· 🔴 规范差异（需修复后复测）· 📚 规范引用（无 SDK 落点）

| 功能面 | CCC (DK R3/v4.0.0) | ICCOA (DK 3.0/DK 4.0) | ICCE (T/CA 110-2020) | 证据（SDK-TASKS / 知识库） |
|:------|:------------------:|:---------------------:|:--------------------:|:--------------------------|
| **BLE 指令帧** | ✅ 代码就位（4 字节帧头 + APDU 0x0B/0x0C） | ✅ 代码就位（DK3.0 帧 + CTRL[cmd][param] + 枚举映射） | ✅ 代码就位（control_command 38B + 通用映射） | 2b-E / 2b-F |
| **安全通道** | ✅ 代码就位（HKDF SystemKeys + AES-128/CMAC, SCP03 风格） | ⏳ 链路层 LE SC 真机待验（应用层无加密） | ✅ 代码就位（SM4-CBC + HMAC-SHA256; ⚠️ IV/hmac 范围待真机 R-3/R-4） | 2b-E / 2b-F AD-1/AD-2 |
| **NFC 备用解锁** | ✅ 代码就位 / ⏳ 真机待验（entitlement/tech-list） | ✅ 代码就位 / ⏳ 真机待验 | ✅ 代码就位 / ⏳ 真机待验（断电场景） | 2b-H |
| **UWB 测距** | ✅ 代码就位 / ⏳ 真机待验（token 交换/会话参数） | ✅ 代码就位 / ⏳ 真机待验 | ✅ 代码就位 / ⏳ 真机待验（5 分区） | 2b-G |
| **后台运行** | ✅ 代码就位 / ⏳ 真机待验（state restoration） | ✅ 代码就位 / ⏳ 真机待验（前台服务） | ✅ 代码就位 / ⏳ 真机待验 | 2b-I |
| **分享** | ✅ 链路就位 / ⏳ 物理机 E2E 单列（Relay/Mailbox） | 🔴 客户端就位, 生产路径 stub（S2S 接入 P0） | 🔴 E2E mock 就位, 生产接入待厂商 API | 4.4 / 4.5 + iccoa-compliance-audit |
| **远程控车** | ✅ 代码就位（HubClient + 4.2 E2E） | ✅ 代码就位 | ✅ 代码就位 | 2a + 4.2 |
| **钥匙生命周期** | ✅ 代码就位（Create/Activate/Share/Revoke/Suspend/Resume/Delete） | ✅ 代码就位（BIND/UNBIND/8 权限位; ⚠️ 状态模型缺 SUSPENDED/TERMINATED） | ✅ 代码就位（KeyBind→Delete 全链） | 2a/2c + 审计 §3.2 |
| **离线能力** | 📚 规范引用（SDK 侧: 钥匙在 SE 可离线解锁, 无网可用） | 📚 规范引用 | ✅ 车端边缘计算（SDK 侧仅离线解锁） | `../sdk/SDK-ARCHITECTURE.md` §7 |

### 3.1 平台 × 协议适配状态（iOS / Android）

| 平台能力 | iOS | Android | 备注 |
|:--------|:---:|:-------:|:-----|
| BLE 后台 | ✅ restore identifier + 唤醒选项 | ✅ 前台服务 + 权限矩阵（41 API level 穷举） | 2b-I, 均真机待验 |
| UWB | ✅ NearbyInteraction（YDKNIUWBManager, typecheck 零错误） | ✅ android.uwb API 34+ + 版本降级 | 2b-G, token 交换真机待验 |
| NFC | ✅ CoreNFC（桩模块 12/12 断言） | ✅ NfcAdapter/IsoDep（33/33 交叉验证） | 2b-H |
| 加密 | CryptoKit（HMAC/ECDSA）+ Sm4.swift | javax.crypto + Sm4.kt | 2b-E/F |
| SDK×Hub 契约 | 4.2 E2E 11 段 21 断言 | 同左 | snake_case 特判已记录 |

---

## 4. PICS 引用与提交清单（送测前）

| 文档 | 状态 | 位置 |
|:-----|:----:|:-----|
| CCC PICS | ✅ v1.1（本次修补: v4.0.0 BLE 安全通道 + SDK 补充） | `docs/certification/pics-ccc.md` |
| ICCOA PICS | ✅ v1.1（本次修补: 帧端序/安全模型/S2S 状态） | `docs/certification/pics-iccoa.md` |
| ICCE PICS | ✅ v1.1（本次修补: control_command 帧 + 待确认项） | `docs/certification/pics-icce.md` |
| PIXIT（公共） | ✅ v1.0 | `docs/certification/pixit-common.md` |
| Relay Server PICS/PIXIT | ✅ v1.0 | `docs/compliance/PICS_PIXIT_RELAY.md` |
| 认证费用预算 | ✅ v1.0 | `docs/certification/certification-budget.md` |
| 实验室联系 | ✅ v1.0 | `docs/certification/lab-contacts.md` |
| 车端认证 checklist | ✅ v1.0（嵌入式侧） | `docs/CERTIFICATION-CHECKLIST.md` |

**送测前阻塞项（按优先级）**:
1. 🔴 ICCOA/ICCE S2S 生产路径接入（`hub/main.go` 换 `NewICCOAAdapterWithClient`）— 影响 ICCOA 分享认证
2. 🔴 ICCOA 钥匙状态模型补齐（SUSPENDED/TERMINATED）— 影响状态一致性测试
3. ⏳ UWB / NFC / 后台真机联调 — 三项硬件面认证测试的前置
4. ⏳ CCC 分享物理机 E2E — 分享认证前置
5. ⏳ R-3/R-4 真机确认（ICCE hmac 覆盖范围、SM4 IV 协商）

---

## 5. 版本历史

| 版本 | 日期 | 变更内容 | 作者 |
|:----:|:----:|:--------|:----:|
| v1.0 | 2026-08-01 | 初始版本 — SDK 侧认证测试清单 + 厂商适配矩阵 + PICS 缺口修补 | Hermes |
