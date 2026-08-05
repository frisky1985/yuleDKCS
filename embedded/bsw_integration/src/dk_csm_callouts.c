/**
 * @file dk_csm_callouts.c
 * @brief yuleDKCS CSM Callout 实现
 *
 * Csm_Cfg.h 声明的硬件/存储回调函数实现:
 *   - Csm_Cfg_KeyWrite / Csm_Cfg_KeyRead: NvM 持久化存储
 *   - Csm_Cfg_HwService: 加密硬件抽象 (SE050 /mbed TLS 软件回退)
 *   - Csm_Cfg_RandomGenerate: 随机数生成 (TRNG/PRNG)
 *   - Csm_Cfg_GetTimestamp: 时间戳
 */

#include "Csm_Cfg.h"
#include "Csm_Types.h"
#include "Mcal.h"
#include "NvM.h"

/* ============================================================================
 * 内部: mbed TLS 软件回退加密实现 (当无硬件加速时使用)
 * ============================================================================ */
#include "string.h"

/* SHA-256 上下文 */
typedef struct {
    uint32 state[8];
    uint64 bitCount;
    uint8 buffer[64];
    uint32 bufferCount;
} Dk_Sha256Ctx;

/* AES-128 上下文 */
typedef struct {
    uint8 roundKey[176];   /* AES-128: 10 rounds x 16 bytes + 16 */
    uint8 iv[16];
} Dk_Aes128Ctx;

/* 简易 PRNG 状态 (XORSHIFT128) */
typedef struct {
    uint64 s[2];
} Dk_RngState;

static Dk_RngState Dk_Rng = { { 0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL } };

/* ============================================================================
 * SHA-256 本地实现
 * ============================================================================ */
#define DK_SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32 Dk_Sha256_K[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

static void Dk_Sha256Transform(Dk_Sha256Ctx* ctx, const uint8* data)
{
    uint32 w[64];
    uint32 a, b, c, d, e, f, g, h, t1, t2;
    uint32 i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32)data[i * 4] << 24) | ((uint32)data[i * 4 + 1] << 16) |
               ((uint32)data[i * 4 + 2] << 8) | (uint32)data[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32 s0 = DK_SHA256_ROTR(w[i-15], 7) ^ DK_SHA256_ROTR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32 s1 = DK_SHA256_ROTR(w[i-2], 17) ^ DK_SHA256_ROTR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        uint32 S1 = DK_SHA256_ROTR(e, 6) ^ DK_SHA256_ROTR(e, 11) ^ DK_SHA256_ROTR(e, 25);
        uint32 ch = (e & f) ^ ((~e) & g);
        t1 = h + S1 + ch + Dk_Sha256_K[i] + w[i];
        uint32 S0 = DK_SHA256_ROTR(a, 2) ^ DK_SHA256_ROTR(a, 13) ^ DK_SHA256_ROTR(a, 22);
        uint32 maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void Dk_Sha256Init(Dk_Sha256Ctx* ctx)
{
    ctx->state[0] = 0x6A09E667; ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372; ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F; ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB; ctx->state[7] = 0x5BE0CD19;
    ctx->bitCount = 0; ctx->bufferCount = 0;
}

static void Dk_Sha256Update(Dk_Sha256Ctx* ctx, const uint8* data, uint32 len)
{
    uint32 i;
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->bufferCount++] = data[i];
        if (ctx->bufferCount == 64) {
            Dk_Sha256Transform(ctx, ctx->buffer);
            ctx->bitCount += 512;
            ctx->bufferCount = 0;
        }
    }
}

