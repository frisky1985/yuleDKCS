/******************************************************************************
 * @file    test_ccc_keymgmt.c
 * @brief   CCC R2.0 密钥管理模块测试
 *          测试: 钥匙创建/删除/查询, 密钥持久化, 
 *                钥匙分享/撤销/暂停/恢复, 有效期验证
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-26
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ccc_digital_key.h"
#include "nvm_key_storage.h"

/* Forward declarations from ccc_core.c for testing */
void nvm_secure_zero(void *ptr, size_t len);
error_t ccc_generate_challenge(uint8_t *challenge);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */
static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02X", data[i]);
    }
    printf("... (%zu bytes)\n", len);
}

static void fill_key_id(uint8_t *key_id, uint8_t val)
{
    memset(key_id, val, KEY_ID_LEN);
}

static ccc_digital_key_t create_test_key(uint8_t id_val, uint8_t key_type)
{
    ccc_digital_key_t key;
    memset(&key, 0, sizeof(key));

    fill_key_id(key.key_id, id_val);
    memset(key.vehicle_id, 0xAB, VEHICLE_ID_LEN);
    memset(key.owner_id, 0xCD, OWNER_ID_LEN);

    key.key_type = key_type;
    key.access_rights[0] = ACCESS_LOCK_UNLOCK | ACCESS_ENGINE_START;
    key.access_rights[1] = 0;
    key.access_rights[2] = 0;
    key.access_rights[3] = 0;

    key.valid_from = 1704067200;  /* 2024-01-01 */
    key.valid_until = 1735689600; /* 2025-01-01 */
    key.state = KEY_STATE_ACTIVE;
    key.version = 1;
    key.se_key_id = id_val + 0x10;

    return key;
}

/* ========================================================================
 * 测试: 密钥管理初始化/反初始化
 * ======================================================================== */
static int test_keymgmt_init_deinit(void)
{
    printf("  [Test] key_mgmt_init / key_mgmt_deinit ... ");

    ccc_status_t ret = key_mgmt_init();
    assert(ret == CCC_OK);

    ret = key_mgmt_deinit();
    assert(ret == CCC_OK);

    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙创建与获取
 * ======================================================================== */
static int test_key_create_get(void)
{
    printf("  [Test] 钥匙创建与获取 ... ");

    ccc_status_t ret = key_mgmt_init();
    assert(ret == CCC_OK);

    ccc_digital_key_t key = create_test_key(0x01, KEY_TYPE_OWNER);

    ret = key_create(&key);
    assert(ret == CCC_OK);

    /* 获取钥匙 */
    ccc_digital_key_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));

    ret = key_get(key.key_id, &retrieved);
    assert(ret == CCC_OK);
    assert(memcmp(retrieved.key_id, key.key_id, KEY_ID_LEN) == 0);
    assert(retrieved.key_type == KEY_TYPE_OWNER);
    assert(retrieved.state == KEY_STATE_ACTIVE);

    ret = key_mgmt_deinit();
    assert(ret == CCC_OK);

    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙重复创建防护
 * ======================================================================== */
