/**
 * @file edge_condition.h
 * @brief Dynamic Condition Tree — Allocation, Serialization & NVM Loading
 *
 * == Purpose ==
 * The ICCE edge engine's condition tree previously relied on embedded static
 * pointers (icce_condition_t::left/right), which prevented:
 *   - Loading complex condition trees from NVM (pointers are runtime-only)
 *   - Safe static initialization (compound literals had dangling addresses)
 *   - Arbitrary tree depth beyond compile-time nesting
 *
 * This module fixes all three by providing:
 *   1. Pool-based dynamic allocator for condition nodes (no heap malloc)
 *   2. Binary serialization/deserialization (index-based child refs)
 *   3. File/NVM loading interface
 *   4. Static condition tree as fallback (BACKWARD COMPATIBLE)
 *
 * == Memory Pool ==
 * Uses a fixed-size node pool to avoid malloc in bare-metal embedded contexts.
 * The pool size is configurable at compile time (EDGE_COND_POOL_SIZE).
 *
 * == Serialization Format ==
 * Flat array of serialized_condition_node_t records. Child pointers are
 * replaced with uint16_t indices into the array (0xFFFF = NULL).
 * This makes the tree fully portable across power cycles.
 *
 * == Backward Compatibility ==
 * The existing evaluate_condition_tree() in icce_edge.c continues to work
 * unchanged because icce_condition_t::left/right pointers are still valid
 * after dynamic allocation — the allocator simply returns real pointers from
 * the pool rather than dangling compound literal addresses.
 *
 * @version 1.0
 * @date 2026-07-16
 */

#ifndef EDGE_CONDITION_H
#define EDGE_CONDITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "icce_digital_key.h"
#include "storage_driver.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 *  Constants
 * ======================================================================== */

/** Maximum condition nodes in the dynamic pool */
#ifndef EDGE_COND_POOL_SIZE
#define EDGE_COND_POOL_SIZE        64
#endif

/** Maximum depth of serializable condition tree */
#define EDGE_COND_MAX_DEPTH        16

/** Null index for serialized child references (0xFFFF = no child) */
#define EDGE_COND_IDX_NULL         0xFFFFU

/** Maximum serialized tree buffer (64 nodes × ~12 bytes each) */
#define EDGE_COND_SERIALIZE_MAX    4096

/** NVM storage key prefix for condition trees */
#define EDGE_COND_NVM_KEY_PREFIX   "edge_cond_"

/** Maximum conditions per rule in NVM config */
#define EDGE_COND_NVM_MAX_RULES    16

/* ========================================================================
 *  Serialized Node Format (for NVM/transport)
 * ========================================================================
 *
 * Binary layout of a single condition node in serialized form.
 * Child references are indices into the flat node array, not pointers.
 *
 * Layout (12 bytes per node):
 *   [0]    uint8_t  op          — icce_condition_op_e
 *   [1-4]  int32_t  threshold   — Numeric parameter
 *   [5]    uint8_t  zone_id     — Zone parameter
 *   [6-7]  uint16_t left_idx    — Index of left child (EDGE_COND_IDX_NULL = none)
 *   [8-9]  uint16_t right_idx   — Index of right child (EDGE_COND_IDX_NULL = none)
 *   [10-11] uint16_t reserved   — For future expansion (set to 0)
 */

typedef struct __attribute__((packed)) {
    uint8_t  op;                /**< icce_condition_op_e */
    int32_t  threshold;         /**< Numeric threshold (RSSI dBm, distance mm) */
    uint8_t  zone_id;           /**< Zone ID for ZONE_EQ */
    uint16_t left_idx;          /**< Index of left child, EDGE_COND_IDX_NULL if none */
    uint16_t right_idx;         /**< Index of right child, EDGE_COND_IDX_NULL if none */
    uint16_t reserved;          /**< Must be 0 */
} serialized_condition_node_t;

/**
 * Serialized condition tree header + nodes.
 * The entire serialized form begins with a header followed by nodes.
 */
typedef struct __attribute__((packed)) {
    uint16_t node_count;        /**< Number of nodes in nodes[] */
    uint16_t root_idx;          /**< Index of root node */
    serialized_condition_node_t nodes[];  /**< Flexible array of nodes */
} serialized_condition_tree_t;

/* ========================================================================
 *  Serialized Rule + Condition Format
 * ========================================================================
 *
 * For loading full rules from NVM, each rule is serialized as:
 *   [icce_edge_rule_t fields] + [flat condition tree]
 */

