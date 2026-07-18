package com.digitalkey.adapter.grpcserver;

import com.digitalkey.adapter.core.AdapterMetrics;
import com.digitalkey.adapter.core.AdapterRegistry;
import com.digitalkey.adapter.core.TspAdapter;
import com.digitalkey.adapter.grpc.*;
import io.grpc.stub.StreamObserver;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.util.List;
import java.util.concurrent.CompletableFuture;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

/**
 * Unit tests for {@link AdapterServiceImpl}.
 * Tests gRPC service method routing, adapter selection, error handling, and metric recording.
 *
 * <p><strong>Note:</strong> These tests require generated protobuf/grpc classes
 * (from {@code adapter.proto}) on the classpath. Run protobuf compilation first:
 * {@code mvn generate-sources -pl adapter-grpc-server -am}
 */
@ExtendWith(MockitoExtension.class)
@DisplayName("AdapterServiceImpl")
class AdapterServiceImplTest {

    @Mock
    private AdapterRegistry registry;

    @Mock
    private AdapterMetrics metrics;

    @Mock
    private TspAdapter cccAdapter;

    @Mock
    private StreamObserver<VehicleListResponse> vehicleObserver;

    @Mock
    private StreamObserver<KeyResponse> keyObserver;

    @Mock
    private StreamObserver<HealthCheckResponse> healthObserver;

    @Captor
    private ArgumentCaptor<VehicleListResponse> vehicleResponseCaptor;

    @Captor
    private ArgumentCaptor<KeyResponse> keyResponseCaptor;

    @Captor
    private ArgumentCaptor<HealthCheckResponse> healthResponseCaptor;

    private AdapterServiceImpl service;

    @BeforeEach
    void setUp() {
        service = new AdapterServiceImpl(registry, metrics);
        lenient().when(cccAdapter.getAdapterName()).thenReturn("CCC");
        lenient().when(cccAdapter.isEnabled()).thenReturn(true);
    }

    // ── GetVehicles ──────────────────────────────────────────────

    @Nested
    @DisplayName("GetVehicles")
    class GetVehiclesTests {

        @Test
        @DisplayName("should delegate to adapter and return success response")
        void successfulGetVehicles() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(cccAdapter);
            TspAdapter.VehicleListResponse expectedResult = new TspAdapter.VehicleListResponse(
                true, "OK", List.of(
                    new TspAdapter.VehicleInfo("v1", "WBA3A5C5XDF123456", "BMW", "i4", 2025)
                )
            );
            when(cccAdapter.getVehicles("user-1"))
                .thenReturn(CompletableFuture.completedFuture(expectedResult));

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("user-1")
                .setAdapterType("ccc")
                .build();

            service.getVehicles(request, vehicleObserver);

            verify(vehicleObserver).onNext(vehicleResponseCaptor.capture());
            verify(vehicleObserver).onCompleted();

            VehicleListResponse response = vehicleResponseCaptor.getValue();
            assertThat(response.getSuccess()).isTrue();
            assertThat(response.getMessage()).isEqualTo("OK");
            assertThat(response.getVehiclesList()).hasSize(1);
            assertThat(response.getVehicles(0).getVehicleId()).isEqualTo("v1");
        }

        @Test
        @DisplayName("should return error when no adapter found")
        void noAdapterFound() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(null);
            when(registry.getAdapter("ccc")).thenReturn(null);

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("user-1")
                .setAdapterType("ccc")
                .build();

            service.getVehicles(request, vehicleObserver);

            verify(vehicleObserver).onNext(vehicleResponseCaptor.capture());
            verify(vehicleObserver).onCompleted();

