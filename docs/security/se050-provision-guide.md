# SE050 产线密钥注入方案

> **版本**: 1.0.0 | **日期**: 2026-07-16
> **适用**: yuleDKCS 嵌入式车端 (NXP S32K3 + SE050)
> **安全等级**: P0-Critical (KR-IMPL-01)

---

## 1. 背景

### 1.1 问题

SE050 安全芯片出厂时，默认传输密钥 (K_ENC, K_MAC, K_RMAC) 为全零 `0x0000...0000`。如果以默认密钥状态部署，攻击者可以：
1. 建立 SCP03 安全通道
2. 读取/写入 SE050 中的所有密钥材料
3. 克隆或篡改数字钥匙

**严重度**: 🔴 P0-Critical — 全系统沦陷风险

### 1.2 目标

在生产制造过程中，为每颗 SE050 芯片注入唯一的传输密钥，确保：
- 每颗芯片有独立的身份和密钥
- SCP03 通道安全性不依赖默认密钥
- 注入流程可审计、可追溯、可验证

---

## 2. 密钥架构

### 2.1 密钥层次

```
HSM (硬件安全模块)
  └── Master Key (MK)
       ├── K_ENC[i] = AES-128(MK, 0x01 || UID[i] || 0x0000000000)
       ├── K_MAC[i] = AES-128(MK, 0x02 || UID[i] || 0x0000000000)
       └── K_RMAC[i] = AES-128(MK, 0x03 || UID[i] || 0x0000000000)
           ↑
        每颗 SE050 芯片有唯一 UID + 派生密钥
```

### 2.2 派生公式

| 密钥 | 算法 | 输入 |
|:-----|:-----|:-----|
| K_ENC | AES-128-ECB(MK, 0x01 \|\| UID \|\| 0x00⁵) | Master Key, Chip UID |
| K_MAC | AES-128-ECB(MK, 0x02 \|\| UID \|\| 0x00⁵) | Master Key, Chip UID |
| K_RMAC | AES-128-ECB(MK, 0x03 \|\| UID \|\| 0x00⁵) | Master Key, Chip UID |

### 2.3 密钥存储

| 组件 | 存储内容 | 存储方式 |
|:-----|:---------|:---------|
| **HSM** | Master Key (MK) | 硬件安全模块，物理隔离 |
| **产线系统** | UID → (K_ENC, K_MAC, K_RMAC) | HSM 派生，不在产线系统明文存储 |
| **SE050** | K_ENC, K_MAC, K_RMAC | 芯片内部 NVM，不可读 |
| **云端 DB** | UID 注册记录，无密钥材料 | `se050_uid`, `provisioned_at`, `status` |

---

## 3. 产线注入流程

### 3.1 前置条件

| 条件 | 要求 |
|:-----|:------|
| **HSM** | PKCS#11 或 AWS CloudHSM，存储 Master Key |
| **通信** | SE050 I2C (标准 400kHz)，生产可通过 SWD/JTAG 编程器 |
| **工具** | NXP SE05X 中间件 (se05x_api.h) |
| **环境** | 洁净室或受控产线环境，防侧信道探测 |
| **人员** | 授权操作员，操作需双人验证 |

### 3.2 注入步骤

#### 步骤 1: 芯片身份读取
```
操作: 产线编程器通过 I2C 读取 SE050 UID
命令: se05x_get_uid() → UID (18 bytes)
输出: 芯片唯一标识符 UID[i]
验证: UID 不为全零，与芯片标签一致
```

#### 步骤 2: 密钥派生 (HSM 内部)
```
操作: 产线系统发送 UID[i] 到 HSM
HSM: 计算 K_ENC/K_MAC/K_RMAC = Derive(MK, UID[i])
输出: 派生密钥 (HSM 内部，不暴露明文)
验证: HSM 返回派生密钥的 SHA-256 校验值
```