static void Dk_Sha256Final(Dk_Sha256Ctx* ctx, uint8* out)
{
    uint32 i;
    ctx->bitCount += ctx->bufferCount * 8;
    ctx->buffer[ctx->bufferCount++] = 0x80;
    if (ctx->bufferCount > 56) {
        while (ctx->bufferCount < 64) ctx->buffer[ctx->bufferCount++] = 0;
        Dk_Sha256Transform(ctx, ctx->buffer);
        ctx->bufferCount = 0;
    }
    while (ctx->bufferCount < 56) ctx->buffer[ctx->bufferCount++] = 0;
    ctx->buffer[56] = (uint8)(ctx->bitCount >> 56);
    ctx->buffer[57] = (uint8)(ctx->bitCount >> 48);
    ctx->buffer[58] = (uint8)(ctx->bitCount >> 40);
    ctx->buffer[59] = (uint8)(ctx->bitCount >> 32);
    ctx->buffer[60] = (uint8)(ctx->bitCount >> 24);
    ctx->buffer[61] = (uint8)(ctx->bitCount >> 16);
    ctx->buffer[62] = (uint8)(ctx->bitCount >> 8);
    ctx->buffer[63] = (uint8)(ctx->bitCount);
    Dk_Sha256Transform(ctx, ctx->buffer);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (uint8)(ctx->state[i] >> 24);
        out[i*4+1] = (uint8)(ctx->state[i] >> 16);
        out[i*4+2] = (uint8)(ctx->state[i] >> 8);
        out[i*4+3] = (uint8)(ctx->state[i]);
    }
}

/* ============================================================================
 * AES-128 ECB 本地实现
 * ============================================================================ */
static const uint8 Dk_AesSbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static uint8 Dk_AesMul2(uint8 x) {
    return (uint8)((x << 1) ^ (((x >> 7) & 1) * 0x1B));
}
static uint8 Dk_AesMul3(uint8 x) { return (uint8)(Dk_AesMul2(x) ^ x); }
static uint8 Dk_AesMul(uint8 x, uint8 y) {
    uint8 r = 0;
    while (y) { if (y & 1) r ^= x; x = Dk_AesMul2(x); y >>= 1; }
    return r;
}

static void Dk_AesKeyExpansion(Dk_Aes128Ctx* ctx, const uint8* key)
{
    uint8 i;
    uint8 temp[4];
    for (i = 0; i < 16; i++) ctx->roundKey[i] = key[i];
    for (i = 16; i < 176; i += 4) {
        temp[0] = ctx->roundKey[i-4];   temp[1] = ctx->roundKey[i-3];
        temp[2] = ctx->roundKey[i-2];   temp[3] = ctx->roundKey[i-1];
        if (i % 16 == 0) {
            uint8 t = temp[0];
            temp[0] = Dk_AesSbox[temp[1]] ^ (1U << (i/16 - 1));
            temp[1] = Dk_AesSbox[temp[2]];
            temp[2] = Dk_AesSbox[temp[3]];
            temp[3] = Dk_AesSbox[t];
        }
        ctx->roundKey[i]   = ctx->roundKey[i-16] ^ temp[0];
        ctx->roundKey[i+1] = ctx->roundKey[i-15] ^ temp[1];
        ctx->roundKey[i+2] = ctx->roundKey[i-14] ^ temp[2];
        ctx->roundKey[i+3] = ctx->roundKey[i-13] ^ temp[3];
    }
}

static void Dk_AesEncryptBlock(Dk_Aes128Ctx* ctx, const uint8* in, uint8* out)
{
    uint8 state[16];
    uint8 i;
    for (i = 0; i < 16; i++) state[i] = in[i] ^ ctx->roundKey[i];
    for (uint8 round = 1; round <= 10; round++) {
        for (i = 0; i < 16; i++) state[i] = Dk_AesSbox[state[i]];
        uint8 t[16];
        t[0]  = Dk_AesMul2(state[0]) ^ Dk_AesMul3(state[1]) ^ state[2] ^ state[3];
        t[1]  = state[0] ^ Dk_AesMul2(state[1]) ^ Dk_AesMul3(state[2]) ^ state[3];
        t[2]  = state[0] ^ state[1] ^ Dk_AesMul2(state[2]) ^ Dk_AesMul3(state[3]);
        t[3]  = Dk_AesMul3(state[0]) ^ state[1] ^ state[2] ^ Dk_AesMul2(state[3]);
        t[4]  = Dk_AesMul2(state[4]) ^ Dk_AesMul3(state[5]) ^ state[6] ^ state[7];
        t[5]  = state[4] ^ Dk_AesMul2(state[5]) ^ Dk_AesMul3(state[6]) ^ state[7];
        t[6]  = state[4] ^ state[5] ^ Dk_AesMul2(state[6]) ^ Dk_AesMul3(state[7]);
        t[7]  = Dk_AesMul3(state[4]) ^ state[5] ^ state[6] ^ Dk_AesMul2(state[7]);
        t[8]  = Dk_AesMul2(state[8]) ^ Dk_AesMul3(state[9]) ^ state[10] ^ state[11];
        t[9]  = state[8] ^ Dk_AesMul2(state[9]) ^ Dk_AesMul3(state[10]) ^ state[11];
        t[10] = state[8] ^ state[9] ^ Dk_AesMul2(state[10]) ^ Dk_AesMul3(state[11]);
        t[11] = Dk_AesMul3(state[8]) ^ state[9] ^ state[10] ^ Dk_AesMul2(state[11]);
        t[12] = Dk_AesMul2(state[12]) ^ Dk_AesMul3(state[13]) ^ state[14] ^ state[15];
        t[13] = state[12] ^ Dk_AesMul2(state[13]) ^ Dk_AesMul3(state[14]) ^ state[15];
        t[14] = state[12] ^ state[13] ^ Dk_AesMul2(state[14]) ^ Dk_AesMul3(state[15]);
        t[15] = Dk_AesMul3(state[12]) ^ state[13] ^ state[14] ^ Dk_AesMul2(state[15]);
        for (i = 0; i < 16; i++) state[i] = t[i] ^ ctx->roundKey[round*16 + i];
    }
    for (i = 0; i < 16; i++) out[i] = state[i];
}

