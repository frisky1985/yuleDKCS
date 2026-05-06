package com.digitalkey.adapter.core;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Abstract base class for TSP adapters providing common functionality.
 * Protocol-specific adapters extend this class.
 */
public abstract class AbstractTspAdapter implements TspAdapter {

    protected final Logger log = LoggerFactory.getLogger(getClass());
    protected final ExecutorService executor = Executors.newCachedThreadPool(r -> {
        Thread t = new Thread(r, getAdapterName() + "-executor");
        t.setDaemon(true);
        return t;
    });

    private volatile boolean enabled = true;
    private volatile boolean initialized = false;

    @Override
    public boolean isEnabled() {
        return enabled;
    }

    /**
     * Set the enabled state.
     */
    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        log.info("Adapter {} enabled state changed to: {}", getAdapterName(), enabled);
    }

    @Override
    public CompletableFuture<Void> initialize() {
        return CompletableFuture.runAsync(() -> {
            try {
                log.info("Initializing adapter: {}", getAdapterName());
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

    @Override
    public CompletableFuture<VehicleListResponse> getVehicles(String userId) {
        return checkEnabled()
            .thenCompose(v -> doGetVehicles(userId))
            .whenComplete((r, e) -> {
                if (e != null) {
                    log.error("getVehicles failed for user {}: {}", userId, e.getMessage());
                }
            });
    }

    @Override
    public CompletableFuture<KeyResponse> requestKeys(KeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> doRequestKeys(request))
            .whenComplete((r, e) -> {
                if (e != null) {
                    log.error("requestKeys failed: {}", e.getMessage());
                }
            });
    }

    @Override
    public CompletableFuture<KeyResponse> revokeKeys(KeyRequest request) {
        return checkEnabled()
            .thenCompose(v -> doRevokeKeys(request))
            .whenComplete((r, e) -> {
                if (e != null) {
                    log.error("revokeKeys failed: {}", e.getMessage());
                }
            });
    }

    @Override
    public boolean healthCheck() {
        if (!enabled) {
            return false;
        }
        return doHealthCheck();
    }

    /**
     * Check if adapter is enabled, throw exception if not.
     */
    protected CompletableFuture<Void> checkEnabled() {
        if (!enabled) {
            return CompletableFuture.failedFuture(
                new IllegalStateException("Adapter " + getAdapterName() + " is disabled")
            );
        }
        if (!initialized) {
            return CompletableFuture.failedFuture(
                new IllegalStateException("Adapter " + getAdapterName() + " is not initialized")
            );
        }
        return CompletableFuture.completedFuture(null);
    }

    // Abstract methods for protocol-specific implementation

    /**
     * Protocol-specific initialization logic.
     */
    protected abstract void doInitialize();

    /**
     * Protocol-specific shutdown logic.
     */
    protected abstract void doShutdown();

    /**
     * Protocol-specific vehicle list retrieval.
     */
    protected abstract CompletableFuture<VehicleListResponse> doGetVehicles(String userId);

    /**
     * Protocol-specific key request.
     */
    protected abstract CompletableFuture<KeyResponse> doRequestKeys(KeyRequest request);

    /**
     * Protocol-specific key revocation.
     */
    protected abstract CompletableFuture<KeyResponse> doRevokeKeys(KeyRequest request);

    /**
     * Protocol-specific health check.
     */
    protected abstract boolean doHealthCheck();
}
