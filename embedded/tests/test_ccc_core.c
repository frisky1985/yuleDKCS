/******************************************************************************
 * @file    test_ccc_core.c
 * @brief   CCC Digital Key R3 核心测试
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ccc.h"

/* 模拟 SE 接口 */
static int mock_se_init(void) { return 0; }
static int mock_se_deinit(void) { return 0; }
static int mock_gen_key(uint8_t *pub, uint8_t *priv) {
    memset(pub, 0xAA, 65);
    memset(priv, 0xBB, 32);
    return 0;
}
static int mock_sign(const uint8_t *d, size_t l, const uint8_t *k, uint8_t *s) {
    memset(s, 0xCC, 64);
    return 0;
}
static int mock_verify(const uint8_t *d, size_t l, const uint8_t *k, const uint8_t *s) {
    return 1;
}
static int mock_derive(const uint8_t *p, const uint8_t *k, uint8_t *s) {
    memset(s, 0xDD, 32);
    return 0;
}

void test_ccc_init_deinit(void)
{
    printf("Testing ccc_init/deinit...\n");
    
    ccc_se_interface_t se = {
        .init = mock_se_init,
        .deinit = mock_se_deinit,
        .generate_key_pair = mock_gen_key,
        .sign = mock_sign,
        .verify = mock_verify,
        .derive_shared_secret = mock_derive,
    };
    
    error_t ret = ccc_init(&se);
    assert(ret == OK);
    
    ret = ccc_deinit();
    assert(ret == OK);
    
    printf("  PASSED\n");
}

void test_ccc_pairing_session(void)
{
    printf("Testing ccc pairing session...\n");
    
    ccc_se_interface_t se = {
        .init = mock_se_init,
        .deinit = mock_se_deinit,
        .generate_key_pair = mock_gen_key,
        .sign = mock_sign,
        .verify = mock_verify,
        .derive_shared_secret = mock_derive,
    };
    
    error_t ret = ccc_init(&se);
    assert(ret == OK);
    
    ccc_pairing_config_t config;
    memset(&config, 0, sizeof(config));
    memcpy(config.vehicle_vin, "LSVNV2182E2100001", 17);
    
    ccc_session_context_t *session = NULL;
    ret = ccc_create_pairing_session(&config, &session);
    assert(ret == OK);
    assert(session != NULL);
    
    ccc_destroy_session(session);
    ccc_deinit();
    
    printf("  PASSED\n");
}

void test_ccc_challenge(void)
{
    printf("Testing ccc challenge generation...\n");
    
    uint8_t challenge1[CCC_CHALLENGE_LEN];
    uint8_t challenge2[CCC_CHALLENGE_LEN];
    
    error_t ret = ccc_generate_challenge(challenge1);
    assert(ret == OK);
    
    ret = ccc_generate_challenge(challenge2);
    assert(ret == OK);
    
    /* 两次生成的挑战应该不同 */
    assert(memcmp(challenge1, challenge2, CCC_CHALLENGE_LEN) != 0);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== CCC Core Tests ===\n\n");
    
    test_ccc_init_deinit();
    test_ccc_pairing_session();
    test_ccc_challenge();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
