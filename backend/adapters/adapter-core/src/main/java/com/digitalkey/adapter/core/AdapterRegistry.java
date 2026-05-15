package com.digitalkey.adapter.core;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Registry for managing multiple TSP adapters.
 * Provides centralized access and routing to protocol-specific adapters.
 */
@Component
public class AdapterRegistry {

    private static final Logger log = LoggerFactory.getLogger(AdapterRegistry.class);

    private final Map<String, TspAdapter> adapters = new ConcurrentHashMap<>();
    private final List<String> adapterOrder = new CopyOnWriteArrayList<>();
    private final AtomicInteger currentIndex = new AtomicInteger(0);

    // Optional: metrics collector
    private AdapterMetrics metrics;

    public void setMetrics(AdapterMetrics metrics) {
        this.metrics = metrics;
    }

    /**
     * Register an adapter.
     * @param adapter the adapter to register
     */
    public void register(TspAdapter adapter) {
        String name = adapter.getAdapterName();
        adapters.put(name, adapter);
        if (!adapterOrder.contains(name)) {
            adapterOrder.add(name);
        }
        log.info("Registered adapter: {}", name);
    }

    /**
     * Unregister an adapter.
     * @param adapterName the adapter name to unregister
     */
    public void unregister(String adapterName) {
        TspAdapter removed = adapters.remove(adapterName);
        adapterOrder.remove(adapterName);
        if (removed != null) {
            log.info("Unregistered adapter: {}", adapterName);
        }
    }

    /**
     * Get adapter by name.
     * @param name adapter name
     * @return the adapter or null if not found
     */
    public TspAdapter getAdapter(String name) {
        return adapters.get(name);
    }

    /**
     * Get all registered adapter names.
     */
    public List<String> getAdapterNames() {
        return new ArrayList<>(adapters.keySet());
    }

    /**
     * Get all registered adapters.
     */
    public Collection<TspAdapter> getAllAdapters() {
        return adapters.values();
    }

    /**
     * Get enabled adapters.
     */
    public List<TspAdapter> getEnabledAdapters() {
        return adapters.values().stream()
            .filter(TspAdapter::isEnabled)
            .toList();
    }

    /**
     * Round-robin to get next available adapter.
     * @return next adapter or null if none available
     */
    public TspAdapter getNextAdapter() {
        List<TspAdapter> enabled = getEnabledAdapters();
        if (enabled.isEmpty()) {
            return null;
        }
        int index = currentIndex.getAndIncrement() % enabled.size();
        return enabled.get(index);
    }

    /**
     * Get adapter by protocol name.
     * @param protocol protocol identifier (ccc, iccoa, icce)
     * @return matching adapter or null
     */
    public TspAdapter getAdapterByProtocol(String protocol) {
        String normalized = protocol.toLowerCase();
        return adapters.values().stream()
            .filter(a -> a.getClass().getSimpleName().toLowerCase().contains(normalized))
            .findFirst()
            .orElse(null);
    }

    /**
     * Enable an adapter.
     */
    public void enableAdapter(String name) {
        TspAdapter adapter = adapters.get(name);
        if (adapter instanceof AbstractTspAdapter abstractAdapter) {
            abstractAdapter.setEnabled(true);
        }
    }

    /**
     * Disable an adapter.
     */
    public void disableAdapter(String name) {
        TspAdapter adapter = adapters.get(name);
        if (adapter instanceof AbstractTspAdapter abstractAdapter) {
            abstractAdapter.setEnabled(false);
        }
    }

    /**
     * Check overall registry health.
     */
    public boolean isHealthy() {
        return adapters.values().stream().anyMatch(TspAdapter::healthCheck);
    }

    /**
     * Initialize all registered adapters.
     */
    @PostConstruct
    public void initializeAdapters() {
        log.info("Initializing all adapters...");
        adapters.values().forEach(adapter -> {
            try {
                adapter.initialize().join();
            } catch (Exception e) {
                log.error("Failed to initialize adapter {}: {}", adapter.getAdapterName(), e.getMessage());
            }
        });
    }

    /**
     * Shutdown all adapters.
     */
    @PreDestroy
    public void shutdownAdapters() {
        log.info("Shutting down all adapters...");
        adapters.values().forEach(adapter -> {
            try {
                adapter.shutdown().join();
            } catch (Exception e) {
                log.error("Error shutting down adapter {}: {}", adapter.getAdapterName(), e.getMessage());
            }
        });
    }
}
