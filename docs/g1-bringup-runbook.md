# G1 硬件 Bring-up 作业指导书

| 字段 | 内容 |
|---|---|
| 文档编号 | G1-BR-2026-001 |
| 关联计划 | RMP-2026-001 §5（P2 阶段 / P0-05 / G1 门禁） |
| 版本 | v1.3（增加串口日志分析工具与判读规则） |
| 适用硬件 | KW47A EVB + ST25R501 + NCJ29D6 + SE050 |
| 前置条件 | 固件可在 host-sim 与真实 target 构建通过；软件侧 P0 已闭环（`test_all` 247 项 0 失败，含安全启动 13/13）；T=1' 传输层单测通过 |

> 本指导书用于**首次上电**（bring-up）与门禁取证。每一步都给出可观测的通过判据，
> 结果需按 TES-2026-001《测试证据管理规范》留档（设备序列号、仪器原始数据、时间戳、归档哈希）。

---

## 1. 台架准备

| 项 | 要求 |
|---|---|
| 电源 | 可编程直流电源，限流 500 mA，初始 3.3 V |
| 调试器 | J-Link / CMSIS-DAP，SWD 4 线 |
| 串口 | USB-UART，115200 8N1，接调试 UART |
| 仪器 | 示波器（≥100 MHz）、逻辑分析仪（SPI/I2C 解码）、电流探头 |
| 软件 | `cmake`、Arm GNU Toolchain、SE050 Plug&Trust 工具链（可选） |

构建（真实 target）：

```bash
cmake -S embedded/firmware -B build-arm \
      -DCMAKE_TOOLCHAIN_FILE=<arm-toolchain>.cmake \
      -DBRINGUP_SELFTEST=ON \
      -DSECURE_BOOT=ON          # 验证安全启动时启用
cmake --build build-arm
# 产物: build-arm/src/yuleDKCS_fw.elf / .bin / .hex
```

`-DBRINGUP_SELFTEST=ON` 会在启动后打印 7 项基础自检；启用 `SECURE_BOOT=ON` 时追加第 08 项：

```
[BRINGUP] === G1 hardware self-test ===
[BRINGUP] 01 time-base          PASS
[BRINGUP] 02 storage            PASS
[BRINGUP] 03 se050-init         PASS
...
[BRINGUP] 08 secure-boot        PASS
[BRINGUP] RESULT: 8 passed, 0 failed, 0 skipped
```

---

## 2. 上电与基础检查（P0-05-T1）

| # | 步骤 | 通过判据 |
|---|---|---|
| 2.1 | 限流 100 mA 上电，不烧录 | 3.3 V 轨电压 3.3 V ±5%，电流 < 80 mA，无异常发热 |
| 2.2 | SWD 连接，读 IDCODE | 识别到 Cortex-M33 内核，目标电压正常 |
| 2.3 | 烧录 ELF，复位 | 串口输出 `=== yuleDKCS Firmware v2.3.0-rc1 ===` |
| 2.4 | 测量系统时钟（MCO/SWO） | 96 MHz ±100 ppm（`KW47A_SYSTEM_CLOCK_HZ`） |
| 2.5 | 自检 01 time-base | PASS（SysTick 递增） |
| 2.6 | 自检 02 storage | PASS（写入/回读一致） |

失败排查：无串口输出先查 UART 引脚复用与波特率；时钟不对查 `kw47a_clock_init()` 与晶振负载电容。

---

## 3. SE050 安全芯片（P0-05-T2 / P0-04）

### 3.1 电气与寻址

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.1.1 | 逻辑分析仪抓 I2C（SDA/SCL） | 上电后有地址 `0x48` 的写事务；SCL ≤ 400 kHz |
| 3.1.2 | 读 SE050 供电与复位脚 | VCC 1.8/3.3 V 符合模组规格；RST 释放为高 |
| 3.1.3 | 自检 03 se050-init | PASS |

### 3.2 T=1' 传输层验证

固件通过 `se050_t1oi2c.c` 发送 T=1' 帧（NAD|PCB|LEN|INF|CRC16）。

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.2.1 | 逻辑分析仪解码首帧 | `5A 00 08 80 50 00 00 08 <8字节挑战> CRC16` |
| 3.2.2 | 检查响应帧 | 首字节 `5B`（SE NAD），CRC16 校验通过 |
| 3.2.3 | 观察重传 | 无持续 R-block 重传；无 CRC 错误日志 |

> T=1' 常量集中在 `se050_t1oi2c.h`（NAD/PCB/CRC16）。若真机 NAD/PCB 与实现不符，
> 只需改该头文件，并在本表补充实测值作为证据。

### 3.3 SCP03 握手

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.3.1 | 使用出厂默认传输密钥（全 0）建立会话 | `INITIALIZE UPDATE` 返回 `9000`，卡密码学校验通过 |
| 3.3.2 | `EXTERNAL AUTHENTICATE` | 返回 `9000`，会话状态 `SCP03_STATE_OPEN` |
| 3.3.3 | 发送一条受保护 APDU | C-MAC 校验通过，响应 R-MAC 可验证 |
| 3.3.4 | 关闭会话 | 会话密钥被清零（调试器查看内存为 0） |

