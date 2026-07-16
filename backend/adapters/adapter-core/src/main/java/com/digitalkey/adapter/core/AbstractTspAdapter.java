package com.digitalkey.adapter.core;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Abstract base class for TSP adapters providing common functionality.
 *
 * <h3>What this provides</h3>
 * <ul>
 *   <li>Async execution via cached daemon thread pool</li>
 *   <li>Lifecycle management (init/shutdown)</li>
 *   <li>Enabled / disabled guard</li>
 *   <li>Exponential-backoff retry via {@link RetryUtil}</li>
 *   <li>Response validation after each operation</li>
 * </ul>
 *
 * <p>Subclasses implement the {@code do*()} abstract methods.
 */
public abstract class AbstractTspAdapter implements TspAdapter {

    protected final Logger log = LoggerFactory.getLogger(getClass());
    protected final ExecutorService executor = Executors.newCachedThreadPool(r -> {
        Thread t = new Thread(r, getAdapterName() + "-executor");
        t.setDaemon(true);
        return t;
    });

    /** Retry utility configured from adapter properties. */
    private volatile RetryUtil retryUtil;

    private volatile boolean enabled = true;
    private volatile boolean initialized = false;

    // ── Retry configuration (call from subclass constructor) ───────────

    /**
     * Configure retry. Call from subclass constructor if custom values needed.
     */
    protected void configureRetry(int maxRetries, long initialDelayMs, long maxTimeoutMs) {
        this.retryUtil = new RetryUtil(getAdapterName(), log, maxRetries, initialDelayMs, maxTimeoutMs);
    }

    /**
     * Configure retry from {@link AdapterConfig.AdapterProperties}.
     */
    protected void configureRetry(AdapterConfig.AdapterProperties props) {
        if (props.isRetryEnabled()) {
            this.retryUtil = new RetryUtil(getAdapterName(), log,
                props.getMaxRetries(), 1000L, props.getTimeoutMs());
        }
    }

    /** Access the configured RetryUtil (may be null if retry disabled). */
    protected RetryUtil getRetryUtil() {
        return retryUtil;
    }

    // ── Enabled / disabled ────────────────────────────────────────────

    @Override
    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        log.info("Adapter {} enabled state changed to: {}", getAdapterName(), enabled);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────

    @Override
    public CompletableFuture<Void> initialize() {
        return CompletableFuture.runAsync(() -> {
            try {
                log.info("Initializing adapter: {}", getAdapterName());
                // Configure default retry if not explicitly set
                if (retryUtil == null) {
                    this.retryUtil = new RetryUtil(getAdapterName(), log, 3, 1000, 30000);
                }
                doInitialize();
                initialized = true;
                log.info("Adapter {} initialized successfully", getAdapterName());
            } catch (Exception e) {
                log.error("Failed to initialize adapter {}: {}", getAdapterName(), e.getMessage(), e);
                throw new RuntimeException("Adapter initialization failed: " + getAdapterName(), e);
            }
        }, executor);
    }

    @Override
    public CompletableFuture<Void> shutdown() {
        return CompletableFuture.runAsync(() -> {
            try {
                log.info("Shutting down adapter: {}", getAdapterName());
                doShutdown();
                initialized = false;
                executor.shutdown();
                if (!executor.awaitTermination(10, TimeUnit.SECONDS)) {
                    executor.shutdownNow();
                }
                log.info("Adapter {} shut down successfully", getAdapterName());
            } catch (Exception e) {
                log.error("Error shutting down adapter {}: {}", getAdapterName(), e.getMessage(), e);
            }
        }, executor);
    }

    // ── Business operations with retry + validation ────────────────────

    @Override
    public CompletableFuture<VehicleListResponse> getVehicles(String userId) {
        return checkEnabled()
            .thenCompose(v -> executeWithRetry(() -> doGetVehicles(userId)))
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("getVehicles failed for user {}: {}", userId, e.getMessage());
            });
    }

    @Override
    public CompletableFuture<KeyResponse> requestKeys(KeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> executeWithRetry(() -> doRequestKeys(request)))
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("requestKeys failed: {}", e.getMessage());
            });
    }

    @Override
    public CompletableFuture<KeyResponse> revokeKeys(KeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> executeWithRetry(() -> doRevokeKeys(request)))
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("revokeKeys failed: {}", e.getMessage());
            });
    }

    @Override
    public CompletableFuture<BindKeyResponse> bindKey(BindKeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> {
                // Pre-flight validation
                var errors = ResponseValidator.validate(request);
                if (!errors.isEmpty()) {
                    String msg = "BindKeyRequest validation failed: " + errors;
                    log.error(msg);
                    return CompletableFuture.completedFuture(
                        new BindKeyResponse(false, msg, null, null, null, null, 0, List.of()));
                }
                return executeWithRetry(() -> doBindKey(request));
            })
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("bindKey failed: {}", e.getMessage());
            });
    }

    @Override
    public CompletableFuture<KeyResponse> unbindKey(UnbindKeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> executeWithRetry(() -> doUnbindKey(request)))
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("unbindKey failed: {}", e.getMessage());
            });
    }

    @Override
    public CompletableFuture<KeyStatusResponse> getKeyStatus(String keyId) {
        return checkEnabled()
            .thenCompose(v -> executeWithRetry(() -> doGetKeyStatus(keyId)))
            .thenApply(resp -> {
                ResponseValidator.validate(resp);
                return resp;
            })
            .whenComplete((r, e) -> {
                if (e != null) log.error("getKeyStatus failed for key {}: {}", keyId, e.getMessage());
            });
    }

    @Override
    public boolean healthCheck() {
        if (!enabled) return false;
        return doHealthCheck();
    }

    // ── Internal helpers ──────────────────────────────────────────────

    protected CompletableFuture<Void> checkEnabled() {
        if (!enabled) {
            return CompletableFuture.failedFuture(
                new IllegalStateException("Adapter " + getAdapterName() + " is disabled"));
        }
        if (!initialized) {
            return CompletableFuture.failedFuture(
                new IllegalStateException("Adapter " + getAdapterName() + " is not initialized"));
        }
        return CompletableFuture.completedFuture(null);
    }

    /**
     * Execute a callable with retry if {@link #retryUtil} is configured.
     * Falls back to direct execution on the executor.
     */
    private <T> CompletableFuture<T> executeWithRetry(java.util.concurrent.Callable<T> callable) {
        RetryUtil ru = retryUtil;
        if (ru != null) {
            return ru.executeAsync(callable, executor);
        }
        return CompletableFuture.supplyAsync(() -> {
            try {
                return callable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }, executor);
    }

    // ════════════════════════════════════════════════════════════════════
    //  Abstract methods for protocol-specific implementation
    // ════════════════════════════════════════════════════════════════════

    protected abstract void doInitialize();
    protected abstract void doShutdown();
    protected abstract CompletableFuture<VehicleListResponse> doGetVehicles(String userId);
    protected abstract CompletableFuture<KeyResponse> doRequestKeys(KeyRequest request);
    protected abstract CompletableFuture<KeyResponse> doRevokeKeys(KeyRequest request);

    /** Bind a key — protocol-specific TSP call. */
    protected abstract CompletableFuture<BindKeyResponse> doBindKey(BindKeyRequest request);

    /** Unbind a key — protocol-specific TSP call. */
    protected abstract CompletableFuture<KeyResponse> doUnbindKey(UnbindKeyRequest request);

    /** Query key status from the TSP. */
    protected abstract CompletableFuture<KeyStatusResponse> doGetKeyStatus(String keyId);

    protected abstract boolean doHealthCheck();
}
