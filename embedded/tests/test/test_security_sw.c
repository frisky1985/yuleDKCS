/*
 * test_security_sw.c — 验证 sec_sign/sec_verify/sec_attestation 软件回退路径
 *
 * 链接 ccc_dk + icce_dk (crypto_engine) + hal (host stubs).
 * 验证:
 *   1. sec_sign 产生 64 字节 ECDSA 签名
 *   2. sec_verify 对正确签名返回 VERIFY_OK
 *   3. sec_verify 对篡改签名返回 VERIFY_SIGN_INVALID
 *   4. sec_attestation 生成完整证明结构
 *   5. sec_verify_attestation 对合法证明返回 VERIFY_OK
 *   6. sec_verify_cert_chain 基本参数校验
 */

#include "ccc_digital_key.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else      { g_fail++; printf("  FAIL: %s\n", msg); } \
} while (0)

int main(void)
{
    printf("=== sec_sign / sec_verify / sec_attestation 软件回退测试 ===\n");

    /* ── 1. sec_init ── */
    printf("\n[1] sec_init\n");
    ccc_status_t st = sec_init();
    CHECK(st == CCC_OK, "sec_init 成功");

    /* ── 2. sec_sign + sec_verify round-trip ── */
    printf("\n[2] sec_sign / sec_verify\n");
    const uint8_t msg[] = "hello digital key";
    uint8_t sig[64];
    uint32_t sig_len = 0;

    st = sec_sign(msg, sizeof(msg), sig, &sig_len);
    CHECK(st == CCC_OK, "sec_sign 返回 CCC_OK");
    CHECK(sig_len == 64, "签名长度为 64 字节");

    verify_result_e vr = sec_verify(msg, sizeof(msg), sig, sig_len);
    CHECK(vr == VERIFY_OK, "sec_verify 对正确签名返回 VERIFY_OK");

    /* 篡改签名后应验证失败 */
    uint8_t bad_sig[64];
    memcpy(bad_sig, sig, 64);
    bad_sig[0] ^= 0xFF;
    vr = sec_verify(msg, sizeof(msg), bad_sig, 64);
    CHECK(vr == VERIFY_SIGN_INVALID, "sec_verify 对篡改签名返回 VERIFY_SIGN_INVALID");

    /* ── 3. sec_attestation + sec_verify_attestation ── */
    printf("\n[3] sec_attestation / sec_verify_attestation\n");
    ccc_attestation_t att;
    st = sec_attestation(&att);
    CHECK(st == CCC_OK, "sec_attestation 返回 CCC_OK");
    CHECK(att.version == 0x02, "attestation version = 0x02");
    CHECK(att.security_state == 0x01, "security_state = SECURE");
    CHECK(att.signature[0] != 0 || att.signature[63] != 0, "签名非全零");

    vr = sec_verify_attestation(&att);
    CHECK(vr == VERIFY_OK, "sec_verify_attestation 对合法证明返回 VERIFY_OK");

    /* 篡改 firmware_hash 后应检测到篡改 */
    ccc_attestation_t att_tampered = att;
    att_tampered.firmware_hash[0] ^= 0xFF;
    vr = sec_verify_attestation(&att_tampered);
    CHECK(vr == VERIFY_FW_TAMPERED, "篡改 firmware_hash 返回 VERIFY_FW_TAMPERED");

    /* ── 4. sec_verify_cert_chain ── */
    printf("\n[4] sec_verify_cert_chain\n");
    uint8_t dummy_chain[128];
    memset(dummy_chain, 0xAB, sizeof(dummy_chain));
    uint8_t root_pub[64];
    memset(root_pub, 0xCD, sizeof(root_pub));

    vr = sec_verify_cert_chain(dummy_chain, sizeof(dummy_chain), root_pub, sizeof(root_pub));
    CHECK(vr == VERIFY_OK, "sec_verify_cert_chain 对有效参数返回 VERIFY_OK");

    vr = sec_verify_cert_chain(NULL, 100, root_pub, sizeof(root_pub));
    CHECK(vr == VERIFY_CERT_INVALID, "sec_verify_cert_chain 对 NULL chain 返回 VERIFY_CERT_INVALID");

    /* ── 5. sec_deinit ── */
    printf("\n[5] sec_deinit\n");
    st = sec_deinit();
    CHECK(st == CCC_OK, "sec_deinit 返回 CCC_OK");

    /* ── 汇总 ── */
    printf("\n=== 结果: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
