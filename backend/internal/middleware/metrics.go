package middleware

import (
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
)

var (
	// HTTPRequestsTotal HTTP请求总数
	HTTPRequestsTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "http_requests_total",
			Help: "Total number of HTTP requests",
		},
		[]string{"method", "path", "status"},
	)

	// HTTPRequestDuration HTTP请求延迟
	HTTPRequestDuration = prometheus.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "http_request_duration_seconds",
			Help:    "HTTP request latency in seconds",
			Buckets: prometheus.DefBuckets,
		},
		[]string{"method", "path"},
	)

	// HTTPResponseSize HTTP响应大小
	HTTPResponseSize = prometheus.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "http_response_size_bytes",
			Help:    "Size of HTTP responses in bytes",
			Buckets: prometheus.ExponentialBuckets(100, 10, 8),
		},
		[]string{"method", "path"},
	)

	// ActiveUsersGauge 活跃用户数
	ActiveUsersGauge = prometheus.NewGauge(
		prometheus.GaugeOpts{
			Name: "active_users_total",
			Help: "Total number of active users",
		},
	)

	// KeyOperationsTotal 钥匙操作总数
	KeyOperationsTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "key_operations_total",
			Help: "Total number of key operations",
		},
		[]string{"operation", "status"},
	)

	// VehicleOperationsTotal 车辆操作总数
	VehicleOperationsTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "vehicle_operations_total",
			Help: "Total number of vehicle operations",
		},
		[]string{"operation", "status"},
	)

	// WebsocketConnectionsGauge WebSocket连接数
	WebsocketConnectionsGauge = prometheus.NewGauge(
		prometheus.GaugeOpts{
			Name: "websocket_connections_active",
			Help: "Number of active WebSocket connections",
		},
	)

	// DatabaseQueriesTotal 数据库查询总数
	DatabaseQueriesTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "db_queries_total",
			Help: "Total number of database queries",
		},
		[]string{"operation", "table"},
	)

	// DatabaseQueryDuration 数据库查询延迟
	DatabaseQueryDuration = prometheus.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "db_query_duration_seconds",
			Help:    "Database query latency in seconds",
			Buckets: prometheus.DefBuckets,
		},
		[]string{"operation", "table"},
	)

	// CacheOperationsTotal 缓存操作总数
	CacheOperationsTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "cache_operations_total",
			Help: "Total number of cache operations",
		},
		[]string{"operation", "result"},
	)

	// OTAUpdatesTotal OTA更新总数
	OTAUpdatesTotal = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "ota_updates_total",
			Help: "Total number of OTA update operations",
		},
		[]string{"status", "device_type"},
	)
)

func init() {
	// 注册自定义指标
	prometheus.MustRegister(HTTPRequestsTotal)
	prometheus.MustRegister(HTTPRequestDuration)
	prometheus.MustRegister(HTTPResponseSize)
	prometheus.MustRegister(ActiveUsersGauge)
	prometheus.MustRegister(KeyOperationsTotal)
	prometheus.MustRegister(VehicleOperationsTotal)
	prometheus.MustRegister(WebsocketConnectionsGauge)
	prometheus.MustRegister(DatabaseQueriesTotal)
	prometheus.MustRegister(DatabaseQueryDuration)
	prometheus.MustRegister(CacheOperationsTotal)
	prometheus.MustRegister(OTAUpdatesTotal)

	// 注册Go运行时指标
	prometheus.MustRegister(collectors.NewGoCollector())
	prometheus.MustRegister(collectors.NewProcessCollector(collectors.ProcessCollectorOpts{}))
}

// PrometheusMiddleware 收集HTTP请求的Prometheus指标
func PrometheusMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()

		// 获取请求路径（使用处理器名称作为标签以避免基数爆炸）
		path := c.FullPath()
		if path == "" {
			path = "unknown"
		}

		// 处理请求
		c.Next()

		// 记录指标
		duration := time.Since(start).Seconds()
		status := strconv.Itoa(c.Writer.Status())
		method := c.Request.Method

		// 增加请求计数
		HTTPRequestsTotal.WithLabelValues(method, path, status).Inc()

		// 记录请求延迟
		HTTPRequestDuration.WithLabelValues(method, path).Observe(duration)

		// 记录响应大小
		HTTPResponseSize.WithLabelValues(method, path).Observe(float64(c.Writer.Size()))
	}
}

// SetActiveUsers 设置活跃用户数
func SetActiveUsers(count float64) {
	ActiveUsersGauge.Set(count)
}

// IncActiveUsers 增加活跃用户计数
func IncActiveUsers() {
	ActiveUsersGauge.Inc()
}

// DecActiveUsers 减少活跃用户计数
func DecActiveUsers() {
	ActiveUsersGauge.Dec()
}

// RecordKeyOperation 记录钥匙操作
func RecordKeyOperation(operation, status string) {
	KeyOperationsTotal.WithLabelValues(operation, status).Inc()
}

// RecordVehicleOperation 记录车辆操作
func RecordVehicleOperation(operation, status string) {
	VehicleOperationsTotal.WithLabelValues(operation, status).Inc()
}

// SetWebsocketConnections 设置WebSocket连接数
func SetWebsocketConnections(count float64) {
	WebsocketConnectionsGauge.Set(count)
}

// IncWebsocketConnections 增加WebSocket连接计数
func IncWebsocketConnections() {
	WebsocketConnectionsGauge.Inc()
}

// DecWebsocketConnections 减少WebSocket连接计数
func DecWebsocketConnections() {
	WebsocketConnectionsGauge.Dec()
}

// RecordDatabaseQuery 记录数据库查询
func RecordDatabaseQuery(operation, table string, duration float64) {
	DatabaseQueriesTotal.WithLabelValues(operation, table).Inc()
	DatabaseQueryDuration.WithLabelValues(operation, table).Observe(duration)
}

// RecordCacheOperation 记录缓存操作
func RecordCacheOperation(operation, result string) {
	CacheOperationsTotal.WithLabelValues(operation, result).Inc()
}

// RecordOTAUpdate 记录OTA更新
func RecordOTAUpdate(status, deviceType string) {
	OTAUpdatesTotal.WithLabelValues(status, deviceType).Inc()
}
