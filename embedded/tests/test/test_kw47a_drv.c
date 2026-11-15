/**
 * @file test_kw47a_drv.c
 * @brief KW47A 内部 Flash 页擦/编程 + eFuse OTP 低层驱动测试
 *
 * 假 Flash：静态 8KB，base=0x1000/size=0x2000/page=512，支持故障注入。
 * 假 OTP：4 个 32 位 word，program 只允许 0→1，支持失败注入。
 */

#include "unity.h"
#include "kw47a_flash_ll.h"
#include "kw47a_otp_ll.h"

#include <string.h>

#define FLASH_BASE  0x1000u
#define FLASH_SIZE  0x2000u
#define FLASH_PAGE  512u

static uint8_t g_flash[FLASH_SIZE];
static int     g_erase_rc;
static int     g_program_rc;
static int     g_read_rc;
static int     g_program_corrupt;
static int     g_enter_count;
static int     g_exit_count;

/* ── 假 Flash ───────────────────────────────────────────── */
static int fake_in_range(uint32_t addr, uint32_t len)
{
    if (addr < FLASH_BASE || len > FLASH_SIZE) {
        return -1;
    }
    if ((addr - FLASH_BASE) > (FLASH_SIZE - len)) {
        return -1;
    }
    return 0;
}

static int fake_erase(uint32_t addr, uint32_t len)
{
    if (g_erase_rc != 0) {
        return g_erase_rc;
    }
    if (fake_in_range(addr, len) != 0) {
        return -1;
    }
    (void)memset(&g_flash[addr - FLASH_BASE], 0xFF, len);
    return 0;
}

static int fake_program(uint32_t addr, const uint8_t *src, uint32_t len)
{
    uint32_t off;
    uint32_t i;

    if (g_program_rc != 0) {
        return g_program_rc;
    }
    if (src == NULL || fake_in_range(addr, len) != 0) {
        return -1;
    }
    off = addr - FLASH_BASE;
    for (i = 0; i < len; i++) {
        g_flash[off + i] = g_program_corrupt ?
                           (uint8_t)(src[i] ^ 0xFFu) : src[i];
    }
    return 0;
}

static int fake_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    if (g_read_rc != 0) {
        return g_read_rc;
    }
    if (dst == NULL || fake_in_range(addr, len) != 0) {
        return -1;
    }
    (void)memcpy(dst, &g_flash[addr - FLASH_BASE], len);
    return 0;
}

static void fake_critical_enter(void)
{
    g_enter_count++;
}

static void fake_critical_exit(void)
{
    g_exit_count++;
}

static kw47a_flash_port_t make_flash_port(void)
{
    kw47a_flash_port_t port;

    port.base = FLASH_BASE;
    port.size = FLASH_SIZE;
    port.page_size = FLASH_PAGE;
    port.erase = fake_erase;
    port.program = fake_program;
    port.read = fake_read;
    port.critical_enter = fake_critical_enter;
    port.critical_exit = fake_critical_exit;
    return port;
}

static void flash_env_reset(void)
{
    (void)memset(g_flash, 0xFF, sizeof(g_flash));
    g_erase_rc = 0;
    g_program_rc = 0;
    g_read_rc = 0;
    g_program_corrupt = 0;
    g_enter_count = 0;
    g_exit_count = 0;
    kw47a_flash_ll_unbind();
}

static void bind_fake_flash(void)
{
    kw47a_flash_port_t port = make_flash_port();

    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_bind(&port));
}

/* ── Flash 用例 ─────────────────────────────────────────── */

/* 1) bind 参数校验 */
static void test_flash_bind_validation(void)
{
    kw47a_flash_port_t port;
    uint8_t buf[4] = { 0 };

    flash_env_reset();

    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(NULL));
    TEST_ASSERT_EQUAL_UINT(0u, kw47a_flash_ll_page_size());

    port = make_flash_port();
    port.page_size = 500u;              /* 非 2 的幂 */
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(&port));

    port = make_flash_port();
    port.size = 0x2100u;                /* 非页大小整数倍 */
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(&port));

    port = make_flash_port();
    port.erase = NULL;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(&port));

    port = make_flash_port();
    port.program = NULL;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(&port));

    port = make_flash_port();
    port.read = NULL;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_bind(&port));

    /* 绑定失败不得留下半绑定状态 */
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, buf, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, buf, 4u));

    port = make_flash_port();
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_bind(&port));
    TEST_ASSERT_EQUAL_UINT(FLASH_PAGE, kw47a_flash_ll_page_size());
}

