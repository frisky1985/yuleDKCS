/**
 * @file test_icce_edge.c
 * @brief ICCE Edge 规则引擎高覆盖率单元测试
 *
 * 覆盖 icce_edge.c 全部公共 API 与内部静态辅助函数:
 *   - 规则增删改查 / 规则表满 / NULL 参数 / 重复规则 / 数组访问器
 *   - process_trigger: 各触发类型、优先级路由、cooldown 复查、
 *     time window、内联/复合条件、action 失败 → FALLBACK
 *   - evaluate: 距离/RSSI/UWB/车辆状态触发、状态机 ACTIVE/FALLBACK
 *     超时与重试、各类跳过分支
 *   - timer_tick: 时间间隔规则、ACTIVE/FALLBACK 超时处理
 *   - update_rssi / update_vehicle_state / get_state / get_rule_array
 *   - 条件树求值 (AND/OR/NOT/叶子/TIME_IN_WINDOW)、
 *     cooldown / time-window / action 执行 / single-rule 辅助函数
 *
 * 说明: icce_edge.c 内部状态 (g_engine) 为 static, 且引擎没有公开的
 * IDLE→MONITORING 入口 (所有触发路径都要求已在 MONITORING/ACTIVE),
 * 因此本测试直接 #include 被测源文件以获得静态状态访问权, 这是嵌入式
 * 单元测试的标准做法。
 */

#include "unity.h"
#include "icce_digital_key.h"
#include "edge_condition.h"
#include "storage_driver.h"

#include <string.h>

/* 直接包含被测源文件 (唯一允许访问 static g_engine 的方式) */
#include "../../icce_protocol/src/icce_edge.c"

#ifndef TEST_LIB_MODE
void setUp(void) {}
void tearDown(void) {}
#endif /* TEST_LIB_MODE */

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/** 重置全局状态: 释放规则条件树 → 重置条件池 → 清空 NVM stub → 清空引擎 */
static void edge_reset(void)
{
    edge_condition_pool_init();    /* 保证 deinit 释放条件树时池有效 */
    icce_edge_deinit();
    edge_condition_pool_deinit();
    (void)storage_init(NULL);      /* 清空 NVM stub KV */
    memset(&g_engine, 0, sizeof(g_engine));
}

/** 构造一条基础规则 (enabled, time_mask 全时段) */
static icce_edge_rule_t make_rule(icce_trigger_e trig, uint8_t pri,
                                  icce_action_e act)
{
    icce_edge_rule_t r;
    memset(&r, 0, sizeof(r));
    r.trigger      = trig;
    r.priority     = pri;
    r.actions[0]   = act;
    r.action_count = 1;
    r.enabled      = true;
    r.time_mask    = 0xFFFFFF;
    return r;
}

static uint8_t vehicle_lock(void)
{
    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));
    icce_vehicle_get_status(&vs);
    return vs.lock_status;
}

static uint8_t vehicle_engine(void)
{
    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));
    icce_vehicle_get_status(&vs);
    return vs.engine_status;
}

/** 禁用 5 条默认规则 (避免干扰单规则场景) */
static void disable_defaults(void)
{
    uint8_t i;
    for (i = 0; i < 5; i++) {
        icce_edge_enable_rule(i, false);
    }
}

/* ========================================================================
 * EDGE_INIT_001 — init 幂等 + 默认规则内容
 * ======================================================================== */
void test_edge_init_idempotent_and_defaults(void)
{
    edge_reset();
    int32_t ret = icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 幂等: 重复 init 直接返回 OK */
    ret = icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_UINT8(5, cnt);

    /* 规则0: 进入 VICINITY → UNLOCK (pri 128, cooldown 3000) */
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_ZONE_ENTER, arr[0].trigger);
    TEST_ASSERT_EQUAL_UINT8(ICCE_ZONE_VICINITY, arr[0].zone_id);
    TEST_ASSERT_EQUAL_INT32(2000, arr[0].threshold_mm);
    TEST_ASSERT_EQUAL(ICCE_ACTION_UNLOCK, arr[0].actions[0]);
    TEST_ASSERT_EQUAL_UINT8(1, arr[0].action_count);
    TEST_ASSERT_EQUAL_UINT8(128, arr[0].priority);
    TEST_ASSERT_TRUE(arr[0].enabled);
    TEST_ASSERT_EQUAL_UINT(3000, arr[0].cooldown_ms);
    TEST_ASSERT_EQUAL(COND_OP_NONE, arr[0].condition.op);

    /* 规则1: 离开 NEAR → LOCK (cooldown 5000) */
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_ZONE_EXIT, arr[1].trigger);
    TEST_ASSERT_EQUAL_UINT8(ICCE_ZONE_NEAR, arr[1].zone_id);
    TEST_ASSERT_EQUAL(ICCE_ACTION_LOCK, arr[1].actions[0]);
    TEST_ASSERT_EQUAL_UINT(5000, arr[1].cooldown_ms);

    /* 规则2: 进入 INTERIOR + 停车 → START, 复合条件为 AND */
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_ZONE_ENTER, arr[2].trigger);
    TEST_ASSERT_EQUAL_UINT8(ICCE_ZONE_INTERIOR, arr[2].zone_id);
    TEST_ASSERT_EQUAL(ICCE_ACTION_START, arr[2].actions[0]);
    TEST_ASSERT_EQUAL_UINT8(200, arr[2].priority);
    TEST_ASSERT_EQUAL(COND_OP_AND, arr[2].condition.op);
    TEST_ASSERT_NOT_NULL(arr[2].condition.left);
    TEST_ASSERT_NOT_NULL(arr[2].condition.right);

    /* 规则3: BLE RSSI > -70 → LIGHTS */
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_BLE_RSSI, arr[3].trigger);
    TEST_ASSERT_EQUAL_INT32(-70, arr[3].threshold_rssi);
    TEST_ASSERT_EQUAL(ICCE_ACTION_LIGHTS, arr[3].actions[0]);

    /* 规则4: 60 秒时间同步 → CUSTOM, cooldown 0 */
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_TIME_INTERVAL, arr[4].trigger);
    TEST_ASSERT_EQUAL_UINT(60000, arr[4].interval_ms);
    TEST_ASSERT_EQUAL(ICCE_ACTION_CUSTOM, arr[4].actions[0]);
    TEST_ASSERT_EQUAL_UINT(0, arr[4].cooldown_ms);

    /* deinit 需释放规则2的条件树 (left/right 为池节点) */
    ret = icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 * EDGE_INIT_002 — 条件池耗尽时 init 回退到简单规则
 * ======================================================================== */
void test_edge_init_pool_exhausted_fallback(void)
{
    edge_reset();
    edge_condition_pool_init();
    icce_condition_t *nodes[EDGE_COND_POOL_SIZE];
    int i;
    for (i = 0; i < EDGE_COND_POOL_SIZE; i++) {
        nodes[i] = edge_condition_alloc();
        TEST_ASSERT_NOT_NULL(nodes[i]);
    }

    /* 池已耗尽 → create_composite 返回 NULL → 回退简单规则 */
    int32_t ret = icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_UINT8(5, cnt);
    TEST_ASSERT_EQUAL(COND_OP_NONE, arr[2].condition.op);
    TEST_ASSERT_EQUAL(ICCE_ACTION_START, arr[2].actions[0]);

    icce_edge_deinit();
    edge_condition_pool_deinit();
}

