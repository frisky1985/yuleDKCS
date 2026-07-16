package com.digitalkey.adapter.iccoa;

import com.digitalkey.adapter.core.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * ICCOA (Intelligent Car Connectivity Over Air) TSP Adapter.
 * Implements communication with ICCOA cloud services for digital key management.
 *
 * <p>Protocol version: ICCOA Digital Key 3.0
 * <br>API base: {@code /v1/}
 *
 * <h3>OEM prerequisites</h3>
 * <ul>
 *   <li>ICCOA TSP endpoint URL configured via {@code adapter.iccoa.api-url}</li>
 *   <li>App ID and App Secret for authentication</li>
 *   <li>Region setting (cn/us) for regional routing</li>
 *   <li>Network access to the TSP endpoint (HTTPS only)</li>
 * </ul>
 */
@Component
@ConditionalOnProperty(name = "adapter.iccoa.enabled", havingValue = "true", matchIfMissing = true)
public class IccoaAdapter extends AbstractTspAdapter {

    private final AdapterConfig.IccoaProperties config;
    private final IccoaClient client;

    @Autowired
    public IccoaAdapter(AdapterConfig.IccoaProperties config) {
        this.config = config;
        this.client = new IccoaClient(config);
    }

    IccoaAdapter(AdapterConfig.IccoaProperties config, IccoaClient client) {
        this.config = config;
        this.client = client;
    }

    @Override
    public String getAdapterName() {
        return "iccoa-adapter";
    }

    @Override
    protected void doInitialize() {
        log.info("Initializing ICCOA adapter with API URL: {} region: {}",
            config.getApiUrl(), config.getRegion());
        client.init();
        log.info("ICCOA adapter initialized");
    }

    @Override
    protected void doShutdown() {
        log.info("Shutting down ICCOA adapter");
        client.close();
    }

    @Override
    protected CompletableFuture<VehicleListResponse> doGetVehicles(String userId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting ICCOA vehicles for user: {}", userId);
            return client.getVehicles(userId);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRequestKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Requesting ICCOA keys for vehicle: {}", request.vehicleId());
            return client.requestKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doRevokeKeys(KeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Revoking ICCOA keys for vehicle: {}", request.vehicleId());
            return client.revokeKeys(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<BindKeyResponse> doBindKey(BindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Binding ICCOA key for vehicle: {} device: {}",
                request.vehicleId(), request.deviceId());
            return client.bindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyResponse> doUnbindKey(UnbindKeyRequest request) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Unbinding ICCOA key: {} reason: {}", request.keyId(), request.reason());
            return client.unbindKey(request);
        }, executor);
    }

    @Override
    protected CompletableFuture<KeyStatusResponse> doGetKeyStatus(String keyId) {
        return CompletableFuture.supplyAsync(() -> {
            log.debug("Getting ICCOA key status: {}", keyId);
            return client.getKeyStatus(keyId);
        }, executor);
    }

    @Override
    protected boolean doHealthCheck() {
        return client.isConnected();
    }
}
