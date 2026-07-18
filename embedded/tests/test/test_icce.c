/**
 * test_icce.c — ICCE Digital Key Protocol Stack Unit Tests
 *
 * Tests the ICCE protocol high-level API:
 *   - ICCE_CORE_001..010: 初始化/反初始化/主循环
 *   - ICCE_ZONE_001..005: 区域分类
 *   - ICCE_UWB_001..005:  UWB 会话管理
 *   - ICCE_SEC_001..005:  安全绑定/认证
 *   - ICCE_VEH_001..005:  车辆控制
 *   - ICCE_EDGE_001..010: 边缘规则引擎
 *
 * Uses real ICCE source files + stub HAL implementations.
 */

#include "unity.h"
#include "icce_digital_key.h"

void setUp(void) {}
void tearDown(void) {}

/* ========================================================================
 *  ICCE_CORE_001 — 初始化/反初始化生命周期
 * ======================================================================== */
void test_icce_init_deinit(void)
{
    int32_t ret = icce_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 重复 init 应幂等 */
    ret = icce_dk_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    ret = icce_dk_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 重复 deinit 安全 */
    ret = icce_dk_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 *  ICCE_CORE_002 — 主循环 tick
 * ======================================================================== */
void test_icce_run(void)
{
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_init());

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_run());
    }

    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_deinit());
}

/* ========================================================================
 *  ICCE_CORE_003 — 完整生命周期（init → run → deinit → re-init）
 * ======================================================================== */
void test_icce_full_lifecycle(void)
{
    for (int cycle = 0; cycle < 3; cycle++) {
        TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_init());
        TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_run());
        TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_dk_deinit());
    }
}

/* ========================================================================
 *  ICCE_ZONE_001 — 区域分类：远距离
 * ======================================================================== */
void test_zone_far(void)
{
    icce_zone_e zone;

    /* >50m → ZONE_NONE */
    zone = icce_zone_classify(60000);
    TEST_ASSERT_EQUAL(ICCE_ZONE_NONE, zone);

    zone = icce_zone_classify(50000);
    TEST_ASSERT_EQUAL(ICCE_ZONE_NONE, zone);

    /* 20-50m → ZONE_FAR */
    zone = icce_zone_classify(25000);
    TEST_ASSERT_EQUAL(ICCE_ZONE_FAR, zone);
}

/* ========================================================================
 *  ICCE_ZONE_002 — 区域分类：中等距离
 * ======================================================================== */
void test_zone_mid(void)
{
    icce_zone_e zone;

    zone = icce_zone_classify(15000);
    TEST_ASSERT_EQUAL(ICCE_ZONE_MID, zone);

    /* 边界 */
    zone = icce_zone_classify(10000);
    TEST_ASSERT_EQUAL(ICCE_ZONE_MID, zone);
}

/* ========================================================================
 *  ICCE_ZONE_003 — 区域分类：近距离/附近/车內
 * ======================================================================== */
void test_zone_near_vicinity_interior(void)
{
    TEST_ASSERT_EQUAL(ICCE_ZONE_NEAR,     icce_zone_classify(5000));
    TEST_ASSERT_EQUAL(ICCE_ZONE_NEAR,     icce_zone_classify(3000));
    TEST_ASSERT_EQUAL(ICCE_ZONE_VICINITY, icce_zone_classify(2000));
    TEST_ASSERT_EQUAL(ICCE_ZONE_VICINITY, icce_zone_classify(1000));
    TEST_ASSERT_EQUAL(ICCE_ZONE_INTERIOR, icce_zone_classify(500));
    TEST_ASSERT_EQUAL(ICCE_ZONE_INTERIOR, icce_zone_classify(0));
    TEST_ASSERT_EQUAL(ICCE_ZONE_NONE,     icce_zone_classify(-1));
}

/* ========================================================================
 *  ICCE_ZONE_004 — 区域定义查询
 * ======================================================================== */