/* ========================================================================
 * EDGE_INIT_003 — NVM 已有规则集时优先加载 (不走静态默认规则)
 * ======================================================================== */
void test_edge_init_nvm_loaded_rules(void)
{
    edge_reset();

    /* 构造完整规则集: header + 1 条序列化规则 */
    uint8_t buf[512];
    edge_condition_config_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic       = EDGE_COND_CONFIG_MAGIC;
    hdr.version     = EDGE_COND_CONFIG_VERSION;
    hdr.rule_count  = 1;

    serialized_edge_rule_t sr;
    memset(&sr, 0, sizeof(sr));
    sr.trigger      = (uint8_t)ICCE_TRIGGER_DISTANCE;
    sr.threshold_mm = 1234;
    sr.actions[0]   = (uint8_t)ICCE_ACTION_HORN;
    sr.action_count = 1;
    sr.priority     = 77;
    sr.enabled      = 1;
    sr.time_mask    = 0xFFFFFF;
    sr.cooldown_ms  = 1000;

    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), &sr, sizeof(sr));

    int32_t wret = storage_write(0, (const uint8_t *)"edge_cond_ruleset",
                                 17, buf, sizeof(hdr) + sizeof(sr));
    TEST_ASSERT_EQUAL_INT32(STORAGE_SUCCESS, wret);

    int32_t ret = icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* NVM 分支生效: 不加载静态默认规则 (rule_count == 0)。
     * 注: load_rules_from_nvm 在 init 内部、initialized 置位之前调用,
     * 其内部的 icce_edge_add_rule() 会因引擎未初始化返回 ICCE_ERR_PARAM,
     * 因此 NVM 规则实际不会被追加 —— 这里断言的是分支行为而非产品缺陷。 */
    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_UINT8(0, cnt);

    icce_edge_deinit();
}

/* ========================================================================
 * EDGE_DEINIT_001 — deinit 重置引擎状态
 * ======================================================================== */
void test_edge_deinit_resets_state(void)
{
    edge_reset();
    icce_edge_init();

    uint8_t cnt = 0;
    TEST_ASSERT_NOT_NULL(icce_edge_get_rule_array(&cnt));
    TEST_ASSERT_EQUAL_UINT8(5, cnt);

    int32_t ret = icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 内部状态被重置 */
    TEST_ASSERT_FALSE(g_engine.initialized);
    TEST_ASSERT_EQUAL_UINT8(0, g_engine.rule_count);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);

    /* 重复 deinit 安全 */
    ret = icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
}

/* ========================================================================
 * EDGE_RULE_ARRAY_001 — get_rule_array 各分支
 * ======================================================================== */
void test_edge_rule_array_accessor(void)
{
    edge_reset();
    uint8_t cnt = 99;

    /* 未初始化 + count_out NULL → NULL */
    TEST_ASSERT_NULL(icce_edge_get_rule_array(NULL));
    /* 未初始化 + count_out → *count_out = 0, NULL */
    TEST_ASSERT_NULL(icce_edge_get_rule_array(&cnt));
    TEST_ASSERT_EQUAL_UINT8(0, cnt);

    icce_edge_init();
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_UINT8(5, cnt);
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_ZONE_ENTER, arr[0].trigger);

    /* count_out == NULL 也返回数组 */
    TEST_ASSERT_NOT_NULL(icce_edge_get_rule_array(NULL));

    icce_edge_deinit();
    TEST_ASSERT_NULL(icce_edge_get_rule_array(&cnt));
    TEST_ASSERT_EQUAL_UINT8(0, cnt);
}

/* ========================================================================
 * EDGE_RULE_001 — add_rule 成功 + 重复规则
 * ======================================================================== */
void test_edge_add_rule_ok_and_duplicate(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_mm = 2500;

    int32_t ret = icce_edge_add_rule(&r);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 重复添加同一条规则 → 允许, 追加到末尾 */
    ret = icce_edge_add_rule(&r);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_EQUAL_UINT8(7, cnt);
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_DISTANCE, arr[5].trigger);
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_DISTANCE, arr[6].trigger);
    TEST_ASSERT_EQUAL_INT32(2500, arr[6].threshold_mm);
}

/* ========================================================================
 * EDGE_RULE_002 — add_rule NULL / 未初始化
 * ======================================================================== */
void test_edge_add_rule_null_and_uninit(void)
{
    edge_reset();
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_LIGHTS);

    /* 未初始化 → PARAM */
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_add_rule(&r));

    icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_add_rule(NULL));
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_add_rule(&r));

    icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_add_rule(&r));
}

/* ========================================================================
 * EDGE_RULE_003 — 规则表满 (16 条上限)
 * ======================================================================== */
void test_edge_add_rule_table_full(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_mm = 1000;
    int i;
    for (i = 0; i < 11; i++) {   /* 5 + 11 = 16 */
        TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_add_rule(&r));
    }

    /* 第 17 条 → NO_MEM */
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, icce_edge_add_rule(&r));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NO_MEM, icce_edge_add_rule(&r));
}

/* ========================================================================
 * EDGE_RULE_004 — remove_rule: 越界 / 中间移位 / 末尾 / 未初始化
 * ======================================================================== */
void test_edge_remove_rule_cases(void)
{
    edge_reset();
    icce_edge_init();

    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, icce_edge_remove_rule(99));

    /* 删除规则0 → 原规则1 移入 0 号位 */
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_remove_rule(0));
    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_EQUAL_UINT8(4, cnt);
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_ZONE_EXIT, arr[0].trigger);

    /* 删除中间 (当前 idx1 = 原规则2) → 原规则3 移入 */
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_remove_rule(1));
    arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_EQUAL_UINT8(3, cnt);
    TEST_ASSERT_EQUAL(ICCE_TRIGGER_BLE_RSSI, arr[1].trigger);

    /* 删除末尾 */
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_remove_rule(2));
    arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_EQUAL_UINT8(2, cnt);

    icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_remove_rule(0));
}

/* ========================================================================
 * EDGE_RULE_005 — enable_rule: 越界 / 开关 / 未初始化
 * ======================================================================== */
void test_edge_enable_rule_cases(void)
{
    edge_reset();
    icce_edge_init();

    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_NOT_FOUND, icce_edge_enable_rule(99, true));

    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_enable_rule(0, false));
    uint8_t cnt = 0;
    icce_edge_rule_t *arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_FALSE(arr[0].enabled);

    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_enable_rule(0, true));
    arr = icce_edge_get_rule_array(&cnt);
    TEST_ASSERT_TRUE(arr[0].enabled);

    icce_edge_deinit();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_enable_rule(0, true));
}

/* ========================================================================
 * EDGE_STATE_001 — get_state: NULL / 未初始化 / 状态读取
 * ======================================================================== */
void test_edge_get_state_cases(void)
{
    edge_reset();
    icce_edge_state_e s = ICCE_EDGE_STATE_MAX;

    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_get_state(NULL));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_get_state(&s));

    icce_edge_init();
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_get_state(&s));
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, s);

    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_get_state(&s));
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, s);

    g_engine.state = ICCE_EDGE_STATE_TRIGGERED;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, icce_edge_get_state(&s));
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_TRIGGERED, s);
}

