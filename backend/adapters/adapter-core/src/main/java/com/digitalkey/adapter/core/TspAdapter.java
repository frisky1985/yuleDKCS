package com.digitalkey.adapter.core;

import java.util.concurrent.CompletableFuture;

/**
 * TSP (Trusted Service Provider) Adapter interface.
 * All protocol-specific adapters (CCC, ICCOA, ICCE) must implement this interface.
 */
public interface TspAdapter {

    /**
     * Get the adapter identifier.
     * @return unique adapter name
     */
    String getAdapterName();

    /**
     * Check if this adapter is currently enabled.
     * @return true if enabled
     */
    boolean isEnabled();

    /**
     * Initialize the adapter and establish connections.
     * @return CompletableFuture that completes when initialization is done
     */
    CompletableFuture<Void> initialize();

    /**
     * Close connections and cleanup resources.
     * @return CompletableFuture that completes when shutdown is done
     */
    CompletableFuture<Void> shutdown();

    /**
     * Get the vehicle list from the TSP.
     * @param userId user identifier
     * @return CompletableFuture with vehicle list response
     */
    CompletableFuture<VehicleListResponse> getVehicles(String userId);

    /**
     * Request vehicle keys.
     * @param request key request details
     * @return CompletableFuture with key response
     */
    CompletableFuture<KeyResponse> requestKeys(KeyRequest request);

    /**
     * Revoke vehicle keys.
     * @param request revocation details
     * @return CompletableFuture with revocation response
     */
    CompletableFuture<KeyResponse> revokeKeys(KeyRequest request);

    /**
     * Health check for this adapter.
     * @return true if adapter is healthy
     */
    boolean healthCheck();

    /**
     * Get protocol-specific metadata.
     * @return adapter metadata map
     */
    default java.util.Map<String, String> getMetadata() {
        return java.util.Map.of(
            "adapter", getAdapterName(),
            "enabled", String.valueOf(isEnabled()),
            "protocol", getClass().getSimpleName()
        );
    }

    // DTO classes

    record VehicleListResponse(
        boolean success,
        String message,
        java.util.List<VehicleInfo> vehicles
    ) {}

    record VehicleInfo(
        String vehicleId,
        String vin,
        String make,
        String model,
        int modelYear
    ) {}

    record KeyRequest(
        String userId,
        String vehicleId,
        String vin,
        java.util.List<String> keyTypes,
        java.util.Map<String, String> options
    ) {}

    record KeyResponse(
        boolean success,
        String message,
        String keyId,
        java.util.List<String> keyData
    ) {}
}
