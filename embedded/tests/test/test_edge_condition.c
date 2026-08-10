/**
 * test_edge_condition.c — ICCE 动态条件树单元测试
 *
 * Covers edge_condition.c public API:
 *   - Pool lifecycle: init/deinit idempotence, overflow, stats
 *   - Tree construction: create_leaf / create_composite / free_tree
 *   - Serialization round-trip (flat binary ↔ tree)
 *   - Deserialization error handling (bad data, pool exhaustion)
 *   - deep_copy / equal / dump / upgrade_rule
 *   - NVM persistence round-trip (via storage driver stub)
 *
 * Uses real ICCE source files + stub storage driver.
 */

#include "unity.h"
#include "icce_digital_key.h"
#include "edge_condition.h"

#include <string.h>

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* ========================================================================
 *  COND_POOL_001 — 池初始化/反初始化 (幂等)
 * ======================================================================== */
void test_cond_pool_init_deinit(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    int32_t r1 = edge_condition_pool_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r1);
    /* 幂等: 重复 init 不报错 */
    int32_t r2 = edge_condition_pool_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r2);

    edge_condition_pool_stats_t stats;
    edge_condition_get_pool_stats(&stats);
    TEST_ASSERT_EQUAL_UINT(EDGE_COND_POOL_SIZE, stats.total_nodes);
    TEST_ASSERT_EQUAL_UINT(0, stats.used_nodes);

    int32_t r3 = edge_condition_pool_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r3);
    int32_t r4 = edge_condition_pool_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r4);
}

/* ========================================================================
 *  COND_POOL_002 — 池耗尽: 分配满池后返回 NULL, 释放后可复用
 * ======================================================================== */
void test_cond_alloc_overflow(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();
    icce_condition_t *nodes[EDGE_COND_POOL_SIZE];
    int i;

    for (i = 0; i < EDGE_COND_POOL_SIZE; i++) {
        nodes[i] = edge_condition_alloc();
        TEST_ASSERT_NOT_NULL(nodes[i]);
        TEST_ASSERT_EQUAL(COND_OP_NONE, nodes[i]->op);
    }

    /* 池耗尽 */
    icce_condition_t *over = edge_condition_alloc();
    TEST_ASSERT_NULL(over);

    edge_condition_pool_stats_t stats;
    edge_condition_get_pool_stats(&stats);
    TEST_ASSERT_EQUAL_UINT(EDGE_COND_POOL_SIZE, stats.used_nodes);
    TEST_ASSERT_EQUAL_UINT(1, stats.overflow_count);

    /* 释放一个后恢复可用 */
    edge_condition_free_tree(nodes[0]);
    icce_condition_t *reused = edge_condition_alloc();
    TEST_ASSERT_NOT_NULL(reused);

    /* 清理剩余 */
    for (i = 1; i < EDGE_COND_POOL_SIZE; i++) {
        edge_condition_free_tree(nodes[i]);
    }
    edge_condition_free_tree(reused);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_TREE_001 — 叶子节点创建与字段验证
 * ======================================================================== */
void test_cond_create_leaf(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_DIST_LT, 1500, 0);
    TEST_ASSERT_NOT_NULL(leaf);
    TEST_ASSERT_EQUAL(COND_OP_DIST_LT, leaf->op);
    TEST_ASSERT_EQUAL_INT32(1500, leaf->threshold);
    TEST_ASSERT_NULL(leaf->left);
    TEST_ASSERT_NULL(leaf->right);

    icce_condition_t *zone = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 3);
    TEST_ASSERT_NOT_NULL(zone);
    TEST_ASSERT_EQUAL(COND_OP_ZONE_EQ, zone->op);
    TEST_ASSERT_EQUAL_UINT8(3, zone->zone_id);

    /* 宽松契约: 叶子 op 由调用方保证, 无效 op 也创建节点 (不校验) */
    icce_condition_t *loose = edge_condition_create_leaf((icce_condition_op_e)99, 0, 0);
    TEST_ASSERT_NOT_NULL(loose);
    TEST_ASSERT_EQUAL(99, loose->op);

    edge_condition_free_tree(leaf);
    edge_condition_free_tree(zone);
    edge_condition_free_tree(loose);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_TREE_002 — 复合节点 (AND/OR/NOT) 与释放
 * ======================================================================== */
