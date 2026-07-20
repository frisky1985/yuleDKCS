/**
 * @file offline_decision.c
 * @brief 离线决策模块实现
 * @version 1.0
 * @date 2026-05-05
 * 
 * 提供本地认证决策功能
 */

#include "offline_decision.h"
#include "cache_manager.h"
#include "security_auth.h"
#include <string.h>
#include <stdlib.h>

/* 私有定义 */
#define MAX_RULES               64
#define MAX_DECISION_HISTORY    256
#define MAX_RISK_FACTORS        8
#define RATE_LIMIT_WINDOW_MS    60000
#define MAX_REQUESTS_PER_MINUTE 10

/* 风险因素位定义 */
#define RISK_FACTOR_UNKNOWN_DEVICE      (1 << 0)
#define RISK_FACTOR_NEW_LOCATION        (1 << 1)
#define RISK_FACTOR_UNUSUAL_TIME        (1 << 2)
#define RISK_FACTOR_RSSI_WEAK           (1 << 3)
#define RISK_FACTOR_REPEATED_FAILURE    (1 << 4)
#define RISK_FACTOR_OFFLINE_TOO_LONG    (1 << 5)
#define RISK_FACTOR_PERMISSION_CHANGE   (1 << 6)
#define RISK_FACTOR_KEY_EXPIRING        (1 << 7)

/* 决策历史记录 */
typedef struct {
    decision_output_t decision;
    uint32_t timestamp;
} decision_history_entry_t;

/* 速率限制项 */
typedef struct {
    uint32_t user_id;
    uint32_t request_count;
    uint32_t window_start;
} rate_limit_entry_t;

/* 决策引擎实例 */
typedef struct {
    bool initialized;
    decision_rule_t rules[MAX_RULES];
    uint32_t rule_count;
    decision_history_entry_t history[MAX_DECISION_HISTORY];
    uint32_t history_index;
    rate_limit_entry_t rate_limits[32];
    uint32_t decision_counter;
} decision_engine_t;

/* 全局实例 */
static decision_engine_t g_decision = {0};

/* 前向声明 */
static int32_t check_key_validity(uint32_t key_id, key_cache_item_t *key_info);
static int32_t check_permission(uint32_t user_id, uint8_t command, 
                                 permission_cache_item_t *perm);
static int32_t check_signature(uint32_t key_id, const uint8_t *nonce,
                                const uint8_t *signature);
static int32_t check_rate_limit(uint32_t user_id);
static int32_t calculate_risk_score(const decision_request_t *request,
                                     const key_cache_item_t *key_info,
                                     risk_score_t *score);
static void log_decision(const decision_output_t *decision);

/*============================================================================
 * 公开API实现
 *===========================================================================*/

int32_t decision_init(void)
{
    if (g_decision.initialized) {
        return 0;
    }
    
    memset(g_decision.rules, 0, sizeof(g_decision.rules));
    memset(g_decision.history, 0, sizeof(g_decision.history));
    memset(g_decision.rate_limits, 0, sizeof(g_decision.rate_limits));
    
    g_decision.rule_count = 0;
    g_decision.history_index = 0;
    g_decision.decision_counter = 0;
    
    /* 添加默认规则 */
    
    /* 规则1: 关键安全操作需要在线验证 */
    decision_rule_t rule1 = {
        .rule_id = 1,
        .rule_type = 1,  // 安全敏感
        .action = DECISION_REQUIRE_ONLINE,
        .priority = 100,
        .enabled = 1
    };
    decision_add_rule(&rule1);
    
    /* 规则2: 高风险操作拒绝 */
    decision_rule_t rule2 = {
        .rule_id = 2,
        .rule_type = 2,  // 风险控制
        .action = DECISION_DENY,
        .priority = 90,
        .enabled = 1
    };
    decision_add_rule(&rule2);
    
    /* 规则3: 低风险操作允许 */
    decision_rule_t rule3 = {
        .rule_id = 3,
        .rule_type = 3,  // 默认规则
        .action = DECISION_ALLOW,
        .priority = 10,
        .enabled = 1
    };
    decision_add_rule(&rule3);
    
    g_decision.initialized = true;
    return 0;
}