### 3.4 产线密钥个性化（量产必做）

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.4.1 | 调用 `se050_scp03_provision_keys()` 注入 K_ENC/K_MAC/K_RMAC | 个性化后默认全 0 密钥无法再建链 |
| 3.4.2 | 密钥写入产线密钥库并记录序列号 | 一机一密，可追溯 |
| 3.4.3 | 重新上电复测 | 仅个性化密钥可建链 |
| 3.4.4 | 产线工位 PKCS#11 适配器注入 MK 并派生传输密钥 | MK 不出 HSM；派生 K_ENC/K_MAC/K_RMAC 与软件参考实现指纹一致（`hsm_provision_fingerprint`） |
| 3.4.5 | 同 UID/版本重复派生 | 结果确定一致；更换版本或 UID 结果不同 |
| 3.4.6 | 工位日志与密钥指纹归档 | 一机一密可追溯；日志不含明文密钥 |

> **红线**：默认全 0 传输密钥不得用于任何客户样机或量产件。
> 工位适配器通过 `hsm_prov_pkcs11_ops_t` 注入，禁止把 MK 读入产线 PC 内存。

### 3.5 PUT KEY / REPLACE KEYS 真机验证（P0-04-T5）

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.5.1 | 使用派生密钥执行 PUT KEY（INS `0xD8`） | 返回 `9000`；指定 key id 可读回元数据 |
| 3.5.2 | 断开后仅用新密钥重建 SCP03 | `9000`，状态 `SCP03_STATE_OPEN` |
| 3.5.3 | 执行 REPLACE KEYS（INS `0xD2`） | 返回 `9000`，密钥版本号单调 +1 |
| 3.5.4 | 连续 3 次轮换演练 | 每次轮换后新版本可建链；旧版本行为按卡策略记录实测结果 |
| 3.5.5 | 轮换中途掉电/复位 | 旧密钥仍可用（可用性），不得变砖；日志与卡状态一致 |

> 轮换失败不得清除唯一可用密钥；失败必须告警并保留旧版本回退能力。

### 3.6 CCC 认证链路真机联调（P0-04-T3/T4）

前提：产线已通过 `sec_set_peer_trust_root()` 预置设备信任根；手机侧持有由该根签发的 leaf→intermediate 链与 attestation 材料。

| # | 步骤 | 通过判据 |
|---|---|---|
| 3.6.1 | 读取预置根指纹并重启 | 根指纹不变；运行期无法被 BLE 报文覆盖 |
| 3.6.2 | 手机下发证书链 + attestation | 链验证通过，叶子公钥注册成功，`sec_verify` 通过 |
| 3.6.3 | 手机伪造自带 root 的链路 | 必须拒绝（对端 `root_pub_key` 不参与锚定），状态 `LOCKED` |
| 3.6.4 | 篡改链 / 过期证书 | 拒绝并锁定，串口日志给出 `VERIFY_CERT_INVALID` 原因 |
| 3.6.5 | 无链、仅预置对端公钥 | 签名正确则通过；未预置必须 `LOCKED` |
| 3.6.6 | 云端独立验真 | 云端用同一根验证 attestation（device_id/key_id/fw_hash/security_state）通过 |
| 3.6.7 | 设置可信时间后重放过期链 | 按 `not_before/not_after` 拒绝 |

> 真机必须调用 `sec_set_trusted_time()` 注入可信时间，否则有效期强制不生效（软件默认放行）。

---

## 4. NFC 前端 ST25R501（P0-05-T4）

| # | 步骤 | 通过判据 |
|---|---|---|
| 4.1 | 自检 06 nfc | PASS |
| 4.2 | 读芯片 ID 寄存器（SPI） | 返回 ST25R501 预期 ID（记录实测值） |
| 4.3 | 场检测：手机/卡片靠近 | `hal_nfc_poll` 返回 >0，帧内容非全 0 |
| 4.4 | 天线匹配 | 13.56 MHz 谐振点偏差 < 5%，场强满足读距要求 |

---

## 5. UWB NCJ29D6（P0-05-T3）

| # | 步骤 | 通过判据 |
|---|---|---|
| 5.1 | 自检 07 uwb | PASS |
| 5.2 | 读设备 ID | 与 NCJ29D6 规格一致 |
| 5.3 | 双板测距（1 m / 5 m / 10 m） | 距离误差 ≤ ±10 cm，丢包率 < 1% |
| 5.4 | IRQ 时序 | 中断响应 < 100 µs，无丢中断 |

---

## 6. BLE KW47A（P0-05-T5）

| # | 步骤 | 通过判据 |
|---|---|---|
| 6.1 | 自检 05 ble | PASS |
| 6.2 | nRF Connect 扫描 | 可见广播名与 CCC/ICCE 服务 UUID |
| 6.3 | 连接、配对、订阅通知 | 连接稳定，MTU ≥ 185，无异常断开 |
| 6.4 | 吞吐 | 通知吞吐 ≥ 20 kB/s（记录实测） |

---

## 7. 端到端（G1 退出）

