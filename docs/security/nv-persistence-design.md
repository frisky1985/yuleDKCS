# CRL 与产线锁定 NV 持久化设计

| 项 | 内容 |
|---|---|
| 文档编号 | SEC-NV-2026-001 |
| 版本 | v1.5（keystore OTP 水位） |
| 关联 | RMP-2026-001 §0.2；`sec_revocation.c`；`key_mgmt.c`；`kw47a_flash.ld` |
| 目标 | 让 CRL（含 epoch）与 provisioning 锁定状态在复位/断电后保持，防回滚、防擦除绕过 |
| 实现状态 | 软件侧已落地：`sec_nv`（CRL/Lock/Keystore 三组双 Bank + 各自 OTP 水位）、`kw47a_nv` 绑定、Flash/OTP 驱动层；`test_security_nv` 18/18、`test_kw47a_nv` 7/7、`test_kw47a_drv` 12/12，`test_all` 291 项 0 失败；真机 PFLASH 时序、擦写缓存失效与 eFuse 位定义待验证 |

## 1. 现状与必须先修的问题

| # | 问题 | 影响 | 处理 |
|---|---|---|---|
| F1 | `key_mgmt.c` 使用 `KEYSTORE_FLASH_ADDR 0x080E0000`（STM32 占位地址） | KW47A 上密钥实际未持久化 | 已修正：keystore 改为 NV 双 Bank 记录（基址 `0x74000`），并具备掉电回退 |
| F2 | `kw47a_flash.ld` 的 `STORAGE` 位于 `0x00080000`，而 Flash 仅 512KB（末地址 `0x7FFFF`） | 分区越界，链接/烧录异常 | 已修正：`FLASH_B` 缩为 144KB，`STORAGE` 移至 `0x74000`/48KB |
| F3 | `virt_flash_read/write/erase` 仅有 weak 桩，返回 -1 | 无真实 NV 读写 | 已由 `sec_nv` Keystore 双 Bank 记录替代，并经 KW47A Flash 驱动落地 |
| F4 | `sec_revocation` 与锁定状态仅驻留 RAM | 复位即可绕过锁定、重放旧 CRL | 本文方案 |
| F5 | SE050 仅有 SCP03/签名 APDU，无对象存储/单调计数器接口 | 无法直接用作 NV 根 | 列为阶段 2，接口预留 |
| F6 | Bootloader “write-protected via SE050” 尚未实现 | NV 分区可被物理改写 | 调试熔断 + 写保护，G1 验证 |

## 2. 威胁模型与设计原则

- **T1 复位绕过**：攻击者断电重启，期待锁定状态丢失。
- **T2 重放旧 CRL**：恢复旧镜像/旧分区，让已吊销设备重新可用。
- **T3 掉电破坏**：擦写过程中断电，记录半写导致设备变砖或回到未锁定态。
- **T4 物理读取/改写**：调试口或外部编程器直接改 Flash。
- **T5 分区误写**：密钥区、CRL 区、锁记录区相互覆盖。

原则：

1. **双 Bank + 序号 + CRC-32 + commit-last**：任一时刻至少一份有效记录。
2. **高位存储做水位**：OTP/eFuse 或 SE050 单调计数器记录 CRL epoch 与锁定，Flash 仅作本地镜像。
3. **区分“空白”与“损坏”**：全 `0xFF` = 出厂空白；有魔数但校验失败 = 损坏 → 按已锁定/拒绝处理（fail-closed）。
4. **先校验后生效**：CRL 必须验签通过才进入 RAM；锁定状态以“任一来源为锁定”即锁定。
5. **写入顺序不可逆**：先提交 Flash，再抬高 OTP 水位；水位只增不减。

## 3. Flash 分区

### 3.1 容量确认（前置）

`kw47a_board.h` 标注 512KB，但 `STORAGE` 放在 `0x80000`，两者矛盾。必须先确认实际型号容量，二选一：

**方案 A：512KB（已按此实现，兼容 8KB 物理扇区）**