int32_t decision_evaluate(const decision_request_t *request,
                          decision_output_t *output)
{
    if (!g_decision.initialized || !request || !output) {
        return -1;
    }
    
    memset(output, 0, sizeof(decision_output_t));
    output->decision_id = ++g_decision.decision_counter;
    output->user_id = request->user_id;
    output->key_id = request->key_id;
    output->command_type = request->command_type;
    
    /* Step 1: 检查请求格式 */
    if (request->timestamp == 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_TIME_INVALID;
        log_decision(output);
        return 0;
    }
    
    /* Step 2: 检查速率限制 */
    if (check_rate_limit(request->user_id) != 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_RATE_LIMITED;
        output->valid_duration = 60;  // 冷却60秒
        log_decision(output);
        return 0;
    }
    
    /* Step 3: 查找钥匙缓存 */
    key_cache_item_t key_info;
    if (check_key_validity(request->key_id, &key_info) != 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_KEY_NOT_FOUND;
        log_decision(output);
        return 0;
    }
    
    /* Step 4: 检查钥匙有效性 */
    uint32_t current_time = 0;  // TODO: 获取实际时间
    if (key_info.status != 1 || current_time > key_info.expiry_time) {
        output->result = DECISION_DENY;
        output->reason = REASON_KEY_EXPIRED;
        log_decision(output);
        return 0;
    }
    
    /* Step 5: 检查权限 */
    permission_cache_item_t perm;
    if (check_permission(request->user_id, request->command_type, &perm) != 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_PERMISSION_DENIED;
        log_decision(output);
        return 0;
    }
    
    /* Step 6: 验证签名 */
    if (check_signature(request->key_id, request->nonce, 
                        request->signature) != 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_SIGNATURE_INVALID;
        log_decision(output);
        return 0;
    }
    
    /* Step 7: 风险评估 */
    if (calculate_risk_score(request, &key_info, &output->risk_score) != 0) {
        output->result = DECISION_DENY;
        output->reason = REASON_PERMISSION_DENIED;
        log_decision(output);
        return 0;
    }
    
    /* Step 8: 根据风险等级决策 */
    if (output->risk_score.level >= RISK_CRITICAL) {
        output->result = DECISION_DENY;
        output->reason = REASON_RISK_TOO_HIGH;
    }
    else if (output->risk_score.level >= RISK_HIGH) {
        output->result = DECISION_REQUIRE_ONLINE;
        output->reason = REASON_SUCCESS;
    }
    else if (output->risk_score.level >= RISK_MEDIUM) {
        output->result = DECISION_CHALLENGE_REQUIRED;
        output->reason = REASON_SUCCESS;
        /* 生成额外挑战 */
        /* TODO: 生成随机挑战 */
    }
    else {
        output->result = DECISION_ALLOW;
        output->reason = REASON_SUCCESS;
        output->valid_duration = 30;  // 30秒有效
    }
    
    log_decision(output);
    return 0;
}

int32_t decision_add_rule(const decision_rule_t *rule)
{
    if (!g_decision.initialized || !rule) {
        return -1;
    }
    
    if (g_decision.rule_count >= MAX_RULES) {
        return -1;
    }
    
    /* 查找插入位置 (按优先级排序) */
    int32_t insert_pos = g_decision.rule_count;
    for (int i = 0; i < g_decision.rule_count; i++) {
        if (rule->priority > g_decision.rules[i].priority) {
            insert_pos = i;
            break;
        }
    }
    
    /* 移动后续规则 */
    for (int i = g_decision.rule_count; i > insert_pos; i--) {
        memcpy(&g_decision.rules[i], &g_decision.rules[i-1],
               sizeof(decision_rule_t));
    }
    
    /* 插入新规则 */
    memcpy(&g_decision.rules[insert_pos], rule, sizeof(decision_rule_t));
    g_decision.rule_count++;
    
    return 0;
}

