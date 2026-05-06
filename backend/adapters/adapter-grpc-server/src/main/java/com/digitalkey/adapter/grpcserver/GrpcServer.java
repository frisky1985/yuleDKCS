package com.digitalkey.adapter.grpcserver;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;

/**
 * gRPC Server application for Adapter Service.
 * This server exposes TSP adapter functionality to Go backend via gRPC.
 */
@SpringBootApplication(scanBasePackages = "com.digitalkey.adapter")
@ConditionalOnProperty(name = "adapter.grpc.enabled", havingValue = "true", matchIfMissing = true)
public class GrpcServer {

    private static final Logger log = LoggerFactory.getLogger(GrpcServer.class);

    public static void main(String[] args) {
        SpringApplication.run(GrpcServer.class, args);
    }

    @Bean
    public CommandLineRunner runner() {
        return args -> {
            log.info("=".repeat(60));
            log.info("DigitalKey Adapter gRPC Server Starting...");
            log.info("Java Version: {}", System.getProperty("java.version"));
            log.info("Server Port: 6565 (gRPC default)");
            log.info("=".repeat(60));
        };
    }
}