| 区域 | 起止 | 大小 | 说明 |
|---|---|---|---|
| Boot header | `0x00000000–0x000003FF` | 1KB | 不变 |
| Bootloader | `0x00000400–0x0001FFFF` | 127KB | 不变 |
| Slot A | `0x00020000–0x0004FFFF` | 192KB | 不变 |
| Slot B | `0x00050000–0x00073FFF` | 144KB | 尾部让出 48KB |
| **NV** | `0x00074000–0x0007FFFF` | 48KB | 6 × 8KB 物理扇区 |

OTA 镜像上限 = `min(192KB,144KB) = 144KB`（当前镜像约 106KB，余量 38KB），CI 增加 `arm-none-eabi-size` 门禁。

**方案 B：容量 ≥1MB 时**：可保持 A/B 各 192KB，将 NV 上移至 `0x00080000`；NV 内部布局不变，仅改 `SEC_NV_FLASH_BASE` 与链接脚本 `STORAGE`。当前实现选择方案 A，保证 512KB 器件可用。

两方案均由板级头文件宏定义，链接脚本与代码共用，并用 `_Static_assert` 锁定 Bank 边界与不重叠。

### 3.2 NV 分区内部（48KB，6 × 8KB）

| 偏移 | 大小 | 用途 |
|---|---|---|
| `+0x0000` | 8KB | Keystore Bank A |
| `+0x2000` | 8KB | Keystore Bank B |
| `+0x4000` | 8KB | CRL Bank A |
| `+0x6000` | 8KB | CRL Bank B |
| `+0x8000` | 8KB | Lock Bank A |
| `+0xA000` | 8KB | Lock Bank B |

每个 Bank 独占一个 8KB 物理扇区，擦除粒度为扇区；记录仅占头部 + payload，其余保持 0xFF。Keystore 最大记录 5747B，受 `SEC_NV_MAX_KEYSTORE=6144B` 约束（编译期断言）。

## 4. 记录格式

### 4.1 公共头（32 字节，小端显式读写）

| 偏移 | 字段 | 说明 |
|---|---|---|
| 0 | magic `u32` | 字节 `"YRNV"`，LE 值 `0x564E5259` |
| 4 | type `u8` | 1=CRL，2=LOCK，3=KEYSTORE |
| 5 | version `u8` | 1 |
| 6 | flags `u16` | 保留，0 |
| 8 | seq `u32` | 单调递增，跨 Bank 取最大有效值 |
| 12 | payload_len `u16` | 不含头 |
| 14 | reserved `u16` | 0 |
| 16 | crc32 `u32` | 覆盖 `[0,16)` 头 + payload |
| 20 | commit `u32` | `"COMT"`，**最后写入** |
| 24 | reserved `[8]` | 0 |

有效性 = `magic` 正确 && `type/version` 合法 && `payload_len` 在范围内 && `commit=="COMT"` && CRC 匹配。

### 4.2 CRL 记录

- payload = 现有签名 CRL blob（≤596B） + 8B 元数据：`installed_at u32`、`source u8`（0=本地/1=OTA/2=EOL）、`reserved[3]`。
- 加载时必须用**预置吊销根公钥**验签；验签失败直接丢弃该 Bank。
- `epoch` 从 blob 解析，与 OTP 水位比较：`epoch < watermark` → 拒绝加载。

### 4.3 Lock 记录

| 字段 | 大小 | 说明 |
|---|---|---|
| state | 1B | 0=未锁定，1=已锁定 |
| reason | 1B | 0=EOL，1=策略，2=维修 |
| reserved | 2B | 0 |
| lock_epoch | 4B | 锁定事件序号 |
| station_id | 16B | EOL 工位标识（可选） |

### 4.4 OTP / eFuse 映射（若芯片提供）