int32_t decision_remove_rule(uint32_t rule_id)
{
    if (!g_decision.initialized) {
        return -1;
    }
    
    for (int i = 0; i < g_decision.rule_count; i++) {
        if (g_decision.rules[i].rule_id == rule_id) {
            /* 移动后续规则 */
            for (int j = i; j < g_decision.rule_count - 1; j++) {
                memcpy(&g_decision.rules[j], &g_decision.rules[j+1],
                       sizeof(decision_rule_t));
            }
            g_decision.rule_count--;
            return 0;
        }
    }
    
    return -1;
}

int32_t decision_calculate_risk(const decision_request_t *request,
                                risk_score_t *score)
{
    if (!g_decision.initialized || !request || !score) {
        return -1;
    }
    
    key_cache_item_t key_info;
    if (check_key_validity(request->key_id, &key_info) != 0) {
        return -1;
    }
    
    return calculate_risk_score(request, &key_info, score);
}

int32_t decision_log(const decision_output_t *decision)
{
    if (!g_decision.initialized || !decision) {
        return -1;
    }
    
    log_decision(decision);
    return 0;
}

int32_t decision_get_history(uint32_t user_id,
                             decision_output_t *history,
                             uint32_t *count)
{
    if (!g_decision.initialized || !history || !count) {
        return -1;
    }
    
    uint32_t found = 0;
    
    for (int i = 0; i < MAX_DECISION_HISTORY && found < *count; i++) {
        int idx = (g_decision.history_index - 1 - i + MAX_DECISION_HISTORY) 
                  % MAX_DECISION_HISTORY;
        
        if (g_decision.history[idx].decision.decision_id != 0 &&
            (user_id == 0 || 
             g_decision.history[idx].decision.user_id == user_id)) {
            memcpy(&history[found], &g_decision.history[idx].decision,
                   sizeof(decision_output_t));
            found++;
        }
    }
    
    *count = found;
    return 0;
}

/*============================================================================
 * 私有函数实现
 *===========================================================================*/

static int32_t check_key_validity(uint32_t key_id, key_cache_item_t *key_info)
{
    uint8_t key_buf[128];
    uint32_t buf_len = sizeof(key_buf);
    
    uint8_t cache_key[8];
    cache_key[0] = 'K';
    cache_key[1] = 'E';
    cache_key[2] = 'Y';
    *((uint32_t*)&cache_key[4]) = key_id;
    
    if (cache_get(cache_key, 8, key_buf, &buf_len) != CACHE_SUCCESS) {
        return -1;
    }
    
    if (buf_len != sizeof(key_cache_item_t)) {
        return -1;
    }
    
    memcpy(key_info, key_buf, sizeof(key_cache_item_t));
    return 0;
}

static int32_t check_permission(uint32_t user_id, uint8_t command,
                                 permission_cache_item_t *perm)
{
    uint8_t perm_buf[128];
    uint32_t buf_len = sizeof(perm_buf);
    
    uint8_t cache_key[8];
    cache_key[0] = 'P';
    cache_key[1] = 'E';
    cache_key[2] = 'R';
    *((uint32_t*)&cache_key[4]) = user_id;
    
    if (cache_get(cache_key, 8, perm_buf, &buf_len) != CACHE_SUCCESS) {
        return -1;
    }
    
    if (buf_len != sizeof(permission_cache_item_t)) {
        return -1;
    }
    
    memcpy(perm, perm_buf, sizeof(permission_cache_item_t));
    
    /* 检查权限位 */
    uint8_t byte_idx = command / 8;
    uint8_t bit_idx = command % 8;
    
    if (byte_idx >= sizeof(perm->permissions)) {
        return -1;
    }
    
    if (!(perm->permissions[byte_idx] & (1 << bit_idx))) {
        return -1;
    }
    
    /* 检查时间有效性 */
    uint32_t current_time = 0;  // TODO
    if (current_time < perm->valid_from || current_time > perm->valid_until) {
        return -1;
    }
    
    return 0;
}