#### 步骤 3: SCP03 通道建立 (默认密钥)
```
操作: 使用默认全零密钥建立临时 SCP03 通道
命令: se05x_scp03_initialize_update(0x0000...0)
       se05x_scp03_external_authenticate()
验证: 通道建立成功，状态码 0x9000
```

#### 步骤 4: 传输密钥写入
```
操作: 通过 SCP03 加密通道写入新密钥
命令: se05x_put_key(K_ENC, K_MAC, K_RMAC)  // 写入到 SE050 密钥槽
       se05x_put_key_usage_qualifier()        // 设置密钥用途限定符
验证: 写入后读回密钥版本号，确认写入成功
```

#### 步骤 5: 新密钥验证 (Put Key 验证)
```
操作: 关闭当前 SCP03 通道，用新密钥重新建立
命令: se05x_scp03_close_session()
       se05x_scp03_initialize_update(K_ENC_new, K_MAC_new, K_RMAC_new)
       se05x_scp03_external_authenticate()
验证: 新 SCP03 通道建立成功 ✅
      默认密钥通道建立失败 ❌
```

#### 步骤 6: 产线记录
```
操作: 记录注入结果到产线数据库
字段: UID, provisioned_at, hsm_key_id, operator_id, checksum
输出: 每颗芯片一条记录，包含 SHA-256(UID || K_ENC_hash)
验证: 记录完整性检查
```

### 3.3 时序图

```
产线编程器          SE050               HSM              产线DB
    │                │                  │                  │
    │── UID请求 ──────→                  │                  │
    │←── UID ────────│                  │                  │
    │                                  │                  │
    │── UID[i] ─────────────────────────→│                  │
    │←── Derive(MK, UID[i]) ────────────│                  │
    │                                  │                  │
    │── SCP03(全零) ──→                  │                  │
    │←── SCP03 OK ────│                  │                  │
    │                                  │                  │
    │── PutKey(K_ENC/K_MAC/K_RMAC) ───→│                  │
    │←── PutKey OK ───│                  │                  │
    │                                  │                  │
    │── SCP03 Close ──→                  │                  │
    │                                  │                  │
    │── SCP03(新密钥) →│                  │                  │
    │←── SCP03 OK ────│                  │                  │
    │                                  │                  │
    │───────────────────────────────────────── 记录 ────→│
    │←───────────────────────────────────────── ACK ←──│
```

---

## 4. 安全措施

### 4.1 HSM 安全要求

| 要求 | 说明 |
|:-----|:------|
| **物理安全** | HSM 在受控物理环境中运行，防篡改密封 |
| **访问控制** | 双人操作模式，需两人同时认证才能访问 Master Key |
| **密钥轮换** | Master Key 每 12 个月轮换一次，旧密钥保留 3 个月用于验证 |
| **审计日志** | 所有 HSM 操作记录到不可篡改审计日志 |
| **备份** | Master Key 分片备份 (Shamir Secret Sharing, 3-of-5) |

### 4.2 产线安全要求

| 要求 | 说明 |
|:-----|:------|
| **通信加密** | 产线编程器 ↔ HSM 之间使用 mTLS 或专用加密通道 |
| **物理隔离** | 密钥材料不在产线系统内存中明文驻留 |
| **废弃处理** | 注入失败的芯片物理销毁 (S32K3 + SE050 BGA 研磨) |
| **抽检验证** | 每批次抽取 0.1% 芯片，送独立安全实验室验证注入结果 |

### 4.3 验证测试

| 测试 | 方法 | 频率 |
|:-----|:------|:-----|
| 默认密钥拒绝 | 尝试用全零密钥建立 SCP03 | 每片 |
| 新密钥可连接 | 用注入密钥建立 SCP03 | 每片 |
| 密钥唯一性 | 随机抽查 1000 片，确认无重复派生密钥 | 每批次 |
| UID 一致性 | 芯片标签 UID 与 SE050 读取的 UID 一致 | 每片 |
| 渗透测试 | 模拟攻击者尝试侧信道提取密钥 | 每季度 |