/* 2) 页擦成功 / 非对齐 / 越界 */
static void test_flash_erase_single_page(void)
{
    uint32_t i;

    flash_env_reset();
    bind_fake_flash();
    (void)memset(g_flash, 0x00, FLASH_PAGE * 2u);

    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_erase(FLASH_BASE));
    for (i = 0; i < FLASH_PAGE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xFF, g_flash[i]);
    }
    TEST_ASSERT_EQUAL_UINT8(0x00, g_flash[FLASH_PAGE]);   /* 只擦一页 */
    TEST_ASSERT_EQUAL_INT(1, g_enter_count);
    TEST_ASSERT_EQUAL_INT(1, g_exit_count);

    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_erase(FLASH_BASE + FLASH_SIZE -
                                                  FLASH_PAGE));

    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE + 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE + FLASH_SIZE));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE - 4u));
}

/* 3) 编程成功并回读；对齐/跨页/越界拒绝 */
static void test_flash_program_and_readback(void)
{
    const uint8_t src[8] = { 0x01, 0x23, 0x45, 0x67,
                             0x89, 0xAB, 0xCD, 0xEF };
    uint8_t out[8];

    flash_env_reset();
    bind_fake_flash();

    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_program(FLASH_BASE + 16u,
                                                    src, sizeof(src)));
    (void)memset(out, 0, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_read(FLASH_BASE + 16u,
                                                 out, sizeof(out)));
    TEST_ASSERT_EQUAL_MEMORY(src, out, sizeof(src));
    TEST_ASSERT_EQUAL_INT(1, g_enter_count);
    TEST_ASSERT_EQUAL_INT(1, g_exit_count);

    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE + 17u,
                                                     src, 4u));   /* 非对齐 */
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE + 16u,
                                                     src, 6u));   /* 长度 */
    TEST_ASSERT_EQUAL_INT(-1,
        kw47a_flash_ll_program(FLASH_BASE + FLASH_PAGE - 4u, src, 8u));
    TEST_ASSERT_EQUAL_INT(-1,
        kw47a_flash_ll_program(FLASH_BASE + FLASH_SIZE - 4u, src, 8u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, NULL, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, src, 0u));
}

/* 4) 未擦除区域编程失败且数据不变 */
static void test_flash_program_requires_erased_area(void)
{
    const uint8_t src[4] = { 0x11, 0x22, 0x33, 0x44 };

    flash_env_reset();
    bind_fake_flash();
    (void)memset(g_flash, 0x00, 32u);

    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE + 8u,
                                                     src, 4u));
    TEST_ASSERT_EQUAL_UINT8(0x00, g_flash[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, g_flash[11]);

    /* 仅一个字节非 0xFF 也必须拒绝 */
    g_flash[12] = 0xFF;
    g_flash[13] = 0xFF;
    g_flash[14] = 0xFF;
    g_flash[15] = 0xFE;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE + 12u,
                                                     src, 4u));
    TEST_ASSERT_EQUAL_UINT8(0xFE, g_flash[15]);
}

/* 5) 擦/编程注入失败；回读不一致必须报错 */
static void test_flash_io_failure_injection(void)
{
    const uint8_t src[4] = { 0xA5, 0x5A, 0x01, 0x02 };
    uint8_t out[4];

    flash_env_reset();
    bind_fake_flash();

    g_erase_rc = -2;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE));
    g_erase_rc = 0;

    g_program_rc = -3;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, src, 4u));
    g_program_rc = 0;

    /* 编程返回成功但回读不一致（半写）→ -1 */
    g_program_corrupt = 1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, src, 4u));
    g_program_corrupt = 0;

    /* 回读失败 → -1 */
    g_read_rc = -1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE + 8u,
                                                     src, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, out, 4u));
    g_read_rc = 0;

    /* 擦除后恢复正常 */
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_erase(FLASH_BASE));
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_program(FLASH_BASE, src, 4u));
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_read(FLASH_BASE, out, 4u));
    TEST_ASSERT_EQUAL_MEMORY(src, out, 4u);
}

/* 6) read 边界与未绑定 */
static void test_flash_read_bounds_and_unbound(void)
{
    uint8_t out[4] = { 0 };

    flash_env_reset();

    TEST_ASSERT_EQUAL_UINT(0u, kw47a_flash_ll_page_size());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, out, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_erase(FLASH_BASE));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_program(FLASH_BASE, out, 4u));

    bind_fake_flash();
    TEST_ASSERT_EQUAL_INT(0, kw47a_flash_ll_read(FLASH_BASE, out, 4u));
    TEST_ASSERT_EQUAL_INT(0,
        kw47a_flash_ll_read(FLASH_BASE + FLASH_SIZE - 4u, out, 4u));
    TEST_ASSERT_EQUAL_INT(-1,
        kw47a_flash_ll_read(FLASH_BASE + FLASH_SIZE - 2u, out, 4u));
    TEST_ASSERT_EQUAL_INT(-1,
        kw47a_flash_ll_read(FLASH_BASE + FLASH_SIZE, out, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE - 4u, out, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, NULL, 4u));
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, out, 0u));

    kw47a_flash_ll_unbind();
    TEST_ASSERT_EQUAL_UINT(0u, kw47a_flash_ll_page_size());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_flash_ll_read(FLASH_BASE, out, 4u));
}