typedef struct __attribute__((packed)) {
    uint8_t  trigger;           /**< icce_trigger_e */
    uint8_t  zone_id;
    int32_t  threshold_mm;
    int32_t  threshold_rssi;
    uint32_t time_mask;
    uint32_t interval_ms;
    uint8_t  actions[ICCE_EDGE_RULE_MAX_ACTIONS];  /**< icce_action_e values */
    uint8_t  action_count;
    uint8_t  priority;
    uint8_t  enabled;
    uint32_t cooldown_ms;
    uint16_t cond_node_count;   /**< Number of serialized condition nodes following */
    uint16_t cond_root_idx;     /**< Index of root condition node */
    /* Followed by cond_node_count × serialized_condition_node_t */
} serialized_edge_rule_t;

/* ========================================================================
 *  File/NVM Config Format
 * ========================================================================
 *
 * A complete NVM condition config is a sequence of serialized rules:
 *
 *   uint32_t          magic          (0x49434543 = "ICEC")
 *   uint32_t          version        (0x00010000 = v1.0)
 *   uint32_t          rule_count     (number of rules)
 *   serialized_edge_rule_t rules[]
 *     Each rule followed by its cond_node_count × serialized_condition_node_t
 */

#define EDGE_COND_CONFIG_MAGIC      0x49434543U  /* "ICEC" */
#define EDGE_COND_CONFIG_VERSION    0x00010000U  /* v1.0 */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t rule_count;
    /* Followed by rule_count × serialized_edge_rule_t, each with embedded condition nodes */
} edge_condition_config_header_t;

/* ========================================================================
 *  Dynamic Condition Pool (internal, exposed for testing)
 * ======================================================================== */

/** Condition pool statistics */
typedef struct {
    uint32_t total_nodes;       /**< Pool capacity */
    uint32_t used_nodes;        /**< Currently allocated */
    uint32_t peak_used;         /**< Peak allocation count */
    uint32_t allocation_count;  /**< Total allocations since init */
    uint32_t free_count;        /**< Total frees since init */
    uint32_t overflow_count;    /**< Allocation failures due to pool exhaustion */
} edge_condition_pool_stats_t;

/* ========================================================================
 *  Public API
 * ======================================================================== */

/**
 * Initialize the condition node memory pool.
 * Must be called before any other edge_condition_* functions.
 * Safe to call multiple times (idempotent).
 */
int32_t edge_condition_pool_init(void);

/**
 * De-initialize the condition node pool, freeing all nodes.
 * All allocated condition trees become invalid after this call.
 */
int32_t edge_condition_pool_deinit(void);

/**
 * Allocate a single condition node from the pool.
 * The node is zero-initialized (op = COND_OP_NONE).
 *
 * @return Pointer to allocated node, or NULL if pool exhausted.
 */
icce_condition_t *edge_condition_alloc(void);

/**
 * Allocate and initialize a leaf condition node.
 *
 * @param op        Condition operator (COND_OP_RSSI_GT, COND_OP_DIST_LT, etc.)
 * @param threshold Numeric threshold parameter
 * @param zone_id   Zone ID (for COND_OP_ZONE_EQ; ignored otherwise)
 * @return Pointer to allocated node, or NULL on failure.
 */
icce_condition_t *edge_condition_create_leaf(icce_condition_op_e op,
                                              int32_t threshold,
                                              uint8_t zone_id);

/**
 * Allocate a composite operator node and link children.
 *
 * @param op    COND_OP_AND, COND_OP_OR, or COND_OP_NOT
 * @param left  Left child (required for AND/OR/NOT)
 * @param right Right child (NULL for NOT, required for AND/OR)
 * @return Pointer to allocated node, or NULL on failure.
 */
icce_condition_t *edge_condition_create_composite(icce_condition_op_e op,
                                                   icce_condition_t *left,
                                                   icce_condition_t *right);

/**
 * Free a condition tree rooted at `root`.
 * Recursively frees all descendant nodes back into the pool.
 * Safe to call with NULL (no-op).
 */
void edge_condition_free_tree(icce_condition_t *root);

/**
 * Serialize a condition tree into a flat binary buffer.
 *
 * @param root     Root of the condition tree to serialize (may be NULL → empty tree)
 * @param buffer   Output buffer (must be at least EDGE_COND_SERIALIZE_MAX bytes)
 * @param buf_size Size of output buffer
 * @param out_len  [out] Number of bytes written
 * @return ICCE_OK on success, ICCE_ERR_NO_MEM if buffer too small, ICCE_ERR_PARAM on invalid input.
 */
