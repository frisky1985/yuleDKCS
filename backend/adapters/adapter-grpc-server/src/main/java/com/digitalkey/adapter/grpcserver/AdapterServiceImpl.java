package com.digitalkey.adapter.grpcserver;

import com.digitalkey.adapter.core.*;
import com.digitalkey.adapter.grpc.*;
import io.grpc.stub.StreamObserver;
import net.devh.boot.grpc.server.service.GrpcService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;

import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * gRPC service implementation bridging Go backend to Java TSP adapters.
 */
@GrpcService
public class AdapterServiceImpl extends AdapterServiceGrpc.AdapterServiceImplBase {

    private static final Logger log = LoggerFactory.getLogger(AdapterServiceImpl.class);

    private final AdapterRegistry registry;
    private final AdapterMetrics metrics;

    @Autowired
    public AdapterServiceImpl(AdapterRegistry registry, AdapterMetrics metrics) {
        this.registry = registry;
        this.metrics = metrics;
    }

    @Override
    public void getVehicles(VehicleListRequest request, StreamObserver<VehicleListResponse> responseObserver) {
        long startTime = System.currentTimeMillis();
        String adapterType = request.getAdapterType();
        
        log.info("gRPC: GetVehicles request for user {} adapter {}", 
            request.getUserId(), adapterType);
        
        try {
            TspAdapter adapter = selectAdapter(adapterType);
            if (adapter == null) {
                sendVehicleError(responseObserver, "No adapter available for type: " + adapterType);
                metrics.recordFailure(adapterType, "getVehicles", 
                    System.currentTimeMillis() - startTime, "NO_ADAPTER");
                return;
            }

            adapter.getVehicles(request.getUserId())
                .orTimeout(30, TimeUnit.SECONDS)
                .whenComplete((result, error) -> {
                    if (error != null) {
                        log.error("GetVehicles failed: {}", error.getMessage());
                        sendVehicleError(responseObserver, error.getMessage());
                        metrics.recordFailure(adapter.getAdapterName(), "getVehicles",
                            System.currentTimeMillis() - startTime, error.getClass().getSimpleName());
                    } else {
                        VehicleListResponse response = VehicleListResponse.newBuilder()
                            .setSuccess(result.success())
                            .setMessage(result.message())
                            .addAllVehicles(result.vehicles().stream()
                                .map(v -> Vehicle.newBuilder()
                                    .setVehicleId(v.vehicleId())
                                    .setVin(v.vin())
                                    .setMake(v.make())
                                    .setModel(v.model())
                                    .setModelYear(v.modelYear())
                                    .build())
                                .toList())
                            .build();
                        
                        responseObserver.onNext(response);
                        responseObserver.onCompleted();
                        metrics.recordSuccess(adapter.getAdapterName(), "getVehicles",
                            System.currentTimeMillis() - startTime);
                        log.info("GetVehicles completed successfully for user {}", request.getUserId());
                    }
                });
                
        } catch (Exception e) {
            log.error("GetVehicles exception: {}", e.getMessage(), e);
            sendVehicleError(responseObserver, e.getMessage());
            metrics.recordFailure(adapterType, "getVehicles",
                System.currentTimeMillis() - startTime, "EXCEPTION");
        }
    }

    @Override
    public void requestKeys(KeyRequest request, StreamObserver<KeyResponse> responseObserver) {
        long startTime = System.currentTimeMillis();
        String adapterType = request.getAdapterType();
        
        log.info("gRPC: RequestKeys for user {} vehicle {} adapter {}",
            request.getUserId(), request.getVehicleId(), adapterType);
        
        try {
            TspAdapter adapter = selectAdapter(adapterType);
            if (adapter == null) {
                sendKeyError(responseObserver, "No adapter available for type: " + adapterType);
                metrics.recordFailure(adapterType, "requestKeys",
                    System.currentTimeMillis() - startTime, "NO_ADAPTER");
                return;
            }

            TspAdapter.KeyRequest keyRequest = new TspAdapter.KeyRequest(
                request.getUserId(),
                request.getVehicleId(),
                request.getVin(),
                request.getKeyTypesList(),
                request.getOptionsMap()
            );

            adapter.requestKeys(keyRequest)
                .orTimeout(30, TimeUnit.SECONDS)
                .whenComplete((result, error) -> {
                    if (error != null) {
                        log.error("RequestKeys failed: {}", error.getMessage());
                        sendKeyError(responseObserver, error.getMessage());
                        metrics.recordFailure(adapter.getAdapterName(), "requestKeys",
                            System.currentTimeMillis() - startTime, error.getClass().getSimpleName());
                    } else {
                        KeyResponse response = KeyResponse.newBuilder()
                            .setSuccess(result.success())
                            .setMessage(result.message())
                            .setKeyId(result.keyId() != null ? result.keyId() : "")
                            .addAllKeyData(result.keyData())
                            .build();
                        
                        responseObserver.onNext(response);
                        responseObserver.onCompleted();
                        metrics.recordSuccess(adapter.getAdapterName(), "requestKeys",
                            System.currentTimeMillis() - startTime);
                        log.info("RequestKeys completed successfully: {}", result.keyId());
                    }
                });

        } catch (Exception e) {
            log.error("RequestKeys exception: {}", e.getMessage(), e);
            sendKeyError(responseObserver, e.getMessage());
            metrics.recordFailure(adapterType, "requestKeys",
                System.currentTimeMillis() - startTime, "EXCEPTION");
        }
    }