static int test_key_duplicate(void)
{
    printf("  [Test] 钥匙重复创建防护 ... ");

    key_mgmt_init();

    ccc_digital_key_t key = create_test_key(0x02, KEY_TYPE_OWNER);

    ccc_status_t ret = key_create(&key);
    assert(ret == CCC_OK);

    /* 使用相同 key_id 再次创建应该失败 */
    ret = key_create(&key);
    assert(ret == CCC_ERR_ALREADY_EXISTS);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙删除
 * ======================================================================== */
static int test_key_delete(void)
{
    printf("  [Test] 钥匙删除 ... ");

    key_mgmt_init();

    ccc_digital_key_t key = create_test_key(0x03, KEY_TYPE_OWNER);
    ccc_status_t ret = key_create(&key);
    assert(ret == CCC_OK);

    /* 删除钥匙 */
    ret = key_delete(key.key_id);
    assert(ret == CCC_OK);

    /* 删除后获取应该失败 */
    ccc_digital_key_t retrieved;
    ret = key_get(key.key_id, &retrieved);
    assert(ret == CCC_ERR_NOT_FOUND);

    /* 删除不存在的钥匙应该失败 */
    uint8_t fake_id[KEY_ID_LEN];
    memset(fake_id, 0xFF, KEY_ID_LEN);
    ret = key_delete(fake_id);
    assert(ret == CCC_ERR_NOT_FOUND);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙列表
 * ======================================================================== */
static int test_key_list(void)
{
    printf("  [Test] 钥匙列表枚举 ... ");

    key_mgmt_init();

    /* 创建多个钥匙 */
    ccc_digital_key_t keys[3];
    for (int i = 0; i < 3; i++) {
        keys[i] = create_test_key(0x10 + i, KEY_TYPE_OWNER);
        ccc_status_t ret = key_create(&keys[i]);
        assert(ret == CCC_OK);
    }

    /* 枚举 */
    ccc_digital_key_t listed[8];
    uint8_t count = 8;
    ccc_status_t ret = key_list(listed, &count);
    assert(ret == CCC_OK);
    assert(count >= 3);

    printf("  --> 列出 %d 个钥匙\n", count);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙分享
 * ======================================================================== */
static int test_key_share(void)
{
    printf("  [Test] 钥匙分享 (Owner → Friend) ... ");

    key_mgmt_init();

    /* 创建 Owner 钥匙 */
    ccc_digital_key_t owner_key = create_test_key(0x20, KEY_TYPE_OWNER);
    ccc_status_t ret = key_create(&owner_key);
    assert(ret == CCC_OK);

    /* 分享为 Friend 钥匙, 30天有效 */
    ret = key_share(owner_key.key_id, KEY_TYPE_FRIEND, 2592000);
    assert(ret == CCC_OK);

    /* 验证新钥匙创建成功 */
    /* 应该有一个额外的钥匙记录 */
    ccc_digital_key_t listed[8];
    uint8_t count = 8;
    ret = key_list(listed, &count);
    assert(ret == CCC_OK);
    assert(count >= 2);

    /* 非 Owner 钥匙不能分享 */
    ccc_digital_key_t friend_key = create_test_key(0x21, KEY_TYPE_FRIEND);
    ret = key_create(&friend_key);
    assert(ret == CCC_OK);

    ret = key_share(friend_key.key_id, KEY_TYPE_TEMPORARY, 3600);
    assert(ret == CCC_ERR_DENIED);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙撤销/暂停/恢复
 * ======================================================================== */
static int test_key_suspend_resume_revoke(void)
{
    printf("  [Test] 钥匙状态管理 (暂停/恢复/撤销) ... ");

    key_mgmt_init();

    ccc_digital_key_t key = create_test_key(0x30, KEY_TYPE_OWNER);
    ccc_status_t ret = key_create(&key);
    assert(ret == CCC_OK);

    /* 暂停 */
    ret = key_suspend(key.key_id);
    assert(ret == CCC_OK);

    ccc_digital_key_t retrieved;
    ret = key_get(key.key_id, &retrieved);
    assert(ret == CCC_OK);
    assert(retrieved.state == KEY_STATE_SUSPENDED);

    /* 暂停状态下验证应该失败 */
    ret = key_validate(key.key_id);
    assert(ret == CCC_ERR_DENIED);

    /* 恢复 */
    ret = key_resume(key.key_id);
    assert(ret == CCC_OK);

    ret = key_get(key.key_id, &retrieved);
    assert(ret == CCC_OK);
    assert(retrieved.state == KEY_STATE_ACTIVE);

    /* 撤销 */
    ret = key_revoke(key.key_id);
    assert(ret == CCC_OK);

    ret = key_get(key.key_id, &retrieved);
    assert(ret == CCC_OK);
    assert(retrieved.state == KEY_STATE_REVOKED);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙容量限制
 * ======================================================================== */
static int test_key_capacity(void)
{
    printf("  [Test] 钥匙容量上限 (MAX_KEYS) ... ");

    key_mgmt_init();

    /* 装满所有槽位 */
    ccc_status_t ret;
    int created = 0;
    for (int i = 0; i < 10; i++) {
        ccc_digital_key_t key = create_test_key(0x40 + i, KEY_TYPE_OWNER);
        ret = key_create(&key);
        if (ret == CCC_OK) {
            created++;
        } else {
            break;
        }
    }

    printf("  --> 成功创建 %d 个钥匙\n", created);

    /* 尝试在满的情况下创建 */
    ccc_digital_key_t extra = create_test_key(0xFF, KEY_TYPE_OWNER);
    ret = key_create(&extra);

    if (ret == CCC_ERR_NO_MEM) {
        printf("  --> 容量正确: 超出上限被拒绝\n");
    }

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 钥匙验证 (有效期)
 * ======================================================================== */
static int test_key_validate(void)
{
    printf("  [Test] 钥匙有效性验证 ... ");

    key_mgmt_init();

    /* 创建一个有效的钥匙 */
    ccc_digital_key_t valid_key = create_test_key(0x50, KEY_TYPE_OWNER);
    /* valid_from 和 valid_until 已经设置好 */
    ccc_status_t ret = key_create(&valid_key);
    assert(ret == CCC_OK);

    ret = key_validate(valid_key.key_id);
    assert(ret == CCC_OK);

    /* 验证不存在的钥匙 */
    uint8_t fake_id[KEY_ID_LEN];
    memset(fake_id, 0xFF, KEY_ID_LEN);
    ret = key_validate(fake_id);
    assert(ret == CCC_ERR_NOT_FOUND);

    /* NULL 参数 */
    ret = key_validate(NULL);
    assert(ret == CCC_ERR_INVALID_PARAM);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 测试: 空参数校验
 * ======================================================================== */
static int test_key_null_params(void)
{
    printf("  [Test] NULL 参数校验 ... ");

    key_mgmt_init();

    assert(key_create(NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_get(NULL, NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_delete(NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_list(NULL, NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_share(NULL, KEY_TYPE_FRIEND, 0) == CCC_ERR_INVALID_PARAM);
    assert(key_revoke(NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_suspend(NULL) == CCC_ERR_INVALID_PARAM);
    assert(key_resume(NULL) == CCC_ERR_INVALID_PARAM);

    key_mgmt_deinit();
    printf("PASSED\n");
    return 0;
}

/* ========================================================================
 * 主测试入口
 * ======================================================================== */
int main(void)
{
    printf("========================================\n");
    printf("  CCC R2.0 密钥管理测试套件\n");
    printf("========================================\n\n");

    int total = 0;
    int passed = 0;
    int failed = 0;

    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"初始化/反初始化",            test_keymgmt_init_deinit},
        {"钥匙创建与获取",            test_key_create_get},
        {"钥匙重复创建防护",          test_key_duplicate},
        {"钥匙删除",                  test_key_delete},
        {"钥匙列表枚举",              test_key_list},
        {"钥匙分享 (Owner→Friend)",   test_key_share},
        {"状态管理 (暂停/恢复/撤销)",  test_key_suspend_resume_revoke},
        {"容量上限 (MAX_KEYS)",       test_key_capacity},
        {"有效性验证",                test_key_validate},
        {"NULL 参数校验",             test_key_null_params},
    };

    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; i++) {
        total++;
        printf("[%zu/%zu] %s\n", i + 1, num_tests, tests[i].name);
        int result = tests[i].fn();
        if (result == 0) {
            passed++;
        } else {
            failed++;
            printf("  --> FAILED (code %d)\n", result);
        }
        printf("\n");
    }

    printf("========================================\n");
    printf("  测试总结\n");
    printf("========================================\n");
    printf("  总数:  %d\n", total);
    printf("  通过:  %d\n", passed);
    printf("  失败:  %d\n", failed);
    printf("========================================\n");

    return (failed == 0) ? 0 : 1;
}