            VehicleListResponse response = vehicleResponseCaptor.getValue();
            assertThat(response.getSuccess()).isFalse();
            assertThat(response.getMessage()).contains("No adapter available");
        }

        @Test
        @DisplayName("should use next available adapter when adapterType is empty")
        void emptyAdapterTypeUsesNext() {
            when(registry.getNextAdapter()).thenReturn(cccAdapter);
            TspAdapter.VehicleListResponse expectedResult = new TspAdapter.VehicleListResponse(
                true, "OK", List.of()
            );
            when(cccAdapter.getVehicles("user-1"))
                .thenReturn(CompletableFuture.completedFuture(expectedResult));

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("user-1")
                .build(); // no adapter_type

            service.getVehicles(request, vehicleObserver);

            verify(vehicleObserver).onNext(vehicleResponseCaptor.capture());
            assertThat(vehicleResponseCaptor.getValue().getSuccess()).isTrue();
        }

        @Test
        @DisplayName("should return error when adapter future fails")
        void adapterFutureFails() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(cccAdapter);
            when(cccAdapter.getVehicles("user-1"))
                .thenReturn(CompletableFuture.failedFuture(new RuntimeException("API timeout")));

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("user-1")
                .setAdapterType("ccc")
                .build();

            service.getVehicles(request, vehicleObserver);

            verify(vehicleObserver).onNext(vehicleResponseCaptor.capture());
            VehicleListResponse response = vehicleResponseCaptor.getValue();
            assertThat(response.getSuccess()).isFalse();
        }
    }

    // ── RequestKeys ──────────────────────────────────────────────

    @Nested
    @DisplayName("RequestKeys")
    class RequestKeysTests {

        @Test
        @DisplayName("should delegate to adapter and return key response")
        void successfulKeyRequest() {
            when(registry.getAdapterByProtocol("iccoa")).thenReturn(cccAdapter);
            TspAdapter.KeyResponse expectedResult = new TspAdapter.KeyResponse(
                true, "Key generated", "key-abc", List.of("keydata1")
            );
            when(cccAdapter.requestKeys(any())).thenReturn(CompletableFuture.completedFuture(expectedResult));

            KeyRequest request = KeyRequest.newBuilder()
                .setUserId("user-1")
                .setVehicleId("v1")
                .setAdapterType("iccoa")
                .addKeyTypes("digital")
                .build();

            service.requestKeys(request, keyObserver);

            verify(keyObserver).onNext(keyResponseCaptor.capture());
            verify(keyObserver).onCompleted();

            KeyResponse response = keyResponseCaptor.getValue();
            assertThat(response.getKeyId()).isEqualTo("key-abc");
            assertThat(response.getKeyDataList()).containsExactly("keydata1");
        }

        @Test
        @DisplayName("should return error when no adapter available for type")
        void noAdapterForKeyRequest() {
            when(registry.getAdapterByProtocol("iccoa")).thenReturn(null);
            when(registry.getAdapter("iccoa")).thenReturn(null);

            KeyRequest request = KeyRequest.newBuilder()
                .setUserId("user-1")
                .setVehicleId("v1")
                .setAdapterType("iccoa")
                .build();

            service.requestKeys(request, keyObserver);

            verify(keyObserver).onNext(keyResponseCaptor.capture());
            assertThat(keyResponseCaptor.getValue().getSuccess()).isFalse();
        }
    }

    // ── RevokeKeys ───────────────────────────────────────────────

    @Nested
    @DisplayName("RevokeKeys")
    class RevokeKeysTests {

        @Test
        @DisplayName("should delegate to adapter and return success")
        void successfulRevoke() {
            when(registry.getAdapterByProtocol("icce")).thenReturn(cccAdapter);
            TspAdapter.KeyResponse expectedResult = new TspAdapter.KeyResponse(
                true, "Revoked", "key-abc", List.of()
            );
            when(cccAdapter.revokeKeys(any())).thenReturn(CompletableFuture.completedFuture(expectedResult));

            KeyRevokeRequest request = KeyRevokeRequest.newBuilder()
                .setUserId("user-1")
                .setVehicleId("v1")
                .setKeyId("key-abc")
                .setAdapterType("icce")
                .build();

            service.revokeKeys(request, keyObserver);

            verify(keyObserver).onNext(keyResponseCaptor.capture());
            assertThat(keyResponseCaptor.getValue().getSuccess()).isTrue();
        }
    }

    // ── HealthCheck ──────────────────────────────────────────────

    @Nested
    @DisplayName("HealthCheck")
    class HealthCheckTests {

        @Test
        @DisplayName("should return healthy when adapter is healthy")
        void healthyAdapter() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(cccAdapter);
            when(cccAdapter.healthCheck()).thenReturn(true);

            HealthCheckRequest request = HealthCheckRequest.newBuilder()
                .setAdapterType("ccc")
                .build();

            service.healthCheck(request, healthObserver);

            verify(healthObserver).onNext(healthResponseCaptor.capture());
            verify(healthObserver).onCompleted();

            HealthCheckResponse response = healthResponseCaptor.getValue();
            assertThat(response.getHealthy()).isTrue();
            assertThat(response.getStatus()).isEqualTo("UP");
        }

        @Test
        @DisplayName("should return unhealthy when adapter is unhealthy")
        void unhealthyAdapter() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(cccAdapter);
            when(cccAdapter.healthCheck()).thenReturn(false);

            HealthCheckRequest request = HealthCheckRequest.newBuilder()
                .setAdapterType("ccc")
                .build();

            service.healthCheck(request, healthObserver);

            verify(healthObserver).onNext(healthResponseCaptor.capture());
            assertThat(healthResponseCaptor.getValue().getHealthy()).isFalse();
        }

        @Test
        @DisplayName("should return unhealthy when adapter not found")
        void adapterNotFound() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(null);
            when(registry.getAdapter("ccc")).thenReturn(null);

            HealthCheckRequest request = HealthCheckRequest.newBuilder()
                .setAdapterType("ccc")
                .build();

            service.healthCheck(request, healthObserver);

            verify(healthObserver).onNext(healthResponseCaptor.capture());
            assertThat(healthResponseCaptor.getValue().getHealthy()).isFalse();
        }
    }

    // ── Metrics ──────────────────────────────────────────────────

    @Nested
    @DisplayName("metrics recording")
    class MetricsTests {

        @Test
        @DisplayName("should record success metric on successful getVehicles")
        void recordsSuccessMetric() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(cccAdapter);
            TspAdapter.VehicleListResponse result = new TspAdapter.VehicleListResponse(true, "OK", List.of());
            when(cccAdapter.getVehicles(any())).thenReturn(CompletableFuture.completedFuture(result));

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("u1")
                .setAdapterType("ccc")
                .build();

            service.getVehicles(request, vehicleObserver);

            verify(metrics).recordSuccess(eq("CCC"), eq("getVehicles"), anyLong());
        }

        @Test
        @DisplayName("should record failure metric when no adapter found")
        void recordsFailureMetricOnNoAdapter() {
            when(registry.getAdapterByProtocol("ccc")).thenReturn(null);
            when(registry.getAdapter("ccc")).thenReturn(null);

            VehicleListRequest request = VehicleListRequest.newBuilder()
                .setUserId("u1")
                .setAdapterType("ccc")
                .build();

            service.getVehicles(request, vehicleObserver);

            verify(metrics).recordFailure(eq("ccc"), eq("getVehicles"), anyLong(), eq("NO_ADAPTER"));
        }
    }
}