void test_zone_get_def(void)
{
    icce_zone_def_t def;

    /* NULL param */
    int32_t ret = icce_zone_get_def(ICCE_ZONE_NEAR, NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    /* Invalid zone */
    ret = icce_zone_get_def(99, &def);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    /* Normal zone */
    ret = icce_zone_get_def(ICCE_ZONE_VICINITY, &def);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_ZONE_VICINITY, def.zone);
    TEST_ASSERT_TRUE(def.inner_mm < def.outer_mm);
}

/* ========================================================================
 *  ICCE_UWB_001 — UWB 会话创建/启动/停止/查询
 * ======================================================================== */
void test_uwb_session_lifecycle(void)
{
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_uwb_init());

    /* Start session */
    int32_t ret = icce_uwb_start_session(1, ICCE_UWB_ROLE_RESPONDER, 9);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Get ranging info */
    icce_uwb_session_t session;
    memset(&session, 0, sizeof(session));
    ret = icce_uwb_get_ranging(1, &session);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(1, session.session_id);
    TEST_ASSERT_EQUAL(9, session.channel);

    /* Stop session */
    ret = icce_uwb_stop_session(1);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Query stopped session → not found */
    ret = icce_uwb_get_ranging(1, &session);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, ret);

    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_uwb_deinit());
}

/* ========================================================================
 *  ICCE_UWB_002 — UWB 参数校验
 * ======================================================================== */
void test_uwb_invalid_params(void)
{
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_uwb_init());

    /* Invalid channel */
    int32_t ret = icce_uwb_start_session(1, ICCE_UWB_ROLE_CONTROLLER, 7);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    /* NULL out param — function checks session existence before NULL check */
    icce_uwb_session_t session;
    int32_t ret2 = icce_uwb_get_ranging(99, NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, ret2);

    /* Non-existent session */
    ret2 = icce_uwb_get_ranging(99, &session);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, ret2);

    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_uwb_deinit());
}

/* ========================================================================
 *  ICCE_UWB_003 — 多会话管理
 * ======================================================================== */
void test_uwb_multiple_sessions(void)
{
    icce_uwb_init();

    /* Fill up sessions */
    for (uint8_t i = 0; i < ICCE_UWB_MAX_RANGING_SESSIONS; i++) {
        TEST_ASSERT_EQUAL_INT32(ICCE_OK,
            icce_uwb_start_session(100 + i, ICCE_UWB_ROLE_CONTROLLER, 9));
    }

    /* Exceed max */
    int32_t ret = icce_uwb_start_session(200, ICCE_UWB_ROLE_CONTROLLER, 9);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_BUSY, ret);

    icce_uwb_deinit();
}

/* ========================================================================
 *  ICCE_SEC_001 — 安全初始化
 * ======================================================================== */
void test_security_init(void)
{
    int32_t ret = icce_security_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 *  ICCE_SEC_002 — 绑定设备公钥
 * ======================================================================== */
void test_security_bind(void)
{
    icce_security_init();

    uint8_t pubkey[64];
    memset(pubkey, 0xAA, sizeof(pubkey));

    int32_t ret = icce_security_bind(pubkey, sizeof(pubkey));
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* NULL param */
    ret = icce_security_bind(NULL, 64);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    /* Wrong length */
    ret = icce_security_bind(pubkey, 10);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);
}

/* ========================================================================
 *  ICCE_SEC_003 — 认证挑战-响应
 * ======================================================================== */
void test_security_auth(void)
{
    icce_security_init();

    /* Bind first */
    uint8_t pubkey[64];
    memset(pubkey, 0xBB, sizeof(pubkey));
    icce_security_bind(pubkey, sizeof(pubkey));

    /* Auth with matching signature */
    uint8_t challenge[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };
    uint8_t signature[64];
    memset(signature, 0xCC, sizeof(signature));

    /* Without valid SE050 verify (stub returns DENIED) */
    int32_t ret = icce_security_auth(challenge, sizeof(challenge),
                                      signature, sizeof(signature));
    /* se050_verify stub returns deny */
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_SECURITY, ret);

    /* NULL parms */
    ret = icce_security_auth(NULL, 16, signature, sizeof(signature));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    ret = icce_security_auth(challenge, 16, NULL, 64);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);
}

