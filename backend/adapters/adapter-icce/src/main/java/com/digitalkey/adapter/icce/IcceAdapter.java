package com.digitalkey.adapter.icce;

import com.digitalkey.adapter.core.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * ICCE (Car Connectivity Experience) TSP Adapter.
 * Implements communication with ICCE digital key services.
 *
 * <p>Protocol version: ICCE Digital Key 2.0
 * <br>API base: {@code /api/v1/}
 *
 * <h3>OEM prerequisites</h3>
 * <ul>
 *   <li>ICCE TSP endpoint URL configured via {@code adapter.icce.api-url}</li>
 *   <li>API Key and Tenant ID for authentication</li>
 *   <li>Network access to the TSP endpoint (HTTPS only)</li>
 * </ul>
 */
@Component
@ConditionalOnProperty(name = "adapter.icce.enabled", havingValue = "true", matchIfMissing = true)
public class IcceAdapter extends AbstractTspAdapter {

    private final AdapterConfig.IcceProperties config;
    private final IcceClient client;

    @Autowired
    public IcceAdapter(AdapterConfig.IcceProperties config) {
        this.config = config;
        this.client = new IcceClient(config);
    }

    IcceAdapter(AdapterConfig.IcceProperties config, IcceClient client) {
        this.config = config;
        this.client = client;
    }

    @Override
    public String getAdapterName() {
        return "icce-adapter";
    }

    @Override
    protected void doInitialize() {
        log.info("Initializing ICCE adapter with API URL: {}", config.getApiUrl());
        client.init();
        log.info("ICCE adapter initialized");
    }

    @Override
    protected void doShutdown() {
        log.info("Shutting down ICCE adapter");
        client.close();
    }

    @Override
    protected CompletableFuture<VehicleListResponse> doGetVehicles(String userId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting ICCE vehicles for user: {}", userId);
            return client.getVehicles(userId);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRequestKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Requesting ICCE keys for vehicle: {}", request.vehicleId());
            return client.requestKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRevokeKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Revoking ICCE keys for vehicle: {}", request.vehicleId());
            return client.revokeKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<BindKeyResponse> doBindKey(BindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Binding ICCE key for vehicle: {} device: {}", request.vehicleId(), request.deviceId());
            return client.bindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doUnbindKey(UnbindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Unbinding ICCE key: {} reason: {}", request.keyId(), request.reason());
            return client.unbindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyStatusResponse> doGetKeyStatus(String keyId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting ICCE key status: {}", keyId);
            return client.getKeyStatus(keyId);
        }, executor);
    }

    @Override
    protected boolean doHealthCheck() {
        return client.isConnected();
    }
}
