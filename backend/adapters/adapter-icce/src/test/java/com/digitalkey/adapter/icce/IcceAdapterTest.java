package com.digitalkey.adapter.icce;

import com.digitalkey.adapter.core.*;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

/**
 * Unit tests for {@link IcceAdapter}.
 * Uses Mockito to mock the {@link IcceClient} and verify adapter behavior.
 */
@ExtendWith(MockitoExtension.class)
class IcceAdapterTest {

    @Mock
    private IcceClient mockClient;

    private AdapterConfig.IcceProperties config;
    private IcceAdapter adapter;

    @BeforeEach
    void setUp() {
        config = new AdapterConfig.IcceProperties();
        config.setEnabled(true);
        config.setApiUrl("https://icce-api.example.com");
        config.setApiKey("test-api-key");
        config.setTenantId("test-tenant");

        adapter = new IcceAdapter(config, mockClient);
        adapter.setEnabled(true);
        adapter.initialize().join();
    }

    @Test
    void shouldReturnAdapterName() {
        assertEquals("icce-adapter", adapter.getAdapterName());
    }

    @Test
    void shouldDelegateGetVehiclesToClient() throws Exception {
        var expected = new TspAdapter.VehicleListResponse(
            true, "ok", List.of(
                new TspAdapter.VehicleInfo("v1", "WBA3A5C5XDF123456", "Audi", "A4", 2024)
            ));
        when(mockClient.getVehicles("user-1")).thenReturn(expected);

        TspAdapter.VehicleListResponse result = adapter.getVehicles("user-1").get();

        assertTrue(result.success());
        assertEquals(1, result.vehicles().size());
        verify(mockClient).getVehicles("user-1");
    }

    @Test
    void shouldDelegateRequestKeysToClient() throws Exception {
        var request = new TspAdapter.KeyRequest(
            "user-1", "v1", "VIN", List.of("owner"), Map.of());
        var expected = new TspAdapter.KeyResponse(true, "ok", "icce-key-123", List.of("keydata"));
        when(mockClient.requestKeys(request)).thenReturn(expected);

        TspAdapter.KeyResponse result = adapter.requestKeys(request).get();

        assertTrue(result.success());
        assertEquals("icce-key-123", result.keyId());
        verify(mockClient).requestKeys(request);
    }

    @Test
    void shouldDelegateRevokeKeysToClient() throws Exception {
        var request = new TspAdapter.KeyRequest(
            "user-1", "v1", "", List.of("key-123"), Map.of());
        var expected = new TspAdapter.KeyResponse(true, "ok", null, List.of());
        when(mockClient.revokeKeys(request)).thenReturn(expected);

        TspAdapter.KeyResponse result = adapter.revokeKeys(request).get();

        assertTrue(result.success());
        verify(mockClient).revokeKeys(request);
    }

    @Test
    void shouldDelegateBindKeyToClient() throws Exception {
        var request = new TspAdapter.BindKeyRequest(
            "user-1", "v1", "VIN", "dev-1", "pubkey==", "attest", Map.of());
        var expected = new TspAdapter.BindKeyResponse(true, "ok", "icce-key-123",
            "sharedSecret==", "tspPubKey==", "session-1", 1, List.of("keydata"));
        when(mockClient.bindKey(request)).thenReturn(expected);

        TspAdapter.BindKeyResponse result = adapter.bindKey(request).get();

        assertTrue(result.success());
        assertEquals("sharedSecret==", result.sharedSecret());
        assertEquals("tspPubKey==", result.tspPublicKey());
        verify(mockClient).bindKey(request);
    }

    @Test
    void shouldDelegateUnbindKeyToClient() throws Exception {
        var request = new TspAdapter.UnbindKeyRequest("user-1", "key-123", "v1", "lost device");
        var expected = new TspAdapter.KeyResponse(true, "ok", "key-123", List.of());
        when(mockClient.unbindKey(request)).thenReturn(expected);

        TspAdapter.KeyResponse result = adapter.unbindKey(request).get();

        assertTrue(result.success());
        verify(mockClient).unbindKey(request);
    }

    @Test
    void shouldDelegateGetKeyStatusToClient() throws Exception {
        var expected = new TspAdapter.KeyStatusResponse(true, "ok", "key-123",
            "ACTIVE", 1000L, 2000L, "dev-1", Map.of());
        when(mockClient.getKeyStatus("key-123")).thenReturn(expected);

        TspAdapter.KeyStatusResponse result = adapter.getKeyStatus("key-123").get();

        assertTrue(result.success());
        assertEquals("ACTIVE", result.status());
        verify(mockClient).getKeyStatus("key-123");
    }

    @Test
    void shouldReportHealth() {
        when(mockClient.isConnected()).thenReturn(true);
        assertTrue(adapter.healthCheck());

        when(mockClient.isConnected()).thenReturn(false);
        assertFalse(adapter.healthCheck());
    }

    @Test
    void shouldReturnMetadata() {
        Map<String, String> meta = adapter.getMetadata();
        assertEquals("icce-adapter", meta.get("adapter"));
        assertEquals("true", meta.get("enabled"));
    }

    @Test
    void shouldFailWhenDisabled() {
        adapter.setEnabled(false);
        assertThrows(ExecutionException.class,
            () -> adapter.getVehicles("user-1").get());
    }
}