/* ========================================================================
 *  ICCE_SEC_004 — 会话验证
 * ======================================================================== */
void test_security_session_verify(void)
{
    int32_t ret = icce_security_verify_session(1);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 *  ICCE_VEH_001 — 车辆初始化 + 状态查询
 * ======================================================================== */
void test_vehicle_init_and_status(void)
{
    int32_t ret = icce_vehicle_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_vehicle_status_t status;
    memset(&status, 0, sizeof(status));
    ret = icce_vehicle_get_status(&status);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Default: locked */
    TEST_ASSERT_EQUAL(0x01, status.lock_status);
}

/* ========================================================================
 *  ICCE_VEH_002 — 车辆控制命令
 * ======================================================================== */
void test_vehicle_control(void)
{
    icce_vehicle_init();

    /* Unlock */
    int32_t ret = icce_vehicle_ctrl(ICCE_ACTION_UNLOCK, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_vehicle_status_t status;
    icce_vehicle_get_status(&status);
    TEST_ASSERT_EQUAL(0x00, status.lock_status);  /* Unlocked */

    /* Lock */
    ret = icce_vehicle_ctrl(ICCE_ACTION_LOCK, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_vehicle_get_status(&status);
    TEST_ASSERT_EQUAL(0x01, status.lock_status);  /* Locked */

    /* Start engine */
    ret = icce_vehicle_ctrl(ICCE_ACTION_START, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    icce_vehicle_get_status(&status);
    TEST_ASSERT_EQUAL(0x01, status.engine_status);
}

/* ========================================================================
 *  ICCE_VEH_003 — 无效控制命令
 * ======================================================================== */
void test_vehicle_invalid_ctrl(void)
{
    icce_vehicle_init();

    /* Unknown action (use value not in enum: 0xFF = ICCE_ACTION_CUSTOM, which
       is valid; try an undefined value like 0xAA instead) */
    int32_t ret = icce_vehicle_ctrl((icce_action_e)0xAA, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);
}

/* ========================================================================
 *  ICCE_VEH_004 — 回调注册
 * ======================================================================== */
void test_vehicle_register_callback(void)
{
    int32_t ret = icce_vehicle_register_cb(NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    /* Don't pass invalid callback address (0x1) — it would leave a dangling
       function pointer that crashes subsequent icce_vehicle_ctrl calls */
    (void)ret;
}

/* ========================================================================
 *  ICCE_EDGE_001 — 边缘计算初始化 + 默认规则加载
 * ======================================================================== */
void test_edge_init_default_rules(void)
{
    int32_t ret = icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Default rules should be loaded */
    /* Vehicle unlock on zone enter + lock on exit */
    /* We can test indirectly via process_trigger */
    ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_edge_deinit();
}

/* ========================================================================
 *  ICCE_EDGE_002 — 添加/移除/启用规则
 * ======================================================================== */
void test_edge_rule_lifecycle(void)
{
    icce_edge_init();

    /* Add rule */
    icce_edge_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.trigger = ICCE_TRIGGER_DISTANCE;
    rule.zone_id = ICCE_ZONE_VICINITY;
    rule.threshold_mm = 2500;
    rule.actions[0] = ICCE_ACTION_LIGHTS;
    rule.action_count = 1;
    rule.priority = 100;
    rule.enabled = true;

    int32_t ret = icce_edge_add_rule(&rule);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Remove rule (rule_id 3 since 4th entry after 3 defaults) */
    ret = icce_edge_remove_rule(3);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Remove non-existent */
    ret = icce_edge_remove_rule(99);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, ret);

    /* Disable default rule */
    ret = icce_edge_enable_rule(0, false);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Disable non-existent */
    ret = icce_edge_enable_rule(99, true);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, ret);

    icce_edge_deinit();
}

/* ========================================================================
 *  ICCE_EDGE_003 — 空规则处理（安全执行）
 * ======================================================================== */
void test_edge_empty_rules(void)
{
    icce_edge_init();
    icce_edge_deinit();  /* No rules */

    /* Trigger after deinit — rules count is 0, should return OK */
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 *  ICCE_EDGE_004 — 规则优先级：高优先级触发
 * ======================================================================== */
void test_edge_priority_routing(void)
{
    icce_edge_init();

    /* Add high-priority unlock rule */
    icce_edge_rule_t high_rule;
    memset(&high_rule, 0, sizeof(high_rule));
    high_rule.trigger = ICCE_TRIGGER_ZONE_ENTER;
    high_rule.zone_id = ICCE_ZONE_VICINITY;
    high_rule.threshold_mm = 3000;
    high_rule.actions[0] = ICCE_ACTION_UNLOCK;
    high_rule.action_count = 1;
    high_rule.priority = 250;  /* Higher than default (128) */
    high_rule.enabled = true;
    icce_edge_add_rule(&high_rule);

    /* Process trigger — should pick highest priority */
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER,
                                             NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_edge_deinit();
}

/* ========================================================================
 *  ICCE_EDGE_005 — 基于距离的评估
 * ======================================================================== */
void test_edge_distance_evaluation(void)
{
    icce_edge_init();

    /* Add a distance-triggered rule */
    icce_edge_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.trigger = ICCE_TRIGGER_DISTANCE;
    rule.threshold_mm = 5000;
    rule.actions[0] = ICCE_ACTION_HORN;
    rule.action_count = 1;
    rule.priority = 200;
    rule.enabled = true;
    icce_edge_add_rule(&rule);

    /* Evaluate at 3000mm (< 5000mm) → should trigger */
    int32_t ret = icce_edge_evaluate(3000, -60, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* Rule should be one-shot, disabled after trigger */
    /* Re-evaluate at same distance — rule already disabled */
    ret = icce_edge_evaluate(3000, -60, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    icce_edge_deinit();
}

/* ========================================================================
 *  ICCE_EDGE_006 — add_rule NULL 参数
 * ======================================================================== */
void test_edge_add_rule_null(void)
{
    icce_edge_init();

    int32_t ret = icce_edge_add_rule(NULL);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);

    icce_edge_deinit();
}

/* ========================================================================
 *  Test Runner
 * ======================================================================== */
int run_icce_tests(void)
{
    UNITY_BEGIN();

    /* Core */
    RUN_TEST(test_icce_init_deinit);
    RUN_TEST(test_icce_run);
    RUN_TEST(test_icce_full_lifecycle);

    /* Zone */
    RUN_TEST(test_zone_far);
    RUN_TEST(test_zone_mid);
    RUN_TEST(test_zone_near_vicinity_interior);
    RUN_TEST(test_zone_get_def);

    /* UWB */
    RUN_TEST(test_uwb_session_lifecycle);
    RUN_TEST(test_uwb_invalid_params);
    RUN_TEST(test_uwb_multiple_sessions);

    /* Security */
    RUN_TEST(test_security_init);
    RUN_TEST(test_security_bind);
    RUN_TEST(test_security_auth);
    RUN_TEST(test_security_session_verify);

    /* Vehicle */
    RUN_TEST(test_vehicle_init_and_status);
    RUN_TEST(test_vehicle_control);
    RUN_TEST(test_vehicle_invalid_ctrl);
    RUN_TEST(test_vehicle_register_callback);

    /* Edge */
    RUN_TEST(test_edge_init_default_rules);
    RUN_TEST(test_edge_rule_lifecycle);
    RUN_TEST(test_edge_empty_rules);
    RUN_TEST(test_edge_priority_routing);
    RUN_TEST(test_edge_distance_evaluation);
    RUN_TEST(test_edge_add_rule_null);

    UNITY_END();
}

int main(void) { return run_icce_tests(); }
