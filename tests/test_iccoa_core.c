/******************************************************************************
 * @file    test_iccoa_core.c
 * @brief   ICCOA 核心测试
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "iccoa.h"

/* 模拟 SE 接口 */
static int mock_init(void) { return 0; }
static int mock_deinit(void) { return 0; }
static int mock_gen(uint8_t *p, uint8_t *k) {
    memset(p, 0xAA, 65);
    memset(k, 0xBB, 32);
    return 0;
}

void test_iccoa_init_deinit(void)
{
    printf("Testing iccoa_init/deinit...\n");
    
    iccoa_se_interface_t se = {
        .init = mock_init,
        .deinit = mock_deinit,
        .generate_keypair = mock_gen,
    };
    
    error_t ret = iccoa_init(&se);
    assert(ret == OK);
    
    ret = iccoa_deinit();
    assert(ret == OK);
    
    printf("  PASSED\n");
}

void test_iccoa_tlv(void)
{
    printf("Testing iccoa TLV operations...\n");
    
    iccoa_tlv_t tlv;
    uint8_t value[] = {0x01, 0x02, 0x03, 0x04};
    
    error_t ret = iccoa_build_tlv(&tlv, ICCOA_TLV_DEVICE_ID, 4, value);
    assert(ret == OK);
    assert(tlv.type == ICCOA_TLV_DEVICE_ID);
    assert(tlv.length == 4);
    assert(memcmp(tlv.value, value, 4) == 0);
    
    if (tlv.value) free(tlv.value);
    
    printf("  PASSED\n");
}

void test_iccoa_message(void)
{
    printf("Testing iccoa message build/serialize...\n");
    
    iccoa_tlv_t tlvs[2];
    uint8_t id[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t payload[] = {0xAA, 0xBB};
    
    iccoa_build_tlv(&tlvs[0], ICCOA_TLV_DEVICE_ID, 4, id);
    iccoa_build_tlv(&tlvs[1], ICCOA_TLV_PAYLOAD, 2, payload);
    
    iccoa_message_t msg;
    error_t ret = iccoa_build_message(&msg, ICCOA_CMD_VEHICLE_UNLOCK,
                                           tlvs, 2, 1);
    assert(ret == OK);
    assert(msg.header.magic == ICCOA_MAGIC);
    assert(msg.header.cmd == ICCOA_CMD_VEHICLE_UNLOCK);
    assert(msg.tlv_count == 2);
    
    /* 序列化 */
    uint8_t buffer[256];
    size_t len = sizeof(buffer);
    ret = iccoa_serialize_message(&msg, buffer, &len);
    assert(ret == OK);
    assert(len > 8);
    assert(buffer[0] == ICCOA_MAGIC);
    
    /* 清理 */
    if (tlvs[0].value) free(tlvs[0].value);
    if (tlvs[1].value) free(tlvs[1].value);
    iccoa_free_message(&msg);
    
    printf("  PASSED\n");
}

void test_iccoa_error_strings(void)
{
    printf("Testing iccoa error strings...\n");
    
    const char *str = iccoa_error_to_string(ICCOA_OK);
    assert(str != NULL);
    
    str = iccoa_error_to_string(ICCOA_ERR_AUTH_FAIL);
    assert(str != NULL);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== ICCOA Core Tests ===\n\n");
    
    test_iccoa_init_deinit();
    test_iccoa_tlv();
    test_iccoa_message();
    test_iccoa_error_strings();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
