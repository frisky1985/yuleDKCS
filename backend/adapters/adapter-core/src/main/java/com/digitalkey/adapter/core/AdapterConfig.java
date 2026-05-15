package com.digitalkey.adapter.core;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Configuration;

/**
 * Configuration properties for TSP adapters.
 * Supports enable/disable per adapter and protocol-specific settings.
 */
@Configuration
@EnableConfigurationProperties
public class AdapterConfig {

    /**
     * Global adapter configuration properties.
     */
    @ConfigurationProperties(prefix = "adapter")
    public static class AdapterProperties {

        /** Enable/disable all adapters */
        private boolean enabled = true;

        /** Default timeout in milliseconds */
        private long timeoutMs = 30000;

        /** Enable retry on failure */
        private boolean retryEnabled = true;

        /** Max retry attempts */
        private int maxRetries = 3;

        /** Per-adapter configuration */
        private CccProperties ccc = new CccProperties();
        private IccoaProperties iccoa = new IccoaProperties();
        private IcceProperties icce = new IcceProperties();

        // Getters and setters
        public boolean isEnabled() { return enabled; }
        public void setEnabled(boolean enabled) { this.enabled = enabled; }
        public long getTimeoutMs() { return timeoutMs; }
        public void setTimeoutMs(long timeoutMs) { this.timeoutMs = timeoutMs; }
        public boolean isRetryEnabled() { return retryEnabled; }
        public void setRetryEnabled(boolean retryEnabled) { this.retryEnabled = retryEnabled; }
        public int getMaxRetries() { return maxRetries; }
        public void setMaxRetries(int maxRetries) { this.maxRetries = maxRetries; }
        public CccProperties getCcc() { return ccc; }
        public void setCcc(CccProperties ccc) { this.ccc = ccc; }
        public IccoaProperties getIccoa() { return iccoa; }
        public void setIccoa(IccoaProperties iccoa) { this.iccoa = iccoa; }
        public IcceProperties getIcce() { return icce; }
        public void setIcce(IcceProperties icce) { this.icce = icce; }
    }

    /**
     * CCC联盟适配器配置
     */
    public static class CccProperties {
        private boolean enabled = true;
        private String apiUrl;
        private String clientId;
        private String clientSecret;
        private int connectionTimeout = 10000;
        private int readTimeout = 30000;

        public boolean isEnabled() { return enabled; }
        public void setEnabled(boolean enabled) { this.enabled = enabled; }
        public String getApiUrl() { return apiUrl; }
        public void setApiUrl(String apiUrl) { this.apiUrl = apiUrl; }
        public String getClientId() { return clientId; }
        public void setClientId(String clientId) { this.clientId = clientId; }
        public String getClientSecret() { return clientSecret; }
        public void setClientSecret(String clientSecret) { this.clientSecret = clientSecret; }
        public int getConnectionTimeout() { return connectionTimeout; }
        public void setConnectionTimeout(int connectionTimeout) { this.connectionTimeout = connectionTimeout; }
        public int getReadTimeout() { return readTimeout; }
        public void setReadTimeout(int readTimeout) { this.readTimeout = readTimeout; }
    }

    /**
     * ICCOA适配器配置
     */
    public static class IccoaProperties {
        private boolean enabled = true;
        private String apiUrl;
        private String appId;
        private String appSecret;
        private String region = "cn";

        public boolean isEnabled() { return enabled; }
        public void setEnabled(boolean enabled) { this.enabled = enabled; }
        public String getApiUrl() { return apiUrl; }
        public void setApiUrl(String apiUrl) { this.apiUrl = apiUrl; }
        public String getAppId() { return appId; }
        public void setAppId(String appId) { this.appId = appId; }
        public String getAppSecret() { return appSecret; }
        public void setAppSecret(String appSecret) { this.appSecret = appSecret; }
        public String getRegion() { return region; }
        public void setRegion(String region) { this.region = region; }
    }

    /**
     * ICCE适配器配置
     */
    public static class IcceProperties {
        private boolean enabled = true;
        private String apiUrl;
        private String apiKey;
        private String tenantId;

        public boolean isEnabled() { return enabled; }
        public void setEnabled(boolean enabled) { this.enabled = enabled; }
        public String getApiUrl() { return apiUrl; }
        public void setApiUrl(String apiUrl) { this.apiUrl = apiUrl; }
        public String getApiKey() { return apiKey; }
        public void setApiKey(String apiKey) { this.apiKey = apiKey; }
        public String getTenantId() { return tenantId; }
        public void setTenantId(String tenantId) { this.tenantId = tenantId; }
    }
}
