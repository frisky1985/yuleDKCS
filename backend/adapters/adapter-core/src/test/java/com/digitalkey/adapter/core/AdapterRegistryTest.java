package com.digitalkey.adapter.core;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.util.Collection;
import java.util.List;
import java.util.concurrent.CompletableFuture;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.*;

/**
 * Unit tests for {@link AdapterRegistry}.
 * Verifies registration, lookup, round-robin routing, and lifecycle management.
 */
@ExtendWith(MockitoExtension.class)
@DisplayName("AdapterRegistry")
class AdapterRegistryTest {

    private AdapterRegistry registry;

    @Mock
    private TspAdapter cccAdapter;

    @Mock
    private TspAdapter iccoaAdapter;

    @Mock
    private TspAdapter icceAdapter;

    @BeforeEach
    void setUp() {
        registry = new AdapterRegistry();

        lenient().when(cccAdapter.getAdapterName()).thenReturn("CCC");
        lenient().when(iccoaAdapter.getAdapterName()).thenReturn("ICCOA");
        lenient().when(icceAdapter.getAdapterName()).thenReturn("ICCE");
    }

    // ── Registration ──────────────────────────────────────────────

    @Nested
    @DisplayName("registration")
    class RegistrationTests {

        @Test
        @DisplayName("should register a single adapter")
        void registerSingleAdapter() {
            registry.register(cccAdapter);

            assertThat(registry.getAdapterNames()).containsExactly("CCC");
            assertThat(registry.getAdapter("CCC")).isSameAs(cccAdapter);
        }

        @Test
        @DisplayName("should register multiple adapters")
        void registerMultipleAdapters() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            registry.register(icceAdapter);

            assertThat(registry.getAdapterNames())
                .hasSize(3)
                .containsExactlyInAnyOrder("CCC", "ICCOA", "ICCE");
        }