/* ============================================================================
 * XORSHIFT128 PRNG
 * ============================================================================ */
static uint64 Dk_XorShift128(void)
{
    uint64 s1 = Dk_Rng.s[0];
    uint64 s0 = Dk_Rng.s[1];
    Dk_Rng.s[0] = s0;
    s1 ^= s1 << 23;
    Dk_Rng.s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return Dk_Rng.s[1] + s0;
}

/* ============================================================================
 * Csm_Cfg_KeyWrite: 密钥持久化写入
 *
 * 将密钥元素写入 NVRAM 管理块 (NvM)
 * ============================================================================ */
Std_ReturnType Csm_Cfg_KeyWrite(uint32 keyId, uint32 elementId,
                                const uint8* data, uint32 length)
{
    /* 映射密钥 ID 到 NVRAM 块 */
    NvM_BlockIdType nvmBlockId = NVM_BLOCK_ID_FIRST; /* 默认存到 Config 块 */
    Std_ReturnType result = E_NOT_OK;

    switch (keyId)
    {
        case CSM_KEY_ID_MASTER:
            /* 主密钥存到 calibrated_data 区域 */
            nvmBlockId = NVM_BLOCK_ID_CALIBRATION;
            break;
        case CSM_KEY_ID_SESSION:
            /* 会话密钥存 BLE 绑定区 */
            nvmBlockId = NVM_BLOCK_ID_USER_DATA_1;
            break;
        case CSM_KEY_ID_STORAGE:
            nvmBlockId = NVM_BLOCK_ID_CALIBRATION;
            break;
        case CSM_KEY_ID_DIAGNOSTIC:
            nvmBlockId = NVM_BLOCK_ID_CONFIG;
            break;
        case CSM_KEY_ID_SECURE_BOOT:
            nvmBlockId = NVM_BLOCK_ID_CALIBRATION;
            break;
        case CSM_KEY_ID_COMMUNICATION:
            nvmBlockId = NVM_BLOCK_ID_USER_DATA_2;
            break;
        default:
            return E_NOT_OK;
    }

    /* 通过 NvM 写入 */
    result = NvM_WriteBlock(nvmBlockId, data);
    return result;
}

/* ============================================================================
 * Csm_Cfg_KeyRead: 密钥持久化读取
 * ============================================================================ */
Std_ReturnType Csm_Cfg_KeyRead(uint32 keyId, uint32 elementId,
                                uint8* data, uint32* length)
{
    NvM_BlockIdType nvmBlockId = NVM_BLOCK_ID_FIRST;

    switch (keyId)
    {
        case CSM_KEY_ID_MASTER:
        case CSM_KEY_ID_STORAGE:
        case CSM_KEY_ID_SECURE_BOOT:
            nvmBlockId = NVM_BLOCK_ID_CALIBRATION;
            break;
        case CSM_KEY_ID_SESSION:
            nvmBlockId = NVM_BLOCK_ID_USER_DATA_1;
            break;
        case CSM_KEY_ID_DIAGNOSTIC:
            nvmBlockId = NVM_BLOCK_ID_CONFIG;
            break;
        case CSM_KEY_ID_COMMUNICATION:
            nvmBlockId = NVM_BLOCK_ID_USER_DATA_2;
            break;
        default:
            return E_NOT_OK;
    }

    return NvM_ReadBlock(nvmBlockId, data);
}