---

## 5. 验证脚本 (嵌入式验证)

### 5.1 生产后验证 (在 S32K3 上运行)

```c
// se050_provision_verify.c — 产线验证固件
#include "se050_scp03.h"
#include "se05x_api.h"

bool verify_provision() {
    // 1. 尝试用默认密钥建立 SCP03 — 应该失败
    se05x_scp03_open(SE05X_AUTH_KEY_DEAULT);
    if (se05x_scp03_is_open()) {
        printf("❌ 默认密钥 SCP03 通道仍可建立！注入失败！\n");
        return false;
    }
    printf("✅ 默认密钥 SCP03 通道已拒绝\n");

    // 2. 用注入密钥建立 SCP03 — 应该成功
    se05x_scp03_open(SE05X_AUTH_KEY_PROD);
    if (!se05x_scp03_is_open()) {
        printf("❌ 生产密钥 SCP03 通道建立失败！\n");
        return false;
    }
    printf("✅ 生产密钥 SCP03 通道建立成功\n");

    // 3. 读取芯片 UID 与标签核对
    uint8_t uid[18];
    se05x_get_uid(uid);
    printf("✅ SE050 UID: %s\n", uid_to_hex_string(uid));

    // 4. 签名验证 — 用注入密钥签名并验证
    uint8_t test_data[] = "yuleDKCS_PROVISION_VERIFY_2026";
    uint8_t signature[64];
    se05x_sign(SE05X_KEY_SIGN, test_data, sizeof(test_data), signature);
    printf("✅ 签名验证通过\n");

    return true;
}
```

---

## 6. 实现文件

### 6.1 要新增/修改的文件

| 文件 | 动作 | 说明 |
|:-----|:------|:------|
| `embedded/ccc_protocol/include/se050_provision.h` | 新增 | 产线注入 API 声明 |
| `embedded/ccc_protocol/src/security/se050_provision.c` | 新增 | 产线注入实现 (~500 行) |
| `embedded/ccc_protocol/src/security/se050_scp03.c` | 修改 | 增加默认密钥拒绝逻辑 |
| `docs/security/se050-provision-guide.md` | 新增 | 产线操作员手册 |

### 6.2 se050_provision.h API

```c
#ifndef SE050_PROVISION_H
#define SE050_PROVISION_H

#include <stdint.h>
#include <stdbool.h>

// 产线注入结果
typedef struct {
    bool    success;
    uint8_t uid[18];
    uint8_t key_version;
    uint8_t checksum[32];  // SHA-256(UID || K_ENC_hash)
    char    error_msg[128];
} provision_result_t;

// 读取芯片 UID
bool provision_read_uid(uint8_t *uid, size_t uid_len);

// 执行密钥注入 (在产线 SCP03 通道上)
bool provision_inject_keys(const uint8_t *k_enc, const uint8_t *k_mac,
                           const uint8_t *k_rmac, provision_result_t *result);

// 验证注入结果
bool provision_verify(const uint8_t *expected_uid, provision_result_t *result);

// 关闭 SCP03 并清理
void provision_finalize(void);

#endif // SE050_PROVISION_H
```

---

## 7. 风险与应对

| 风险 | 影响 | 应对 |
|:-----|:------|:------|
| HSM Master Key 泄露 | 全批次芯片可被克隆 | Shamir 分片 + 双人操作 + 物理安全 |
| 产线 SCP03 被嗅探 | 该芯片密钥泄露 | 加密 I2C + 单次注入后默认密钥失效 |
| 注入过程中断电 | 芯片处于未知状态 | 增加写入确认步骤，失败重试 |
| 芯片物理销毁成本 | 废弃芯片成本 | 10-20 美元/片，纳入 BOM 成本核算 |

---

*方案完成。实现代码约 500 行 C，CI 验证脚本约 100 行 Python。*
