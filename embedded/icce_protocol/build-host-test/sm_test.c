/* SM 算法功能验证 — 主机平台编译测试 */
#define CONFIG_ENABLE_CRYPTO 1
#define USE_SM_CRYPTO 1
#define DEBUG_RNG 1

#include <stdio.h>
#include <string.h>
#include "../src/crypto/sm3.h"
#include "../src/crypto/sm4.h"
/* crypto_utils.h 会被 sm3.h/sm4.h 通过 crypto_types.h 包含 */

int main(void) {
    int failures = 0;

    /* ---- SM3 测试向量 (GB/T 32905-2016 附录 A) ---- */
    {
        const char *msg = "abc";
        uint8_t expected[32] = {
            0x66,0xC7,0xF0,0xF4,0x62,0xEE,0xED,0xD9,
            0xD1,0xF2,0xD4,0x6B,0xDC,0x10,0xE4,0xE2,
            0x41,0x67,0xC4,0x87,0x5C,0xF2,0xF7,0xA2,
            0x29,0x7D,0xA0,0x2B,0x8F,0x4B,0xA8,0xE0
        };
        uint8_t hash[32];
        sm3_hash((const uint8_t *)msg, 3, hash);
        if (memcmp(hash, expected, 32) == 0) {
            (void)printf("[PASS] SM3 test vector (abc)\n");
        } else {
            (void)printf("[FAIL] SM3 test vector (abc)\n");
            printf("  got:      "); for(int i=0;i<32;i++) printf("%02X",hash[i]); printf("\n");
            printf("  expected: "); for(int i=0;i<32;i++) printf("%02X",expected[i]); printf("\n");
            failures++;
        }
    }

    /* ---- SM3 长消息测试 ---- */
    {
        const char *msg64 = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd";
        uint8_t expected[32] = {
            0xDE,0xBE,0x9F,0xF9,0x22,0x75,0xB8,0xA1,
            0x38,0x60,0x48,0x89,0xC1,0x8E,0x5A,0x4D,
            0x6F,0xDB,0x70,0xE5,0x38,0x7E,0x57,0x65,
            0x29,0x3D,0xCB,0xA3,0x9C,0x0C,0x57,0x32
        };
        uint8_t hash[32];
        sm3_hash((const uint8_t *)msg64, 64, hash);
        if (memcmp(hash, expected, 32) == 0) {
            (void)printf("[PASS] SM3 test vector (64-byte block)\n");
        } else {
            (void)printf("[FAIL] SM3 test vector (64-byte block)\n");
            failures++;
        }
    }

    /* ---- SM4 ECB 测试向量 ---- */
    {
        uint8_t key[16] = {
            0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
            0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10
        };
        uint8_t pt[16] = {
            0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
            0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10
        };
        uint8_t expected[16] = {
            0x68,0x1E,0xDF,0x34,0xD2,0x06,0x96,0x5E,
            0x86,0xB3,0xE9,0x4F,0x53,0x6E,0x42,0x46
        };
        uint8_t ct[16], pt2[16];
        sm4_key_t sk;
        (void)sm4_set_key(key, &sk);
        (void)sm4_ecb_encrypt(&sk, pt, 16, ct);
        (void)sm4_ecb_decrypt(&sk, ct, 16, pt2);
        if (memcmp(ct, expected, 16) == 0 && memcmp(pt, pt2, 16) == 0) {
            (void)printf("[PASS] SM4 ECB encrypt/decrypt\n");
        } else {
            (void)printf("[FAIL] SM4 ECB encrypt/decrypt\n");
            failures++;
        }
    }

    /* ---- SM4-GCM 往返测试 ---- */
    {
        uint8_t key[16] = {0};
        uint8_t iv[12] = {0};
        uint8_t pt[32] = {0x01,0x02,0x03};
        uint8_t ct[32] = {0};
        uint8_t tag[16];
        uint8_t dec[32] = {0};
        int ret;

        ret = sm4_gcm_encrypt(key, iv, 12, NULL, 0, pt, 32, ct, tag, 16);
        if (ret != CRYPTO_SUCCESS) {
            (void)printf("[FAIL] SM4-GCM encrypt returned %d\n", ret);
            failures++;
        } else {
            ret = sm4_gcm_decrypt(key, iv, 12, NULL, 0, ct, 32, tag, 16, dec);
            if (ret != CRYPTO_SUCCESS) {
                (void)printf("[FAIL] SM4-GCM decrypt returned %d\n", ret);
                failures++;
            } else if (memcmp(pt, dec, 32) == 0) {
                (void)printf("[PASS] SM4-GCM round-trip (zero IV/key)\n");
            } else {
                (void)printf("[FAIL] SM4-GCM round-trip mismatch\n");
                failures++;
            }
        }
    }

    /* ---- SM3-HMAC 测试 ---- */
    {
        uint8_t key[16] = {0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
                           0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b};
        const char *msg = "Hi There";
        uint8_t mac[32];
        int ret = sm3_hmac(key, 16, (const uint8_t *)msg, 8, mac);
        if (ret == CRYPTO_SUCCESS) {
            (void)printf("[PASS] SM3-HMAC basic (no errors)\n");
        } else {
            (void)printf("[FAIL] SM3-HMAC returned %d\n", ret);
            failures++;
        }
    }

    (void)printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
