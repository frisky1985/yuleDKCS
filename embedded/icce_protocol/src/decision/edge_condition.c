/**
 * @file edge_condition.c
 * @brief Dynamic Condition Tree — Implementation
 *
 * Implements pool-based allocation, binary serialization/deserialization,
 * and NVM loading for ICCE edge engine condition trees.
 *
 * @version 1.0
 * @date 2026-07-16
 */

#include "edge_condition.h"
#include "icce_digital_key.h"

#include <string.h>

/* ========================================================================
 *  Internal Helpers
 * ======================================================================== */

/** Forward declaration for recursive tree free */
static void cond_free_recursive(icce_condition_t *node);

/** Forward declaration for recursive serialization index assignment */
static uint16_t assign_indices(const icce_condition_t *node,
                                serialized_condition_node_t *nodes,
                                uint16_t *next_idx,
                                uint16_t max_nodes);

/** Forward declaration for recursive deserialization */
static int32_t deserialize_node(const serialized_condition_node_t *serial,
                                 uint16_t node_idx,
                                 uint16_t node_count,
                                 icce_condition_t **out);

/** Forward declaration for recursive dump */
static void dump_recursive(const icce_condition_t *node,
                            char *buf, uint32_t *pos, uint32_t buf_size);

/** Forward declaration for recursive deep copy */
static int32_t deep_copy_recursive(const icce_condition_t *src,
                                    icce_condition_t **out);

/** Forward declaration for recursive equality check */
static bool equal_recursive(const icce_condition_t *a,
                             const icce_condition_t *b);

/** Forward declaration for recursive node count */
static uint16_t count_nodes(const icce_condition_t *root);

/** Build a storage key string from rule_tag */
static void build_nvm_key(const char *rule_tag, uint8_t *key_buf, uint16_t *key_len);

/* ========================================================================
 *  Memory Pool Implementation
 * ========================================================================
 *
 * Fixed-size array of icce_condition_t nodes managed as a free list.
 * No heap allocation required — suitable for bare-metal embedded targets.
 */

/** Pool node (wraps the condition with free-list linkage) */
typedef struct cond_pool_node {
    icce_condition_t cond;             /**< The condition node itself */
    bool             allocated;        /**< true if currently in use */
} cond_pool_node_t;

/** The condition node pool */
static struct {
    cond_pool_node_t nodes[EDGE_COND_POOL_SIZE];
    bool             initialized;

    /* Statistics */
    uint32_t peak_used;
    uint32_t allocation_count;
    uint32_t free_count;
    uint32_t overflow_count;
} g_cond_pool = {0};

int32_t edge_condition_pool_init(void)
{
    if (g_cond_pool.initialized) {
        return ICCE_OK;
    }

    memset(&g_cond_pool, 0, sizeof(g_cond_pool));
    g_cond_pool.initialized = true;

    return ICCE_OK;
}

int32_t edge_condition_pool_deinit(void)
{
    g_cond_pool.initialized = false;
    memset(&g_cond_pool, 0, sizeof(g_cond_pool));
    return ICCE_OK;
}

icce_condition_t *edge_condition_alloc(void)
{
    if (!g_cond_pool.initialized) {
        return NULL;
    }

    /* Linear scan for free slot (small pool, O(n) is fine) */
    for (uint32_t i = 0; i < EDGE_COND_POOL_SIZE; i++) {
        if (!g_cond_pool.nodes[i].allocated) {
            g_cond_pool.nodes[i].allocated = true;
            memset(&g_cond_pool.nodes[i].cond, 0, sizeof(icce_condition_t));

            /* Update stats */
            g_cond_pool.allocation_count++;
            uint32_t used = 0;
            for (uint32_t j = 0; j < EDGE_COND_POOL_SIZE; j++) {
                if (g_cond_pool.nodes[j].allocated) used++;
            }
            if (used > g_cond_pool.peak_used) {
                g_cond_pool.peak_used = used;
            }

            return &g_cond_pool.nodes[i].cond;
        }
    }

    /* Pool exhausted */
    g_cond_pool.overflow_count++;
    return NULL;
}

