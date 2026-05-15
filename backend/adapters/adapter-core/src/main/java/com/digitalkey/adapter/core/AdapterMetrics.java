package com.digitalkey.adapter.core;

import io.micrometer.core.instrument.*;
import org.springframework.stereotype.Component;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Metrics collector for adapter operations.
 * Integrates with Micrometer/Prometheus for monitoring.
 */
@Component
public class AdapterMetrics {

    private final MeterRegistry meterRegistry;
    private final ConcurrentHashMap<String, Counter> requestCounters = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, Counter> errorCounters = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, Timer> latencyTimers = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String, AtomicLong> activeRequests = new ConcurrentHashMap<>();

    public AdapterMetrics(MeterRegistry meterRegistry) {
        this.meterRegistry = meterRegistry;
    }

    /**
     * Record a request start.
     */
    public void recordRequestStart(String adapterName, String operation) {
        String key = adapterName + "_" + operation;
        activeRequests.computeIfAbsent(key, k -> {
            AtomicLong gauge = new AtomicLong(0);
            Gauge.builder("adapter.active_requests", gauge, AtomicLong::get)
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .description("Number of active requests")
                .register(meterRegistry);
            return gauge;
        }).incrementAndGet();
    }

    /**
     * Record a successful request.
     */
    public void recordSuccess(String adapterName, String operation, long durationMs) {
        String key = adapterName + "_" + operation;
        
        // Increment success counter
        Counter successCounter = requestCounters.computeIfAbsent(key + "_success", k ->
            Counter.builder("adapter.requests")
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .tag("status", "success")
                .description("Total number of adapter requests")
                .register(meterRegistry)
        );
        successCounter.increment();

        // Record latency
        Timer latencyTimer = latencyTimers.computeIfAbsent(key, k ->
            Timer.builder("adapter.latency")
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .description("Adapter request latency")
                .register(meterRegistry)
        );
        latencyTimer.record(durationMs, TimeUnit.MILLISECONDS);

        // Decrement active requests
        AtomicLong active = activeRequests.get(key);
        if (active != null) {
            active.decrementAndGet();
        }
    }

    /**
     * Record a failed request.
     */
    public void recordFailure(String adapterName, String operation, long durationMs, String errorType) {
        String key = adapterName + "_" + operation;
        
        // Increment error counter
        Counter errorCounter = errorCounters.computeIfAbsent(key + "_error", k ->
            Counter.builder("adapter.errors")
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .tag("error_type", errorType)
                .description("Total number of adapter errors")
                .register(meterRegistry)
        );
        errorCounter.increment();

        // Also count as request
        Counter requestCounter = requestCounters.computeIfAbsent(key + "_failure", k ->
            Counter.builder("adapter.requests")
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .tag("status", "failure")
                .description("Total number of adapter requests")
                .register(meterRegistry)
        );
        requestCounter.increment();

        // Record latency
        Timer latencyTimer = latencyTimers.computeIfAbsent(key, k ->
            Timer.builder("adapter.latency")
                .tag("adapter", adapterName)
                .tag("operation", operation)
                .description("Adapter request latency")
                .register(meterRegistry)
        );
        latencyTimer.record(durationMs, TimeUnit.MILLISECONDS);

        // Decrement active requests
        AtomicLong active = activeRequests.get(key);
        if (active != null) {
            active.decrementAndGet();
        }
    }

    /**
     * Get current metrics snapshot.
     */
    public MetricsSnapshot getSnapshot() {
        return new MetricsSnapshot(
            requestCounters.values().stream().mapToDouble(Counter::count).sum(),
            errorCounters.values().stream().mapToDouble(Counter::count).sum(),
            activeRequests.entrySet().stream()
                .mapToLong(e -> e.getValue().get())
                .sum()
        );
    }

    public record MetricsSnapshot(double totalRequests, double totalErrors, long activeRequests) {}
}