    @Override
    public void revokeKeys(KeyRevokeRequest request, StreamObserver<KeyResponse> responseObserver) {
        long startTime = System.currentTimeMillis();
        String adapterType = request.getAdapterType();
        
        log.info("gRPC: RevokeKeys for user {} vehicle {} key {}",
            request.getUserId(), request.getVehicleId(), request.getKeyId());
        
        try {
            TspAdapter adapter = selectAdapter(adapterType);
            if (adapter == null) {
                sendKeyError(responseObserver, "No adapter available for type: " + adapterType);
                metrics.recordFailure(adapterType, "revokeKeys",
                    System.currentTimeMillis() - startTime, "NO_ADAPTER");
                return;
            }

            TspAdapter.KeyRequest keyRequest = new TspAdapter.KeyRequest(
                request.getUserId(),
                request.getVehicleId(),
                "", // VIN not needed for revoke
                java.util.List.of(request.getKeyId()),
                Map.of()
            );

            adapter.revokeKeys(keyRequest)
                .orTimeout(30, TimeUnit.SECONDS)
                .whenComplete((result, error) -> {
                    if (error != null) {
                        log.error("RevokeKeys failed: {}", error.getMessage());
                        sendKeyError(responseObserver, error.getMessage());
                        metrics.recordFailure(adapter.getAdapterName(), "revokeKeys",
                            System.currentTimeMillis() - startTime, error.getClass().getSimpleName());
                    } else {
                        KeyResponse response = KeyResponse.newBuilder()
                            .setSuccess(result.success())
                            .setMessage(result.message())
                            .setKeyId(result.keyId() != null ? result.keyId() : "")
                            .addAllKeyData(result.keyData())
                            .build();
                        
                        responseObserver.onNext(response);
                        responseObserver.onCompleted();
                        metrics.recordSuccess(adapter.getAdapterName(), "revokeKeys",
                            System.currentTimeMillis() - startTime);
                        log.info("RevokeKeys completed successfully");
                    }
                });

        } catch (Exception e) {
            log.error("RevokeKeys exception: {}", e.getMessage(), e);
            sendKeyError(responseObserver, e.getMessage());
            metrics.recordFailure(adapterType, "revokeKeys",
                System.currentTimeMillis() - startTime, "EXCEPTION");
        }
    }

    @Override
    public void healthCheck(HealthCheckRequest request, StreamObserver<HealthCheckResponse> responseObserver) {
        String adapterType = request.getAdapterType();
        
        log.debug("gRPC: HealthCheck for adapter {}", adapterType);
        
        try {
            TspAdapter adapter = selectAdapter(adapterType);
            boolean healthy = adapter != null && adapter.healthCheck();
            
            HealthCheckResponse response = HealthCheckResponse.newBuilder()
                .setHealthy(healthy)
                .setAdapterName(adapter != null ? adapter.getAdapterName() : "none")
                .setStatus(healthy ? "UP" : "DOWN")
                .build();
            
            responseObserver.onNext(response);
            responseObserver.onCompleted();
            
        } catch (Exception e) {
            log.error("HealthCheck exception: {}", e.getMessage());
            HealthCheckResponse response = HealthCheckResponse.newBuilder()
                .setHealthy(false)
                .setAdapterName(adapterType)
                .setStatus("ERROR: " + e.getMessage())
                .build();
            
            responseObserver.onNext(response);
            responseObserver.onCompleted();
        }
    }

    private TspAdapter selectAdapter(String adapterType) {
        if (adapterType == null || adapterType.isEmpty()) {
            return registry.getNextAdapter();
        }
        TspAdapter adapter = registry.getAdapterByProtocol(adapterType);
        if (adapter == null) {
            adapter = registry.getAdapter(adapterType);
        }
        return adapter;
    }

    private void sendVehicleError(StreamObserver<VehicleListResponse> observer, String message) {
        VehicleListResponse response = VehicleListResponse.newBuilder()
            .setSuccess(false)
            .setMessage(message)
            .build();
        observer.onNext(response);
        observer.onCompleted();
    }

    private void sendKeyError(StreamObserver<KeyResponse> observer, String message) {
        KeyResponse response = KeyResponse.newBuilder()
            .setSuccess(false)
            .setMessage(message)
            .build();
        observer.onNext(response);
        observer.onCompleted();
    }
}
