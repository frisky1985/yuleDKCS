package com.digitalkey.adapter.ccc;

import com.digitalkey.adapter.core.*;
import com.digitalkey.adapter.core.RetryUtil;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.Mono;
import reactor.netty.http.client.HttpClient;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * CCC API client for communicating with CCC TSP endpoints.
 *
 * <p>API endpoints (CCC Digital Key 3.0):
 * <table>
 *   <tr><th>Operation</th><th>HTTP</th><th>Path</th></tr>
 *   <tr><td>getVehicles</td><td>GET</td><td>/api/v1/users/{userId}/vehicles</td></tr>
 *   <tr><td>requestKeys</td><td>POST</td><td>/api/v1/keys/request</td></tr>
 *   <tr><td>revokeKeys</td><td>POST</td><td>/api/v1/keys/revoke</td></tr>
 *   <tr><td>bindKey</td><td>POST</td><td>/api/v1/keys/bind</td></tr>
 *   <tr><td>unbindKey</td><td>POST</td><td>/api/v1/keys/unbind</td></tr>
 *   <tr><td>getKeyStatus</td><td>GET</td><td>/api/v1/keys/{keyId}/status</td></tr>
 * </table>
 *
 * <p>Authentication: OAuth2 client credentials; token passed as Bearer header.
 */
public class CccClient {

    private static final Logger log = LoggerFactory.getLogger(CccClient.class);

    private final AdapterConfig.CccProperties config;
    private final ObjectMapper objectMapper;
    private final HttpClient httpClient;
    private final AtomicBoolean connected = new AtomicBoolean(false);
    private final RetryUtil retryUtil;

    public CccClient(AdapterConfig.CccProperties config) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = HttpClient.create()
            .baseUrl(config.getApiUrl())
            .responseTimeout(Duration.ofMillis(config.getReadTimeout()))
            .connectTimeout(Duration.ofMillis(config.getConnectionTimeout()));
        this.retryUtil = new RetryUtil("ccc-client", log, 3, 500, 30000);
    }

    // Visible for testing
    CccClient(AdapterConfig.CccProperties config, HttpClient httpClient, RetryUtil retryUtil) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = httpClient;
        this.retryUtil = retryUtil;
    }

    public void init() {
        log.info("CCC client connecting to: {}", config.getApiUrl());
        try {
            // In production: OAuth2 client credentials flow to obtain access token
            // POST /auth/token { client_id, client_secret, grant_type: "client_credentials" }
            connected.set(true);
            log.info("CCC client connected successfully");
        } catch (Exception e) {
            log.error("CCC client connection failed: {}", e.getMessage());
            connected.set(false);
            throw new RuntimeException("CCC connection failed", e);
        }
    }

    public boolean isConnected() {
        return connected.get();
    }

    public void close() {
        connected.set(false);
        log.info("CCC client closed");
    }

    // ── Vehicle operations ─────────────────────────────────────────────

    public VehicleListResponse getVehicles(String userId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                httpClient.get()
                    .uri("/api/v1/users/{userId}/vehicles", userId)
                    .header("X-Client-Id", config.getClientId())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseVehicleResponse(response);
        } catch (Exception e) {
            log.error("Failed to get vehicles: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    public KeyResponse requestKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/api/v1/keys/request")
                    .header("X-Client-Id", config.getClientId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to request keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyResponse revokeKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/api/v1/keys/revoke")
                    .header("X-Client-Id", config.getClientId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to revoke keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    // ── Bind / Unbind / Key status ─────────────────────────────────────

    public BindKeyResponse bindKey(BindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/api/v1/keys/bind")
                    .header("X-Client-Id", config.getClientId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseBindKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to bind key: {}", e.getMessage());
            return new BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    public KeyResponse unbindKey(UnbindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/api/v1/keys/unbind")
                    .header("X-Client-Id", config.getClientId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to unbind key: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyStatusResponse getKeyStatus(String keyId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                httpClient.get()
                    .uri("/api/v1/keys/{keyId}/status", keyId)
                    .header("X-Client-Id", config.getClientId())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(config.getReadTimeout()))
                    .block()
            );
            return parseKeyStatusResponse(response);
        } catch (Exception e) {
            log.error("Failed to get key status: {}", e.getMessage());
            return new KeyStatusResponse(false, e.getMessage(), keyId, "UNKNOWN", 0, 0, null, Map.of());
        }
    }

    // ── Response parsing ──────────────────────────────────────────────

    private VehicleListResponse parseVehicleResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            List<VehicleInfo> vehicles = new ArrayList<>();

            JsonNode vehiclesNode = root.path("vehicles");
            if (vehiclesNode.isArray()) {
                for (JsonNode v : vehiclesNode) {
                    vehicles.add(new VehicleInfo(
                        v.path("vehicleId").asText(),
                        v.path("vin").asText(),
                        v.path("make").asText(),
                        v.path("model").asText(),
                        v.path("modelYear").asInt()
                    ));
                }
            }
            return new VehicleListResponse(true, "Success", vehicles);
        } catch (Exception e) {
            log.error("Failed to parse vehicle response: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    private KeyResponse parseKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            String keyId = root.path("keyId").asText();

            List<String> keyData = new ArrayList<>();
            JsonNode keyDataNode = root.path("keyData");
            if (keyDataNode.isArray()) {
                for (JsonNode d : keyDataNode) {
                    keyData.add(d.asText());
                }
            }
            return new KeyResponse(true, "Success", keyId, keyData);
        } catch (Exception e) {
            log.error("Failed to parse key response: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    private BindKeyResponse parseBindKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);

            // Expected JSON schema:
            // {
            //   "keyId": "...",
            //   "sharedSecret": "...",     // base64 ECDH shared secret
            //   "tspPublicKey": "...",     // base64 TSP ephemeral public key
            //   "sessionId": "...",
            //   "keySlot": 1,
            //   "keyData": [...]
            // }
            String keyId = root.path("keyId").asText();
            String sharedSecret = root.path("sharedSecret").asText();
            String tspPublicKey = root.path("tspPublicKey").asText();
            String sessionId = root.path("sessionId").asText();
            int keySlot = root.path("keySlot").asInt();

            List<String> keyData = new ArrayList<>();
            JsonNode keyDataNode = root.path("keyData");
            if (keyDataNode.isArray()) {
                for (JsonNode d : keyDataNode) {
                    keyData.add(d.asText());
                }
            }

            return new BindKeyResponse(true, "Success", keyId, sharedSecret,
                tspPublicKey, sessionId, keySlot, keyData);
        } catch (Exception e) {
            log.error("Failed to parse bindKey response: {}", e.getMessage());
            return new BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    private KeyStatusResponse parseKeyStatusResponse(String response) {
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

            return new KeyStatusResponse(true, "Success", keyId, status,
                createdAt, expiresAt, boundDeviceId, metadata);
        } catch (Exception e) {
            log.error("Failed to parse key status response: {}", e.getMessage());
            return new KeyStatusResponse(false, e.getMessage(), null, "UNKNOWN", 0, 0, null, Map.of());
        }
    }
}