| 位/区 | 语义 | 写入时机 |
|---|---|---|
| word0 bit0 `PROD_LOCK` | 量产锁定，一次性 | EOL 锁定流程最后一步 |
| word0 bit1 `DEBUG_LOCK` | 调试口永久关闭 | EOL 或试产结束 |
| word1 `CRL_EPOCH` | 单调水位，31 个单向位（bit i 置 1 表示 epoch ≥ i+1） | 每次成功安装 CRL 后 |
| word2+word3 `KS_EPOCH` | keystore 单调水位，62 个单向位，跨 word 连续计数 | 每次成功保存 keystore 后 |

无 OTP 时，水位由 Flash 双 Bank 的 `seq` 兜底；残余风险见 §9。

### 4.5 Keystore 记录

- payload = 现有密钥 blob：`magic(4) "KEYS"` + `version(2)` + `count(1)` + `count × ccc_digital_key_t` + `crc32(4)`。
- 由 `key_mgmt` 构造/解析；`sec_nv` 只按 `type=3` 原样存取，不做密钥语义解析。
- 双 Bank 轮换 + `seq` + CRC + commit 最后写：commit 前掉电保留上一代；单 Bank 损坏回退另一 Bank；两 Bank 均损坏返回 `CCC_ERR_SECURITY`。
- 最大记录 5747B（`MAX_KEYS=8` 时），受 `SEC_NV_MAX_KEYSTORE=6144B` 与 8KB Bank 约束；SE050 仍为第一优先级，NV 为回退路径。
- **OTP 水位（word2+word3，62 位一元码）**：记录头 `flags` 复用为 keystore epoch；保存时 `new_epoch = max(记录 epoch, OTP 水位) + 1`，Flash 提交后再抬水位；加载时 `记录 epoch < OTP 水位` → `CCC_ERR_SECURITY`，可识别“整片 Flash 回滚到旧镜像”攻击。水位耗尽（62 次，无 OTP 时 255 次）返回 `CCC_ERR_STATE` 且不写 Flash；OTP 抬高失败返回 `CCC_ERR_HARDWARE`，但记录已提交、加载仍放行（不砖机）；OTP 读失败按降级跳过校验。一元读按**连续置位**计数，跨 word 部分烧写不会虚高水位。

## 5. 提交协议（掉电安全）

写入一条记录：

1. 选 `seq` 较小的 Bank 为“非活动 Bank”；
2. 擦除非活动 Bank；
3. 写入头（`commit=0xFFFFFFFF`）+ payload；
4. 回读并校验 CRC；
5. **最后**单独写入 `commit="COMT"`；
6. 再次回读校验；
7. 成功后才更新 RAM 与 OTP 水位。

启动选择：两个 Bank 都尝试解析，取 `seq` 最大且校验通过者；另一个视为备份。

| 掉电时机 | 结果 |
|---|---|
| 擦除后 / 写 payload 中 / 写 commit 前 | 新记录无效，旧 Bank 仍有效 → 选旧记录 |
| commit 写完后 | 新记录有效，`seq` 更大 → 选新记录 |
| 两 Bank 均无效且非全 `0xFF` | 判定损坏 → 锁定 + 拒绝 attestation，进入维修 |
| 全 `0xFF` | 出厂空白 → 未锁定、无 CRL（仅 DEV 生命周期允许供给） |

## 6. 启动流程与失效策略

```
sec_nv_init()
  ├─ 校验分区边界（编译期 + 运行期）
  ├─ 载入 Lock：Flash 最高 seq ∪ OTP PROD_LOCK  → 任一为锁即锁
  ├─ 载入 CRL：验签 + CRC + epoch ≥ OTP 水位
  ├─ 应用策略：PROD 生命周期下记录缺失/损坏 → fail-closed
  └─ 失败仅记录诊断，不阻断串口自检
```

| 场景 | 行为 |
|---|---|
| Lock 记录 CRC 坏但魔数存在 | 按已锁定处理，打印告警 |
| Lock 两 Bank 全坏（非空白） | 已锁定；拒绝对端认证，进入维修模式 |
| CRL 验签失败 | 丢弃该 Bank，不改变 RAM 中旧 CRL，告警 |
| CRL epoch 低于 OTP 水位 | 拒绝加载，要求重新下发 |
| OTP 写失败 | Flash 锁定仍生效；EOL 判定失败并告警 |
| 未安装 CRL | 按策略：默认放行（离线可用），可配置为强制拒绝 |

