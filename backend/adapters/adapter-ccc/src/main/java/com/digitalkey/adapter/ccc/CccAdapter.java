package com.digitalkey.adapter.ccc;

import com.digitalkey.adapter.core.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import java.util.concurrent.CompletableFuture;

/**
 * CCC Alliance TSP Adapter implementation.
 * Connects to CCC (Car Connectivity Consortium) digital key TSP.
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
    protected boolean doHealthCheck() {
        return client.isConnected();
    }
}