int32_t edge_condition_serialize(const icce_condition_t *root,
                                  uint8_t *buffer,
                                  uint32_t buf_size,
                                  uint32_t *out_len);

/**
 * Deserialize a condition tree from a flat binary buffer.
 * Allocates nodes from the dynamic pool.
 *
 * @param buffer   Serialized buffer (as produced by edge_condition_serialize)
 * @param buf_len  Length of buffer
 * @param out_root [out] Pointer to the deserialized root node (NULL if empty tree)
 * @return ICCE_OK on success, ICCE_ERR_PARAM on invalid data, ICCE_ERR_NO_MEM on pool exhaustion.
 */
int32_t edge_condition_deserialize(const uint8_t *buffer,
                                    uint32_t buf_len,
                                    icce_condition_t **out_root);

/**
 * Load a condition tree from NVM storage.
 *
 * @param storage_handle Storage driver handle (from storage_init)
 * @param rule_tag       Rule identifier string (e.g., "unlock_approach")
 * @param out_root       [out] Pointer to deserialized root condition
 * @return ICCE_OK on success, ICCE_ERR_NOT_FOUND if no config stored.
 */
int32_t edge_condition_load_from_nvm(uint8_t storage_handle,
                                      const char *rule_tag,
                                      icce_condition_t **out_root);

/**
 * Save a condition tree to NVM storage.
 *
 * @param storage_handle Storage driver handle
 * @param rule_tag       Rule identifier string
 * @param root           Root of condition tree to save
 * @return ICCE_OK on success.
 */
int32_t edge_condition_save_to_nvm(uint8_t storage_handle,
                                    const char *rule_tag,
                                    const icce_condition_t *root);

/**
 * Delete a condition tree from NVM storage.
 *
 * @param storage_handle Storage driver handle
 * @param rule_tag       Rule identifier string
 * @return ICCE_OK on success or if not found.
 */
int32_t edge_condition_delete_from_nvm(uint8_t storage_handle,
                                        const char *rule_tag);

/**
 * Load an entire edge rule set (rules + condition trees) from NVM.
 * Appends loaded rules to the edge engine via icce_edge_add_rule().
 * If NVM has no config, returns ICCE_ERR_NOT_FOUND (caller falls back to static).
 *
 * @param storage_handle Storage driver handle
 * @return ICCE_OK on success, ICCE_ERR_NOT_FOUND if no config, error otherwise.
 */
int32_t edge_condition_load_rules_from_nvm(uint8_t storage_handle);

/**
 * Save the current edge engine rule set (including condition trees) to NVM.
 *
 * @param storage_handle Storage driver handle
 * @return ICCE_OK on success.
 */
int32_t edge_condition_save_rules_to_nvm(uint8_t storage_handle);

/**
 * Print condition tree structure (for debug logging).
 * Output format: "(AND (RSSI_GT -70) (VEHICLE_PARKED))"
 *
 * @param root       Root of condition tree
 * @param out_buf    Output string buffer
 * @param buf_size   Size of output buffer
 */
void edge_condition_dump(const icce_condition_t *root,
                          char *out_buf, uint32_t buf_size);

/**
 * Get pool allocation statistics.
 */
void edge_condition_get_pool_stats(edge_condition_pool_stats_t *stats);

/**
 * Reset the condition tree in an edge rule to use dynamic allocation.
 * Shallow-copies the condition from a static initializer to a pool-allocated node.
 * Frees the original static tree and replaces it with the dynamic copy.
 *
 * This is the bridge between compile-time rule definitions and runtime
 * dynamic trees.  Used by icce_edge_init() to convert static rules.
 *
 * @param rule Rule whose condition tree should be upgraded to dynamic allocation.
 * @return ICCE_OK on success.
 */
int32_t edge_condition_upgrade_rule(icce_edge_rule_t *rule);

/**
 * Deep-copy a condition tree (allocates new nodes from pool).
 *
 * @param src Root of source tree
 * @param out_dst [out] Pointer to copied root
 * @return ICCE_OK on success, ICCE_ERR_NO_MEM on pool exhaustion.
 */
int32_t edge_condition_deep_copy(const icce_condition_t *src,
                                  icce_condition_t **out_dst);

/**
 * Compare two condition trees for structural equality.
 *
 * @param a First tree root
 * @param b Second tree root
 * @return true if trees are structurally identical.
 */
bool edge_condition_equal(const icce_condition_t *a,
                           const icce_condition_t *b);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_CONDITION_H */