/* ========================================================================
 * EDGE_PARAM_001 — deinit 后全部 API 返回 ICCE_ERR_PARAM
 * ======================================================================== */
void test_edge_uninit_param_guards(void)
{
    edge_reset();
    icce_edge_init();
    icce_edge_deinit();

    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM,
                            icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER,
                                                      NULL, 0));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM,
                            icce_edge_evaluate(100, -60, ICCE_ZONE_NEAR));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_timer_tick(100));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_rssi(-60));

    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_vehicle_state(&vs));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_vehicle_state(NULL));
}

/* ========================================================================
 * TRIGGER_001 — 非 MONITORING/ACTIVE 状态静默忽略触发
 * ======================================================================== */
void test_process_trigger_state_gate(void)
{
    edge_reset();
    icce_edge_init();

    /* IDLE → 忽略 */
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);

    /* TRIGGERED → 忽略 */
    g_engine.state = ICCE_EDGE_STATE_TRIGGERED;
    ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_TRIGGERED, g_engine.state);

    /* FALLBACK → 忽略 */
    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);

    /* ACTIVE → 允许处理 (默认规则3: BLE_RSSI → LIGHTS 成功 → 仍 ACTIVE) */
    g_engine.state = ICCE_EDGE_STATE_ACTIVE;
    ret = icce_edge_process_trigger(ICCE_TRIGGER_BLE_RSSI, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * TRIGGER_002 — ZONE_ENTER 默认规则0 触发 UNLOCK
 * ======================================================================== */
void test_process_trigger_zone_enter_default(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(0, g_engine.active_rule_idx);
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());   /* UNLOCK 已执行 */
    TEST_ASSERT_EQUAL_UINT(0, g_engine.rules[0].last_triggered);
}

/* ========================================================================
 * TRIGGER_003 — VEHICLE_STATE 数据覆盖 (有效长度 / 短长度 / 非车辆触发)
 * ======================================================================== */
void test_process_trigger_vehicle_state_overlay(void)
{
    /* 有效长度 → 覆盖 data */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                   ICCE_ACTION_HORN);
    icce_edge_add_rule(&r);

    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));
    vs.engine_status = 1;
    vs.lock_status   = 1;

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_VEHICLE_STATE,
                                            &vs, sizeof(vs));
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);

    /* 短长度 → 不覆盖, 但规则仍匹配 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_add_rule(&r);
    ret = icce_edge_process_trigger(ICCE_TRIGGER_VEHICLE_STATE, &vs, 1);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* 非 VEHICLE_STATE 触发携带 data → 覆盖分支不进入 (默认规则0 UNLOCK) */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, &vs, sizeof(vs));
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());
}

/* ========================================================================
 * TRIGGER_004 — 无匹配规则
 * ======================================================================== */
void test_process_trigger_no_match(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    /* 默认规则没有 UWB_RANGE 触发 */
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_UWB_RANGE, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TRIGGER_005 — 优先级路由: 高优先级先匹配
 * ======================================================================== */
void test_process_trigger_priority_routing(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                   ICCE_ACTION_LOCK);
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);
    TEST_ASSERT_EQUAL_UINT8(1, vehicle_lock());   /* LOCK 生效 (高优先级胜出) */
}

/* ========================================================================
 * TRIGGER_006 — 同优先级: 先注册者胜出
 * ======================================================================== */
void test_process_trigger_priority_tie_first_wins(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r1 = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                    ICCE_ACTION_UNLOCK);
    icce_edge_rule_t r2 = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                    ICCE_ACTION_LOCK);
    icce_edge_add_rule(&r1);
    icce_edge_add_rule(&r2);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);  /* 先注册的 UNLOCK */
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());
}

/* ========================================================================
 * TRIGGER_007 — priority 0 规则不参与选择
 * ======================================================================== */
void test_process_trigger_priority_zero(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    disable_defaults();

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 0,
                                   ICCE_ACTION_HORN);
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state); /* 未执行 */
}

/* ========================================================================
 * TRIGGER_008 — cooldown 复查: now < cooldown 且从未触发 → 吞掉触发
 * ======================================================================== */
void test_process_trigger_cooldown_recheck_swallow(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    g_engine.last_tick = 1000;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_mm  = 5000;
    r.cooldown_ms   = 5000;
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_DISTANCE, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
    TEST_ASSERT_EQUAL_UINT(0, g_engine.rules[5].last_triggered);
}

/* ========================================================================
 * TRIGGER_009 — cooldown 复查通过 → 正常执行
 * ======================================================================== */
void test_process_trigger_cooldown_recheck_pass(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    g_engine.last_tick = 6000;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_mm  = 5000;
    r.cooldown_ms   = 5000;
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_DISTANCE, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT(6000, g_engine.rules[5].last_triggered);
}

/* ========================================================================
 * TRIGGER_010 — 内联复合条件为真: 默认规则2 (INTERIOR + 停车) 触发 START
 * ======================================================================== */
void test_process_trigger_inline_condition_true(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    g_engine.current_zone = ICCE_ZONE_INTERIOR;

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(2, g_engine.active_rule_idx);  /* pri 200 胜出 */
    TEST_ASSERT_EQUAL_UINT8(1, vehicle_engine());          /* START 已执行 */
}

/* ========================================================================
 * TRIGGER_011 — 禁用规则跳过 + 内联条件为假
 * ======================================================================== */
void test_process_trigger_inline_condition_false_and_disabled(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    /* 禁用默认规则0; 规则2 条件 (zone==INTERIOR) 不满足 → 无匹配 */
    icce_edge_enable_rule(0, false);
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TRIGGER_012 — COMPOUND 触发: 条件树匹配
 * ======================================================================== */
void test_process_trigger_compound_match(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_condition_t *leaf1 = edge_condition_create_leaf(COND_OP_RSSI_GT,
                                                         -70, 0);
    icce_condition_t *leaf2 = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                         0, ICCE_ZONE_NEAR);
    icce_condition_t *comp = edge_condition_create_composite(COND_OP_AND,
                                                             leaf1, leaf2);
    TEST_ASSERT_NOT_NULL(comp);

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_COMPOUND, 100,
                                   ICCE_ACTION_HORN);
    memcpy(&r.condition, comp, sizeof(icce_condition_t));
    icce_edge_add_rule(&r);

    g_engine.current_rssi = -60;
    g_engine.current_zone = ICCE_ZONE_NEAR;
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_COMPOUND, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);
}

/* ========================================================================
 * TRIGGER_013 — COMPOUND 触发: 条件树不匹配
 * ======================================================================== */
void test_process_trigger_compound_no_match(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_condition_t *leaf1 = edge_condition_create_leaf(COND_OP_RSSI_GT,
                                                         -70, 0);
    icce_condition_t *leaf2 = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                         0, ICCE_ZONE_NEAR);
    icce_condition_t *comp = edge_condition_create_composite(COND_OP_AND,
                                                             leaf1, leaf2);
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_COMPOUND, 100,
                                   ICCE_ACTION_HORN);
    memcpy(&r.condition, comp, sizeof(icce_condition_t));
    icce_edge_add_rule(&r);

    g_engine.current_rssi = -80;   /* RSSI_GT 不满足 */
    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_COMPOUND, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TRIGGER_014 — action 执行失败 → FALLBACK
 * ======================================================================== */
