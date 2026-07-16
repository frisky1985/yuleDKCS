package com.digitalkey.adapter.core;

import org.junit.jupiter.api.Test;

import java.util.List;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for {@link ResponseValidator} schema validation.
 */
class ResponseValidatorTest {

    // ── VehicleListResponse ───────────────────────────────────────────

    @Test
    void shouldAcceptValidVehicleListResponse() {
        var resp = new TspAdapter.VehicleListResponse(true, "ok", List.of(
            new TspAdapter.VehicleInfo("v1", "WBA3A5C5XDF123456", "BMW", "3 Series", 2023)
        ));
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.isEmpty(), "Expected no warnings: " + warnings);
    }

    @Test
    void shouldWarnOnNullVehicles() {
        var resp = new TspAdapter.VehicleListResponse(true, "ok", null);
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("vehicles list is null")));
    }

    @Test
    void shouldWarnOnBlankVehicleId() {
        var resp = new TspAdapter.VehicleListResponse(true, "ok", List.of(
            new TspAdapter.VehicleInfo("", "WBA3A5C5XDF123456", "BMW", "3 Series", 2023)
        ));
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("vehicleId is blank")));
    }

    @Test
    void shouldWarnOnInvalidVin() {
        var resp = new TspAdapter.VehicleListResponse(true, "ok", List.of(
            new TspAdapter.VehicleInfo("v1", "invalid-vin", "BMW", "3 Series", 2023)
        ));
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("does not match VIN pattern")));
    }

    // ── KeyResponse ───────────────────────────────────────────────────

    @Test
    void shouldAcceptValidKeyResponse() {
        var resp = new TspAdapter.KeyResponse(true, "ok", "key-123", List.of("data1"));
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.isEmpty(), "Expected no warnings: " + warnings);
    }

    @Test
    void shouldWarnOnSuccessWithBlankKeyId() {
        var resp = new TspAdapter.KeyResponse(true, "ok", "", List.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("keyId is blank")));
    }

    // ── BindKeyResponse (critical: sharedSecret) ────────────────────

    @Test
    void shouldAcceptValidBindKeyResponse() {
        var resp = new TspAdapter.BindKeyResponse(true, "ok", "key-123",
            "base64sharedsecret==", "base64tspkey==", "session-1", 1, List.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.isEmpty(), "Expected no warnings: " + warnings);
    }

    @Test
    void shouldFlagEmptySharedSecretOnSuccess() {
        var resp = new TspAdapter.BindKeyResponse(true, "ok", "key-123",
            "", null, null, 0, List.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("sharedSecret is empty")));
    }

    @Test
    void shouldNotCheckSharedSecretOnFailure() {
        var resp = new TspAdapter.BindKeyResponse(false, "error", null,
            null, null, null, 0, List.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.isEmpty());
    }

    @Test
    void shouldFlagEmptyTspPublicKeyOnSuccess() {
        var resp = new TspAdapter.BindKeyResponse(true, "ok", "key-123",
            "secret", "", "sess", 1, List.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("tspPublicKey is blank")));
    }

    // ── KeyStatusResponse ─────────────────────────────────────────────

    @Test
    void shouldAcceptValidKeyStatusResponse() {
        var resp = new TspAdapter.KeyStatusResponse(true, "ok", "key-123",
            "ACTIVE", 1000L, 2000L, "dev-1", Map.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.isEmpty(), "Expected no warnings: " + warnings);
    }

    @Test
    void shouldWarnOnUnknownStatus() {
        var resp = new TspAdapter.KeyStatusResponse(true, "ok", "key-123",
            "PENDING", 0, 0, null, Map.of());
        var warnings = ResponseValidator.validate(resp);
        assertTrue(warnings.stream().anyMatch(w -> w.contains("not in")));
    }

    // ── BindKeyRequest pre-flight ─────────────────────────────────────

    @Test
    void shouldValidateCompleteBindKeyRequest() {
        var req = new TspAdapter.BindKeyRequest("user1", "v1", "WBA3A5C5XDF123456",
            "dev-1", "pubkey==", "attest-token", Map.of());
        var errors = ResponseValidator.validate(req);
        assertTrue(errors.isEmpty(), "Expected no errors: " + errors);
    }

    @Test
    void shouldRejectBindKeyRequestWithMissingFields() {
        var req = new TspAdapter.BindKeyRequest("", "v1", "", "", "", null, null);
        var errors = ResponseValidator.validate(req);
        assertTrue(errors.size() >= 3); // userId, deviceId, devicePublicKey
    }
}
