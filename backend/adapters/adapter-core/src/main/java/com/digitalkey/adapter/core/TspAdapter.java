package com.digitalkey.adapter.core;

import java.util.concurrent.CompletableFuture;
import java.util.Map;
import java.util.List;

/**
 * TSP (Trusted Service Provider) Adapter interface.
 * All protocol-specific adapters (CCC, ICCOA, ICCE) must implement this interface.
 *
 * <h3>Lifecycle</h3>
 * <ol>
 *   <li>{@link #initialize()} — establish connection / auth</li>
 *   <li>{@link #getVehicles(String)}, {@link #requestKeys(KeyRequest)},
 *       {@link #bindKey(BindKeyRequest)} — business operations</li>
 *   <li>{@link #shutdown()} — cleanup</li>
 * </ol>
 *
 * <h3>Retry guarantee</h3>
 * All async operations in {@link AbstractTspAdapter} apply exponential-backoff
 * retry when {@code adapter.retry-enabled=true}.
 */
public interface TspAdapter {

    // ──────────────────────────────────────────────
    //  Lifecycle
    // ──────────────────────────────────────────────

    /** Unique adapter name, e.g. "ccc-adapter". */
    String getAdapterName();

    /** Whether this adapter is currently enabled. */
    boolean isEnabled();

    /** Initialize connections and authenticate with the TSP. */
    CompletableFuture<Void> initialize();

    /** Gracefully close connections and release resources. */
    CompletableFuture<Void> shutdown();

    // ──────────────────────────────────────────────
    //  Business Operations
    // ──────────────────────────────────────────────

    /** Retrieve the user's vehicle list from the TSP. */
    CompletableFuture<VehicleListResponse> getVehicles(String userId);

    /** Request one or more digital keys from the TSP. */
    CompletableFuture<KeyResponse> requestKeys(KeyRequest request);

    /** Revoke existing digital keys. */
    CompletableFuture<KeyResponse> revokeKeys(KeyRequest request);

    /**
     * Bind a digital key to a user's device.
     * This maps to the Go adapter's BindKey method and involves
     * device attestation, certificate exchange, and shared-secret
     * establishment with the TSP.
     */
    CompletableFuture<BindKeyResponse> bindKey(BindKeyRequest request);

    /**
     * Unbind (permanently remove) a key binding.
     * The TSP must invalidate the key on its side.
     */
    CompletableFuture<KeyResponse> unbindKey(UnbindKeyRequest request);

    /**
     * Query the current status of a key from the TSP.
     * @param keyId  TSP-assigned key identifier
     * @return status response containing state, timestamps, etc.
     */
    CompletableFuture<KeyStatusResponse> getKeyStatus(String keyId);

    // ──────────────────────────────────────────────
    //  Diagnostics
    // ──────────────────────────────────────────────

    /** Health check — returns true if the adapter can service requests. */
    boolean healthCheck();

    /** Protocol-specific metadata for observability. */
    default Map<String, String> getMetadata() {
        return Map.of(
            "adapter", getAdapterName(),
            "enabled", String.valueOf(isEnabled()),
            "protocol", getClass().getSimpleName()
        );
    }

    // ═══════════════════════════════════════════════
    //  DTO Records
    // ═══════════════════════════════════════════════

    // ─── Vehicle ──────────────────────────────────

    record VehicleListResponse(
        boolean success,
        String message,
        List<VehicleInfo> vehicles
    ) {}

    record VehicleInfo(
        String vehicleId,
        String vin,
        String make,
        String model,
        int modelYear
    ) {}

    // ─── Key Management ───────────────────────────

    record KeyRequest(
        String userId,
        String vehicleId,
        String vin,
        List<String> keyTypes,
        Map<String, String> options
    ) {}

    record KeyResponse(
        boolean success,
        String message,
        String keyId,
        List<String> keyData
    ) {}

    // ─── Bind / Unbind ────────────────────────────

    /**
     * BindKey request matching the Go adapter interface.
     * In production, the TSP validates the device certificate
     * and performs ECDH to derive a shared secret.
     */
    record BindKeyRequest(
        String userId,
        String vehicleId,
        String vin,
        String deviceId,
        String devicePublicKey,    // base64-encoded device public key
        String attestationToken,  // device attestation
        Map<String, String> options
    ) {}

    /**
     * BindKey response containing the shared secret and
     * TSP-side parameters for completing the binding.
     */
    record BindKeyResponse(
        boolean success,
        String message,
        String keyId,
        String sharedSecret,        // base64-encoded ECDH shared secret
        String tspPublicKey,        // base64-encoded TSP public key
        String sessionId,
        int keySlot,
        List<String> keyData
    ) {}

    /**
     * UnbindKey request — identifies which binding to remove.
     */
    record UnbindKeyRequest(
        String userId,
        String keyId,
        String vehicleId,
        String reason
    ) {}

    // ─── Key Status ───────────────────────────────

    record KeyStatusResponse(
        boolean success,
        String message,
        String keyId,
        String status,              // ACTIVE / SUSPENDED / REVOKED / EXPIRED
        long createdAtEpochMs,
        long expiresAtEpochMs,
        String boundDeviceId,
        Map<String, String> metadata
    ) {}
}