void test_cond_create_composite(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *dist = edge_condition_create_leaf(COND_OP_DIST_LT, 2000, 0);
    icce_condition_t *zone = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 2);
    icce_condition_t *and = edge_condition_create_composite(COND_OP_AND, dist, zone);
    TEST_ASSERT_NOT_NULL(and);
    TEST_ASSERT_EQUAL(COND_OP_AND, and->op);
    TEST_ASSERT_EQUAL(dist, and->left);
    TEST_ASSERT_EQUAL(zone, and->right);

    /* NOT: 单子节点 */
    icce_condition_t *not_node = edge_condition_create_composite(COND_OP_NOT, dist, NULL);
    TEST_ASSERT_NOT_NULL(not_node);
    TEST_ASSERT_EQUAL(COND_OP_NOT, not_node->op);
    TEST_ASSERT_NULL(not_node->right);

    /* 宽松契约: AND 允许 NULL 子节点 (由上层保证语义), 节点照常创建 */
    icce_condition_t *loose = edge_condition_create_composite(COND_OP_AND, NULL, NULL);
    TEST_ASSERT_NOT_NULL(loose);
    TEST_ASSERT_NULL(loose->left);
    TEST_ASSERT_NULL(loose->right);

    edge_condition_free_tree(and);
    edge_condition_free_tree(not_node);
    edge_condition_free_tree(loose);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_SER_001 — 序列化/反序列化往返 (复合树)
 * ======================================================================== */
void test_cond_serialize_roundtrip(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    /* (DIST_LT 2000) AND (ZONE_EQ 2) OR (RSSI_GT -70) */
    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 2000, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 2);
    icce_condition_t *r = edge_condition_create_leaf(COND_OP_RSSI_GT, -70, 0);
    icce_condition_t *and = edge_condition_create_composite(COND_OP_AND, d, z);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_OR, and, r);
    TEST_ASSERT_NOT_NULL(root);

    uint8_t buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t out_len = 0;
    int32_t ser = edge_condition_serialize(root, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ser);
    TEST_ASSERT_TRUE(out_len > 0);

    /* 反序列化 */
    icce_condition_t *copy = NULL;
    int32_t des = edge_condition_deserialize(buf, out_len, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, des);
    TEST_ASSERT_NOT_NULL(copy);

    /* 结构等价 (equal 返回 bool) */
    int32_t eq = edge_condition_equal(root, copy);
    TEST_ASSERT_TRUE(eq);

    edge_condition_free_tree(root);
    edge_condition_free_tree(copy);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_SER_002 — 序列化错误处理
 * ======================================================================== */
void test_cond_serialize_errors(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_DIST_LT, 100, 0);
    uint8_t buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t out_len = 0;

    /* NULL 参数 */
    int32_t r1 = edge_condition_serialize(leaf, NULL, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r1);
    int32_t r2 = edge_condition_serialize(leaf, buf, sizeof(buf), NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r2);
    /* NULL root → 合法空树 (契约见头文件), 返回 OK */
    int32_t r3 = edge_condition_serialize(NULL, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r3);
    /* 全 NULL 参数 → PARAM */
    int32_t r3b = edge_condition_serialize(NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r3b);

    /* 缓冲区过小 → NO_MEM */
    int32_t r4 = edge_condition_serialize(leaf, buf, 4, &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, r4);

    /* 空树序列化 (NULL root → 空树, 合法非错误, 契约见头文件) */
    int32_t r5 = edge_condition_serialize(NULL, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r5);
    uint32_t r5len = out_len;
    TEST_ASSERT_TRUE(r5len >= 4);  /* header (node_count + root_idx) */

    /* 反序列化坏数据 */
    icce_condition_t *out = NULL;
    uint8_t junk[64];
    memset(junk, 0xFF, sizeof(junk));
    int32_t r6 = edge_condition_deserialize(junk, sizeof(junk), &out);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r6);
    int32_t r7 = edge_condition_deserialize(NULL, 0, &out);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r7);

    edge_condition_free_tree(leaf);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_COPY_001 — 深拷贝 + 相等比较
 * ======================================================================== */