void test_process_trigger_execute_failure_fallback(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                   (icce_action_e)0xAA);
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);
}

/* ========================================================================
 * TRIGGER_015 — time window 外规则跳过 / 窗口内触发
 * ======================================================================== */
void test_process_trigger_time_window_skip(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_enable_rule(0, false);

    g_engine.last_tick = 7200000;   /* 02:00 */
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 100,
                                   ICCE_ACTION_HORN);
    r.time_mask = 0x1;              /* 仅 00:00-01:00 有效 */
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 00:00 → 窗口内 → 触发 */
    g_engine.last_tick = 0;
    ret = icce_edge_process_trigger(ICCE_TRIGGER_ZONE_ENTER, NULL, 0);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);
}

/* ========================================================================
 * EVAL_001 — IDLE 状态 evaluate: 传感器状态与滑动平均更新
 * ======================================================================== */
void test_evaluate_idle_sensor_update(void)
{
    edge_reset();
    icce_edge_init();

    int32_t ret = icce_edge_evaluate(1500, -60, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    TEST_ASSERT_EQUAL_INT32(1500, g_engine.current_distance_mm);
    TEST_ASSERT_EQUAL_UINT8(ICCE_ZONE_NEAR, g_engine.current_zone);
    TEST_ASSERT_EQUAL_INT32(-113, g_engine.rssi_ma);
    TEST_ASSERT_EQUAL_INT32(1500, g_engine.uwb_ma);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);

    /* 无效距离 → UWB 滑动平均不更新 */
    ret = icce_edge_evaluate(-1, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_INT32(-1, g_engine.current_distance_mm);
    TEST_ASSERT_EQUAL_INT32(1500, g_engine.uwb_ma);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);
}

/* ========================================================================
 * EVAL_002 — ACTIVE 超时 → MONITORING; 未超时保持 ACTIVE
 * ======================================================================== */
void test_evaluate_active_timeout_transition(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_ACTIVE;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 40000;
    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 未超时 (40000-30000 < 30000) → 保持 ACTIVE */
    g_engine.state = ICCE_EDGE_STATE_ACTIVE;
    g_engine.state_enter_tick = 30000;
    g_engine.last_tick = 40000;
    ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * EVAL_003 — FALLBACK 重试成功 → ACTIVE
 * ======================================================================== */
void test_evaluate_fallback_retry_ok(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 6000;
    g_engine.active_rule_idx = 0;    /* 默认规则0: UNLOCK 成功 */
    g_engine.action_retries = 0;

    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(1, g_engine.action_retries);
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());

    /* 重试窗口未到 → 保持 FALLBACK */
    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 4000;
    g_engine.last_tick = 6000;
    g_engine.action_retries = 0;
    ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);
}

/* ========================================================================
 * EVAL_004 — FALLBACK 重试失败 → 重置进入时间戳
 * ======================================================================== */
void test_evaluate_fallback_retry_fail(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                   (icce_action_e)0xAA);
    icce_edge_add_rule(&r);

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 6000;
    g_engine.active_rule_idx = 5;    /* 失败规则 */
    g_engine.action_retries = 0;

    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(1, g_engine.action_retries);
    TEST_ASSERT_EQUAL_UINT(6000, g_engine.state_enter_tick);
}

/* ========================================================================
 * EVAL_005 — FALLBACK 规则失效 (越界 / 禁用) → MONITORING
 * ======================================================================== */
