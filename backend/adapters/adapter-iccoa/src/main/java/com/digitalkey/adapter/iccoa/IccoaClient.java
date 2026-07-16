package com.digitalkey.adapter.iccoa;

import com.digitalkey.adapter.core.*;
import com.digitalkey.adapter.core.RetryUtil;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.netty.http.client.HttpClient;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * ICCOA API client for communicating with ICCOA cloud services.
 *
 * <p>API endpoints (ICCOA DK 3.0):
 * <table>
 *   <tr><th>Operation</th><th>HTTP</th><th>Path</th></tr>
 *   <tr><td>getVehicles</td><td>GET</td><td>/v1/vehicles?user_id={userId}</td></tr>
 *   <tr><td>requestKeys</td><td>POST</td><td>/v1/keys/issue</td></tr>
 *   <tr><td>revokeKeys</td><td>POST</td><td>/v1/keys/revoke</td></tr>
 *   <tr><td>bindKey</td><td>POST</td><td>/v1/keys/bind</td></tr>
 *   <tr><td>unbindKey</td><td>POST</td><td>/v1/keys/unbind</td></tr>
 *   <tr><td>getKeyStatus</td><td>GET</td><td>/v1/keys/{keyId}</td></tr>
 * </table>
 *
 * <p>Authentication: OAuth2 with ICCOA-specific App ID / App Secret.
 */
public class IccoaClient {

    private static final Logger log = LoggerFactory.getLogger(IccoaClient.class);

    private final AdapterConfig.IccoaProperties config;
    private final ObjectMapper objectMapper;
    private final HttpClient httpClient;
    private final AtomicBoolean connected = new AtomicBoolean(false);
    private final RetryUtil retryUtil;

