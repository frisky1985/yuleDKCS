package com.digitalkey.adapter.iccoa;

import com.digitalkey.adapter.core.*;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.netty.http.client.HttpClient;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * ICCOA API client for communicating with ICCOA cloud services.
 */
public class IccoaClient {

    private static final Logger log = LoggerFactory.getLogger(IccoaClient.class);

    private final AdapterConfig.IccoaProperties config;
    private final ObjectMapper objectMapper;
    private final HttpClient httpClient;
    private final AtomicBoolean connected = new AtomicBoolean(false);

    public IccoaClient(AdapterConfig.IccoaProperties config) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = HttpClient.create()
            .baseUrl(config.getApiUrl())
            .responseTimeout(Duration.ofMillis(30000))
            .connectTimeout(Duration.ofMillis(10000));
    }

    public void init() {
        log.info("ICCOA client connecting to: {} region: {}", config.getApiUrl(), config.getRegion());
        try {
            // ICCOA uses specific authentication flow
            // In production, implement OAuth2 with ICCOA-specific parameters
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

    public VehicleListResponse getVehicles(String userId) {
        try {
            String response = httpClient.get()
                .uri("/v1/vehicles?user_id={userId}", userId)
                .header("X-App-Id", config.getAppId())
                .header("X-Region", config.getRegion())
                .retrieve()
                .bodyToMono(String.class)
                .block();

            return parseVehicleResponse(response);
        } catch (Exception e) {
            log.error("Failed to get ICCOA vehicles: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    public KeyResponse requestKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = httpClient.post()
                .uri("/v1/keys/issue")
                .header("X-App-Id", config.getAppId())
                .header("Content-Type", "application/json")
                .bodyValue(requestJson)
                .retrieve()
                .bodyToMono(String.class)
                .block();

            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to request ICCOA keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyResponse revokeKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = httpClient.post()
                .uri("/v1/keys/revoke")
                .header("X-App-Id", config.getAppId())
                .header("Content-Type", "application/json")
                .bodyValue(requestJson)
                .retrieve()
                .bodyToMono(String.class)
                .block();

            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to revoke ICCOA keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

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
}
