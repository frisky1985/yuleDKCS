package tracing

import (
	"context"
	"fmt"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/exporters/jaeger"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	semconv "go.opentelemetry.io/otel/semconv/v1.21.0"
	"go.opentelemetry.io/otel/trace"
)

var tracer trace.Tracer

// Config 追踪配置
type Config struct {
	ServiceName    string
	ServiceVersion string
	Environment    string
	JaegerEndpoint string // 例如: http://localhost:14268/api/traces
	Enabled        bool
}

// InitTracer 初始化分布式追踪
func InitTracer(cfg *Config) (*sdktrace.TracerProvider, error) {
	if !cfg.Enabled {
		// 返回无操作的 tracer provider
		return sdktrace.NewTracerProvider(), nil
	}

	// 创建 Jaeger 导出器
	exporter, err := jaeger.New(
		jaeger.WithCollectorEndpoint(
			jaeger.WithEndpoint(cfg.JaegerEndpoint),
		),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to create jaeger exporter: %w", err)
	}

	// 创建资源
	res, err := resource.New(
		context.Background(),
		resource.WithAttributes(
			semconv.ServiceName(cfg.ServiceName),
			semconv.ServiceVersion(cfg.ServiceVersion),
			attribute.String("environment", cfg.Environment),
		),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to create resource: %w", err)
	}

	// 创建 TracerProvider
	tp := sdktrace.NewTracerProvider(
		sdktrace.WithBatcher(exporter),
		sdktrace.WithResource(res),
		sdktrace.WithSampler(sdktrace.AlwaysSample()),
	)

	// 设置为全局 provider
	otel.SetTracerProvider(tp)
	tracer = tp.Tracer(cfg.ServiceName)

	return tp, nil
}

// Tracer 返回 tracer 实例
func Tracer() trace.Tracer {
	if tracer == nil {
		return otel.Tracer("yuledkcs")
	}
	return tracer
}

// StartSpan 开始一个新的 span
func StartSpan(ctx context.Context, name string, opts ...trace.SpanStartOption) (context.Context, trace.Span) {
	return Tracer().Start(ctx, name, opts...)
}

// SpanFromContext 从上下文获取 span
func SpanFromContext(ctx context.Context) trace.Span {
	return trace.SpanFromContext(ctx)
}

// AddEvent 添加事件到 span
func AddEvent(ctx context.Context, name string, attrs ...attribute.KeyValue) {
	span := SpanFromContext(ctx)
	if span != nil {
		span.AddEvent(name, trace.WithAttributes(attrs...))
	}
}

// SetAttributes 设置 span 属性
func SetAttributes(ctx context.Context, attrs ...attribute.KeyValue) {
	span := SpanFromContext(ctx)
	if span != nil {
		span.SetAttributes(attrs...)
	}
}

// RecordError 记录错误到 span
func RecordError(ctx context.Context, err error, opts ...trace.EventOption) {
	span := SpanFromContext(ctx)
	if span != nil {
		span.RecordError(err, opts...)
	}
}
