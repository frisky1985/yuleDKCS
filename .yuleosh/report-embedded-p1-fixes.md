# 嵌入式端 P1 修复报告
> 日期: 2026-07-08

## 修复总览

| ID | 描述 | 模块 | 文件 | 状态 |
|:---|:-----|:-----|:-----|:-----|
| EMB-P1-04 | Nonce 去重/防重放计数器 | ICCE | `security_auth.c` | ✅ |
| EMB-P1-05 | 引擎启动权限检查未全覆盖 | ICCE | `icce_security.c` + `icce_digital_key.h` | ✅ |
| EMB-P1-06 | KDF 密钥派生未验证(错误传播) | ICCE Crypto | `crypto_engine.c` | ✅ |
| EMB-P1-07 | TLV 解析缺少 EOF 截断防护 | ICCE (BERTLV) | `bertlv/decoder.go` (2 copies) | ✅ |
| EMB-P1-08 | Challenge-Response 缺少超时窗口 | ICCE | `security_auth.c` | ✅ |
| EMB-P1-09 | 离线决策缺少时间戳防回滚 | ICCE | `offline_decision.c` | ✅ |
| EMB-P1-10 | BLE bonding cache 无大小限制 | CCC | `ble_kw47a.c` | ✅ |
| EMB-P1-11 | CCC 缺少 PAN ID 变化重连处理 | CCC | `ble_kw47a.c` | ✅ |

## 详细修复说明

### EMB-P1-04: Nonce 去重/防重放计数器
**文件**: `embedded/icce_protocol/src/security/security_auth.c`

修复内容：
- 在 `security_verify_response()` 中添加 `is_nonce_used()` 检查
- 验证响应时间戳晚于挑战时间戳
- 验证通过前调用 `mark_nonce_used()` 标记 Nonce
- 超时路径也标记 Nonce 防止后续重放

### EMB-P1-05: 引擎启动权限检查
**文件**: `embedded/icce_protocol/src/icce_security.c` + `icce_digital_key.h`

修复内容：
- 新增 `icce_security_check_engine_start_perm()` 函数
- 检查绑定设备是否具有引擎启动权限位
- 函数声明添加到 `icce_digital_key.h` 头文件
- 支持未来扩展为查询证书 access_rights

### EMB-P1-06: KDF 密钥派生未验证（错误传播）
**文件**: `embedded/icce_protocol/src/crypto/crypto_engine.c`

修复内容：
- 检查每次 `crypto_hmac_sha256` 调用的返回值
- 添加参数验证（out_len == 0 检查、info buffer 上限检查）
- HMAC 失败时立即返回错误，不继续派生
- 错误路径上安全清零敏感数据

### EMB-P1-07: TLV 解析缺少 EOF 截断防护
**文件**: 
- `backend/cloud/hub/internal/codec/bertlv/decoder.go`
- `backend/hub/pkg/codec/bertlv/decoder.go`

修复内容：
- `readTag()`: 限制多字节 Tag 续延字节最大 3 个
- `readLength()`: 拒绝 0x80 (无限长度编码); 添加长度范围检查 (0-65536)
- `Decode()`: 将 ErrBufferTooShort/ErrUnexpectedEnd 替换为 ErrTruncatedData
- 新增错误类型 ErrTruncatedData 和 ErrLengthOverflow

### EMB-P1-08: Challenge-Response 缺少超时窗口
**文件**: `embedded/icce_protocol/src/security/security_auth.c` (同 EMB-P1-04 一并修复)

修复内容：
- 超时响应标记 Nonce 已使用，防止后续重放
- 验证响应时间戳 > 挑战时间戳，检测回滚攻击

### EMB-P1-09: 离线决策缺少时间戳防回滚
**文件**: `embedded/icce_protocol/src/decision/offline_decision.c`

修复内容：
- 新增 `last_sync_entry_t` 结构和 `check_timestamp_rollback()` 函数
- 检查每个 (key_id + user_id) 组合的时间戳是否单调递增
- 拒绝时间戳回滚 (new <= last) 和跳变过大 (>60秒)
- LRU 淘汰策略管理最多 64 个设备的时间戳记录

### EMB-P1-10: BLE bonding cache 无大小限制
**文件**: `embedded/ccc_protocol/src/ble/ble_kw47a.c`

修复内容：
- 添加 `bonding_cache_entry_t` 结构体和 `MAX_BONDING_CACHE_ENTRIES` (16)
- 实现 `bonding_cache_evict_lru()` LRU 淘汰策略
- 实现 `ble_bonding_cache_add()` / `ble_bonding_cache_find()` 接口
- 已存在条目时更新，满员时执行 LRU 淘汰

### EMB-P1-11: PAN ID 变化重连处理
**文件**: `embedded/ccc_protocol/src/ble/ble_kw47a.c`

修复内容：
- 添加 `g_pan_id_cache` 数组跟踪 PAN ID
- 实现 `ble_check_pan_id_change()` 检测 PAN ID 变化
- PAN ID 变化时自动断开当前连接，更新后重新广播以触发重连
- 在 KW47A_EVT_CONNECTED 处理中调用 PAN ID 检查

## 编译验证
所有修改在嵌入式 freestanding 模式下编译通过（arm-none-eabi-gcc with -Wall -Wextra -Wpedantic -Werror）。
Go 代码修改通过 `go build` 和 `go vet`。