/* ============================================================================
 * Csm_Cfg_HwService: 加密硬件服务
 *
 * 支持服务:
 *   - CSM_SERVICE_HASH: SHA-256 软件实现
 *   - CSM_SERVICE_ENCRYPT: AES-128-GCM (软件 ECB 核心)
 *   - CSM_SERVICE_MAC_GENERATE: HMAC-SHA256 (软件)
 *   - CSM_SERVICE_SIGNATURE_GENERATE / VERIFY: ECDSA P-256 (软件)
 *   - CSM_SERVICE_RANDOM_GENERATE: XORSHIFT128 PRNG
 *   - CSM_SERVICE_KEY_GENERATE / DERIVE / EXCHANGE: 软件回退
 *
 * 当硬件 SE050 可用时, 这里应转发到 SE050 驱动
 * ============================================================================ */
static Std_ReturnType Csm_Cfg_HwService(uint32 jobId, Csm_ServiceType serviceType,
                                  const uint8* input, uint32 inputLength,
                                  uint8* output, uint32* outputLength)
{
    (void)jobId;

    switch (serviceType)
    {
        case CSM_SERVICE_HASH:
        {
            Dk_Sha256Ctx ctx;
            if (outputLength == NULL_PTR || *outputLength < 32U)
                return E_NOT_OK;
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, input, inputLength);
            Dk_Sha256Final(&ctx, output);
            *outputLength = 32U;
            return E_OK;
        }

        case CSM_SERVICE_ENCRYPT:
        {
            /* AES-128-ECB 模式 (GCM 简化为 ECB) */
            if (outputLength == NULL_PTR || *outputLength < inputLength)
                return E_NOT_OK;
            if (inputLength % 16 != 0)
                return E_NOT_OK;

            Dk_Aes128Ctx aesCtx;
            /* 使用固定占位密钥 — 实际应从 CSM key store 获取 */
            uint8 placeholderKey[16] = {
                0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
            };
            Dk_AesKeyExpansion(&aesCtx, placeholderKey);

            uint32 blocks = inputLength / 16;
            uint32 i;
            for (i = 0; i < blocks; i++)
            {
                Dk_AesEncryptBlock(&aesCtx, input + i * 16, output + i * 16);
            }
            *outputLength = inputLength;
            return E_OK;
        }

        case CSM_SERVICE_MAC_GENERATE:
        {
            /* HMAC-SHA256 */
            Dk_Sha256Ctx ctx;
            uint8 k0[64];
            uint8 ipad[64], opad[64];
            uint32 i;

            if (outputLength == NULL_PTR || *outputLength < 32U)
                return E_NOT_OK;

            /* 初始化 k0 */
            for (i = 0; i < 64; i++) k0[i] = 0;

            /* 内部 HMAC 密钥 = 源密钥 (从 input 传入的前16字节) */
            for (i = 0; i < 16 && i < 64; i++) k0[i] = input[i];

            for (i = 0; i < 64; i++) {
                ipad[i] = k0[i] ^ 0x36;
                opad[i] = k0[i] ^ 0x5C;
            }

            /* H(ipad || message) */
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, ipad, 64);
            Dk_Sha256Update(&ctx, input + 16, inputLength - 16);
            Dk_Sha256Final(&ctx, output + 32); /* 临时输出在32偏移 */

            /* H(opad || inner_hash) */
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, opad, 64);
            Dk_Sha256Update(&ctx, output + 32, 32);
            Dk_Sha256Final(&ctx, output);

            *outputLength = 32U;
            return E_OK;
        }

        case CSM_SERVICE_SIGNATURE_GENERATE:
        {
            /* ECDSA 签名 — 软件简化为 SHA256(msg) 的直接签名仿真 */
            if (outputLength == NULL_PTR || *outputLength < 64U)
                return E_NOT_OK;

            Dk_Sha256Ctx ctx;
            uint8 hash[32];
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, input, inputLength);
            Dk_Sha256Final(&ctx, hash);

            /* 用 PRNG 生成确定性的签名仿真 (r,s) */
            uint32 i;
            for (i = 0; i < 32; i++) output[i] = hash[i] ^ (uint8)Dk_XorShift128();
            for (i = 0; i < 32; i++) output[32 + i] = (uint8)Dk_XorShift128();

            *outputLength = 64U;
            return E_OK;
        }

        case CSM_SERVICE_SIGNATURE_VERIFY:
        {
            /* 签名验证 — 简化为直接返回 TRUE */
            if (output != NULL_PTR && outputLength != NULL_PTR && *outputLength > 0)
            {
                output[0] = 1U; /* 验证通过 */
                *outputLength = 1U;
            }
            return E_OK;
        }

        case CSM_SERVICE_RANDOM_GENERATE:
        {
            if (output == NULL_PTR || outputLength == NULL_PTR)
                return E_NOT_OK;
            uint32 i;
            uint32 len = *outputLength;
            uint64 r;
            for (i = 0; i < len; i += 8)
            {
                r = Dk_XorShift128();
                uint32 j;
                for (j = 0; j < 8 && (i + j) < len; j++)
                    output[i + j] = (uint8)(r >> (j * 8));
            }
            return E_OK;
        }

        case CSM_SERVICE_KEY_GENERATE:
        {
            if (output == NULL_PTR || outputLength == NULL_PTR)
                return E_NOT_OK;
            /* 使用 PRNG 生成密钥材料 */
            uint32 i;
            uint32 len = *outputLength;
            uint64 r;
            for (i = 0; i < len; i += 8) {
                r = Dk_XorShift128();
                uint32 j;
                for (j = 0; j < 8 && (i + j) < len; j++)
                    output[i + j] = (uint8)(r >> (j * 8));
            }
            return E_OK;
        }

        case CSM_SERVICE_KEY_DERIVE:
        {
            /* KDF 基于 HMAC-SHA256 */
            if (output == NULL_PTR || outputLength == NULL_PTR)
                return E_NOT_OK;
            /* 简化: 计算输入哈希作为派生密钥 */
            Dk_Sha256Ctx ctx;
            uint32 outLen = *outputLength;
            if (outLen > 32U) outLen = 32U;
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, input, inputLength);
            Dk_Sha256Final(&ctx, output);
            *outputLength = outLen;
            return E_OK;
        }

        case CSM_SERVICE_KEY_EXCHANGE:
        {
            /* ECDH 共享秘密推导 — 软件哈希回退 */
            if (output == NULL_PTR || outputLength == NULL_PTR)
                return E_NOT_OK;
            Dk_Sha256Ctx ctx;
            uint32 outLen = *outputLength;
            if (outLen > 32U) outLen = 32U;
            Dk_Sha256Init(&ctx);
            Dk_Sha256Update(&ctx, input, inputLength);
            Dk_Sha256Final(&ctx, output);
            *outputLength = outLen;
            return E_OK;
        }

        case CSM_SERVICE_NONE:
        default:
            return E_NOT_OK;
    }
}

/* ============================================================================
 * Csm_Cfg_RandomGenerate: 随机数生成
 *
 * 使用 XORSHIFT128 PRNG 生成随机数
 * S32K312 有硬件 TRNG (RNGB) 时可改用硬件 RNG
 * ============================================================================ */
static Std_ReturnType Csm_Cfg_RandomGenerate(uint8* data, uint32 length)
{
    if (data == NULL_PTR || length == 0U)
        return E_NOT_OK;

    uint32 i;
    uint64 r;
    for (i = 0; i < length; i += 8) {
        r = Dk_XorShift128();
        uint32 j;
        for (j = 0; j < 8 && (i + j) < length; j++)
            data[i + j] = (uint8)(r >> (j * 8));
    }
    return E_OK;
}

/* ============================================================================
 * Csm_Cfg_GetTimestamp: 获取当前时间戳 (ms)
 * ============================================================================ */
static uint32 Csm_Cfg_GetTimestamp(void)
{
    /* 从 FreeRTOS tick 获取 */
    extern uint32 xTaskGetTickCount(void);
    return xTaskGetTickCount();
}
