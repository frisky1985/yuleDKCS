package com.digitalkey.adapter.ccc;

import com.digitalkey.adapter.core.*;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import reactor.core.publisher.Mono;
import reactor.netty.http.client.HttpClient;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * CCC API client for communicating with CCC TSP endpoints.
 */
public class CccClient {

    private static final Logger log = LoggerFactory.getLogger(CccClient.class);

    private final AdapterConfig.CccProperties config;
    private final ObjectMapper objectMapper;
    private final HttpClient httpClient;
    private final AtomicBoolean connected = new AtomicBoolean(false);

    public CccClient(AdapterConfig.CccProperties config) {
        this.config = config;
        this.objectMapper = new ObjectMapper();
        this.httpClient = HttpClient.create()
            .baseUrl(config.getApiUrl())
            .responseTimeout(Duration.ofMillis(config.getReadTimeout()))
            .connectTimeout(Duration.ofMillis(config.getConnectionTimeout()));
    }

    public void init() {
        // Perform initial connection check
        log.info("CCC client connecting to: {}", config.getApiUrl());
        try {
            // In production, this would perform OAuth2 client credentials flow
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

    public VehicleListResponse getVehicles(String userId) {
        try {
            String response = httpClient.get()
                .uri("/api/v1/users/{userId}/vehicles", userId)
                .header("X-Client-Id", config.getClientId())
                .retrieve()
                .bodyToMono(String.class)
                .timeout(Duration.ofMillis(config.getReadTimeout()))
                .block();

            return parseVehicleResponse(response);
        } catch (Exception e) {
            log.error("Failed to get vehicles: {}", e.getMessage());
            return new VehicleListResponse(false, e.getMessage(), List.of());
        }
    }

    public KeyResponse requestKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = httpClient.post()
                .uri("/api/v1/keys/request")
                .header("X-Client-Id", config.getClientId())
                .header("Content-Type", "application/json")
                .bodyValue(requestJson)
                .retrieve()
                .bodyToMono(String.class)
                .timeout(Duration.ofMillis(config.getReadTimeout()))
                .block();

            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to request keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

    public KeyResponse revokeKeys(KeyRequest request) {
        try {
            String requestJson = objectMapper.writeValueAsString(request);
            String response = httpClient.post()
                .uri("/api/v1/keys/revoke")
                .header("X-Client-Id", config.getClientId())
                .header("Content-Type", "application/json")
                .bodyValue(requestJson)
                .retrieve()
                .bodyToMono(String.class)
                .timeout(Duration.ofMillis(config.getReadTimeout()))
                .block();

            return parseKeyResponse(response);
        } catch (Exception e) {
            log.error("Failed to revoke keys: {}", e.getMessage());
            return new KeyResponse(false, e.getMessage(), null, List.of());
        }
    }

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
}
