package com.digitalkey.adapter.iccoa;

import com.digitalkey.adapter.core.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * ICCOA (Intelligent Car Connectivity Over Air) TSP Adapter.
 * Implements communication with ICCOA cloud services for digital key management.
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
            log.debug("Getting vehicles for ICCOA user: {}", userId);
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
    protected boolean doHealthCheck() {
        return client.isConnected();
    }
}