icce_condition_t *edge_condition_create_leaf(icce_condition_op_e op,
                                              int32_t threshold,
                                              uint8_t zone_id)
{
    icce_condition_t *node = edge_condition_alloc();
    if (!node) return NULL;

    node->op        = op;
    node->threshold = threshold;
    node->zone_id   = zone_id;
    node->left      = NULL;
    node->right     = NULL;

    return node;
}

icce_condition_t *edge_condition_create_composite(icce_condition_op_e op,
                                                   icce_condition_t *left,
                                                   icce_condition_t *right)
{
    icce_condition_t *node = edge_condition_alloc();
    if (!node) return NULL;

    node->op    = op;
    node->left  = left;
    node->right = (op == COND_OP_NOT) ? NULL : right;
    node->threshold = 0;
    node->zone_id   = 0;

    return node;
}

/* ========================================================================
 *  Free (recursive)
 * ======================================================================== */

static void cond_free_recursive(icce_condition_t *node)
{
    if (!node) return;

    /* Find the pool entry */
    for (uint32_t i = 0; i < EDGE_COND_POOL_SIZE; i++) {
        if (&g_cond_pool.nodes[i].cond == node) {
            if (!g_cond_pool.nodes[i].allocated) {
                return;  /* Double-free guard */
            }

            /* Free children first (depth-first) */
            if (node->op == COND_OP_AND || node->op == COND_OP_OR ||
                node->op == COND_OP_NOT) {
                cond_free_recursive(node->left);
                if (node->op != COND_OP_NOT) {
                    cond_free_recursive(node->right);
                }
            }

            /* Free this node */
            g_cond_pool.nodes[i].allocated = false;
            g_cond_pool.free_count++;
            memset(node, 0, sizeof(icce_condition_t));
            return;
        }
    }
    /* Node not found in pool — it's a static or external node; ignore */
}

void edge_condition_free_tree(icce_condition_t *root)
{
    cond_free_recursive(root);
}

/* ========================================================================
 *  Helper: Count nodes in a condition tree (for serialization sizing)
 * ======================================================================== */

static uint16_t count_nodes(const icce_condition_t *root)
{
    if (!root) return 0;

    uint16_t count = 1;

    if (root->op == COND_OP_AND || root->op == COND_OP_OR ||
        root->op == COND_OP_NOT) {
        count += count_nodes(root->left);
        if (root->op != COND_OP_NOT) {
            count += count_nodes(root->right);
        }
    }

    return count;
}

/* ========================================================================
 *  Serialization
 * ======================================================================== */

/**
 * Build a flat node array from the tree, assigning sequential indices.
 * Returns the index assigned to the given node.
 */
static uint16_t assign_indices(const icce_condition_t *node,
                                serialized_condition_node_t *nodes,
                                uint16_t *next_idx,
                                uint16_t max_nodes)
{
    if (!node || *next_idx >= max_nodes) {
        return EDGE_COND_IDX_NULL;
    }

    uint16_t my_idx = (*next_idx)++;

    nodes[my_idx].op        = (uint8_t)node->op;
    nodes[my_idx].threshold = node->threshold;
    nodes[my_idx].zone_id   = node->zone_id;
    nodes[my_idx].reserved  = 0;

    /* Serialize children */
    if (node->op == COND_OP_AND || node->op == COND_OP_OR ||
        node->op == COND_OP_NOT) {
        nodes[my_idx].left_idx = assign_indices(node->left, nodes, next_idx, max_nodes);
        if (node->op == COND_OP_NOT) {
            nodes[my_idx].right_idx = EDGE_COND_IDX_NULL;
        } else {
            nodes[my_idx].right_idx = assign_indices(node->right, nodes, next_idx, max_nodes);
        }
    } else {
        nodes[my_idx].left_idx  = EDGE_COND_IDX_NULL;
        nodes[my_idx].right_idx = EDGE_COND_IDX_NULL;
    }

    return my_idx;
}

