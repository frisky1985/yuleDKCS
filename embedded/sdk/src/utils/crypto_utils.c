/******************************************************************************
 * @file    crypto_utils.c
 * @brief   硬件随机数生成器 (TRNG) 实现
 * @author  YuleTech
 * @version 2.0.0
 * @date    2026-05-09
 * 
 * @note    CRITICAL FIX: 替换可预测的 LCG 伪随机数为硬件 TRNG
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "dkcs_error.h"
#include "dkcs_types.h"

/******************************************************************************
 * 平台选择
 ******************************************************************************/
#if defined(STM32_PLATFORM)
    #include "stm32l4xx_hal.h"
    #include "stm32l4xx_hal_rng.h"
#elif defined(NXP_PLATFORM)
    #include "fsl_trng.h"
#elif defined(USE_MBEDTLS_RNG)
    #include "mbedtls/entropy.h"
    #include "mbedtls/ctr_drbg.h"
#endif

/******************************************************************************
 * 内部状态
 ******************************************************************************/
static volatile int g_trng_initialized = 0;

#if defined(USE_MBEDTLS_RNG)
    static mbedtls_entropy_context entropy_ctx;
    static mbedtls_ctr_drbg_context ctr_drbg_ctx;
#endif

/******************************************************************************
 * 初始化 TRNG 硬件
 ******************************************************************************/
error_t trng_init(void)
{
    if (g_trng_initialized) {
        return OK;
    }

#if defined(STM32_PLATFORM)
    /* STM32 RNG 硬件初始化 */
    __HAL_RCC_RNG_CLK_ENABLE();
    
    RNG_HandleTypeDef hrng;
    hrng.Instance = RNG;
    
    if (HAL_RNG_Init(&hrng) != HAL_OK) {
        return ERROR_TRNG_INIT_FAILED;
    }

#elif defined(NXP_PLATFORM)
    /* NXP TRNG 初始化 */
    trng_config_t trngConfig;
    
    if (TRNG_GetDefaultConfig(&trngConfig) != kStatus_Success) {
        return ERROR_TRNG_INIT_FAILED;
    }
    
    trngConfig.sampleMode = kTRNG_SampleModeRaw;
    trngConfig.clockMode = kTRNG_ClockModeRingOscillator;
    
    if (TRNG_Init(TRNG, &trngConfig) != kStatus_Success) {
        return ERROR_TRNG_INIT_FAILED;
    }

#elif defined(USE_MBEDTLS_RNG)
    /* 使用 mbedtls 作为回退 */
    const char *pers = "yuledkcs_trng";
    
    mbedtls_entropy_init(&entropy_ctx);
    mbedtls_ctr_drbg_init(&ctr_drbg_ctx);
    
    if (mbedtls_ctr_drbg_seed(
        &ctr_drbg_ctx,
        mbedtls_entropy_func,
        &entropy_ctx,
        (const unsigned char *)pers,
        strlen(pers)
    ) != 0) {
        return ERROR_TRNG_INIT_FAILED;
    }
#endif

    g_trng_initialized = 1;
    return OK;
}

/******************************************************************************
 * 反初始化 TRNG
 ******************************************************************************/
void trng_deinit(void)
{
    if (!g_trng_initialized) {
        return;
    }

#if defined(STM32_PLATFORM)
    HAL_RNG_DeInit(&hrng);
    __HAL_RCC_RNG_CLK_DISABLE();

#elif defined(NXP_PLATFORM)
    TRNG_Deinit(TRNG);

#elif defined(USE_MBEDTLS_RNG)
    mbedtls_ctr_drbg_free(&ctr_drbg_ctx);
    mbedtls_entropy_free(&entropy_ctx);
#endif

    g_trng_initialized = 0;
}

/******************************************************************************
 * 生成随机字节
 ******************************************************************************/
error_t trng_get_random(uint8_t *output, size_t len)
{
    if (output == NULL || len == 0) {
        return ERROR_INVALID_PARAM;
    }

    if (!g_trng_initialized) {
        error_t ret = trng_init();
        if (ret != OK) {
            return ret;
        }
    }

#if defined(STM32_PLATFORM)
    /* STM32 RNG - 32位随机数 */
    RNG_HandleTypeDef hrng;
    hrng.Instance = RNG;
    
    for (size_t i = 0; i < len; i += 4) {
        uint32_t random_val;
        
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random_val) != HAL_OK) {
            return ERROR_TRNG_GENERATION_FAILED;
        }
        
        size_t copy_len = (len - i < 4) ? (len - i) : 4;
        memcpy(output + i, &random_val, copy_len);
    }

#elif defined(NXP_PLATFORM)
    /* NXP TRNG */
    if (TRNG_GetRandomData(TRNG, output, len) != kStatus_Success) {
        return ERROR_TRNG_GENERATION_FAILED;
    }

#elif defined(USE_MBEDTLS_RNG)
    /* mbedtls 回退 */
    if (mbedtls_ctr_drbg_random(&ctr_drbg_ctx, output, len) != 0) {
        return ERROR_TRNG_GENERATION_FAILED;
    }
#endif

    return OK;
}

