/*
 * test_ccc_auth.c — 验证 CCC STATE_AUTH_PROCESS 闭环
 *
 * 流程: ccc_dk_init → 生成证明/签名 → ccc_dk_set_auth_data →
 *       ccc_dk_run (触发 AUTH_PROCESS) → 验证状态转换
 *
 * 成功路径: 证书链OK + 证明OK + 签名OK → STATE_UNLOCKED
 * 失败路径: 签名篡改 → STATE_LOCKED
 */

#include "ccc_digital_key.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else      { g_fail++; printf("  FAIL: %s (state=%d)\n", msg, ccc_dk_get_state()); } \
} while (0)

int main(void)
{
    printf("=== CCC STATE_AUTH_PROCESS 闭环测试 ===\n");

    /* ── 1. 初始化 ── */
    printf("\n[1] ccc_dk_init\n");
    ccc_status_t st = ccc_dk_init();
    CHECK(st == CCC_OK, "ccc_dk_init 成功");
    CHECK(ccc_dk_get_state() == STATE_STANDBY, "初始状态 = STATE_STANDBY");

    /* ── 2. 生成有效认证材料 ── */
    printf("\n[2] 生成认证材料 (证明 + 签名)\n");

    /* 生成设备证明 */
    ccc_attestation_t att;
    st = sec_attestation(&att);
    CHECK(st == CCC_OK, "sec_attestation 成功");

    /* 对认证数据签名 (使用设备私钥) */
    const uint8_t auth_data[] = "ccc-auth-challenge-0001";
    uint8_t auth_sig[64];
    uint32_t sig_len = 0;
    st = sec_sign(auth_data, sizeof(auth_data), auth_sig, &sig_len);
    CHECK(st == CCC_OK && sig_len == 64, "sec_sign 生成 64 字节签名");

    /* 伪造证书链 (sec_verify_cert_chain 简化实现返回 OK) */
    uint8_t cert_chain[64];
    memset(cert_chain, 0xAA, sizeof(cert_chain));
    uint8_t root_pub[64];
    memset(root_pub, 0xCD, sizeof(root_pub));

    /* ── 3. 成功路径: 提交有效数据 → UNLOCKED ── */
    printf("\n[3] 成功路径: 有效认证 → STATE_UNLOCKED\n");

    /* 模拟 BLE 连接 */
    ccc_dk_set_auth_data(cert_chain, sizeof(cert_chain),
                         root_pub, sizeof(root_pub),
                         &att,
                         auth_data, sizeof(auth_data),
                         auth_sig, sig_len);
    /* 此时状态应为 STATE_AUTH_PROCESS */
    CHECK(ccc_dk_get_state() == STATE_AUTH_PROCESS, "set_auth_data 后进入 STATE_AUTH_PROCESS");

    /* 运行状态机: 验钥→证明→签名→UWB(失败回退)→UNLOCKED */
    st = ccc_dk_run();
    CHECK(st == CCC_OK, "ccc_dk_run 成功");
    /* UWB 在 host 模式返回 CCC_ERR_NOT_FOUND, 回退到 UNLOCKED */
    CHECK(ccc_dk_get_state() == STATE_UNLOCKED, "认证成功 → STATE_UNLOCKED");

    /* ── 4. 失败路径: 篡改签名 → LOCKED ── */
    printf("\n[4] 失败路径: 篡改签名 → STATE_LOCKED\n");

    /* 重新初始化回到 STANDBY */
    ccc_dk_deinit();
    ccc_dk_init();
    CHECK(ccc_dk_get_state() == STATE_STANDBY, "重新初始化后回到 STANDBY");

    /* 篡改签名 */
    uint8_t bad_sig[64];
    memcpy(bad_sig, auth_sig, 64);
    bad_sig[0] ^= 0xFF;

    ccc_dk_set_auth_data(cert_chain, sizeof(cert_chain),
                         root_pub, sizeof(root_pub),
                         &att,
                         auth_data, sizeof(auth_data),
                         bad_sig, 64);
    CHECK(ccc_dk_get_state() == STATE_AUTH_PROCESS, "提交数据后进入 STATE_AUTH_PROCESS");

    ccc_dk_run();
    CHECK(ccc_dk_get_state() == STATE_LOCKED, "签名篡改 → STATE_LOCKED");

    /* ── 5. 失败路径: 篡改证明 → LOCKED ── */
    printf("\n[5] 失败路径: 篡改证明 → STATE_LOCKED\n");

    ccc_dk_deinit();
    ccc_dk_init();

    ccc_attestation_t bad_att = att;
    bad_att.firmware_hash[0] ^= 0xFF;  /* 篡改固件哈希 */

    ccc_dk_set_auth_data(cert_chain, sizeof(cert_chain),
                         root_pub, sizeof(root_pub),
                         &bad_att,
                         auth_data, sizeof(auth_data),
                         auth_sig, sig_len);
    ccc_dk_run();
    CHECK(ccc_dk_get_state() == STATE_LOCKED, "证明篡改 → STATE_LOCKED");

    /* ── 清理 ── */
    ccc_dk_deinit();

    /* ── 汇总 ── */
    printf("\n=== 结果: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