int32_t edge_condition_serialize(const icce_condition_t *root,
                                  uint8_t *buffer,
                                  uint32_t buf_size,
                                  uint32_t *out_len)
{
    if (!buffer || !out_len) return ICCE_ERR_PARAM;

    *out_len = 0;

    /* Handle NULL root (empty tree) */
    if (!root) {
        serialized_condition_tree_t *header = (serialized_condition_tree_t *)buffer;
        if (buf_size < sizeof(serialized_condition_tree_t)) {
            return ICCE_ERR_NO_MEM;
        }
        header->node_count = 0;
        header->root_idx   = EDGE_COND_IDX_NULL;
        *out_len = sizeof(serialized_condition_tree_t);
        return ICCE_OK;
    }

    /* Count nodes */
    uint16_t node_count = count_nodes(root);
    uint32_t required = sizeof(serialized_condition_tree_t) +
                        (uint32_t)node_count * sizeof(serialized_condition_node_t);

    if (buf_size < required) {
        return ICCE_ERR_NO_MEM;
    }

    serialized_condition_tree_t *header = (serialized_condition_tree_t *)buffer;
    header->node_count = node_count;

    /* Assign indices to build flat array */
    uint16_t next_idx = 0;
    header->root_idx = assign_indices(root, header->nodes, &next_idx, node_count);

    *out_len = required;
    return ICCE_OK;
}

/* ========================================================================
 *  Deserialization
 * ======================================================================== */

/**
 * Recursively deserialize a single node and its children.
 */
static int32_t deserialize_node(const serialized_condition_node_t *serial,
                                 uint16_t node_idx,
                                 uint16_t node_count,
                                 icce_condition_t **out)
{
    if (node_idx >= node_count || !out) {
        return ICCE_ERR_PARAM;
    }

    const serialized_condition_node_t *s = &serial[node_idx];
    icce_condition_t *node = edge_condition_alloc();
    if (!node) {
        return ICCE_ERR_NO_MEM;
    }

    node->op        = (icce_condition_op_e)s->op;
    node->threshold = s->threshold;
    node->zone_id   = s->zone_id;
    node->left      = NULL;
    node->right     = NULL;

    /* Deserialize children */
    int32_t ret;

    if (s->left_idx != EDGE_COND_IDX_NULL) {
        ret = deserialize_node(serial, s->left_idx, node_count, &node->left);
        if (ret != ICCE_OK) {
            cond_free_recursive(node);
            return ret;
        }
    }

    if (s->right_idx != EDGE_COND_IDX_NULL) {
        ret = deserialize_node(serial, s->right_idx, node_count, &node->right);
        if (ret != ICCE_OK) {
            cond_free_recursive(node);
            return ret;
        }
    }

    *out = node;
    return ICCE_OK;
}

int32_t edge_condition_deserialize(const uint8_t *buffer,
                                    uint32_t buf_len,
                                    icce_condition_t **out_root)
{
    if (!buffer || !out_root) return ICCE_ERR_PARAM;

    *out_root = NULL;

    if (buf_len < sizeof(serialized_condition_tree_t)) {
        return ICCE_ERR_PARAM;
    }

    const serialized_condition_tree_t *header =
        (const serialized_condition_tree_t *)buffer;

    /* Validate node count */
    if (header->node_count == 0) {
        return ICCE_OK;  /* Empty tree — NULL root is valid */
    }

    uint32_t expected_size = sizeof(serialized_condition_tree_t) +
                             (uint32_t)header->node_count * sizeof(serialized_condition_node_t);
    if (buf_len < expected_size) {
        return ICCE_ERR_PARAM;
    }

    /* Validate root index */
    if (header->root_idx >= header->node_count && header->root_idx != EDGE_COND_IDX_NULL) {
        return ICCE_ERR_PARAM;
    }
    if (header->root_idx == EDGE_COND_IDX_NULL) {
        return ICCE_OK;  /* Empty tree */
    }

    /* Deserialize from root */
    return deserialize_node(header->nodes, header->root_idx,
                            header->node_count, out_root);
}

/* ========================================================================
 *  NVM Load/Save (single condition tree by tag)
 * ======================================================================== */

static void build_nvm_key(const char *rule_tag, uint8_t *key_buf, uint16_t *key_len)
{
    const char *prefix = EDGE_COND_NVM_KEY_PREFIX;
    size_t plen = strlen(prefix);
    size_t tlen = strlen(rule_tag);

    /* Total key length: prefix + tag + null terminator */
    size_t total = plen + tlen + 1;
    if (total > 64) {
        total = 64;
    }

    memcpy(key_buf, prefix, plen);
    memcpy(key_buf + plen, rule_tag, tlen);
    key_buf[total - 1] = '\0';

    *key_len = (uint16_t)total;
}