        @Test
        @DisplayName("should overwrite adapter with same name")
        void registerDuplicateName() {
            registry.register(cccAdapter);
            TspAdapter anotherCcc = mock(TspAdapter.class);
            when(anotherCcc.getAdapterName()).thenReturn("CCC");

            registry.register(anotherCcc);

            assertThat(registry.getAdapter("CCC")).isSameAs(anotherCcc);
        }
    }

    // ── Unregistration ───────────────────────────────────────────

    @Nested
    @DisplayName("unregistration")
    class UnregistrationTests {

        @Test
        @DisplayName("should unregister an existing adapter")
        void unregisterExistingAdapter() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);

            registry.unregister("CCC");

            assertThat(registry.getAdapterNames()).containsExactly("ICCOA");
            assertThat(registry.getAdapter("CCC")).isNull();
        }

        @Test
        @DisplayName("should handle unregister of non-existent adapter gracefully")
        void unregisterNonExistent() {
            registry.register(cccAdapter);
            registry.unregister("NONEXISTENT");

            assertThat(registry.getAdapterNames()).containsExactly("CCC");
        }
    }

    // ── Query / retrieval ────────────────────────────────────────

    @Nested
    @DisplayName("query")
    class QueryTests {

        @BeforeEach
        void registerAll() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            registry.register(icceAdapter);
        }

        @Test
        @DisplayName("should return all registered adapters")
        void getAllAdapters() {
            Collection<TspAdapter> all = registry.getAllAdapters();
            assertThat(all).hasSize(3).contains(cccAdapter, iccoaAdapter, icceAdapter);
        }

        @Test
        @DisplayName("should return empty list when no adapters registered")
        void getAdapterNamesEmpty() {
            AdapterRegistry empty = new AdapterRegistry();
            assertThat(empty.getAdapterNames()).isEmpty();
        }

        @Test
        @DisplayName("should find adapter by protocol name (case-insensitive)")
        void getAdapterByProtocol() {
            assertThat(registry.getAdapterByProtocol("ccc")).isSameAs(cccAdapter);
            assertThat(registry.getAdapterByProtocol("ICCOA")).isSameAs(iccoaAdapter);
            assertThat(registry.getAdapterByProtocol("IcCe")).isSameAs(icceAdapter);
        }

        @Test
        @DisplayName("should return null for unknown protocol")
        void getAdapterByUnknownProtocol() {
            assertThat(registry.getAdapterByProtocol("bluetooth")).isNull();
        }
    }

    // ── Round-robin ──────────────────────────────────────────────

    @Nested
    @DisplayName("round-robin")
    class RoundRobinTests {

        @Test
        @DisplayName("should return null when no adapters are registered")
        void getNextAdapterEmpty() {
            assertThat(registry.getNextAdapter()).isNull();
        }

        @Test
        @DisplayName("should return null when all adapters are disabled")
        void getNextAdapterAllDisabled() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            when(cccAdapter.isEnabled()).thenReturn(false);
            when(iccoaAdapter.isEnabled()).thenReturn(false);

            assertThat(registry.getNextAdapter()).isNull();
        }

        @Test
        @DisplayName("should cycle through enabled adapters round-robin")
        void getNextAdapterRoundRobin() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            when(cccAdapter.isEnabled()).thenReturn(true);
            when(iccoaAdapter.isEnabled()).thenReturn(true);

            // First call → CCC (index 0 % 2 = 0)
            assertThat(registry.getNextAdapter()).isSameAs(cccAdapter);
            // Second call → ICCOA (index 1 % 2 = 1)
            assertThat(registry.getNextAdapter()).isSameAs(iccoaAdapter);
            // Third call → CCC (index 2 % 2 = 0)
            assertThat(registry.getNextAdapter()).isSameAs(cccAdapter);
        }
    }

    // ── Enable / disable ─────────────────────────────────────────

    @Nested
    @DisplayName("enable/disable")
    class EnableDisableTests {

        @Test
        @DisplayName("should return only enabled adapters")
        void getEnabledAdapters() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            when(cccAdapter.isEnabled()).thenReturn(true);
            when(iccoaAdapter.isEnabled()).thenReturn(false);

            List<TspAdapter> enabled = registry.getEnabledAdapters();
            assertThat(enabled).containsExactly(cccAdapter);
        }
    }

    // ── Health ───────────────────────────────────────────────────

    @Nested
    @DisplayName("health")
    class HealthTests {

        @Test
        @DisplayName("should be healthy when at least one adapter is healthy")
        void isHealthyWithHealthyAdapter() {
            registry.register(cccAdapter);
            when(cccAdapter.healthCheck()).thenReturn(true);

            assertThat(registry.isHealthy()).isTrue();
        }

        @Test
        @DisplayName("should be unhealthy when no adapters are healthy")
        void isHealthyWhenAllUnhealthy() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            when(cccAdapter.healthCheck()).thenReturn(false);
            when(iccoaAdapter.healthCheck()).thenReturn(false);

            assertThat(registry.isHealthy()).isFalse();
        }

        @Test
        @DisplayName("should be unhealthy when no adapters registered")
        void isHealthyEmpty() {
            assertThat(new AdapterRegistry().isHealthy()).isFalse();
        }
    }

    // ── Lifecycle ────────────────────────────────────────────────

    @Nested
    @DisplayName("lifecycle")
    class LifecycleTests {

        @Test
        @DisplayName("initializeAdapters should call initialize on each adapter")
        void initializeAdapters() {
            registry.register(cccAdapter);
            registry.register(iccoaAdapter);
            when(cccAdapter.initialize()).thenReturn(CompletableFuture.completedFuture(null));
            when(iccoaAdapter.initialize()).thenReturn(CompletableFuture.completedFuture(null));

            registry.initializeAdapters();

            verify(cccAdapter).initialize();
            verify(iccoaAdapter).initialize();
        }

        @Test
        @DisplayName("shutdownAdapters should call shutdown on each adapter")
        void shutdownAdapters() {
            registry.register(cccAdapter);
            when(cccAdapter.shutdown()).thenReturn(CompletableFuture.completedFuture(null));

            registry.shutdownAdapters();

            verify(cccAdapter).shutdown();
        }

        @Test
        @DisplayName("should handle initialize failure gracefully (no exception thrown)")
        void initializeHandlesFailure() {
            registry.register(cccAdapter);
            when(cccAdapter.initialize()).thenReturn(
                CompletableFuture.failedFuture(new RuntimeException("Connection refused"))
            );

            // Should not throw
            registry.initializeAdapters();

            verify(cccAdapter).initialize();
        }
    }

    // ── Thread safety (integration-oriented) ─────────────────────

    @Test
    @DisplayName("should support concurrent register/getAdapterNames (smoke)")
    void concurrentAccess() throws InterruptedException {
        registry.register(cccAdapter);

        Thread t1 = new Thread(() -> {
            for (int i = 0; i < 100; i++) {
                registry.register(iccoaAdapter);
            }
        });
        Thread t2 = new Thread(() -> {
            for (int i = 0; i < 100; i++) {
                registry.getAdapterNames();
            }
        });

        t1.start();
        t2.start();
        t1.join();
        t2.join();

        // Should complete without throwing ConcurrentModificationException
        assertThat(registry.getAdapterNames()).isNotEmpty();
    }
}
