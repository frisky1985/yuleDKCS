/******************************************************************************
 * @file    test_dkcs.c
 * @brief   Yule DKCS 统一接口测试
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "dkcs.h"

void test_version(void)
{
    printf("Testing dkcs_get_version...\n");
    
    const char *version = dkcs_get_version();
    assert(version != NULL);
    assert(strlen(version) > 0);
    
    printf("  Version: %s\n", version);
    printf("  PASSED\n");
}

void test_init_deinit(void)
{
    printf("Testing dkcs_init/deinit...\n");
    
    session_config_t config;
    memset(&config, 0, sizeof(config));
    config.protocol = PROTOCOL_CCC;
    config.conn_type = CONN_BLE;
    
    error_t ret = dkcs_init(&config);
    assert(ret == OK);
    
    ret = dkcs_deinit();
    assert(ret == OK);
    
    printf("  PASSED\n");
}

void test_protocol_types(void)
{
    printf("Testing protocol type definitions...\n");
    
    /* 验证协议值 */
    assert(PROTOCOL_CCC == 0);
    assert(PROTOCOL_ICCE == 1);
    assert(PROTOCOL_ICCOA == 2);
    
    /* 验证连接类型 */
    assert(CONN_BLE == 0);
    assert(CONN_NFC == 1);
    assert(CONN_UWB == 2);
    
    /* 验证角色 */
    assert(KEY_ROLE_OWNER == 0);
    assert(KEY_ROLE_ADMIN == 1);
    assert(KEY_ROLE_GUEST == 2);
    
    printf("  PASSED\n");
}

void test_permissions(void)
{
    printf("Testing permission flags...\n");
    
    /* 验证权限位 */
    assert(PERM_UNLOCK == 0x01);
    assert(PERM_LOCK == 0x02);
    assert(PERM_ENGINE_START == 0x04);
    assert(PERM_SHARE == 0x80);
    
    /* 组合权限 */
    uint16_t owner_perms = PERM_UNLOCK | PERM_LOCK | 
                           PERM_ENGINE_START | PERM_SHARE;
    assert(owner_perms == 0x87);
    
    printf("  PASSED\n");
}

int main(void)
{
    printf("=== Yule DKCS Unified API Tests ===\n\n");
    
    test_version();
    test_init_deinit();
    test_protocol_types();
    test_permissions();
    
    printf("\nAll tests PASSED!\n");
    return 0;
}