int32_t edge_condition_load_from_nvm(uint8_t storage_handle,
                                      const char *rule_tag,
                                      icce_condition_t **out_root)
{
    if (!rule_tag || !out_root) return ICCE_ERR_PARAM;

    *out_root = NULL;

    uint8_t key_buf[64];
    uint16_t key_len;
    build_nvm_key(rule_tag, key_buf, &key_len);

    /* Read from storage */
    uint8_t data_buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t data_len = sizeof(data_buf);

    int32_t ret = storage_read(storage_handle, key_buf, key_len,
                                data_buf, &data_len);
    if (ret != STORAGE_SUCCESS) {
        return ICCE_ERR_NOT_FOUND;
    }

    /* Deserialize */
    return edge_condition_deserialize(data_buf, data_len, out_root);
}

int32_t edge_condition_save_to_nvm(uint8_t storage_handle,
                                    const char *rule_tag,
                                    const icce_condition_t *root)
{
    if (!rule_tag) return ICCE_ERR_PARAM;

    uint8_t serialize_buf[EDGE_COND_SERIALIZE_MAX];
    uint32_t serialized_len;

    int32_t ret = edge_condition_serialize(root, serialize_buf,
                                            sizeof(serialize_buf),
                                            &serialized_len);
    if (ret != ICCE_OK) return ret;

    uint8_t key_buf[64];
    uint16_t key_len;
    build_nvm_key(rule_tag, key_buf, &key_len);

    ret = storage_write(storage_handle, key_buf, key_len,
                         serialize_buf, serialized_len);
    return (ret == STORAGE_SUCCESS) ? ICCE_OK : ICCE_ERR_PARAM;
}

int32_t edge_condition_delete_from_nvm(uint8_t storage_handle,
                                        const char *rule_tag)
{
    if (!rule_tag) return ICCE_ERR_PARAM;

    uint8_t key_buf[64];
    uint16_t key_len;
    build_nvm_key(rule_tag, key_buf, &key_len);

    storage_delete(storage_handle, key_buf, key_len);
    return ICCE_OK;
}

/* ========================================================================
 *  Full Rule Set NVM Load/Save
 * ======================================================================== */

#define EDGE_COND_NVM_RULESET_KEY "edge_cond_ruleset"
#define EDGE_COND_NVM_RULESET_KEY_LEN 18

int32_t edge_condition_load_rules_from_nvm(uint8_t storage_handle)
{
    uint8_t key_buf[] = EDGE_COND_NVM_RULESET_KEY;

    /* Read the full config blob */
    uint8_t data_buf[4096];
    uint32_t data_len = sizeof(data_buf);

    int32_t ret = storage_read(storage_handle, key_buf, sizeof(key_buf) - 1,
                                data_buf, &data_len);
    if (ret != STORAGE_SUCCESS) {
        return ICCE_ERR_NOT_FOUND;
    }

    /* Parse header */
    if (data_len < sizeof(edge_condition_config_header_t)) {
        return ICCE_ERR_PARAM;
    }

    const edge_condition_config_header_t *hdr =
        (const edge_condition_config_header_t *)data_buf;

    if (hdr->magic != EDGE_COND_CONFIG_MAGIC) {
        return ICCE_ERR_PARAM;
    }
    if (hdr->version != EDGE_COND_CONFIG_VERSION) {
        return ICCE_ERR_PARAM;
    }

    uint32_t rule_count = hdr->rule_count;
    if (rule_count > EDGE_COND_NVM_MAX_RULES) {
        rule_count = EDGE_COND_NVM_MAX_RULES;
    }

    /* Parse rules */
    uint32_t offset = sizeof(edge_condition_config_header_t);

    for (uint32_t r = 0; r < rule_count; r++) {
        if (offset + sizeof(serialized_edge_rule_t) > data_len) {
            break;
        }

        const serialized_edge_rule_t *sr =
            (const serialized_edge_rule_t *)&data_buf[offset];
        offset += sizeof(serialized_edge_rule_t);

        /* Build the icce_edge_rule_t from serialized fields */
        icce_edge_rule_t rule;
        memset(&rule, 0, sizeof(rule));

        rule.trigger        = (icce_trigger_e)sr->trigger;
        rule.zone_id        = sr->zone_id;
        rule.threshold_mm   = sr->threshold_mm;
        rule.threshold_rssi = sr->threshold_rssi;
        rule.time_mask      = sr->time_mask;
        rule.interval_ms    = sr->interval_ms;
        rule.action_count   = sr->action_count;

        for (uint8_t a = 0; a < rule.action_count && a < ICCE_EDGE_RULE_MAX_ACTIONS; a++) {
            rule.actions[a] = (icce_action_e)sr->actions[a];
        }

        rule.priority       = sr->priority;
        rule.enabled        = sr->enabled ? true : false;
        rule.cooldown_ms    = sr->cooldown_ms;
        rule.condition.op   = COND_OP_NONE;  /* Will be overwritten by deserialization */
        rule.last_triggered = 0;

        /* Load condition tree if present */
        if (sr->cond_node_count > 0 && sr->cond_root_idx != EDGE_COND_IDX_NULL) {
            /* Deserialize condition nodes from the rule's embedded node array */
            if (offset + (uint32_t)sr->cond_node_count * sizeof(serialized_condition_node_t)
                <= data_len) {

                const serialized_condition_node_t *cond_nodes =
                    (const serialized_condition_node_t *)&data_buf[offset];

                icce_condition_t *cond_root = NULL;
                ret = deserialize_node(cond_nodes, sr->cond_root_idx,
                                        sr->cond_node_count, &cond_root);
                if (ret == ICCE_OK && cond_root) {
                    /* Store condition in rule (shallow copy pointer semantics) */
                    memcpy(&rule.condition, cond_root, sizeof(icce_condition_t));
                }
            }

            offset += (uint32_t)sr->cond_node_count * sizeof(serialized_condition_node_t);
        }

        /* Add rule to edge engine */
        ret = icce_edge_add_rule(&rule);
        if (ret != ICCE_OK) {
            /* If a rule fails, continue loading others (best-effort) */
            continue;
        }
    }

    return ICCE_OK;
}

