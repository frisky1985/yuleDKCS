# yuleDKCS 密码学库集成指南

## 架构概览

```
embedded/sdk/
├── mbedtls/                  # mbedtls 3.6.2 源码 (精简配置)
│   ├── include/              # 头文件
│   ├── library/              # 源码
│   └── mbedtls_config.h      # 自定义配置 (仅启用必需模块)
├── sm/                       # 国密算法模块
│   ├── include/
│   │   ├── sm2.h             # SM2 公钥密码 (基于 mbedtls ECP/MPI)
│   │   ├── sm3.h             # SM3 密码杂凑算法 (纯 C)
│   │   └── sm4.h             # SM4 分组密码算法 (纯 C)
│   └── src/
│       ├── sm2.c             # SM2 实现 (~400行)
│       ├── sm3.c             # SM3 实现 (~250行)
│       └── sm4.c             # SM4 实现 (~280行)
├── CMakeLists.txt            # 构建配置
└── build_crypto.sh           # 交叉编译脚本
```

## 与 yuleASR third_party 的关系

```
┌──────────────────────────────────────────────────────────────┐
│                    yuleDKCS 项目                              │
│  ┌──────────────────────────────────────────────────────────┐│
│  │ embedded/sdk/      独立密码学库, 仅供 yuleDKCS 使用      ││
│  │ mbedtls → yuleDKCS 定制版 (精简配置, 仅 ECDSA/SHA256)    ││
│  │ sm/     → 自研国密模块 (SM2/SM3/SM4)                    ││
│  └──────────────────────────────────────────────────────────┘│
│                                                              │
│                    ┆ 各自独立, 互不冲突                        │
│                    ┆                                          │
│  ┌──────────────────────────────────────────────────────────┐│
│  │ yuleASR/third_party/  yuleASR 平台的第三方依赖            ││
│  │ 可能包含完整版 mbedtls、FreeRTOS、lwIP 等                ││
│  └──────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────┘
```

**关键原则**:

1. **独立性** — `embedded/sdk/` 属于 yuleDKCS, 是数字钥匙应用的密码学库；
   `yuleASR/third_party/` 属于 yuleASR 平台, 为 BSW 提供通用依赖。

2. **版本差异** — 两者可能使用不同版本的 mbedtls:
   - yuleDKCS 使用精简版 mbedtls (仅 ECDSA/SHA256/ASN.1)
   - yuleASR 可能使用完整版 mbedtls (含 TLS 协议、X.509 完整解析等)

3. **链接安全** — 两个 mbedtls 编译时使用不同的 `MBEDTLS_CONFIG_FILE`,
   符号通过静态库隔离, 不存在符号冲突。

4. **升级同步** — 若需要统一版本, 可将 `embedded/sdk/mbedtls` 替换为
   指向 `yuleASR/third_party/mbedtls` 的符号链接 (备选方案)。

## 依赖关系

```
┌────────────────────────────────────────────┐
│             应用层 (数字钥匙)                │
├────────────────────────────────────────────┤
│  ICCE (国密)          CCC/ICCOA (国际)     │
│  ┌────────────────┐  ┌──────────────────┐  │
│  │ sm2_verify()   │  │ mbedtls_ecdsa()  │  │
│  │ sm3_digest()   │  │ mbedtls_sha256() │  │
│  │ sm4_encrypt()  │  │ mbedtls_pk*()   │  │
│  └───────┬────────┘  └────────┬─────────┘  │
│          │                    │             │
├──────────┴────────────────────┴─────────────┤
│  mbedtls ECP + MPI (数学层)                 │
│  mbedtls SHA-256 + AES + ASN.1              │
├────────────────────────────────────────────┤
│  SE050 (硬件加速) → mbedtls PK 桥接层       │
└────────────────────────────────────────────┘
```

## 编译步骤

### 前置条件

1. ARM GCC 工具链 (gcc-arm-none-eabi-10.3+)
2. CMake 3.14+