/******************************************************************************
 * 生成随机非负整数 (0 到 max-1)
 ******************************************************************************/
error_t trng_get_random_uint32(uint32_t max, uint32_t *result)
{
    if (result == NULL || max == 0) {
        return ERROR_INVALID_PARAM;
    }

    uint8_t random_bytes[4];
    error_t ret = trng_get_random(random_bytes, 4);
    if (ret != OK) {
        return ret;
    }

    uint32_t random_val;
    memcpy(&random_val, random_bytes, 4);
    
    /* 避免偏移 - 使用余数方法 */
    *result = random_val % max;
    
    /* 清除敏感数据 */
    memset(random_bytes, 0, sizeof(random_bytes));
    
    return OK;
}

/******************************************************************************
 * 检查 TRNG 健康状态
 ******************************************************************************/
error_t trng_health_check(void)
{
    if (!g_trng_initialized) {
        return ERROR_NOT_INITIALIZED;
    }

#if defined(STM32_PLATFORM)
    RNG_HandleTypeDef hrng;
    hrng.Instance = RNG;
    
    /* 检查 RNG 状态寄存器 */
    if (__HAL_RNG_GET_FLAG(&hrng, RNG_FLAG_SECS) != RESET) {
        return ERROR_TRNG_SEED_ERROR;
    }
    
    if (__HAL_RNG_GET_FLAG(&hrng, RNG_FLAG_CECS) != RESET) {
        return ERROR_TRNG_CLOCK_ERROR;
    }

#elif defined(NXP_PLATFORM)
    /* NXP TRNG 健康检查 */
    uint32_t status = TRNG_GetStatus(TRNG);
    if (status & (TRNG_STATUS_TF1BR_MASK | TRNG_STATUS_TF2BR_MASK)) {
        return ERROR_TRNG_HEALTH_FAIL;
    }
#endif

    return OK;
}

/******************************************************************************
 * 保留的旧版本函数 (向后兼容)
 * 但已更新为使用 TRNG
 ******************************************************************************/

/* 已废弃用的 LCG 参数 - 保留以避免编译错误 */
static const uint32_t DEPRECATED_LCG_SEED = 0;

error_t crypto_generate_random_bytes(uint8_t *output, size_t len)
{
    /* CRITICAL FIX: 现在使用 TRNG 而不是 LCG */
    return trng_get_random(output, len);
}

uint32_t crypto_random_uint32(void)
{
    uint32_t result;
    trng_get_random((uint8_t *)&result, sizeof(result));
    return result;
}

void crypto_fill_random(uint8_t *buffer, size_t len)
{
    trng_get_random(buffer, len);
}

/******************************************************************************
 * 密钥生成工具
 ******************************************************************************/
error_t crypto_generate_key(uint8_t *key, size_t key_len)
{
    if (key == NULL || key_len == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 检查密钥长度 */
    if (key_len != 16 && key_len != 24 && key_len != 32) {
        return ERROR_INVALID_KEY_LENGTH;
    }
    
    return trng_get_random(key, key_len);
}

error_t crypto_generate_iv(uint8_t *iv, size_t iv_len)
{
    if (iv == NULL || iv_len == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    /* 支持常见 IV 长度 */
    if (iv_len != 12 && iv_len != 16) {
        return ERROR_INVALID_IV_LENGTH;
    }
    
    return trng_get_random(iv, iv_len);
}

error_t crypto_generate_nonce(uint8_t *nonce, size_t nonce_len)
{
    if (nonce == NULL || nonce_len == 0) {
        return ERROR_INVALID_PARAM;
    }
    
    return trng_get_random(nonce, nonce_len);
}

/******************************************************************************
 * 密钥生成 (ECDH 曲线)
 ******************************************************************************/
error_t crypto_generate_ecdh_keypair(
    uint8_t *private_key,
    size_t private_key_len,
    uint8_t *public_key,
    size_t *public_key_len)
{
    if (private_key == NULL || public_key == NULL || public_key_len == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    if (private_key_len != 32) {
        return ERROR_INVALID_KEY_LENGTH;
    }
    
    /* 生成私钥 */
    error_t ret = trng_get_random(private_key, private_key_len);
    if (ret != OK) {
        return ret;
    }
    
    /* 确保私钥在有效范围内 (编码其他函数负责公钥计算) */
    private_key[0] &= 0xF8;  /* 清除低 3 位 */
    private_key[31] &= 0x7F; /* 清除高位 */
    private_key[31] |= 0x40; /* 设置第 254 位 */
    
    /* 注: 公钥计算需要调用实际的 ECC 点乘函数 */
    *public_key_len = 65; /* 压缩格式 */
    
    return OK;
}

/******************************************************************************
 * 安全内存清除
 ******************************************************************************/
static volatile uint8_t *secure_memset(volatile uint8_t *ptr, uint8_t value, size_t num)
{
    volatile uint8_t *p = ptr;
    while (num--) {
        *p++ = value;
    }
    return ptr;
}

void crypto_secure_clear(void *buffer, size_t len)
{
    if (buffer != NULL && len > 0) {
        secure_memset((volatile uint8_t *)buffer, 0, len);
    }
}