int32_t edge_condition_save_rules_to_nvm(uint8_t storage_handle)
{
    /* Note: Getting the current rule set requires internal access.
     * For now, this is a NOP placeholder. The actual implementation
     * would iterate over g_engine.rules[] in icce_edge.c.
     * We'll add an internal accessor when needed.
     */
    (void)storage_handle;
    return ICCE_ERR_NOT_FOUND;  /* Not yet implemented — use save_to_nvm for single trees */
}

/* ========================================================================
 *  Simple Integer-to-String Helper (freestanding safe, no stdio.h)
 * ======================================================================== */

/** Convert a signed 32-bit integer to decimal string at *end* of buffer. */
static char *i32_to_str_end(int32_t val, char *end)
{
    *(--end) = '\0';
    if (val == 0) {
        *(--end) = '0';
        return end;
    }
    bool neg = (val < 0);
    if (neg) val = -val;
    while (val > 0) {
        *(--end) = (char)('0' + (val % 10));
        val /= 10;
    }
    if (neg) *(--end) = '-';
    return end;
}

/** Convert unsigned 32-bit to hex string. Returns pointer before hex chars. */
static char *u32_to_hex_end(uint32_t val, char *end, uint8_t digits)
{
    *(--end) = '\0';
    for (uint8_t i = 0; i < digits; i++) {
        uint8_t nib = (uint8_t)(val & 0x0F);
        *(--end) = (char)(nib < 10 ? '0' + nib : 'A' + nib - 10);
        val >>= 4;
        if (val == 0 && (i + 1) >= 4) break;  /* Minimum 4 hex digits for 0x format */
    }
    return end;
}

/**
 * Append a literal string to the buffer at pos.
 * Returns number of characters written (excluding null).
 */
static uint32_t buf_append(char *buf, uint32_t *pos, uint32_t buf_size,
                            const char *str)
{
    uint32_t written = 0;
    while (*pos + written + 1 < buf_size && str[written] != '\0') {
        buf[*pos + written] = str[written];
        written++;
    }
    *pos += written;
    buf[*pos] = '\0';
    return written;
}

/**
 * Append a decimal integer to the buffer.
 */
static void buf_append_int(char *buf, uint32_t *pos, uint32_t buf_size,
                            int32_t val)
{
    /* Build string in temporary buffer (end to start) */
    char tmp[16];
    char *end = tmp + sizeof(tmp);
    char *start = i32_to_str_end(val, end);
    buf_append(buf, pos, buf_size, start);
}