static int32_t check_signature(uint32_t key_id, const uint8_t *nonce,
                                const uint8_t *signature)
{
    /* 这里需要调用安全模块验证签名 */
    /* 简化实现 */
    
    key_cache_item_t key_info;
    if (check_key_validity(key_id, &key_info) != 0) {
        return -1;
    }
    
    crypto_key_t public_key = {
        .key_id = key_id,
        .type = KEY_TYPE_ECC_P256_PUBLIC,
        .length = 64
    };
    memcpy(public_key.data, key_info.public_key, 64);
    
    /* 验证签名 (nonce作为被签名数据) */
    if (security_verify_signature(&public_key, nonce, 16, signature, 64) != SEC_SUCCESS) {
        return -1;
    }
    
    return 0;
}

static int32_t check_rate_limit(uint32_t user_id)
{
    uint32_t current_time = 0;  // TODO
    
    for (int i = 0; i < 32; i++) {
        if (g_decision.rate_limits[i].user_id == user_id) {
            /* 检查是否在时间窗口内 */
            if ((current_time - g_decision.rate_limits[i].window_start) < 
                RATE_LIMIT_WINDOW_MS) {
                g_decision.rate_limits[i].request_count++;
                
                if (g_decision.rate_limits[i].request_count > 
                    MAX_REQUESTS_PER_MINUTE) {
                    return -1;  // 超过速率限制
                }
            } else {
                /* 重置计数器 */
                g_decision.rate_limits[i].request_count = 1;
                g_decision.rate_limits[i].window_start = current_time;
            }
            return 0;
        }
    }
    
    /* 找到空闲槽 */
    for (int i = 0; i < 32; i++) {
        if (g_decision.rate_limits[i].user_id == 0) {
            g_decision.rate_limits[i].user_id = user_id;
            g_decision.rate_limits[i].request_count = 1;
            g_decision.rate_limits[i].window_start = current_time;
            return 0;
        }
    }
    
    return 0;  // 没有空闲槽,允许通过
}

static int32_t calculate_risk_score(const decision_request_t *request,
                                     const key_cache_item_t *key_info,
                                     risk_score_t *score)
{
    uint32_t base_score = 0;
    uint8_t factors = 0;
    
    /* 信号强度风险 */
    if (request->rssi < -80) {
        base_score += 20;
        factors |= RISK_FACTOR_RSSI_WEAK;
    }
    
    /* 设备指纹检查 */
    /* 简化实现: 检查设备指纹是否在历史记录中 */
    bool known_device = false;  // TODO: 实际检查
    if (!known_device) {
        base_score += 15;
        factors |= RISK_FACTOR_UNKNOWN_DEVICE;
    }
    
    /* 离线时间检查 */
    uint32_t current_time = 0;  // TODO
    uint32_t offline_duration = current_time - key_info->last_sync_time;
    if (offline_duration > 24 * 3600) {  // 超过24小时
        base_score += 25;
        factors |= RISK_FACTOR_OFFLINE_TOO_LONG;
    }
    
    /* 钥匙即将过期 */
    uint32_t time_to_expiry = key_info->expiry_time - current_time;
    if (time_to_expiry < 3600) {  // 少于1小时
        base_score += 10;
        factors |= RISK_FACTOR_KEY_EXPIRING;
    }
    
    /* 计算风险等级 */
    score->score = base_score;
    score->factors[0] = factors;
    score->confidence = 0.8f;  // 简化
    
    if (base_score >= 80) {
        score->level = RISK_CRITICAL;
    } else if (base_score >= 50) {
        score->level = RISK_HIGH;
    } else if (base_score >= 25) {
        score->level = RISK_MEDIUM;
    } else {
        score->level = RISK_LOW;
    }
    
    return 0;
}

static void log_decision(const decision_output_t *decision)
{
    decision_history_entry_t *entry = &g_decision.history[g_decision.history_index];
    
    memcpy(&entry->decision, decision, sizeof(decision_output_t));
    entry->timestamp = 0;  // TODO
    
    g_decision.history_index = (g_decision.history_index + 1) % MAX_DECISION_HISTORY;
}

/*============================================================================
 * End of file
 *===========================================================================*/