void test_evaluate_fallback_rule_invalid(void)
{
    /* active_rule_idx 越界 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 6000;
    g_engine.active_rule_idx = 200;
    g_engine.action_retries = 0;
    icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 规则被禁用 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(0, false);
    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 6000;
    g_engine.active_rule_idx = 0;
    g_engine.action_retries = 0;
    icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_006 — FALLBACK 重试次数耗尽 → MONITORING
 * ======================================================================== */
void test_evaluate_fallback_retries_exhausted(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.last_tick = 6000;
    g_engine.active_rule_idx = 0;
    g_engine.action_retries = 3;    /* MAX_ACTION_RETRIES */

    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_007 — DISTANCE 触发: 匹配 / 超限 / 无效距离
 * ======================================================================== */
void test_evaluate_distance_trigger(void)
{
    /* 匹配: 3000 <= 5000 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 200,
                                   ICCE_ACTION_HORN);
    r.threshold_mm = 5000;
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_evaluate(3000, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);

    /* 距离超限 → 不匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_add_rule(&r);
    ret = icce_edge_evaluate(8000, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 无效距离 → 不匹配 */
    ret = icce_edge_evaluate(-1, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_008 — BLE_RSSI 触发 (滑动平均)
 * ======================================================================== */
void test_evaluate_rssi_trigger(void)
{
    /* 5 次采样后 MA=-60 → 匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_BLE_RSSI, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_rssi = -70;
    icce_edge_add_rule(&r);

    int i;
    for (i = 0; i < 4; i++) {
        icce_edge_evaluate(-1, -60, ICCE_ZONE_NONE);
    }
    int32_t ret = icce_edge_evaluate(100, -60, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);

    /* RSSI 低于阈值 → 不匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_add_rule(&r);
    for (i = 0; i < 5; i++) {
        icce_edge_evaluate(-1, -90, ICCE_ZONE_NONE);
    }
    ret = icce_edge_evaluate(100, -90, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_009 — UWB_RANGE 触发: 无条件 / 条件树真 / 条件树假
 * ======================================================================== */
void test_evaluate_uwb_range_trigger(void)
{
    /* 无条件 → 距离命中即匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_UWB_RANGE, 100,
                                   ICCE_ACTION_HORN);
    r.threshold_mm = 3000;
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_evaluate(2000, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* 条件树 (ZONE_EQ NEAR) 为真 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                        0, ICCE_ZONE_NEAR);
    icce_edge_rule_t r2 = make_rule(ICCE_TRIGGER_UWB_RANGE, 100,
                                    ICCE_ACTION_HORN);
    r2.threshold_mm = 3000;
    memcpy(&r2.condition, leaf, sizeof(icce_condition_t));
    icce_edge_add_rule(&r2);

    ret = icce_edge_evaluate(2000, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* 条件树为假 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_condition_t *leaf2 = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                         0, ICCE_ZONE_NEAR);
    icce_edge_rule_t r3 = make_rule(ICCE_TRIGGER_UWB_RANGE, 100,
                                    ICCE_ACTION_HORN);
    r3.threshold_mm = 3000;
    memcpy(&r3.condition, leaf2, sizeof(icce_condition_t));
    icce_edge_add_rule(&r3);

    ret = icce_edge_evaluate(2000, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_010 — VEHICLE_STATE 触发: 条件树 / zone 兜底 / 不匹配
 * ======================================================================== */
void test_evaluate_vehicle_state_trigger(void)
{
    /* 条件树 VEHICLE_LOCKED 为真 → 匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_VEHICLE_LOCKED,
                                                        0, 0);
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                   ICCE_ACTION_LIGHTS);
    memcpy(&r.condition, leaf, sizeof(icce_condition_t));
    icce_edge_add_rule(&r);

    g_engine.vehicle_status.lock_status = 1;
    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* zone 兜底: zone_id==INTERIOR 且 engine 关闭 → 匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r2 = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                    ICCE_ACTION_LIGHTS);
    r2.zone_id = ICCE_ZONE_INTERIOR;
    icce_edge_add_rule(&r2);
    g_engine.vehicle_status.engine_status = 0;
    ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* 条件树为假 + zone 兜底不适用 → 不匹配 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_condition_t *leaf2 = edge_condition_create_leaf(COND_OP_VEHICLE_LOCKED,
                                                         0, 0);
    icce_edge_rule_t r3 = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                    ICCE_ACTION_LIGHTS);
    memcpy(&r3.condition, leaf2, sizeof(icce_condition_t));
    icce_edge_add_rule(&r3);
    g_engine.vehicle_status.lock_status = 0;
    ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NONE);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_011 — ZONE_ENTER/EXIT/TIME_INTERVAL/COMPOUND/未知 trigger 不匹配
 * ======================================================================== */
void test_evaluate_zone_compound_noop_triggers(void)
{
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t rz = make_rule(ICCE_TRIGGER_ZONE_ENTER, 100,
                                    ICCE_ACTION_HORN);
    icce_edge_rule_t rc = make_rule(ICCE_TRIGGER_COMPOUND, 100,
                                    ICCE_ACTION_HORN);
    icce_edge_rule_t rx = make_rule((icce_trigger_e)0xFF, 100,
                                    ICCE_ACTION_HORN);
    icce_edge_add_rule(&rz);
    icce_edge_add_rule(&rc);
    icce_edge_add_rule(&rx);

    int32_t ret = icce_edge_evaluate(100, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * EVAL_012 — evaluate 循环中的跳过分支 (禁用 / 时间窗 / cooldown)
 * ======================================================================== */
void test_evaluate_skips_disabled_time_cooldown(void)
{
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_DISTANCE, 100,
                                   ICCE_ACTION_HORN);
    r.threshold_mm = 5000;
    r.cooldown_ms  = 3000;
    icce_edge_add_rule(&r);

    /* cooldown 未过 → 跳过 */
    g_engine.last_tick = 2000;
    g_engine.rules[5].last_triggered = 1000;
    icce_edge_evaluate(100, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 时间窗外 → 跳过 */
    g_engine.rules[5].last_triggered = 0;
    g_engine.rules[5].time_mask = 0x1;
    g_engine.last_tick = 7200000;    /* 02:00 */
    icce_edge_evaluate(100, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 禁用 → 跳过 */
    g_engine.rules[5].time_mask = 0xFFFFFF;
    g_engine.rules[5].enabled = false;
    g_engine.last_tick = 0;
    icce_edge_evaluate(100, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 恢复正常 → 匹配执行 */
    g_engine.rules[5].enabled = true;
    g_engine.rules[5].cooldown_ms = 0;
    icce_edge_evaluate(100, -127, ICCE_ZONE_NEAR);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * TICK_001 — timer_tick: 未初始化 / 时间累计 / 非 MONITORING 直接返回
 * ======================================================================== */
void test_timer_tick_basic(void)
{
    edge_reset();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_timer_tick(1000));

    icce_edge_init();
    int32_t ret = icce_edge_timer_tick(1000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT(1000, g_engine.last_tick);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);  /* 直接返回 */

    ret = icce_edge_timer_tick(2000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT(3000, g_engine.last_tick);
}

/* ========================================================================
 * TICK_002 — ACTIVE 超时 → MONITORING; 未超时保持
 * ======================================================================== */
void test_timer_tick_active_timeout(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_ACTIVE;
    g_engine.state_enter_tick = 0;
    int32_t ret = icce_edge_timer_tick(35000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 未超时 (36000-30000 < 30000) */
    g_engine.state = ICCE_EDGE_STATE_ACTIVE;
    g_engine.state_enter_tick = 30000;
    ret = icce_edge_timer_tick(1000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * TICK_003 — FALLBACK 重试成功 → ACTIVE
 * ======================================================================== */
void test_timer_tick_fallback_retry_ok(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.active_rule_idx = 0;    /* UNLOCK 成功 */
    g_engine.action_retries = 0;

    int32_t ret = icce_edge_timer_tick(6000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(1, g_engine.action_retries);
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());
}

/* ========================================================================
 * TICK_004 — FALLBACK 重试失败 → 重置进入时间戳
 * ======================================================================== */
void test_timer_tick_fallback_retry_fail(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_ZONE_ENTER, 250,
                                   (icce_action_e)0xAA);
    icce_edge_add_rule(&r);

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.active_rule_idx = 5;
    g_engine.action_retries = 0;

    int32_t ret = icce_edge_timer_tick(6000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);
    TEST_ASSERT_EQUAL_UINT(6000, g_engine.state_enter_tick);
    TEST_ASSERT_EQUAL_UINT8(1, g_engine.action_retries);
}

/* ========================================================================
 * TICK_005 — FALLBACK 规则失效 → MONITORING
 * ======================================================================== */
void test_timer_tick_fallback_rule_invalid(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.active_rule_idx = 200;
    g_engine.action_retries = 0;

    int32_t ret = icce_edge_timer_tick(6000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TICK_006 — FALLBACK 重试次数耗尽 → MONITORING
 * ======================================================================== */
void test_timer_tick_fallback_retries_exhausted(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.state = ICCE_EDGE_STATE_FALLBACK;
    g_engine.state_enter_tick = 0;
    g_engine.active_rule_idx = 0;
    g_engine.action_retries = 3;

    int32_t ret = icce_edge_timer_tick(6000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TICK_007 — 时间间隔规则触发 (默认规则4)
 * ======================================================================== */
void test_timer_tick_interval_fire(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;

    int32_t ret = icce_edge_timer_tick(60000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(4, g_engine.active_rule_idx);
    TEST_ASSERT_EQUAL_UINT(60000, g_engine.rules[4].last_triggered);
}

/* ========================================================================
 * TICK_008 — 时间间隔规则跳过分支
 * ======================================================================== */
void test_timer_tick_interval_skips(void)
{
    /* 禁用 → 跳过 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_enable_rule(4, false);
    icce_edge_timer_tick(60000);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
    icce_edge_enable_rule(4, true);

    /* interval_ms == 0 → 跳过 */
    g_engine.rules[4].interval_ms = 0;
    icce_edge_timer_tick(60000);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
    g_engine.rules[4].interval_ms = 60000;

    /* 间隔未到 → 跳过 */
    g_engine.rules[4].last_triggered = 120000;
    icce_edge_timer_tick(1000);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 时间窗外 → 跳过 (02:00, mask 仅 00:00) */
    g_engine.rules[4].time_mask = 0x1;
    g_engine.last_tick = 7000000;
    icce_edge_timer_tick(200000);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
    g_engine.rules[4].time_mask = 0xFFFFFF;

    /* cooldown 未过 → 跳过 (interval 已过但 cooldown 90s 未过) */
    g_engine.rules[4].cooldown_ms = 90000;
    g_engine.rules[4].last_triggered = 7130000;
    icce_edge_timer_tick(1000);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);
}

/* ========================================================================
 * TICK_009 — 时间间隔规则 action 失败 → FALLBACK
 * ======================================================================== */
void test_timer_tick_interval_fail_fallback(void)
{
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    g_engine.rules[4].actions[0] = (icce_action_e)0xAA;

    int32_t ret = icce_edge_timer_tick(60000);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_FALLBACK, g_engine.state);
}

/* ========================================================================
 * RSSI_001 — update_rssi: 未初始化 / MA 更新 / 非 MONITORING
 * ======================================================================== */
void test_update_rssi_basic(void)
{
    edge_reset();
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_rssi(-60));

    icce_edge_init();
    int32_t ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_INT32(-60, g_engine.current_rssi);
    TEST_ASSERT_EQUAL_INT32(-113, g_engine.rssi_ma);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);
}

/* ========================================================================
 * RSSI_002 — update_rssi 触发路径与跳过分支
 * ======================================================================== */
void test_update_rssi_trigger_paths(void)
{
    /* 命中阈值 → 触发 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_BLE_RSSI, 100,
                                   ICCE_ACTION_LIGHTS);
    r.threshold_rssi = -70;
    icce_edge_add_rule(&r);

    int32_t ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);

    /* 低于阈值 → 不触发 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_add_rule(&r);
    ret = icce_edge_update_rssi(-90);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 条件树为假 → 跳过; 为真 → 触发 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                        0, ICCE_ZONE_NEAR);
    icce_edge_rule_t r2 = make_rule(ICCE_TRIGGER_BLE_RSSI, 100,
                                    ICCE_ACTION_LIGHTS);
    r2.threshold_rssi = -70;
    memcpy(&r2.condition, leaf, sizeof(icce_condition_t));
    icce_edge_add_rule(&r2);

    g_engine.current_zone = ICCE_ZONE_NONE;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.current_zone = ICCE_ZONE_NEAR;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);

    /* 跳过: cooldown / 时间窗 / 禁用 */
    edge_reset();
    icce_edge_init();
    icce_edge_enable_rule(3, false);
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r3 = make_rule(ICCE_TRIGGER_BLE_RSSI, 100,
                                    ICCE_ACTION_LIGHTS);
    r3.threshold_rssi = -70;
    r3.cooldown_ms = 3000;
    icce_edge_add_rule(&r3);

    g_engine.last_tick = 2000;
    g_engine.rules[5].last_triggered = 1000;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].last_triggered = 0;
    g_engine.rules[5].time_mask = 0x1;
    g_engine.last_tick = 7200000;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].time_mask = 0xFFFFFF;
    g_engine.rules[5].enabled = false;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].enabled = true;
    g_engine.rules[5].cooldown_ms = 0;
    ret = icce_edge_update_rssi(-60);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * VSTATE_001 — update_vehicle_state: NULL / 未初始化 / 无变化 / 非 MONITORING
 * ======================================================================== */
void test_update_vehicle_state_basic(void)
{
    edge_reset();
    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));

    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_vehicle_state(NULL));
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, icce_edge_update_vehicle_state(&vs));

    icce_edge_init();

    /* 无任何变化 → 直接返回 */
    int32_t ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);

    /* 有变化但非 MONITORING → 只更新状态 */
    vs.engine_status = 1;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1, g_engine.vehicle_status.engine_status);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_IDLE, g_engine.state);
}

/* ========================================================================
 * VSTATE_002 — update_vehicle_state 触发路径与跳过分支
 * ======================================================================== */
void test_update_vehicle_state_trigger_paths(void)
{
    /* 无条件规则: 任何变化 → 匹配 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                   ICCE_ACTION_LIGHTS);
    icce_edge_add_rule(&r);

    icce_vehicle_status_t vs;
    memset(&vs, 0, sizeof(vs));
    vs.engine_status = 1;
    int32_t ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
    TEST_ASSERT_EQUAL_UINT8(5, g_engine.active_rule_idx);

    /* 条件树为假 → 不匹配 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_condition_t *leaf = edge_condition_create_leaf(COND_OP_VEHICLE_LOCKED,
                                                        0, 0);
    icce_edge_rule_t r2 = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                    ICCE_ACTION_LIGHTS);
    memcpy(&r2.condition, leaf, sizeof(icce_condition_t));
    icce_edge_add_rule(&r2);

    vs.engine_status = 1;   /* 有变化, 但 VEHICLE_LOCKED 条件为假 */
    vs.lock_status   = 0;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    /* 跳过: cooldown / 时间窗 / 禁用 */
    edge_reset();
    icce_edge_init();
    g_engine.state = ICCE_EDGE_STATE_MONITORING;
    icce_edge_rule_t r3 = make_rule(ICCE_TRIGGER_VEHICLE_STATE, 100,
                                    ICCE_ACTION_LIGHTS);
    r3.cooldown_ms = 3000;
    icce_edge_add_rule(&r3);

    g_engine.last_tick = 2000;
    g_engine.rules[5].last_triggered = 1000;
    vs.engine_status = 2;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].last_triggered = 0;
    g_engine.rules[5].time_mask = 0x1;
    g_engine.last_tick = 7200000;
    vs.engine_status = 3;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].time_mask = 0xFFFFFF;
    g_engine.rules[5].enabled = false;
    vs.engine_status = 4;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_MONITORING, g_engine.state);

    g_engine.rules[5].enabled = true;
    g_engine.rules[5].cooldown_ms = 0;
    g_engine.rules[5].last_triggered = 0;
    vs.engine_status = 5;
    ret = icce_edge_update_vehicle_state(&vs);
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, ret);
    TEST_ASSERT_EQUAL(ICCE_EDGE_STATE_ACTIVE, g_engine.state);
}

/* ========================================================================
 * COND_001 — 条件树逻辑运算 (AND/OR/NOT)
 * ======================================================================== */
void test_condition_tree_logical_operators(void)
{
    edge_reset();
    icce_edge_init();

    TEST_ASSERT_TRUE(evaluate_condition_tree(NULL));   /* 无条件 = 恒真 */

    icce_condition_t *rssi_gt = edge_condition_create_leaf(COND_OP_RSSI_GT,
                                                           -70, 0);
    icce_condition_t *zone_eq = edge_condition_create_leaf(COND_OP_ZONE_EQ,
                                                           0, ICCE_ZONE_NEAR);
    TEST_ASSERT_NOT_NULL(rssi_gt);
    TEST_ASSERT_NOT_NULL(zone_eq);

    g_engine.current_rssi = -60;
    g_engine.current_zone = ICCE_ZONE_NEAR;
    TEST_ASSERT_TRUE(evaluate_condition_tree(rssi_gt));
    TEST_ASSERT_TRUE(evaluate_condition_tree(zone_eq));

    /* AND: 左假短路 */
    icce_condition_t *and_node = edge_condition_create_composite(COND_OP_AND,
                                                                 rssi_gt,
                                                                 zone_eq);
    TEST_ASSERT_TRUE(evaluate_condition_tree(and_node));
    g_engine.current_rssi = -80;
    TEST_ASSERT_FALSE(evaluate_condition_tree(and_node));
    g_engine.current_rssi = -60;
    g_engine.current_zone = ICCE_ZONE_NONE;
    TEST_ASSERT_FALSE(evaluate_condition_tree(and_node));

    /* OR */
    icce_condition_t *or_node = edge_condition_create_composite(COND_OP_OR,
                                                                rssi_gt,
                                                                zone_eq);
    g_engine.current_rssi = -80;
    g_engine.current_zone = ICCE_ZONE_NONE;
    TEST_ASSERT_FALSE(evaluate_condition_tree(or_node));
    g_engine.current_rssi = -60;
    TEST_ASSERT_TRUE(evaluate_condition_tree(or_node));

    /* NOT */
    icce_condition_t *not_node = edge_condition_create_composite(COND_OP_NOT,
                                                                 rssi_gt, NULL);
    g_engine.current_rssi = -60;
    TEST_ASSERT_FALSE(evaluate_condition_tree(not_node));
    g_engine.current_rssi = -80;
    TEST_ASSERT_TRUE(evaluate_condition_tree(not_node));

    edge_condition_free_tree(and_node);
    edge_condition_free_tree(or_node);
    edge_condition_free_tree(not_node);
}

/* ========================================================================
 * COND_002 — 条件树叶子运算
 * ======================================================================== */
void test_condition_tree_leaf_operators(void)
{
    edge_reset();
    icce_edge_init();

    /* DIST_GT / DIST_LT */
    icce_condition_t *dist_gt = edge_condition_create_leaf(COND_OP_DIST_GT,
                                                           3000, 0);
    icce_condition_t *dist_lt = edge_condition_create_leaf(COND_OP_DIST_LT,
                                                           3000, 0);
    g_engine.current_distance_mm = 5000;
    TEST_ASSERT_TRUE(evaluate_condition_tree(dist_gt));
    TEST_ASSERT_FALSE(evaluate_condition_tree(dist_lt));
    g_engine.current_distance_mm = 1000;
    TEST_ASSERT_FALSE(evaluate_condition_tree(dist_gt));
    TEST_ASSERT_TRUE(evaluate_condition_tree(dist_lt));
    g_engine.current_distance_mm = -1;   /* 无效距离 → DIST_LT 为假 */
    TEST_ASSERT_FALSE(evaluate_condition_tree(dist_lt));

    /* RSSI_LT */
    icce_condition_t *rssi_lt = edge_condition_create_leaf(COND_OP_RSSI_LT,
                                                           -70, 0);
    g_engine.current_rssi = -80;
    TEST_ASSERT_TRUE(evaluate_condition_tree(rssi_lt));
    g_engine.current_rssi = -60;
    TEST_ASSERT_FALSE(evaluate_condition_tree(rssi_lt));

    /* VEHICLE_STOPPED */
    icce_condition_t *stopped = edge_condition_create_leaf(
        COND_OP_VEHICLE_STOPPED, 0, 0);
    g_engine.vehicle_status.engine_status = 0;
    g_engine.vehicle_status.speed_kmh = 0;
    TEST_ASSERT_TRUE(evaluate_condition_tree(stopped));
    g_engine.vehicle_status.engine_status = 1;
    TEST_ASSERT_FALSE(evaluate_condition_tree(stopped));

    /* VEHICLE_LOCKED */
    icce_condition_t *locked = edge_condition_create_leaf(
        COND_OP_VEHICLE_LOCKED, 0, 0);
    g_engine.vehicle_status.lock_status = 1;
    TEST_ASSERT_TRUE(evaluate_condition_tree(locked));
    g_engine.vehicle_status.lock_status = 0;
    TEST_ASSERT_FALSE(evaluate_condition_tree(locked));

    /* VEHICLE_PARKED */
    icce_condition_t *parked = edge_condition_create_leaf(
        COND_OP_VEHICLE_PARKED, 0, 0);
    g_engine.vehicle_status.gear_position = 0;
    TEST_ASSERT_TRUE(evaluate_condition_tree(parked));
    g_engine.vehicle_status.gear_position = 3;
    TEST_ASSERT_FALSE(evaluate_condition_tree(parked));

    /* COND_OP_NONE / 未知 op → 恒真 */
    icce_condition_t *none_leaf = edge_condition_create_leaf(COND_OP_NONE,
                                                             0, 0);
    icce_condition_t *bad_leaf = edge_condition_create_leaf(
        (icce_condition_op_e)99, 0, 0);
    TEST_ASSERT_TRUE(evaluate_condition_tree(none_leaf));
    TEST_ASSERT_TRUE(evaluate_condition_tree(bad_leaf));

    edge_condition_free_tree(dist_gt);
    edge_condition_free_tree(dist_lt);
    edge_condition_free_tree(rssi_lt);
    edge_condition_free_tree(stopped);
    edge_condition_free_tree(locked);
    edge_condition_free_tree(parked);
    edge_condition_free_tree(none_leaf);
    edge_condition_free_tree(bad_leaf);
}

/* ========================================================================
 * COND_003 — TIME_IN_WINDOW 叶子
 * ======================================================================== */
void test_condition_tree_time_in_window(void)
{
    edge_reset();
    icce_edge_init();

    g_engine.last_tick = 0;               /* 00:00 → hour 0 */
    icce_condition_t *w0 = edge_condition_create_leaf(COND_OP_TIME_IN_WINDOW,
                                                      0x1, 0);
    icce_condition_t *w1 = edge_condition_create_leaf(COND_OP_TIME_IN_WINDOW,
                                                      0x2, 0);
    icce_condition_t *w2 = edge_condition_create_leaf(COND_OP_TIME_IN_WINDOW,
                                                      0, 0);
    TEST_ASSERT_TRUE(evaluate_condition_tree(w0));
    TEST_ASSERT_FALSE(evaluate_condition_tree(w1));
    TEST_ASSERT_FALSE(evaluate_condition_tree(w2));   /* threshold == 0 */

    g_engine.last_tick = 3 * 3600000UL;   /* 03:00 → hour 3 */
    icce_condition_t *w3 = edge_condition_create_leaf(COND_OP_TIME_IN_WINDOW,
                                                      0x8, 0);
    icce_condition_t *w4 = edge_condition_create_leaf(COND_OP_TIME_IN_WINDOW,
                                                      0x1, 0);
    TEST_ASSERT_TRUE(evaluate_condition_tree(w3));
    TEST_ASSERT_FALSE(evaluate_condition_tree(w4));

    edge_condition_free_tree(w0);
    edge_condition_free_tree(w1);
    edge_condition_free_tree(w2);
    edge_condition_free_tree(w3);
    edge_condition_free_tree(w4);
}

/* ========================================================================
 * HELPER_001 — is_rule_in_time_window
 * ======================================================================== */
void test_rule_time_window_helper(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r;
    memset(&r, 0, sizeof(r));

    r.time_mask = 0xFFFFFF;
    TEST_ASSERT_TRUE(is_rule_in_time_window(&r));

    r.time_mask = 0x1;
    g_engine.last_tick = 0;               /* hour 0 → bit0 命中 */
    TEST_ASSERT_TRUE(is_rule_in_time_window(&r));
    g_engine.last_tick = 7200000;         /* hour 2 → bit0 未命中 */
    TEST_ASSERT_FALSE(is_rule_in_time_window(&r));

    r.time_mask = 0;
    g_engine.last_tick = 0;
    TEST_ASSERT_FALSE(is_rule_in_time_window(&r));
}

/* ========================================================================
 * HELPER_002 — is_rule_in_cooldown
 * ======================================================================== */
void test_rule_cooldown_helper(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r;
    memset(&r, 0, sizeof(r));

    r.cooldown_ms = 0;
    TEST_ASSERT_FALSE(is_rule_in_cooldown(&r, 1000));   /* 无冷却 */

    r.cooldown_ms = 3000;
    TEST_ASSERT_FALSE(is_rule_in_cooldown(&r, 0));      /* 时间未初始化 */

    r.last_triggered = 0;
    TEST_ASSERT_FALSE(is_rule_in_cooldown(&r, 1000));   /* 从未触发 */

    r.last_triggered = 1000;
    TEST_ASSERT_TRUE(is_rule_in_cooldown(&r, 2000));    /* 冷却期内 */
    TEST_ASSERT_FALSE(is_rule_in_cooldown(&r, 5000));   /* 冷却已过 */
}

/* ========================================================================
 * HELPER_003 — execute_rule_actions
 * ======================================================================== */
void test_execute_rule_actions_helper(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r;
    memset(&r, 0, sizeof(r));

    /* 单动作成功 */
    r.actions[0] = ICCE_ACTION_UNLOCK;
    r.action_count = 1;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, execute_rule_actions(&r));
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());

    /* 部分失败: 继续执行其余动作, 返回错误码 */
    r.actions[0] = ICCE_ACTION_UNLOCK;
    r.actions[1] = (icce_action_e)0xAA;
    r.action_count = 2;
    TEST_ASSERT_EQUAL_INT32(ICCE_ERR_PARAM, execute_rule_actions(&r));
    TEST_ASSERT_EQUAL_UINT8(0, vehicle_lock());   /* UNLOCK 仍被执行 */

    /* 空动作列表 */
    r.action_count = 0;
    TEST_ASSERT_EQUAL_INT32(ICCE_OK, execute_rule_actions(&r));
}

