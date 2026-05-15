package com.digitalkey.adapter.core;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.scheduling.annotation.EnableAsync;

/**
 * Adapter Core Application.
 * Main entry point for the adapter service.
 */
@SpringBootApplication
@EnableConfigurationProperties(AdapterConfig.AdapterProperties.class)
@EnableAsync
public class AdapterCoreApplication {

    public static void main(String[] args) {
        SpringApplication.run(AdapterCoreApplication.class, args);
    }
}
