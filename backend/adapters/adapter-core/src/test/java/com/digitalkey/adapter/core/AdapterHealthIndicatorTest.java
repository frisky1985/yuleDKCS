package com.digitalkey.adapter.core;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.Status;

import java.util.List;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.when;

/**
 * Unit tests for {@link AdapterHealthIndicator}.
 * Ensures the health endpoint returns correct UP/DOWN status based on registry health.
 */
@ExtendWith(MockitoExtension.class)
@DisplayName("AdapterHealthIndicator")
class AdapterHealthIndicatorTest {

    @Mock
    private AdapterRegistry registry;

    @Mock
    private TspAdapter cccAdapter;

    @Mock
    private TspAdapter iccoaAdapter;

    private AdapterHealthIndicator healthIndicator;

    @BeforeEach
    void setUp() {
        healthIndicator = new AdapterHealthIndicator(registry);
    }

    @Nested
    @DisplayName("health()")
    class HealthMethod {

        @Test
        @DisplayName("should return UP when registry is healthy")
        void upWhenRegistryHealthy() {
            when(registry.isHealthy()).thenReturn(true);
            when(registry.getAdapterNames()).thenReturn(List.of("CCC", "ICCOA"));
            when(registry.getEnabledAdapters()).thenReturn(List.of(cccAdapter, iccoaAdapter));

            Health health = healthIndicator.health();

            assertThat(health.getStatus()).isEqualTo(Status.UP);
            assertThat(health.getDetails())
                .containsEntry("adapters", List.of("CCC", "ICCOA"))
                .containsEntry("enabled", 2);
        }

        @Test
        @DisplayName("should return DOWN when registry is unhealthy")
        void downWhenRegistryUnhealthy() {
            when(registry.isHealthy()).thenReturn(false);
            when(registry.getAdapterNames()).thenReturn(List.of("CCC"));
            when(registry.getEnabledAdapters()).thenReturn(List.of());

            Health health = healthIndicator.health();

            assertThat(health.getStatus()).isEqualTo(Status.DOWN);
            assertThat((String) health.getDetails().get("error"))
                .isEqualTo("No healthy adapters available");
        }

        @Test
        @DisplayName("should include adapter names and enabled count in details")
        void includesAdapterDetails() {
            when(registry.isHealthy()).thenReturn(true);
            when(registry.getAdapterNames()).thenReturn(List.of("CCC"));
            when(registry.getEnabledAdapters()).thenReturn(List.of(cccAdapter));

            Health health = healthIndicator.health();

            @SuppressWarnings("unchecked")
            var adapters = (List<String>) health.getDetails().get("adapters");
            assertThat(adapters).containsExactly("CCC");
            assertThat(health.getDetails()).containsEntry("enabled", 1);
        }

        @Test
        @DisplayName("should return DOWN with empty details when registry has no adapters")
        void downWhenNoAdapters() {
            when(registry.isHealthy()).thenReturn(false);
            when(registry.getAdapterNames()).thenReturn(List.of());
            when(registry.getEnabledAdapters()).thenReturn(List.of());

            Health health = healthIndicator.health();

            assertThat(health.getStatus()).isEqualTo(Status.DOWN);
            assertThat(health.getDetails())
                .containsEntry("adapters", List.of())
                .containsEntry("enabled", 0);
        }
    }
}