| # | 步骤 | 通过判据 |
|---|---|---|
| 7.1 | 手机→BLE→认证→UWB→解锁 | 状态机 `STANDBY→BLE_CONNECTED→AUTH_PROCESS→UWB_RANGING→UNLOCKED` |
| 7.2 | 离开→闭锁 | 距离超阈值后 `LOCKED`，无误闭锁 |
| 7.3 | 断电/复位 | 上电后安全状态为 `LOCKED` |
| 7.4 | 全量自检 | 8/8 PASS，0 SKIP（未启用 `SECURE_BOOT` 时为 7/7） |

---

## 8. 安全启动真机验证（P0-04-T6）

前提：软件链路已通过（`test_secure_boot` 13/13）；固件以 `-DSECURE_BOOT=ON` 构建；清单由 `secboot_tool sign` 生成，受信公钥由产线写入。

| # | 步骤 | 通过判据 |
|---|---|---|
| 8.1 | 烧录合法镜像 + 清单 + 受信公钥，复位 | 正常启动，串口输出 secure boot OK |
| 8.2 | 篡改镜像 1 字节后复位 | 拒绝启动，`BAD_HASH`，进入安全停机 |
| 8.3 | 篡改签名或替换错误公钥 | 拒绝启动，`BAD_SIGNATURE` / `KEY_MISMATCH` |
| 8.4 | 烧录低于当前 anti_rollback 的旧镜像 | 拒绝启动，`ROLLBACK` |
| 8.5 | 首次写入 OTP/eFuse 计数器并回读 | 与清单值一致；写入后不可回退、不可改写 |
| 8.6 | 生命周期切到 PROD/RMA 后连接调试器 | SWD/JTAG 拒绝访问；仅 DEV 可调试 |
| 8.7 | 自检 08 secure-boot | PASS |
| 8.8 | 断电/复位重复 8.1–8.4 | 结果一致，不存在绕过路径 |

> 证据：串口原始日志、示波器复位时序、OTP 回读值、调试器拒绝截图、镜像与清单 SHA-256。
> **红线**：量产件必须关闭调试口并固化 OTP；任一项失败阻断 G1/G5。

---

## 9. 证据与门禁

### 9.1 串口日志采集（推荐）

使用 `embedded/tools/bench/serial_capture.py`（仅 Python 3 标准库，Linux/macOS）：

```bash
python3 embedded/tools/bench/serial_capture.py \
  --port /dev/ttyUSB0 --baud 115200 \
  --firmware build-arm/src/yuleDKCS_fw.elf \
  --label G1-EVB01 --operator "$USER"
# 采集中输入 `3.5.2 新密钥重建SCP03` 回车插入步骤标记；输入 `q` 回车结束
# 定时采集：追加 --duration 600
```

产出 `bench-evidence/G1-<时间>/`：

| 文件 | 内容 |
|---|---|
| `serial.log` | 毫秒时间戳串口原始输出，含 START/END 与 MARKER |
| `steps.tsv` | 步骤号、说明、时间、操作者 |
| `meta.json` | 主机、串口参数、git 提交、固件 SHA-256、行数/字节数、步骤列表 |
| `serial.log.sha256` | 可直接 `sha256sum -c` 校验归档完整性 |

### 9.3 日志分析

使用 `embedded/tools/bench/analyze_serial_log.py` 单遍流式解析采集日志：

```bash
python3 embedded/tools/bench/analyze_serial_log.py \
  bench-evidence/G1-20261112-104500/serial.log

# 仅看问题行并附 2 行上下文
python3 embedded/tools/bench/analyze_serial_log.py serial.log --only-problems --context 2
# CI 严格门禁：警告也判失败；输出 JSON
python3 embedded/tools/bench/analyze_serial_log.py serial.log --fail-on-warn --json
```

判读规则：🟢 正常；🟡 警告/跳过/采集中断；🔴 问题（FATAL/ERROR/FAIL/PANIC/
HardFault/ASSERT、`SECURE_BOOT: FAIL`、`VERIFY_*` 失败码、认证失败）。
`[BRINGUP] NN ... FAIL` 与 `RESULT: ... failed` 会同时计入问题。
自动读取同目录 `meta.json`（台架标签、提交、固件哈希、采集时段）。
`--expect <正则>` 可声明必须出现的记录（如 `BRINGUP\] RESULT`），缺失记为问题；
`--ignore <正则>` 用于屏蔽负向测试中预期出现的失败行。
退出码：0=通过，1=有问题（或 `--fail-on-warn` 有警告），2=参数/读取错误。

### 9.4 归档要求

- 每步记录：日期时间、EVB 序列号、固件提交哈希、仪器截图/原始数据文件、结果。
- 串口日志与仪器截图放入同一证据目录，归档到证据库并登记证据索引。
- G1 判定：§2–§8 全部通过且无开放 P0 问题；质量、硬件、架构三方签字。

## 10. 失败处理原则

1. 先复现，再改代码；禁止用"跳过用例"方式关闭问题。
2. 电气/时序问题优先用示波器/逻辑分析仪定位，不在未取证前修改驱动常量。
3. 任一硬件项失败即阻断 G1，不得带入 G3 HIL。