## 7. 软件接口

新增 `embedded/ccc_protocol/src/security/sec_nv.h/.c`，平台通过 ops 注入，避免绑定具体 Flash 地址：

```c
typedef struct {
    int  (*read)(uint32_t off, void *buf, uint16_t len);
    int  (*write)(uint32_t off, const void *buf, uint16_t len);
    int  (*erase)(uint32_t off, uint16_t len);
    bool (*otp_lock_read)(void);
    int  (*otp_lock_set)(void);
    int  (*otp_epoch_read)(uint32_t *epoch);
    int  (*otp_epoch_raise)(uint32_t epoch);
} sec_nv_ops_t;

ccc_status_t sec_nv_bind(const sec_nv_ops_t *ops);
ccc_status_t sec_nv_init(void);              /* 校验分区边界 */
ccc_status_t sec_nv_save_crl(const uint8_t *blob, uint16_t len);
ccc_status_t sec_nv_load_crl(uint8_t *blob, uint16_t cap, uint16_t *len);
ccc_status_t sec_nv_save_lock(const uint8_t station_id[16]);
ccc_status_t sec_nv_load_lock(bool *locked);
ccc_status_t sec_nv_save_keystore(const uint8_t *blob, uint16_t len);
ccc_status_t sec_nv_load_keystore(uint8_t *blob, uint16_t cap, uint16_t *len);
```

- `sec_revocation_install()`：RAM 校验通过 → `sec_nv_save_crl()` → OTP 水位；
- `sec_lock_provisioning()`：先置 RAM 锁定 → 写 Flash Lock 记录 → 再写 OTP（若可用）；任一步失败仍保持 RAM 锁定并返回诊断；
- `key_mgmt`：SE050 失败 → `sec_nv_save_keystore()/load_keystore()`；
- 未绑定 ops 时保持 RAM 行为并打印 `NV persistence not bound (RAM-only)`；
- 所有 offset 由 `SEC_NV_FLASH_BASE` 派生，业务层不出现绝对地址。

## 8. 测试方案

以 48KB RAM 假 Flash（6 × 8KB Bank）+ 可注入故障的 OTP 桩实现 `sec_nv_ops_t`：

1. CRL 安装 → 模拟复位（保留 Flash、清 RAM、`sec_nv_init`）→ 查询仍命中；
2. 重启后安装更低 epoch → 拒绝；更高 epoch → 接受；
3. 回滚 Flash 镜像 + OTP 水位保留 → 启动拒绝旧 CRL；
4. 锁定 → 复位 → 各 provisioning setter 仍返回 `CCC_ERR_STATE`；
5. Lock 记录 CRC 损坏 → 复位后按锁定处理；
6. 在擦除、payload 写完、commit 前/后四个断点断电 → 始终能选出有效记录；
7. 两 Bank 全坏 → 锁定并拒绝 attestation；
8. Keystore 往返/复位保持、两代覆盖取最新、commit 前掉电保留上一代、单 Bank 损坏回退、双损坏拒绝、参数与未绑定错误；
9. Keystore OTP 水位：连续保存水位 1→2、Flash 回滚快照 + 水位保留 → `CCC_ERR_SECURITY`、抬高失败返回硬件错误但记录可加载、无 OTP 降级、62 次耗尽拒绝且 Flash 不变；
10. `SEC_NV_*` 越界或 Bank 重叠在编译期失败（`_Static_assert`）。

## 9. 残余风险与对策

