package com.digitalkey.adapter.core;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * Lightweight response schema validator for TSP adapter responses.
 *
 * <p>Ensures that response objects returned by TSP API clients
 * conform to expected field formats. Catches structural issues
 * early (null required fields, malformed IDs) before data
 * propagates to the gRPC layer or downstream consumers.</p>
 *
 * <p>This is <b>not</b> a full JSON Schema validator — it's tailored
 * to the known DTO records in {@link TspAdapter}.
 */
public final class ResponseValidator {

    private static final Logger log = LoggerFactory.getLogger(ResponseValidator.class);

    private static final Pattern VIN_PATTERN = Pattern.compile("[A-HJ-NPR-Z0-9]{17}");
    private static final Pattern KEY_ID_PATTERN = Pattern.compile("[a-zA-Z0-9_-]{8,128}");
    private static final Pattern VEHICLE_ID_PATTERN = Pattern.compile("[a-zA-Z0-9_-]{1,64}");

    private ResponseValidator() {}

    // ── Vehicle list ───────────────────────────────────────────────────

    /** Validate a VehicleListResponse. Returns warnings as a list of strings. */
    public static List<String> validate(TspAdapter.VehicleListResponse response) {
        List<String> warnings = new java.util.ArrayList<>();
        if (response == null) {
            warnings.add("VehicleListResponse is null");
            return warnings;
        }
        if (response.vehicles() == null) {
            warnings.add("vehicles list is null");
            return warnings;
        }
        for (int i = 0; i < response.vehicles().size(); i++) {
            TspAdapter.VehicleInfo v = response.vehicles().get(i);
            if (v == null) {
                warnings.add("vehicles[" + i + "] is null");
                continue;
            }
            if (v.vehicleId() == null || v.vehicleId().isBlank()) {
                warnings.add("vehicles[" + i + "].vehicleId is blank");
            } else if (!VEHICLE_ID_PATTERN.matcher(v.vehicleId()).matches()) {
                warnings.add("vehicles[" + i + "].vehicleId='" + v.vehicleId() + "' looks unusual");
            }
            if (v.vin() != null && !v.vin().isBlank() && !VIN_PATTERN.matcher(v.vin()).matches()) {
                warnings.add("vehicles[" + i + "].vin='" + v.vin() + "' does not match VIN pattern");
            }
        }
        if (!warnings.isEmpty()) {
            log.warn("VehicleListResponse validation: {} warning(s): {}", warnings.size(), warnings);
        }
        return warnings;
    }

    // ── Key response ──────────────────────────────────────────────────

    /** Validate a KeyResponse. */
    public static List<String> validate(TspAdapter.KeyResponse response) {
        List<String> warnings = new java.util.ArrayList<>();
        if (response == null) {
            warnings.add("KeyResponse is null");
            return warnings;
        }
        if (response.success() && (response.keyId() == null || response.keyId().isBlank())) {
            warnings.add("KeyResponse success=true but keyId is blank");
        }
        if (response.keyId() != null && !response.keyId().isBlank()
            && !KEY_ID_PATTERN.matcher(response.keyId()).matches()) {
            warnings.add("KeyResponse.keyId='" + response.keyId() + "' looks unusual");
        }
        if (!warnings.isEmpty()) {
            log.warn("KeyResponse validation: {} warning(s): {}", warnings.size(), warnings);
        }
        return warnings;
    }

    // ── BindKey response ──────────────────────────────────────────────

    /** Validate a BindKeyResponse (critical: sharedSecret must be present). */
    public static List<String> validate(TspAdapter.BindKeyResponse response) {
        List<String> warnings = new java.util.ArrayList<>();
        if (response == null) {
            warnings.add("BindKeyResponse is null");
            return warnings;
        }
        if (!response.success()) {
            // Non-success responses are expected — skip structural checks
            return warnings;
        }
        if (response.sharedSecret() == null || response.sharedSecret().isBlank()) {
            warnings.add("CRITICAL: BindKeyResponse success=true but sharedSecret is empty!");
        }
        if (response.keyId() == null || response.keyId().isBlank()) {
            warnings.add("BindKeyResponse success=true but keyId is blank");
        }
        if (response.tspPublicKey() == null || response.tspPublicKey().isBlank()) {
            warnings.add("BindKeyResponse success=true but tspPublicKey is blank");
        }
        if (!warnings.isEmpty()) {
            log.warn("BindKeyResponse validation: {} warning(s): {}", warnings.size(), warnings);
        }
        return warnings;
    }

    // ── KeyStatus response ────────────────────────────────────────────

    /** Validate a KeyStatusResponse. */
    public static List<String> validate(TspAdapter.KeyStatusResponse response) {
        List<String> warnings = new java.util.ArrayList<>();
        if (response == null) {
            warnings.add("KeyStatusResponse is null");
            return warnings;
        }
        if (response.success()) {
            if (response.keyId() == null || response.keyId().isBlank()) {
                warnings.add("KeyStatusResponse success=true but keyId is blank");
            }
            if (response.status() == null || response.status().isBlank()) {
                warnings.add("KeyStatusResponse success=true but status is blank");
            } else {
                List<String> valid = List.of("ACTIVE", "SUSPENDED", "REVOKED", "EXPIRED");
                if (!valid.contains(response.status().toUpperCase())) {
                    warnings.add("KeyStatusResponse.status='" + response.status()
                        + "' not in " + valid);
                }
            }
        }
        if (!warnings.isEmpty()) {
            log.warn("KeyStatusResponse validation: {} warning(s): {}", warnings.size(), warnings);
        }
        return warnings;
    }

    // ── Request validation (pre-flight) ───────────────────────────────

    /** Validate a BindKeyRequest before sending. */
    public static List<String> validate(TspAdapter.BindKeyRequest request) {
        List<String> errors = new java.util.ArrayList<>();
        if (request == null) {
            errors.add("BindKeyRequest is null");
            return errors;
        }
        if (request.userId() == null || request.userId().isBlank()) {
            errors.add("BindKeyRequest.userId is required");
        }
        if (request.vehicleId() == null || request.vehicleId().isBlank()) {
            errors.add("BindKeyRequest.vehicleId is required");
        }
        if (request.deviceId() == null || request.deviceId().isBlank()) {
            errors.add("BindKeyRequest.deviceId is required");
        }
        if (request.devicePublicKey() == null || request.devicePublicKey().isBlank()) {
            errors.add("BindKeyRequest.devicePublicKey is required");
        }
        return errors;
    }
}