/**
 * Append hex value with 0x prefix.
 */
static void buf_append_hex(char *buf, uint32_t *pos, uint32_t buf_size,
                            uint32_t val, uint8_t digits)
{
    char tmp[16];
    char *end = tmp + sizeof(tmp);
    char *hex_str = u32_to_hex_end(val, end, digits);
    buf_append(buf, pos, buf_size, "0x");
    buf_append(buf, pos, buf_size, hex_str);
}

/* ========================================================================
 *  Tree Dump (Debug) — freestanding-safe, no stdio.h dependency
 * ======================================================================== */

static const char *op_name(icce_condition_op_e op)
{
    switch (op) {
        case COND_OP_NONE:         return "NONE";
        case COND_OP_AND:          return "AND";
        case COND_OP_OR:           return "OR";
        case COND_OP_NOT:          return "NOT";
        case COND_OP_RSSI_GT:      return "RSSI_GT";
        case COND_OP_RSSI_LT:      return "RSSI_LT";
        case COND_OP_DIST_GT:      return "DIST_GT";
        case COND_OP_DIST_LT:      return "DIST_LT";
        case COND_OP_ZONE_EQ:      return "ZONE_EQ";
        case COND_OP_VEHICLE_STOPPED: return "VEHICLE_STOPPED";
        case COND_OP_VEHICLE_LOCKED:  return "VEHICLE_LOCKED";
        case COND_OP_VEHICLE_PARKED:  return "VEHICLE_PARKED";
        case COND_OP_TIME_IN_WINDOW:  return "TIME_IN_WINDOW";
        default:                   return "UNKNOWN";
    }
}

static void dump_recursive(const icce_condition_t *node,
                            char *buf, uint32_t *pos, uint32_t buf_size)
{
    if (!node || *pos >= buf_size) return;

    switch (node->op) {
    case COND_OP_AND:
    case COND_OP_OR:
        buf_append(buf, pos, buf_size, "(");
        buf_append(buf, pos, buf_size, op_name(node->op));
        buf_append(buf, pos, buf_size, " ");
        dump_recursive(node->left, buf, pos, buf_size);
        buf_append(buf, pos, buf_size, " ");
        dump_recursive(node->right, buf, pos, buf_size);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_NOT:
        buf_append(buf, pos, buf_size, "(NOT ");
        dump_recursive(node->left, buf, pos, buf_size);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_RSSI_GT:
        buf_append(buf, pos, buf_size, "(RSSI_GT ");
        buf_append_int(buf, pos, buf_size, node->threshold);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_RSSI_LT:
        buf_append(buf, pos, buf_size, "(RSSI_LT ");
        buf_append_int(buf, pos, buf_size, node->threshold);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_DIST_GT:
        buf_append(buf, pos, buf_size, "(DIST_GT ");
        buf_append_int(buf, pos, buf_size, node->threshold);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_DIST_LT:
        buf_append(buf, pos, buf_size, "(DIST_LT ");
        buf_append_int(buf, pos, buf_size, node->threshold);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_ZONE_EQ:
        buf_append(buf, pos, buf_size, "(ZONE_EQ ");
        buf_append_int(buf, pos, buf_size, (int32_t)node->zone_id);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_VEHICLE_STOPPED:
        buf_append(buf, pos, buf_size, "VEHICLE_STOPPED");
        break;

    case COND_OP_VEHICLE_LOCKED:
        buf_append(buf, pos, buf_size, "VEHICLE_LOCKED");
        break;

    case COND_OP_VEHICLE_PARKED:
        buf_append(buf, pos, buf_size, "VEHICLE_PARKED");
        break;

    case COND_OP_TIME_IN_WINDOW:
        buf_append(buf, pos, buf_size, "(TIME_IN_WINDOW ");
        buf_append_hex(buf, pos, buf_size, (uint32_t)node->threshold, 6);
        buf_append(buf, pos, buf_size, ")");
        break;

    case COND_OP_NONE:
    default:
        buf_append(buf, pos, buf_size, "NONE");
        break;
    }
}

