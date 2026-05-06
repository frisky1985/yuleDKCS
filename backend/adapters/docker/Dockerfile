# Multi-stage build for DigitalKey Adapter Service
FROM eclipse-temurin:17-jdk-alpine AS builder

WORKDIR /app

# Install Maven
RUN apk add --no-cache maven

# Copy Maven files
COPY pom.xml .
COPY adapter-core/pom.xml adapter-core/
COPY adapter-ccc/pom.xml adapter-ccc/
COPY adapter-iccoa/pom.xml adapter-iccoa/
COPY adapter-icce/pom.xml adapter-icce/
COPY adapter-grpc-server/pom.xml adapter-grpc-server/

# Download dependencies (cached layer)
RUN mvn dependency:go-offline -B

# Copy source code
COPY adapter-core/src adapter-core/src
COPY adapter-ccc/src adapter-ccc/src
COPY adapter-iccoa/src adapter-iccoa/src
COPY adapter-icce/src adapter-icce/src
COPY adapter-grpc-server/src adapter-grpc-server/src

# Build the application
RUN mvn package -DskipTests -B

# Runtime image
FROM eclipse-temurin:17-jre-alpine

# Security: non-root user
RUN addgroup -g 1001 -S appgroup && \
    adduser -u 1001 -S appuser -G appgroup

WORKDIR /app

# Create logs directory
RUN mkdir -p /app/logs && chown -R appuser:appgroup /app

# Copy built JAR
COPY --from=builder /app/adapter-grpc-server/target/*.jar app.jar

# Set ownership
RUN chown -R appuser:appgroup /app

# Switch to non-root user
USER appuser

# Environment variables
ENV JAVA_OPTS="-Xms256m -Xmx512m -XX:+UseG1GC"
ENV SPRING_PROFILES_ACTIVE=production

# Expose ports
EXPOSE 8080 6565

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD wget -q --spider http://localhost:8080/actuator/health || exit 1

# Run with Spring Boot
ENTRYPOINT ["sh", "-c", "java $JAVA_OPTS -jar app.jar"]
