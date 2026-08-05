package com.digitalkey.adapter.icce;

import com.digitalkey.adapter.core.*;
import com.digitalkey.adapter.core.RetryUtil;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.reactive.function.client.WebClient;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * ICCE API client for communicating with ICCE TSP endpoints.
 *
 * <p>API endpoints:
 * <table>
 *   <tr><th>Operation</th><th>HTTP</th><th>Path</th></tr>
 *   <tr><td>getVehicles</td><td>GET</td><td>/api/v1/vehicles?userId={userId}</td></tr>
 *   <tr><td>requestKeys</td><td>POST</td><td>/api/v1/keys</td></tr>
 *   <tr><td>revokeKeys</td><td>POST</td><td>/api/v1/keys/revoke</td></tr>
 *   <tr><td>bindKey</td><td>POST</td><td>/api/v1/keys/bind</td></tr>
 *   <tr><td>unbindKey</td><td>POST</td><td>/api/v1/keys/unbind</td></tr>
 *   <tr><td>getKeyStatus</td><td>GET</td><td>/api/v1/keys/{keyId}</td></tr>
 * </table>
 *
 * <p>Authentication: API key via {@code X-Api-Key} header.
 */
public class IcceClient {

    private static final Logger log = LoggerFactory.getLogger(IcceClient.class);

    private final AdapterConfig.IcceProperties config;
    private final ObjectMapper objectMapper;
    private final WebClient webClient;
    private final AtomicBoolean connected = new AtomicBoolean(false);
    private final RetryUtil retryUtil;

    public IcceClient(AdapterConfig.IcceProperties config) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.webClient = WebClient.builder()
            .baseUrl(config.getApiUrl())
            .build();
        this.retryUtil = new RetryUtil("icce-client", log, 3, 500, 30000);
    }

    // Visible for testing
    IcceClient(AdapterConfig.IcceProperties config, WebClient webClient, RetryUtil retryUtil) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.webClient = webClient;
        this.retryUtil = retryUtil;
    }

    public void init() {
        log.info("ICCE client connecting to: {}", config.getApiUrl());
        try {
            // In production: authenticate with API key and establish session
            connected.set(true);
            log.info("ICCE client connected successfully");
        } catch (Exception e) {
            log.error("ICCE client connection failed: {}", e.getMessage());
            connected.set(false);
            throw new RuntimeException("ICCE connection failed", e);
        }
    }

    public boolean isConnected() {
        return connected.get();
    }

    public void close() {
        connected.set(false);
        log.info("ICCE client closed");
    }

    // ── Vehicle operations ─────────────────────────────────────────────

    public TspAdapter.VehicleListResponse getVehicles(String userId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                webClient.get()
                    .uri("/api/v1/vehicles?userId={userId}", userId)
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseVehicleResponse(response);
        } catch (Exception e) {
            log.error("Failed to get ICCE vehicles: {}", e.getMessage());
            return new TspAdapter.VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    public TspAdapter.KeyResponse requestKeys(TspAdapter.KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                webClient.post()
                    .uri("/api/v1/keys")
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to request ICCE keys: {}", e.getMessage());
            return new TspAdapter.KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public TspAdapter.KeyResponse revokeKeys(TspAdapter.KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                webClient.post()
                    .uri("/api/v1/keys/revoke")
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to revoke ICCE keys: {}", e.getMessage());
            return new TspAdapter.KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    // ── Bind / Unbind / Key status ─────────────────────────────────────

    public TspAdapter.BindKeyResponse bindKey(TspAdapter.BindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                webClient.post()
                    .uri("/api/v1/keys/bind")
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseBindKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to bind ICCE key: {}", e.getMessage());
            return new TspAdapter.BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    public TspAdapter.KeyResponse unbindKey(TspAdapter.UnbindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                webClient.post()
                    .uri("/api/v1/keys/unbind")
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to unbind ICCE key: {}", e.getMessage());
            return new TspAdapter.KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public TspAdapter.KeyStatusResponse getKeyStatus(String keyId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                webClient.get()
                    .uri("/api/v1/keys/{keyId}", keyId)
                    .header("X-Api-Key", config.getApiKey())
                    .header("X-Tenant-Id", config.getTenantId())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyStatusResponse(response);
        } catch (Exception e) {
            log.error("Failed to get ICCE key status: {}", e.getMessage());
            return new TspAdapter.KeyStatusResponse(false, e.getMessage(), keyId, "UNKNOWN", 0, 0, null, Map.of());
        }
    }

    // ── Response parsing ──────────────────────────────────────────────

    private TspAdapter.VehicleListResponse parseVehicleResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            List<TspAdapter.VehicleInfo> vehicles = new ArrayList<>();

            JsonNode vehiclesNode = root.path("data").path("vehicles");
            if (vehiclesNode.isArray()) {
                for (JsonNode v : vehiclesNode) {
                    vehicles.add(new TspAdapter.VehicleInfo(
                        v.path("vehicleId").asText(),
                        v.path("vin").asText(),
                        v.path("brand").asText(),
                        v.path("modelName").asText(),
                        v.path("year").asInt()
                    ));
                }
            }
            return new TspAdapter.VehicleListResponse(true, "Success", vehicles);
        } catch (Exception e) {
            log.error("Failed to parse ICCE vehicle response: {}", e.getMessage());
            return new TspAdapter.VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    private TspAdapter.KeyResponse parseKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode data = root.path("data");

            String keyId = data.path("keyId").asText();
            List<String> keyData = new ArrayList<>();

            JsonNode keyDataNode = data.path("keyData");
            if (keyDataNode.isArray()) {
                for (JsonNode d : keyDataNode) {
                    keyData.add(d.asText());
                }
            }
            return new TspAdapter.KeyResponse(true, "Success", keyId, keyData);
        } catch (Exception e) {
            log.error("Failed to parse ICCE key response: {}", e.getMessage());
            return new TspAdapter.KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    private TspAdapter.BindKeyResponse parseBindKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode data = root.path("data");

            String keyId = data.path("keyId").asText();
            String sharedSecret = data.path("sharedSecret").asText();
            String tspPublicKey = data.path("tspPublicKey").asText();
            String sessionId = data.path("sessionId").asText();
            int keySlot = data.path("keySlot").asInt();

            List<String> keyData = new ArrayList<>();
            JsonNode keyDataNode = data.path("keyData");
            if (keyDataNode.isArray()) {
                for (JsonNode d : keyDataNode) {
                    keyData.add(d.asText());
                }
            }

            return new TspAdapter.BindKeyResponse(true, "Success", keyId, sharedSecret,
                tspPublicKey, sessionId, keySlot, keyData);
        } catch (Exception e) {
            log.error("Failed to parse ICCE bindKey response: {}", e.getMessage());
            return new TspAdapter.BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    private TspAdapter.KeyStatusResponse parseKeyStatusResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode data = root.path("data");

            String keyId = data.path("keyId").asText();
            String status = data.path("status").asText();
            long createdAt = data.path("createdAt").asLong();
            long expiresAt = data.path("expiresAt").asLong();
            String boundDeviceId = data.path("boundDeviceId").asText();
            Map<String, String> metadata = objectMapper.convertValue(
                data.path("metadata"), Map.class);

            return new TspAdapter.KeyStatusResponse(true, "Success", keyId, status,
                createdAt, expiresAt, boundDeviceId, metadata);
        } catch (Exception e) {
            log.error("Failed to parse ICCE key status response: {}", e.getMessage());
            return new TspAdapter.KeyStatusResponse(false, e.getMessage(), null, "UNKNOWN", 0, 0, null, Map.of());
        }
    }
}