void test_cond_deep_copy_equal(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 3000, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 1);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_AND, d, z);

    icce_condition_t *copy = NULL;
    int32_t r1 = edge_condition_deep_copy(root, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r1);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_NOT_EQUAL(root, copy);
    TEST_ASSERT_NOT_EQUAL(root->left, copy->left);

    /* 相等 */
    TEST_ASSERT_TRUE(edge_condition_equal(root, copy));

    /* 修改副本 → 不相等 */
    copy->left->threshold = 9999;
    TEST_ASSERT_FALSE(edge_condition_equal(root, copy));

    /* NULL 处理 */
    int32_t r2 = edge_condition_deep_copy(NULL, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r2);
    int32_t r3 = edge_condition_deep_copy(root, NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r3);

    edge_condition_free_tree(root);
    edge_condition_free_tree(copy);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_DUMP_001 — 树打印 (dump)
 * ======================================================================== */
void test_cond_dump(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 2000, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 2);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_AND, d, z);

    char buf[512];
    edge_condition_dump(root, buf, sizeof(buf));
    TEST_ASSERT_TRUE(strlen(buf) > 0);

    /* NULL root 不应崩溃 */
    edge_condition_dump(NULL, buf, sizeof(buf));

    edge_condition_free_tree(root);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_UPGRADE_001 — 静态规则升级为动态条件树
 * ======================================================================== */
void test_cond_upgrade_rule(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_edge_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.condition.op = COND_OP_NONE;
    int32_t r1 = edge_condition_upgrade_rule(&rule);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r1);
    int32_t r2 = edge_condition_upgrade_rule(NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r2);

    /* 带条件树的规则升级 */
    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 1500, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 2);
    rule.condition.op = COND_OP_AND;
    rule.condition.left = d;
    rule.condition.right = z;
    int32_t r3 = edge_condition_upgrade_rule(&rule);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r3);

    /* 升级后仍保留结构 (指针指向动态池节点) */
    TEST_ASSERT_EQUAL(COND_OP_AND, rule.condition.op);
    TEST_ASSERT_NOT_NULL(rule.condition.left);
    TEST_ASSERT_EQUAL(COND_OP_DIST_LT, rule.condition.left->op);

    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_NVM_001 — NVM 持久化往返 (storage stub)
 * ======================================================================== */
void test_cond_nvm_roundtrip(void)
{
    uint8_t handle = 0;
    int32_t s0 = storage_init(&handle);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, s0);

    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 2500, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 3);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_AND, d, z);

    int32_t r1 = edge_condition_save_to_nvm(handle, "unlock_approach", root);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r1);

    icce_condition_t *loaded = NULL;
    int32_t r2 = edge_condition_load_from_nvm(handle, "unlock_approach", &loaded);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r2);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_TRUE(edge_condition_equal(root, loaded));

    /* 未存储的 tag → NOT_FOUND */
    icce_condition_t *missing = NULL;
    int32_t r3 = edge_condition_load_from_nvm(handle, "no_such_rule", &missing);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, r3);

    /* 删除 */
    int32_t r4 = edge_condition_delete_from_nvm(handle, "unlock_approach");
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, r4);
    icce_condition_t *gone = NULL;
    int32_t r5 = edge_condition_load_from_nvm(handle, "unlock_approach", &gone);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, r5);

    edge_condition_free_tree(root);
    edge_condition_free_tree(loaded);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_POOL_003 — 统计字段随分配/释放更新
 * ======================================================================== */
