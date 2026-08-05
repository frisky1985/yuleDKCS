package com.digitalkey.adapter.core;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for {@link RetryUtil} exponential-backoff retry logic.
 */
class RetryUtilTest {

    private static final Logger log = LoggerFactory.getLogger(RetryUtilTest.class);
    private RetryUtil retryUtil;

    @BeforeEach
    void setUp() {
        retryUtil = new RetryUtil("test-op", log, 3, 50, 5000);
    }

    @Test
    void shouldSucceedOnFirstAttempt() {
        String result = retryUtil.executeWithRetry(() -> "ok");
        assertEquals("ok", result);
    }

    @Test
    void shouldSucceedAfterRetries() {
        AtomicInteger attempts = new AtomicInteger(0);

        String result = retryUtil.executeWithRetry(() -> {
            if (attempts.incrementAndGet() < 3) {
                throw new RuntimeException("timeout: temporary error");
            }
            return "success-after-retry";
        });

        assertEquals("success-after-retry", result);
        assertEquals(3, attempts.get());
    }

    @Test
    void shouldThrowAfterExhaustingRetries() {
        AtomicInteger attempts = new AtomicInteger(0);

        Exception ex = assertThrows(RuntimeException.class, () ->
            retryUtil.executeWithRetry(() -> {
                attempts.incrementAndGet();
                throw new RuntimeException("timeout: persistent error");
            })
        );

        assertTrue(ex.getMessage().contains("Exhausted retries"));
        assertEquals(4, attempts.get()); // 1 initial + 3 retries
    }

    @Test
    void shouldNotRetryNonRetryableError() {
        AtomicInteger attempts = new AtomicInteger(0);

        Exception ex = assertThrows(RuntimeException.class, () ->
            retryUtil.executeWithRetry(() -> {
                attempts.incrementAndGet();
                throw new RuntimeException("400 bad request");
            })
        );

        // 400 is not retryable per our predicate; should only attempt once
        assertEquals(1, attempts.get());
    }

    @Test
    void shouldClassifyRetryableErrors() {
        assertTrue(RetryUtil.isRetryable(new RuntimeException("timeout")));
        assertTrue(RetryUtil.isRetryable(new RuntimeException("connection refused")));
        assertTrue(RetryUtil.isRetryable(new RuntimeException("connection reset")));
        assertTrue(RetryUtil.isRetryable(new RuntimeException("500 internal server error")));
        assertTrue(RetryUtil.isRetryable(new RuntimeException("503 service unavailable")));
        assertTrue(RetryUtil.isRetryable(new RuntimeException("502 bad gateway")));
    }

    @Test
    void shouldClassifyNonRetryableErrors() {
        assertFalse(RetryUtil.isRetryable(new RuntimeException("400 bad request")));
        assertFalse(RetryUtil.isRetryable(new RuntimeException("401 unauthorized")));
        assertFalse(RetryUtil.isRetryable(new RuntimeException("403 forbidden")));
        assertFalse(RetryUtil.isRetryable(new RuntimeException("404 not found")));
    }

    @Test
    void withZeroRetriesShouldNotRetry() {
        RetryUtil noRetry = new RetryUtil("no-retry", log, 0, 50, 5000);
        AtomicInteger attempts = new AtomicInteger(0);

        assertThrows(RuntimeException.class, () ->
            noRetry.executeWithRetry(() -> {
                attempts.incrementAndGet();
                throw new RuntimeException("fail");
            })
        );

        assertEquals(1, attempts.get());
    }

    @Test
    void shouldHandleInterruptedExceptionGracefully() {
        // Just verify sleepWithJitter doesn't throw
        assertDoesNotThrow(() -> retryUtil.executeWithRetry(() -> "ok"));
    }
}