/* ── 假 OTP ─────────────────────────────────────────────── */
#define OTP_WORDS 4u

static uint32_t g_otp[OTP_WORDS];
static int      g_otp_program_rc;
static int      g_otp_program_ignore;

static uint32_t fake_otp_read(uint32_t idx)
{
    if (idx >= OTP_WORDS) {
        return 0u;
    }
    return g_otp[idx];
}

static int fake_otp_program(uint32_t idx, uint32_t value)
{
    uint32_t old;

    if (g_otp_program_rc != 0) {
        return g_otp_program_rc;
    }
    if (idx >= OTP_WORDS) {
        return -1;
    }
    old = g_otp[idx];
    if ((old & ~value) != 0u) {
        return -1;                      /* 只允许 0→1 */
    }
    if (g_otp_program_ignore) {
        return 0;                       /* 模拟熔丝未烧断 */
    }
    g_otp[idx] = value;
    return 0;
}

static kw47a_otp_port_t make_otp_port(void)
{
    kw47a_otp_port_t port;

    port.read_word = fake_otp_read;
    port.program_word = fake_otp_program;
    return port;
}

static void otp_env_reset(void)
{
    const kw47a_otp_port_t port = make_otp_port();

    (void)memset(g_otp, 0, sizeof(g_otp));
    g_otp_program_rc = 0;
    g_otp_program_ignore = 0;

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_bind(&port));
}

/* 7) 初始 lock=false、epoch=0 */
static void test_otp_initial_state(void)
{
    kw47a_otp_port_t bad;

    /* 首次使用：未绑定 → 读 false/0，写 -1 */
    TEST_ASSERT_FALSE(kw47a_otp_lock_read());
    TEST_ASSERT_FALSE(kw47a_otp_debug_lock_read());
    TEST_ASSERT_EQUAL_UINT(0u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_lock_set());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_debug_lock_set());
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_epoch_raise(1u));

    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_bind(NULL));
    bad = make_otp_port();
    bad.read_word = NULL;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_bind(&bad));

    otp_env_reset();
    TEST_ASSERT_FALSE(kw47a_otp_lock_read());
    TEST_ASSERT_FALSE(kw47a_otp_debug_lock_read());
    TEST_ASSERT_EQUAL_UINT(0u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0u, g_otp[KW47A_OTP_WORD_LOCK]);
    TEST_ASSERT_EQUAL_UINT(0u, g_otp[KW47A_OTP_WORD_EPOCH]);
}

/* 8) lock_set 幂等；编程失败保持 false */
static void test_otp_lock_set_idempotent(void)
{
    otp_env_reset();

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());
    TEST_ASSERT_TRUE(kw47a_otp_lock_read());
    TEST_ASSERT_EQUAL_UINT(0x1u, g_otp[KW47A_OTP_WORD_LOCK]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());   /* 幂等 */
    TEST_ASSERT_EQUAL_UINT(0x1u, g_otp[KW47A_OTP_WORD_LOCK]);

    /* debug lock 为独立 bit1 */
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_debug_lock_set());
    TEST_ASSERT_TRUE(kw47a_otp_debug_lock_read());
    TEST_ASSERT_EQUAL_UINT(0x3u, g_otp[KW47A_OTP_WORD_LOCK]);

    /* 编程失败 → -1 且仍为未锁 */
    otp_env_reset();
    g_otp_program_rc = -1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_lock_set());
    TEST_ASSERT_FALSE(kw47a_otp_lock_read());
    TEST_ASSERT_EQUAL_UINT(0u, g_otp[KW47A_OTP_WORD_LOCK]);

    /* 烧写未生效（回读不符）→ -1 */
    g_otp_program_rc = 0;
    g_otp_program_ignore = 1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_lock_set());
    TEST_ASSERT_FALSE(kw47a_otp_lock_read());
}

/* 9) epoch 单调抬升 */
static void test_otp_epoch_monotonic(void)
{
    otp_env_reset();

    TEST_ASSERT_EQUAL_UINT(0u, kw47a_otp_epoch_read());

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(5u));
    TEST_ASSERT_EQUAL_UINT(5u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x1Fu, g_otp[KW47A_OTP_WORD_EPOCH]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(3u));   /* 已达成 */
    TEST_ASSERT_EQUAL_UINT(5u, kw47a_otp_epoch_read());

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(8u));
    TEST_ASSERT_EQUAL_UINT(8u, kw47a_otp_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0xFFu, g_otp[KW47A_OTP_WORD_EPOCH]);

    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_epoch_raise(32u));
    TEST_ASSERT_EQUAL_UINT(8u, kw47a_otp_epoch_read());

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(0u));
    TEST_ASSERT_EQUAL_UINT(8u, kw47a_otp_epoch_read());

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(31u));
    TEST_ASSERT_EQUAL_UINT(31u, kw47a_otp_epoch_read());
}