void edge_condition_dump(const icce_condition_t *root,
                          char *out_buf, uint32_t buf_size)
{
    if (!out_buf || buf_size == 0) return;

    out_buf[0] = '\0';
    uint32_t pos = 0;

    if (!root) {
        buf_append(out_buf, &pos, buf_size, "(empty)");
        return;
    }

    dump_recursive(root, out_buf, &pos, buf_size);

    /* Ensure null termination */
    if (pos >= buf_size && buf_size > 0) {
        out_buf[buf_size - 1] = '\0';
    }
}

/* ========================================================================
 *  Pool Stats
 * ======================================================================== */

void edge_condition_get_pool_stats(edge_condition_pool_stats_t *stats)
{
    if (!stats) return;

    stats->total_nodes = EDGE_COND_POOL_SIZE;

    uint32_t used = 0;
    for (uint32_t i = 0; i < EDGE_COND_POOL_SIZE; i++) {
        if (g_cond_pool.nodes[i].allocated) used++;
    }

    stats->used_nodes      = used;
    stats->peak_used       = g_cond_pool.peak_used;
    stats->allocation_count = g_cond_pool.allocation_count;
    stats->free_count      = g_cond_pool.free_count;
    stats->overflow_count  = g_cond_pool.overflow_count;
}

/* ========================================================================
 *  Upgrade Rule — Convert static condition to dynamic allocation
 * ======================================================================== */

int32_t edge_condition_upgrade_rule(icce_edge_rule_t *rule)
{
    if (!rule) return ICCE_ERR_PARAM;

    /* If the condition is already NONE, nothing to upgrade */
    if (rule->condition.op == COND_OP_NONE) {
        return ICCE_OK;
    }

    /* Deep-copy the condition tree */
    icce_condition_t *dynamic_copy = NULL;
    int32_t ret = edge_condition_deep_copy(&rule->condition, &dynamic_copy);
    if (ret != ICCE_OK) {
        return ret;
    }

    /* Replace the embedded condition with the dynamic copy */
    memcpy(&rule->condition, dynamic_copy, sizeof(icce_condition_t));

    return ICCE_OK;
}

/* ========================================================================
 *  Deep Copy
 * ======================================================================== */

static int32_t deep_copy_recursive(const icce_condition_t *src,
                                    icce_condition_t **out)
{
    if (!src || !out) return ICCE_ERR_PARAM;
    *out = NULL;

    icce_condition_t *node = edge_condition_alloc();
    if (!node) return ICCE_ERR_NO_MEM;

    /* Shallow copy all fields */
    memcpy(node, src, sizeof(icce_condition_t));

    /* Deep-copy children */
    node->left  = NULL;
    node->right = NULL;

    int32_t ret;

    if (src->op == COND_OP_AND || src->op == COND_OP_OR ||
        src->op == COND_OP_NOT) {
        if (src->left) {
            ret = deep_copy_recursive(src->left, &node->left);
            if (ret != ICCE_OK) {
                cond_free_recursive(node);
                return ret;
            }
        }
        if (src->op != COND_OP_NOT && src->right) {
            ret = deep_copy_recursive(src->right, &node->right);
            if (ret != ICCE_OK) {
                cond_free_recursive(node);
                return ret;
            }
        }
    }

    *out = node;
    return ICCE_OK;
}

int32_t edge_condition_deep_copy(const icce_condition_t *src,
                                  icce_condition_t **out_dst)
{
    if (!src || !out_dst) return ICCE_ERR_PARAM;
    return deep_copy_recursive(src, out_dst);
}

/* ========================================================================
 *  Tree Equality
 * ======================================================================== */

static bool equal_recursive(const icce_condition_t *a,
                             const icce_condition_t *b)
{
    if (a == b) return true;
    if (!a || !b) return false;

    if (a->op        != b->op)        return false;
    if (a->threshold != b->threshold) return false;
    if (a->zone_id   != b->zone_id)   return false;

    /* Compare children recursively */
    if (a->op == COND_OP_AND || a->op == COND_OP_OR ||
        a->op == COND_OP_NOT) {
        if (!equal_recursive(a->left, b->left)) return false;
        if (a->op != COND_OP_NOT) {
            if (!equal_recursive(a->right, b->right)) return false;
        }
    }

    return true;
}

bool edge_condition_equal(const icce_condition_t *a,
                           const icce_condition_t *b)
{
    return equal_recursive(a, b);
}

/* ========================================================================
 *  End of file
 * ======================================================================== */