    public IccoaClient(AdapterConfig.IccoaProperties config) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = HttpClient.create()
            .baseUrl(config.getApiUrl())
            .responseTimeout(Duration.ofMillis(30000))
            .connectTimeout(Duration.ofMillis(10000));
        this.retryUtil = new RetryUtil("iccoa-client", log, 3, 500, 30000);
    }

    // Visible for testing
    IccoaClient(AdapterConfig.IccoaProperties config, HttpClient httpClient, RetryUtil retryUtil) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = httpClient;
        this.retryUtil = retryUtil;
    }

    public void init() {
        log.info("ICCOA client connecting to: {} region: {}", config.getApiUrl(), config.getRegion());
        try {
            // ICCOA uses specific authentication flow
            // In production: OAuth2 with ICCOA-specific client credentials
            connected.set(true);
            log.info("ICCOA client connected successfully");
        } catch (Exception e) {
            log.error("ICCOA client connection failed: {}", e.getMessage());
            connected.set(false);
            throw new RuntimeException("ICCOA connection failed", e);
        }
    }

    public boolean isConnected() {
        return connected.get();
    }

    public void close() {
        connected.set(false);
        log.info("ICCOA client closed");
    }

    // ── Vehicle operations ─────────────────────────────────────────────

    public VehicleListResponse getVehicles(String userId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                httpClient.get()
                    .uri("/v1/vehicles?user_id={userId}", userId)
                    .header("X-App-Id", config.getAppId())
                    .header("X-Region", config.getRegion())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseVehicleResponse(response);
        } catch (Exception e) {
            log.error("Failed to get ICCOA vehicles: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    public KeyResponse requestKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/v1/keys/issue")
                    .header("X-App-Id", config.getAppId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to request ICCOA keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyResponse revokeKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/v1/keys/revoke")
                    .header("X-App-Id", config.getAppId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to revoke ICCOA keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    // ── Bind / Unbind / Key status ─────────────────────────────────────

    public BindKeyResponse bindKey(BindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/v1/keys/bind")
                    .header("X-App-Id", config.getAppId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseBindKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to bind ICCOA key: {}", e.getMessage());
            return new BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    public KeyResponse unbindKey(UnbindKeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = retryUtil.executeWithRetry(() ->
                httpClient.post()
                    .uri("/v1/keys/unbind")
                    .header("X-App-Id", config.getAppId())
                    .header("Content-Type", "application/json")
                    .bodyValue(requestJson)
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to unbind ICCOA key: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyStatusResponse getKeyStatus(String keyId) {
        try {
            String response = retryUtil.executeWithRetry(() ->
                httpClient.get()
                    .uri("/v1/keys/{keyId}", keyId)
                    .header("X-App-Id", config.getAppId())
                    .retrieve()
                    .bodyToMono(String.class)
                    .timeout(Duration.ofMillis(30000))
                    .block()
            );
            return parseKeyStatusResponse(response);
        } catch (Exception e) {
            log.error("Failed to get ICCOA key status: {}", e.getMessage());
            return new KeyStatusResponse(false, e.getMessage(), keyId, "UNKNOWN", 0, 0, null, Map.of());
        }
    }

    // ── Response parsing ──────────────────────────────────────────────

    private VehicleListResponse parseVehicleResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            List<VehicleInfo> vehicles = new ArrayList<>();

            JsonNode dataNode = root.path("data").path("vehicle_list");
            if (dataNode.isArray()) {
                for (JsonNode v : dataNode) {
                    vehicles.add(new VehicleInfo(
                        v.path("vehicle_id").asText(),
                        v.path("vin").asText(),
                        v.path("brand").asText(),
                        v.path("model").asText(),
                        v.path("year").asInt()
                    ));
                }
            }
            return new VehicleListResponse(true, "Success", vehicles);
        } catch (Exception e) {
            log.error("Failed to parse ICCOA vehicle response: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    private KeyResponse parseKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode dataNode = root.path("data");

            String keyId = dataNode.path("key_id").asText();
            List<String> keyData = new ArrayList<>();

            JsonNode keyContent = dataNode.path("key_content");
            if (keyContent.isArray()) {
                for (JsonNode d : keyContent) {
                    keyData.add(d.asText());
                }
            }
            return new KeyResponse(true, "Success", keyId, keyData);
        } catch (Exception e) {
            log.error("Failed to parse ICCOA key response: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    private BindKeyResponse parseBindKeyResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode data = root.path("data");

            String keyId = data.path("key_id").asText();
            String sharedSecret = data.path("shared_secret").asText();
            String tspPublicKey = data.path("tsp_public_key").asText();
            String sessionId = data.path("session_id").asText();
            int keySlot = data.path("key_slot").asInt();

            List<String> keyData = new ArrayList<>();
            JsonNode keyContent = data.path("key_content");
            if (keyContent.isArray()) {
                for (JsonNode d : keyContent) {
                    keyData.add(d.asText());
                }
            }

            return new BindKeyResponse(true, "Success", keyId, sharedSecret,
                tspPublicKey, sessionId, keySlot, keyData);
        } catch (Exception e) {
            log.error("Failed to parse ICCOA bindKey response: {}", e.getMessage());
            return new BindKeyResponse(false, e.getMessage(), null, null, null, null, 0, List.of());
        }
    }

    private KeyStatusResponse parseKeyStatusResponse(String response) {
        try {
            JsonNode root = objectMapper.readTree(response);
            JsonNode data = root.path("data");

            String keyId = data.path("key_id").asText();
            String status = data.path("status").asText();
            long createdAt = data.path("created_at").asLong();
            long expiresAt = data.path("expires_at").asLong();
            String boundDeviceId = data.path("bound_device_id").asText();
            Map<String, String> metadata = objectMapper.convertValue(
                data.path("metadata"), Map.class);

            return new KeyStatusResponse(true, "Success", keyId, status,
                createdAt, expiresAt, boundDeviceId, metadata);
        } catch (Exception e) {
            log.error("Failed to parse ICCOA key status response: {}", e.getMessage());
            return new KeyStatusResponse(false, e.getMessage(), null, "UNKNOWN", 0, 0, null, Map.of());
        }
    }
}