| 风险 | 对策 |
|---|---|
| 无 OTP 时可整体回滚 Flash | 调试熔断 + 安全启动防回滚计数；或在阶段 2 使用 SE050 单调计数器 |
| Keystore 水位耗尽（62 次） | EOL 流程预算保存次数（配对/轮换合计 <62）；超限返回 `CCC_ERR_STATE` 并拒绝写，需重新规划或迁移 SE050 对象存储 |
| Flash 物理读取 | 调试口永久关闭；密钥仅存 SE050，NV 不含明文密钥 |
| 擦写寿命 | CRL/锁定/密钥更新低频；8KB Bank 轮换，远低于 10 万次 |
| 写入期间掉电 | §5 协议保证“旧或新”至少一份有效 |
| Bootloader 区写保护与 NV 写入冲突 | NV 放在 Slot B 尾部（方案 A），不动 Bootloader |
| 镜像尺寸门禁 | Slot B 143KB 可用；当前镜像约 106KB，CI 需加 `arm-none-eabi-size` 上限检查 |

## 10. 落地顺序

1. 确认 Flash 容量与 OTP 能力，修正 F1/F2 地址与链接脚本；
2. 实现 `sec_nv` 与假 Flash 故障注入测试（不依赖硬件）；
3. 板级 KW47A Flash/OTP 驱动，接入 `sec_nv_ops_t`；
4. 启动早期调用 `sec_security_init()`（内部执行 `sec_nv_init()`），EOL 调用锁定与 CRL 安装；
5. G1 §3.7 增加“复位后仍锁定 / 旧 CRL 被拒”验收项。

## 11. 验收标准

- 复位、断电、擦除单 Bank 后：锁定状态不回退，CRL 命中结果不变；
- 安装 epoch 更低的 CRL：`CCC_ERR_STATE`，重启后仍拒绝；
- 任一写入阶段断电：设备可启动且记录为旧值或新值，无“双无效”；
- Lock 记录损坏：设备按已锁定运行，不提供 provisioning 写入；
- 量产件无法通过复位/重新上电恢复未锁定状态。

## 12. 实施状态

| 项 | 状态 |
|---|---|
| `sec_nv` 双 Bank + CRC + commit-last | ✅ 已实现 |
| CRL/Lock 保存与加载、OTP 水位比较 | ✅ 已实现 |
| `sec_security_init()` 上电恢复并接入固件启动 | ✅ 已实现（未绑定时打印 RAM-only 告警） |
| `sec_lock_provisioning()` 持久化 | ✅ 已实现 |
| 地址修正（`key_mgmt`、链接脚本） | ✅ 已修正 |
| 假 Flash 故障注入测试（含 4 个掉电断点） | ✅ 18/18 通过（含 keystore 双 Bank 5 项 + OTP 水位 5 项） |
| Keystore OTP 水位（word2+word3，62 位一元码） | ✅ 已实现：epoch 存记录头 flags、保存后抬水位、回滚快照拒绝、耗尽保护、无 OTP 降级；`test_kw47a_drv` 跨 word 计数 12/12 |
| 8KB 扇区布局（6 Bank，48KB） | ✅ 已实现：`FLASH_B` 144KB、`STORAGE` `0x74000`，几何门禁与 8KB 扇区匹配 |
| Keystore 双 Bank 重构（替代 `virt_flash_*`） | ✅ 已实现：SE050 优先，NV 回退；commit 前掉电回退上一代 |
| 板级 KW47A Flash/OTP 驱动 | 🟨 代码完成：Flash LL（页擦/编程/回读）+ OTP LL（单调位编程/epoch）+ MCUX PFLASH 适配 + eFuse 弱符号钩子；`test_kw47a_drv` 12/12；待真机验证时序/缓存失效 |
| PFLASH 扇区几何 | ✅ 当前布局按 8KB 扇区对齐（Bank=扇区）；若器件扇区更小仍兼容，门禁按 `%` 校验 |
| SE050 对象存储/单调计数器（阶段 2） | ⏳ 待 Plug&Trust 集成 |
| Flash 实际容量确认（512KB / 1MB） | ⏳ 待硬件确认（当前布局在 512KB 内有效） |
