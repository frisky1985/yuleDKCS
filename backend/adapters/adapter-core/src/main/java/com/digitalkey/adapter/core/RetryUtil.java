package com.digitalkey.adapter.core;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.Predicate;

/**
 * Exponential-backoff retry utility for TSP adapter HTTP calls.
 *
 * <p>Usage:
 * <pre>{@code
 *   RetryUtil retry = new RetryUtil("ccc-client", log, 3, 1000, 30000);
 *   String resp = retry.executeWithRetry(() -> httpClient.get(...));
 * }</pre>
 *
 * <p>Retry policy:
 * <ul>
 *   <li>Base delay: {@code initialDelayMs}, doubled each attempt</li>
 *   <li>Jitter: ±25 % (uniform) to avoid thundering-herd</li>
 *   <li>Max total wall-clock: {@code maxTotalTimeoutMs}</li>
 *   <li>Retry only on {@link #isRetryable(Throwable)} (network / 5xx)</li>
 * </ul>
 */
public class RetryUtil {

    private static final Logger log = LoggerFactory.getLogger(RetryUtil.class);

    private final String operationName;
    private final Logger clientLog;
    private final int maxRetries;
    private final long initialDelayMs;
    private final long maxTotalTimeoutMs;
    private final Predicate<Throwable> retryPredicate;

    /**
     * Full constructor.
     *
     * @param operationName    label for log messages
     * @param clientLog        the calling client's logger
     * @param maxRetries       max retry attempts (0 = no retry)
     * @param initialDelayMs   base delay in ms (doubled each attempt)
     * @param maxTotalTimeoutMs  hard timeout from first attempt start
     */
    public RetryUtil(String operationName, Logger clientLog,
                     int maxRetries, long initialDelayMs, long maxTotalTimeoutMs) {
        this(operationName, clientLog, maxRetries, initialDelayMs, maxTotalTimeoutMs,
             RetryUtil::isRetryable);
    }

    /**
     * Constructor with custom retry predicate.
     */
    public RetryUtil(String operationName, Logger clientLog,
                     int maxRetries, long initialDelayMs, long maxTotalTimeoutMs,
                     Predicate<Throwable> retryPredicate) {
        this.operationName = operationName;
        this.clientLog = clientLog;
        this.maxRetries = Math.max(0, maxRetries);
        this.initialDelayMs = Math.max(100, initialDelayMs);
        this.maxTotalTimeoutMs = Math.max(initialDelayMs, maxTotalTimeoutMs);
        this.retryPredicate = retryPredicate;
    }

    // ── Synchronous retry ──────────────────────────────────────────────

    /**
     * Execute {@code callable} with retry. Blocks the calling thread.
     *
     * @throws RuntimeException wrapping the last failure if all attempts fail
     */
    public <T> T executeWithRetry(Callable<T> callable) {
        long deadline = System.currentTimeMillis() + maxTotalTimeoutMs;
        long delay = initialDelayMs;
        Throwable lastException = null;

        for (int attempt = 1; attempt <= maxRetries + 1; attempt++) {
            try {
                if (attempt > 1) {
                    clientLog.warn("[{}] retry attempt {}/{} after {}ms",
                        operationName, attempt - 1, maxRetries, delay);
                }
                return callable.call();
            } catch (Exception e) {
                lastException = e;
                if (!retryPredicate.test(e)) {
                    clientLog.error("[{}] non-retryable error on attempt {}/{}: {}",
                        operationName, attempt, maxRetries + 1, e.getMessage());
                    throw new RuntimeException(e);
                }
                if (System.currentTimeMillis() + delay > deadline || attempt > maxRetries) {
                    clientLog.error("[{}] exhausted retries ({}) after {} attempts, last: {}",
                        operationName, maxRetries, attempt, e.getMessage());
                    throw new RuntimeException("Exhausted retries for " + operationName, e);
                }
                sleepWithJitter(delay);
                delay = Math.min(delay * 2, deadline - System.currentTimeMillis());
            }
        }
        throw new RuntimeException("Unexpected: retry loop exited", lastException);
    }

    // ── Async retry ────────────────────────────────────────────────────

    /**
     * Async version — returns a {@link CompletableFuture} that retries on failure.
     */
    public <T> CompletableFuture<T> executeAsync(Callable<T> callable, Executor executor) {
        return executeAsync(callable, executor, maxRetries, initialDelayMs);
    }

    private <T> CompletableFuture<T> executeAsync(Callable<T> callable, Executor executor,
                                                   int remainingRetries, long delayMs) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                return callable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }, executor).thenCompose(result -> CompletableFuture.completedFuture(result))
          .exceptionallyCompose(ex -> {
              Throwable cause = (ex instanceof RuntimeException && ex.getCause() != null)
                  ? ex.getCause() : ex;
              if (remainingRetries > 0 && retryPredicate.test(cause)) {
                  clientLog.warn("[{}] async retry attempt, {} left, delay={}ms, error: {}",
                      operationName, remainingRetries, delayMs, cause.getMessage());
                  return CompletableFuture.runAsync(() -> sleepWithJitter(delayMs), executor)
                      .thenCompose(v -> executeAsync(callable, executor,
                          remainingRetries - 1, Math.min(delayMs * 2, maxTotalTimeoutMs)));
              }
              return CompletableFuture.failedFuture(cause);
          });
    }

    // ── Helpers ────────────────────────────────────────────────────────

    /** Default retry predicate: network errors and 5xx server errors. */
    public static boolean isRetryable(Throwable t) {
        if (t == null) return false;
        String msg = t.getMessage() != null ? t.getMessage().toLowerCase() : "";
        // Network-level
        if (msg.contains("timeout") || msg.contains("connection refused")
            || msg.contains("connection reset") || msg.contains("connection closed")
            || msg.contains("eof") || msg.contains("i/o error")
            || msg.contains("no route to host") || msg.contains("unknownhost")) {
            return true;
        }
        // 5xx server errors
        if (msg.contains("500") || msg.contains("502") || msg.contains("503")
            || msg.contains("504") || msg.contains("service unavailable")
            || msg.contains("internal server error") || msg.contains("bad gateway")) {
            return true;
        }
        // Retry on cause chain
        if (t.getCause() != null && t.getCause() != t) {
            return isRetryable(t.getCause());
        }
        return false;
    }

    private static void sleepWithJitter(long baseMs) {
        long jitter = (long) (baseMs * 0.25 * (Math.random() - 0.5));
        long actualMs = Math.max(1, baseMs + jitter);
        try {
            Thread.sleep(actualMs);
        } catch (InterruptedException ie) {
            Thread.currentThread().interrupt();
        }
    }
}