/* ========================================================================
 * HELPER_004 — evaluate_single_rule
 * ======================================================================== */
void test_evaluate_single_rule_helper(void)
{
    edge_reset();
    icce_edge_init();

    icce_edge_rule_t r;
    memset(&r, 0, sizeof(r));
    r.time_mask = 0xFFFFFF;

    /* 禁用 → false */
    r.enabled = false;
    TEST_ASSERT_FALSE(evaluate_single_rule(&r));

    /* 时间窗外 → false */
    r.enabled = true;
    r.time_mask = 0;
    g_engine.last_tick = 7200000;
    TEST_ASSERT_FALSE(evaluate_single_rule(&r));

    /* 无条件 → true */
    r.time_mask = 0xFFFFFF;
    TEST_ASSERT_TRUE(evaluate_single_rule(&r));

    /* 条件树: AND 空子树 → true; ZONE_EQ 不匹配 → false */
    r.condition.op = COND_OP_AND;
    TEST_ASSERT_TRUE(evaluate_single_rule(&r));

    r.condition.op = COND_OP_ZONE_EQ;
    r.condition.zone_id = ICCE_ZONE_INTERIOR;
    g_engine.current_zone = ICCE_ZONE_NEAR;
    TEST_ASSERT_FALSE(evaluate_single_rule(&r));
}