### 快速编译

```bash
# 1. 下载工具链 (首次)
wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
tar xf gcc-arm-none-eabi-*.tar.bz2 -C /opt/

# 2. 编译密码学库
cd embedded/sdk
TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-10.3-2021.10 bash build_crypto.sh

# 输出:
#   output/libmbedcrypto.a   (~120KB)
#   output/libsm_crypto.a    (~12KB)
```

### 集成到主项目 CMake

```cmake
# 在 embedded/CMakeLists.txt 中添加:
add_subdirectory(sdk)

target_link_libraries(yuledkcs_fw
    sm_crypto       # SM2/SM3/SM4 国密
    mbedcrypto      # mbedtls ECDSA/SHA256/ASN.1
)
```

## API 参考

### SM3 哈希

```c
#include "sm3.h"

uint8_t digest[32];
sm3_digest(data, data_len, digest);

// 流式处理:
sm3_context_t ctx;
sm3_init(&ctx);
sm3_update(&ctx, part1, len1);
sm3_update(&ctx, part2, len2);
sm3_finish(&ctx, digest);
```

### SM2 签名验证

```c
#include "sm2.h"

// 验证签名 (最常用)
int ret = sm2_verify(pubkey_65bytes, digest_32bytes, signature_64bytes);
// ret == 0: 签名有效
// ret == -1: 签名无效
// ret == -2: 参数错误

// 生成密钥对
sm2_keypair_t kp;
sm2_generate_keypair(&kp);

// 签名 (仅用于测试/后端密钥生成)
sm2_sign(&kp, digest, signature);
```

### SM4 加密

```c
#include "sm4.h"

// ECB 模式
uint8_t out[16];
sm4_context_t ctx;
sm4_init(&ctx, key, 16, SM4_MODE_ECB, NULL);
sm4_encrypt(&ctx, plaintext, out, 16);

// CBC 模式 (带 PKCS7 填充)
uint8_t ct[256];
size_t ct_len;
sm4_cbc_encrypt_pkcs7(key, iv, data, data_len, ct, &ct_len);
```

### mbedtls ECDSA P-256 (用于 ICCOA/CCC)

```c
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>

// SHA-256 哈希
mbedtls_sha256(data, data_len, digest, 0);

// ECDSA P-256 验证 (使用 SE050 或软件)
mbedtls_ecdsa_context ecdsa;
mbedtls_ecdsa_init(&ecdsa);
mbedtls_ecp_group_load(&ecdsa.grp, MBEDTLS_ECP_DP_SECP256R1);
mbedtls_ecp_point_read_binary(&ecdsa.grp, &ecdsa.Q, pubkey, pubkey_len);
mbedtls_ecdsa_read_signature(&ecdsa, digest, sig, sig_len);
```

## 体积分析

| 模块 | Flash | SRAM (缓冲) | 作用 |
|------|-------|------------|------|
| mbedtls (精简) | ~120 KB | ~8 KB | ECDSA/SHA256/ASN.1/SE050 |
| sm2 | ~8 KB | ~2 KB | SM2 验签/密钥生成 |
| sm3 | ~4 KB | ~0.5 KB | SM3 哈希 |
| sm4 | ~4 KB | ~0.5 KB | SM4 加密 |
| **总计** | **~136 KB** | **~11 KB** | |

KW47 Flash: 2 MB → 仅占 **6.8%**
KW47 SRAM: 512 KB → 仅占 **2.1%**

## 升级指南

后续如果 mbedtls 主线增加 SM2 支持，只需:

```bash
# 1. 替换源码
rm -rf sdk/mbedtls
cp -r mbedtls-new-version sdk/mbedtls

# 2. 移除 sm2.c 改用 mbedtls 原生
# 在 CMakeLists.txt 中注释掉 sm2.c

# 3. 重新编译
bash build_crypto.sh
```