/* 10) 只置位（值单调不减）；回读不符注入 → -1 */
static void test_otp_program_only_sets_bits(void)
{
    otp_env_reset();

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(2u));
    TEST_ASSERT_EQUAL_UINT(0x3u, g_otp[KW47A_OTP_WORD_EPOCH]);
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_lock_set());
    TEST_ASSERT_EQUAL_UINT(0x1u, g_otp[KW47A_OTP_WORD_LOCK]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_ll_epoch_raise(4u));
    TEST_ASSERT_EQUAL_UINT(0xFu, g_otp[KW47A_OTP_WORD_EPOCH]);
    TEST_ASSERT_TRUE((g_otp[KW47A_OTP_WORD_EPOCH] & 0x3u) == 0x3u);

    /* 回读不符 → -1，原值不变 */
    g_otp_program_ignore = 1;
    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_ll_epoch_raise(10u));
    TEST_ASSERT_EQUAL_UINT(0xFu, g_otp[KW47A_OTP_WORD_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(4u, kw47a_otp_epoch_read());

    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_debug_lock_set());
    TEST_ASSERT_FALSE(kw47a_otp_debug_lock_read());

    g_otp_program_ignore = 0;
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_debug_lock_set());
    TEST_ASSERT_TRUE(kw47a_otp_debug_lock_read());
    TEST_ASSERT_EQUAL_UINT(0x3u, g_otp[KW47A_OTP_WORD_LOCK]);
}

/* 11) keystore epoch 跨 word 单调抬升、封顶与低位幂等 */
static void test_otp_keystore_epoch_monotonic(void)
{
    otp_env_reset();

    TEST_ASSERT_EQUAL_UINT(0u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(31u));
    TEST_ASSERT_EQUAL_UINT(31u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x7FFFFFFFu, g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(0u, g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(32u));
    TEST_ASSERT_EQUAL_UINT(32u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x7FFFFFFFu, g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH]);
    TEST_ASSERT_EQUAL_UINT(1u, g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u]);

    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(62u));
    TEST_ASSERT_EQUAL_UINT(62u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_UINT(0x7FFFFFFFu,
                           g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u]);

    TEST_ASSERT_EQUAL_INT(-1, kw47a_otp_keystore_epoch_raise(63u));
    TEST_ASSERT_EQUAL_UINT(62u, kw47a_otp_keystore_epoch_read());
    TEST_ASSERT_EQUAL_INT(0, kw47a_otp_keystore_epoch_raise(31u));
    TEST_ASSERT_EQUAL_UINT(62u, kw47a_otp_keystore_epoch_read());
}

/* 12) word3 高位有缺口时只统计从 bit0 起连续的 1 */
static void test_otp_keystore_epoch_contiguous_gap(void)
{
    otp_env_reset();

    g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH] = 0x7FFFFFFFu;
    g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u] = 0x4000000Bu;
    TEST_ASSERT_EQUAL_UINT(33u, kw47a_otp_keystore_epoch_read());

    /* word3 bit0 为 0 时，即使更高位为 1，水位也停在 word2 末尾。 */
    g_otp[KW47A_OTP_WORD_KEYSTORE_EPOCH + 1u] = 0x2u;
    TEST_ASSERT_EQUAL_UINT(31u, kw47a_otp_keystore_epoch_read());
}

int run_kw47a_drv_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flash_bind_validation);
    RUN_TEST(test_flash_erase_single_page);
    RUN_TEST(test_flash_program_and_readback);
    RUN_TEST(test_flash_program_requires_erased_area);
    RUN_TEST(test_flash_io_failure_injection);
    RUN_TEST(test_flash_read_bounds_and_unbound);
    RUN_TEST(test_otp_initial_state);
    RUN_TEST(test_otp_lock_set_idempotent);
    RUN_TEST(test_otp_epoch_monotonic);
    RUN_TEST(test_otp_program_only_sets_bits);
    RUN_TEST(test_otp_keystore_epoch_monotonic);
    RUN_TEST(test_otp_keystore_epoch_contiguous_gap);
    UNITY_END();
}

#if !defined(TEST_KW47A_DRV_NO_MAIN) && !defined(TEST_LIB_MODE)
void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    return run_kw47a_drv_tests();
}
#endif