void test_cond_pool_stats(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    edge_condition_pool_stats_t before;
    edge_condition_get_pool_stats(&before);
    TEST_ASSERT_EQUAL_UINT(0, before.used_nodes);

    icce_condition_t *a = edge_condition_alloc();
    icce_condition_t *b = edge_condition_alloc();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    edge_condition_pool_stats_t mid;
    edge_condition_get_pool_stats(&mid);
    TEST_ASSERT_EQUAL_UINT(2, mid.used_nodes);
    TEST_ASSERT_EQUAL_UINT(2, mid.allocation_count);

    edge_condition_free_tree(a);
    edge_condition_free_tree(b);

    edge_condition_pool_stats_t after;
    edge_condition_get_pool_stats(&after);
    TEST_ASSERT_EQUAL_UINT(0, after.used_nodes);
    TEST_ASSERT_EQUAL_UINT(2, after.free_count);

    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_POOL_004 — 池未初始化时 alloc 拒绝
 * ======================================================================== */
void test_cond_alloc_uninitialized(void)
{
    edge_condition_pool_deinit();  /* 确保未初始化 */

    icce_condition_t *n = edge_condition_alloc();
    TEST_ASSERT_NULL(n);

    edge_condition_pool_init();
}

/* ========================================================================
 *  COND_SER_003 — 单子 NOT 节点序列化 + 小缓冲区 NO_MEM
 * ======================================================================== */
void test_cond_serialize_not_and_oom(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    /* NOT(DIST_LT 1000) */
    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 1000, 0);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_NOT, d, NULL);
    TEST_ASSERT_NOT_NULL(root);

    uint8_t buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t out_len = 0;
    int32_t ser = edge_condition_serialize(root, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ser);

    icce_condition_t *copy = NULL;
    int32_t des = edge_condition_deserialize(buf, out_len, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, des);
    TEST_ASSERT_TRUE(edge_condition_equal(root, copy));

    /* 空树小缓冲区 → NO_MEM (header 放不下) */
    uint32_t tiny_len = 0;
    int32_t oom = edge_condition_serialize(NULL, buf, 1, &tiny_len);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, oom);

    edge_condition_free_tree(root);
    edge_condition_free_tree(copy);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_SER_004 — 池耗尽时 deserialize 返回 NO_MEM
 * ======================================================================== */
void test_cond_deserialize_pool_exhaust(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    /* 建一棵 3 节点树并序列化 */
    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 500, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 1);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_AND, d, z);

    uint8_t buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t out_len = 0;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK,
        edge_condition_serialize(root, buf, sizeof(buf), &out_len));
    edge_condition_free_tree(root);

    /* 占满池 (64 - 0 空闲, 先释放 3 个 → 3 空闲, 树要 3 节点) */
    icce_condition_t *holders[EDGE_COND_POOL_SIZE];
    int i;
    for (i = 0; i < EDGE_COND_POOL_SIZE - 3; i++) {
        holders[i] = edge_condition_alloc();
    }
    TEST_ASSERT_NOT_NULL(holders[EDGE_COND_POOL_SIZE - 4]);

    /* 反序列化 3 节点树, 池只剩 3 空闲 → 恰好成功 */
    icce_condition_t *copy = NULL;
    int32_t des = edge_condition_deserialize(buf, out_len, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, des);
    edge_condition_free_tree(copy);

    /* 再占 1 个 → 池只剩 2, 树要 3 → NO_MEM */
    icce_condition_t *extra = edge_condition_alloc();
    TEST_ASSERT_NOT_NULL(extra);
    icce_condition_t *copy2 = NULL;
    int32_t oom = edge_condition_deserialize(buf, out_len, &copy2);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, oom);

    for (i = 0; i < EDGE_COND_POOL_SIZE - 3; i++) {
        edge_condition_free_tree(holders[i]);
    }
    edge_condition_free_tree(extra);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_NVM_002 — NVM 规则配置头校验 (坏 magic/version)
 * ======================================================================== */
