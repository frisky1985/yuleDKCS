package com.digitalkey.adapter.core;

import org.springframework.boot.actuate.health.Health;
import org.springframework.boot.actuate.health.HealthIndicator;
import org.springframework.stereotype.Component;

/**
 * Health indicator for adapter registry.
 * Reports overall adapter health to Spring Boot Actuator.
 */
@Component
public class AdapterHealthIndicator implements HealthIndicator {

    private final AdapterRegistry registry;

    public AdapterHealthIndicator(AdapterRegistry registry) {
        this.registry = registry;
    }

    @Override
    public Health health() {
        boolean healthy = registry.isHealthy();
        
        if (healthy) {
            return Health.up()
                .withDetail("adapters", registry.getAdapterNames())
                .withDetail("enabled", registry.getEnabledAdapters().size())
                .build();
        } else {
            return Health.down()
                .withDetail("adapters", registry.getAdapterNames())
                .withDetail("enabled", registry.getEnabledAdapters().size())
                .withDetail("error", "No healthy adapters available")
                .build();
        }
    }
}
