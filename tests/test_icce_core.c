/******************************************************************************
 * @file    test_icce_core.c
 * @brief   ICCE 核心测试
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "icce.h"

/* 模拟 SE 接口 */
static int mock_open(uint8_t *a, size_t *l) { return 0; }
static int mock_close(void) { return 0; }
static int mock_trans(const uint8_t *a, size_t al, uint8_t *r, size_t *rl) {
    /* 模拟成功响应 */
    r[0] = 0x90;
    r[1] = 0x00;
    *rl = 2;
    return 0;
}

void test_icce_init_deinit(void)
{
    printf("Testing icce_init/deinit...\n");
    
    icce_se_interface_t se = {
        .open_channel = mock_open,
        .close_channel = mock_close,
        .transmit = mock_trans,
    };
    
    error_t ret = icce_init(&se);
    assert(ret == OK);
    
    ret = icce_deinit();
    assert(ret == OK);
    
    printf("  PASSED\n");
}

void test_icce_apdu_build_parse(void)
{
    printf("Testing icce APDU build/parse...\n");
    
    icce_apdu_t apdu;
    uint8_t data[] = {0x01, 0x02, 0x03};
    
    error_t ret = icce_build_apdu(&apdu, 0x00, ICCE_CMD_PAIRING_INIT,
                                        0x00, 0x00, data, 3, 0);
    assert(ret == OK);
    assert(apdu.cla == 0x00);
    assert(apdu.ins == ICCE_CMD_PAIRING_INIT);
    assert(apdu.lc == 3);
    assert(memcmp(apdu.data, data, 3) == 0);
    
    printf("  PASSED\n");
}

void test_icce_error_strings(void)
{
    printf("Testing icce error strings...\n");
    
    const char *str = icce_error_to_string(ICCE_OK);
    assert(str != NULL);
    
    str = icce_error_to_string(ICCE_ERR_AUTH_FAIL);
    assert(str != NULL);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== ICCE Core Tests ===\n\n");
    
    test_icce_init_deinit();
    test_icce_apdu_build_parse();
    test_icce_error_strings();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
