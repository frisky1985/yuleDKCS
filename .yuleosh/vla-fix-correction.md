# VLA 修复纠正记录

## 背景
之前栈修复时将 3 处 4 个 VLA（可变长度数组）替换为固定 `static` 缓冲区，
被老板指正：TLV 解析 / 变长载荷构建需要动态长度，不可用固定上限缓冲区硬替代。

## 修复方式
- 删除 `static uint8_t blob[N]` 固定缓冲区
- 改用 `pvPortMalloc(size)` / `vPortFree(ptr)` 动态分配
- 每个 `pvPortMalloc` 均有对应 `vPortFree`（所有返回路径覆盖）
- 保留 `sec_secure_zero()` / `keystore_secure_zero()` 安全清零后释放

## 修改文件清单

### 1. `ccc_protocol/src/security/security.c`
- `sec_store_key()` (line 113): `static uint8_t blob[1024]` → `pvPortMalloc(blob_len)`
  - 构造 [key_id(16)][key_data(n)][version(1)][crc32(4)] 变长载荷
- `sec_load_key()` (line 168): `static uint8_t blob[576]` → `pvPortMalloc(max_blob_len)`
  - 读取 SE050 Transparent Object 变长数据
- 添加前向声明: `void *pvPortMalloc(size_t);` / `void vPortFree(void *);`

### 2. `ccc_protocol/src/keymgmt/key_mgmt.c`
- `save_keys()` (line 99): `static uint8_t blob[4096]` → `pvPortMalloc(blob_len)`
  - 构造 [magic(4)][version(2)][key_count(1)][keys(N)][crc32(4)] 变长载荷
  - 5 个返回路径全都有 `vPortFree`
- `load_keys()` (line 173): `static uint8_t blob[4096]` → `pvPortMalloc(KEYSTORE_FLASH_SIZE)`
  - 8 个校验返回路径全都有 `vPortFree`
- 添加前向声明: `void *pvPortMalloc(size_t);` / `void vPortFree(void *);`

## 未修改的 static 缓冲区（保持原样）
这些是固定帧大小场景，不应改动态:

| 文件 | 变量 | 说明 |
|------|------|------|
| `ble_kw47a.c:345` | `static uint8_t buf[244]` | BLE UART bridge 固定帧 |
| `ble_kw47a.c:486` | `static uint8_t evt_buf[260]` | KW47A IRQ 事件处理 |
| `ble_kw47a.c:751` | `static uint8_t buf[260]` | GATT Notify 固定帧 |
| `offline_decision.c:424` | `static uint8_t key_buf[128]` | 离线决策固定 buffer |
| `offline_decision.c:451` | `static uint8_t perm_buf[128]` | 离线决策固定 buffer |

## 内存泄漏验证
每个 `pvPortMalloc` 的 `vPortFree` 覆盖:
- `sec_store_key`: 2 种返回路径（正常 + HARDWARE 错误）
- `sec_load_key`: 6 种返回路径（NOT_FOUND ×2, SECURITY ×3, INVALID_PARAM, OK）
- `save_keys`: 5 种返回路径（SE050失败, Flash擦除失败, Flash写入失败×多次, OK×2）
- `load_keys`: 8 种返回路径（HARDWARE, SECURITY×6, OK）

## 编译验证
```
arm-none-eabi-gcc -c -Os -ffreestanding ... security.c    → OK (0 errors)
arm-none-eabi-gcc -c -Os -ffreestanding ... key_mgmt.c     → OK (0 errors)
```