void test_cond_nvm_bad_config_header(void)
{
    uint8_t handle = 0;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, storage_init(&handle));

    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    /* 写一个 magic 错误的配置 blob (key: edge_cond_ruleset) */
    uint8_t bad[16];
    memset(bad, 0xAB, sizeof(bad));  /* magic = 0xABABABAB ≠ "ICEC" */
    int32_t wr = storage_write(handle, (const uint8_t *)"edge_cond_ruleset",
                               (uint16_t)strlen("edge_cond_ruleset"),
                               bad, sizeof(bad));
    TEST_ASSERT_EQUAL_INT32(STORAGE_SUCCESS, wr);

    int32_t r = edge_condition_load_rules_from_nvm(handle);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r);

    /* 太短的 blob → PARAM */
    uint8_t short_blob[2] = {0, 0};
    int32_t wr2 = storage_write(handle, (const uint8_t *)"edge_cond_ruleset",
                                (uint16_t)strlen("edge_cond_ruleset"),
                                short_blob, sizeof(short_blob));
    TEST_ASSERT_EQUAL_INT32(STORAGE_SUCCESS, wr2);
    int32_t r2 = edge_condition_load_rules_from_nvm(handle);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, r2);

    edge_condition_pool_deinit();
}

/* ========================================================================
 *  COND_COPY_002 — 池耗尽时 deep_copy 返回 NO_MEM
 * ======================================================================== */
void test_cond_deepcopy_pool_exhaust(void)
{
    edge_condition_pool_deinit();  /* 重置前序套件遗留状态 */
    edge_condition_pool_init();

    icce_condition_t *d = edge_condition_create_leaf(COND_OP_DIST_LT, 700, 0);
    icce_condition_t *z = edge_condition_create_leaf(COND_OP_ZONE_EQ, 0, 2);
    icce_condition_t *root = edge_condition_create_composite(COND_OP_AND, d, z);

    /* 占满剩余池 (64 - 3) */
    icce_condition_t *holders[EDGE_COND_POOL_SIZE];
    int i;
    for (i = 0; i < EDGE_COND_POOL_SIZE - 3; i++) {
        holders[i] = edge_condition_alloc();
    }

    icce_condition_t *copy = NULL;
    int32_t oom = edge_condition_deep_copy(root, &copy);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, oom);
    TEST_ASSERT_NULL(copy);

    for (i = 0; i < EDGE_COND_POOL_SIZE - 3; i++) {
        edge_condition_free_tree(holders[i]);
    }
    edge_condition_free_tree(root);
    edge_condition_pool_deinit();
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_edge_condition_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cond_pool_init_deinit);
    RUN_TEST(test_cond_alloc_overflow);
    RUN_TEST(test_cond_create_leaf);
    RUN_TEST(test_cond_create_composite);
    RUN_TEST(test_cond_serialize_roundtrip);
    RUN_TEST(test_cond_serialize_errors);
    RUN_TEST(test_cond_deep_copy_equal);
    RUN_TEST(test_cond_dump);
    RUN_TEST(test_cond_upgrade_rule);
    RUN_TEST(test_cond_nvm_roundtrip);
    RUN_TEST(test_cond_pool_stats);
    RUN_TEST(test_cond_alloc_uninitialized);
    RUN_TEST(test_cond_serialize_not_and_oom);
    RUN_TEST(test_cond_deserialize_pool_exhaust);
    RUN_TEST(test_cond_nvm_bad_config_header);
    RUN_TEST(test_cond_deepcopy_pool_exhaust);
    UNITY_END();
}

#ifndef TEST_EDGE_COND_NO_MAIN
int main(void) { return run_edge_condition_tests(); }
#endif /* TEST_EDGE_COND_NO_MAIN */