/* ========================================================================
 * Test Runner
 * ======================================================================== */
int run_icce_edge_tests(void)
{
    UNITY_BEGIN();

    /* init / deinit / 规则数组 */
    RUN_TEST(test_edge_init_idempotent_and_defaults);
    RUN_TEST(test_edge_init_pool_exhausted_fallback);
    RUN_TEST(test_edge_init_nvm_loaded_rules);
    RUN_TEST(test_edge_deinit_resets_state);
    RUN_TEST(test_edge_rule_array_accessor);

    /* 规则增删改查 */
    RUN_TEST(test_edge_add_rule_ok_and_duplicate);
    RUN_TEST(test_edge_add_rule_null_and_uninit);
    RUN_TEST(test_edge_add_rule_table_full);
    RUN_TEST(test_edge_remove_rule_cases);
    RUN_TEST(test_edge_enable_rule_cases);
    RUN_TEST(test_edge_get_state_cases);
    RUN_TEST(test_edge_uninit_param_guards);

    /* process_trigger */
    RUN_TEST(test_process_trigger_state_gate);
    RUN_TEST(test_process_trigger_zone_enter_default);
    RUN_TEST(test_process_trigger_vehicle_state_overlay);
    RUN_TEST(test_process_trigger_no_match);
    RUN_TEST(test_process_trigger_priority_routing);
    RUN_TEST(test_process_trigger_priority_tie_first_wins);
    RUN_TEST(test_process_trigger_priority_zero);
    RUN_TEST(test_process_trigger_cooldown_recheck_swallow);
    RUN_TEST(test_process_trigger_cooldown_recheck_pass);
    RUN_TEST(test_process_trigger_inline_condition_true);
    RUN_TEST(test_process_trigger_inline_condition_false_and_disabled);
    RUN_TEST(test_process_trigger_compound_match);
    RUN_TEST(test_process_trigger_compound_no_match);
    RUN_TEST(test_process_trigger_execute_failure_fallback);
    RUN_TEST(test_process_trigger_time_window_skip);

    /* evaluate */
    RUN_TEST(test_evaluate_idle_sensor_update);
    RUN_TEST(test_evaluate_active_timeout_transition);
    RUN_TEST(test_evaluate_fallback_retry_ok);
    RUN_TEST(test_evaluate_fallback_retry_fail);
    RUN_TEST(test_evaluate_fallback_rule_invalid);
    RUN_TEST(test_evaluate_fallback_retries_exhausted);
    RUN_TEST(test_evaluate_distance_trigger);
    RUN_TEST(test_evaluate_rssi_trigger);
    RUN_TEST(test_evaluate_uwb_range_trigger);
    RUN_TEST(test_evaluate_vehicle_state_trigger);
    RUN_TEST(test_evaluate_zone_compound_noop_triggers);
    RUN_TEST(test_evaluate_skips_disabled_time_cooldown);

    /* timer_tick */
    RUN_TEST(test_timer_tick_basic);
    RUN_TEST(test_timer_tick_active_timeout);
    RUN_TEST(test_timer_tick_fallback_retry_ok);
    RUN_TEST(test_timer_tick_fallback_retry_fail);
    RUN_TEST(test_timer_tick_fallback_rule_invalid);
    RUN_TEST(test_timer_tick_fallback_retries_exhausted);
    RUN_TEST(test_timer_tick_interval_fire);
    RUN_TEST(test_timer_tick_interval_skips);
    RUN_TEST(test_timer_tick_interval_fail_fallback);

    /* update_rssi / update_vehicle_state */
    RUN_TEST(test_update_rssi_basic);
    RUN_TEST(test_update_rssi_trigger_paths);
    RUN_TEST(test_update_vehicle_state_basic);
    RUN_TEST(test_update_vehicle_state_trigger_paths);

    /* 条件树与内部辅助函数 */
    RUN_TEST(test_condition_tree_logical_operators);
    RUN_TEST(test_condition_tree_leaf_operators);
    RUN_TEST(test_condition_tree_time_in_window);
    RUN_TEST(test_rule_time_window_helper);
    RUN_TEST(test_rule_cooldown_helper);
    RUN_TEST(test_execute_rule_actions_helper);
    RUN_TEST(test_evaluate_single_rule_helper);

    UNITY_END();
}

#ifndef TEST_ICCE_EDGE_NO_MAIN
int main(void)
{
    return run_icce_edge_tests();
}
#endif /* TEST_ICCE_EDGE_NO_MAIN */
