/** @file test_buffer_pool.c
 * @brief 缓冲区池测试
 *
 * @copyright Copyright (c) 2024 YuleTech
 * @license MIT
 */

#include "unity.h"

/* 前向声明缓冲区池函数 */
extern bool MicroDDS_BufferPool_Init(void);
extern void MicroDDS_BufferPool_Shutdown(void);
extern void* MicroDDS_BufferPool_Alloc(void);
extern void MicroDDS_BufferPool_Free(void* buffer);
extern uint16_t MicroDDS_BufferPool_GetBufferSize(void);
extern uint32_t MicroDDS_BufferPool_GetAvailableCount(void);

void setUp(void) {
    MicroDDS_BufferPool_Init();
}

void tearDown(void) {
    MicroDDS_BufferPool_Shutdown();
}

void test_buffer_pool_init(void) {
    bool result = MicroDDS_BufferPool_Init();
    TEST_ASSERT_TRUE(result);
}

void test_buffer_pool_alloc_and_free(void) {
    void* buffer = MicroDDS_BufferPool_Alloc();
    TEST_ASSERT_NOT_NULL(buffer);
    
    MicroDDS_BufferPool_Free(buffer);
    /* 释放后可以重新分配 */
    void* buffer2 = MicroDDS_BufferPool_Alloc();
    TEST_ASSERT_NOT_NULL(buffer2);
    TEST_ASSERT_EQUAL(buffer, buffer2);
}

void test_buffer_pool_alloc_exhaustion(void) {
    /* 分配所有可用缓冲区 */
    void* buffers[16];
    uint32_t count = 0;
    
    for (int i = 0; i < 16; i++) {
        buffers[i] = MicroDDS_BufferPool_Alloc();
        if (buffers[i] == NULL) {
            break;
        }
        count++;
    }
    
    /* 此时应该无法再分配 */
    void* extra = MicroDDS_BufferPool_Alloc();
    TEST_ASSERT_NULL(extra);
    
    /* 释放所有缓冲区 */
    for (uint32_t i = 0; i < count; i++) {
        MicroDDS_BufferPool_Free(buffers[i]);
    }
}

void test_buffer_pool_free_null(void) {
    /* 释放null不应崩溃 */
    MicroDDS_BufferPool_Free(NULL);
    TEST_PASS();
}

void test_buffer_pool_get_buffer_size(void) {
    uint16_t size = MicroDDS_BufferPool_GetBufferSize();
    TEST_ASSERT_GREATER_THAN(0, size);
}

void test_buffer_pool_get_available_count(void) {
    uint32_t initial = MicroDDS_BufferPool_GetAvailableCount();
    TEST_ASSERT_GREATER_THAN(0, initial);
    
    void* buffer = MicroDDS_BufferPool_Alloc();
    uint32_t after_alloc = MicroDDS_BufferPool_GetAvailableCount();
    TEST_ASSERT_EQUAL(initial - 1, after_alloc);
    
    MicroDDS_BufferPool_Free(buffer);
    uint32_t after_free = MicroDDS_BufferPool_GetAvailableCount();
    TEST_ASSERT_EQUAL(initial, after_free);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_buffer_pool_init);
    RUN_TEST(test_buffer_pool_alloc_and_free);
    RUN_TEST(test_buffer_pool_alloc_exhaustion);
    RUN_TEST(test_buffer_pool_free_null);
    RUN_TEST(test_buffer_pool_get_buffer_size);
    RUN_TEST(test_buffer_pool_get_available_count);
    
    return UNITY_END();
}
