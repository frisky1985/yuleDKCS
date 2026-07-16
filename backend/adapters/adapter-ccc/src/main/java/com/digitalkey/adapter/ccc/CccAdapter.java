package com.digitalkey.adapter.ccc;

import com.digitalkey.adapter.core.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * CCC Alliance TSP Adapter implementation.
 * Connects to CCC (Car Connectivity Consortium) digital key TSP.
 *
 * <p>Protocol version: CCC Digital Key 3.0
 * <br>API base: {@code /api/v1/}
 *
 * <h3>OEM prerequisites</h3>
 * <ul>
 *   <li>CCC TSP endpoint URL configured via {@code adapter.ccc.api-url}</li>
 *   <li>Client ID / Client Secret for OAuth2 client credentials flow</li>
 *   <li>Network access to the TSP endpoint (HTTPS only)</li>
 * </ul>
 */
@Component
@ConditionalOnProperty(name = "adapter.ccc.enabled", havingValue = "true", matchIfMissing = true)
public class CccAdapter extends AbstractTspAdapter {

    private final AdapterConfig.CccProperties config;
    private final CccClient client;

    @Autowired
    public CccAdapter(AdapterConfig.CccProperties config) {
        this.config = config;
        this.client = new CccClient(config);
    }

    CccAdapter(AdapterConfig.CccProperties config, CccClient client) {
        this.config = config;
        this.client = client;
    }

    @Override
    public String getAdapterName() {
        return "ccc-adapter";
    }

    @Override
    protected void doInitialize() {
        log.info("Initializing CCC adapter with API URL: {}", config.getApiUrl());
        client.init();
        log.info("CCC adapter initialized");
    }

    @Override
    protected void doShutdown() {
        log.info("Shutting down CCC adapter");
        client.close();
    }

    @Override
    protected CompletableFuture<VehicleListResponse> doGetVehicles(String userId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting vehicles for user: {}", userId);
            return client.getVehicles(userId);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRequestKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Requesting keys for vehicle: {}", request.vehicleId());
            return client.requestKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRevokeKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Revoking keys for vehicle: {}", request.vehicleId());
            return client.revokeKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<BindKeyResponse> doBindKey(BindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Binding key for vehicle: {} device: {}", request.vehicleId(), request.deviceId());
            return client.bindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doUnbindKey(UnbindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Unbinding key: {} reason: {}", request.keyId(), request.reason());
            return client.unbindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyStatusResponse> doGetKeyStatus(String keyId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting key status: {}", keyId);
            return client.getKeyStatus(keyId);
        }, executor);
    }

    @Override
    protected boolean doHealthCheck() {
        return client.isConnected();
    }

    public CccClient getClient() {
        return client;
    }

    public AdapterConfig.CccProperties getConfig() {
        return config;
    }
}
