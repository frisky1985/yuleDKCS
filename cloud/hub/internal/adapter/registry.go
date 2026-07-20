package adapter

import (
	"context"
	"sync"

	"go.uber.org/zap"
	pb "github.com/digitalkey/hub/api/v1"
)

// Adapter 统一接口 - 所有手机厂商适配器实现此接口
type Adapter interface {
	// 厂商标识
	Vendor() string
	// 支持的协议
	Protocol() string
	// 密钥绑定
	BindKey(ctx context.Context, req *pb.BindKeyRequest) (*pb.BindKeyResponse, error)
	// 密钥解绑
	UnbindKey(ctx context.Context, keyID string) error
	// 密钥撤销通知
	RevokeNotify(ctx context.Context, keyID string, reason string) error
	// 密钥分享
	ShareKey(ctx context.Context, req *pb.CreateShareRequest) (*pb.CreateShareResponse, error)
	// 接收分享
	AcceptShare(ctx context.Context, req *pb.AcceptShareRequest) (*pb.AcceptShareResponse, error)
	// 推送通知
	Notify(ctx context.Context, userID string, notification *pb.VehicleStatusUpdate) error
	// 健康检查
	HealthCheck(ctx context.Context) (*pb.AdapterStatus, error)
}

// Registry 适配器注册中心
type Registry struct {
	mu        sync.RWMutex
	adapters  map[string]Adapter  // key: "vendor:protocol"
	logger    *zap.Logger
}

func NewRegistry(logger *zap.Logger) *Registry {
	return &Registry{
		adapters: make(map[string]Adapter),
		logger:   logger,
	}
}

func (r *Registry) Register(vendor, protocol string, a Adapter) {
	r.mu.Lock()
	defer r.mu.Unlock()
	key := vendor + ":" + protocol
	r.adapters[key] = a
	r.logger.Info("adapter registered",
		zap.String("vendor", vendor),
		zap.String("protocol", protocol),
	)
}

func (r *Registry) Get(vendor string, protocol string) (Adapter, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	key := vendor + ":" + protocol
	a, ok := r.adapters[key]
	return a, ok
}

// GetByVendor 查找厂商默认适配器
func (r *Registry) GetByVendor(vendor string) (Adapter, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	for k, a := range r.adapters {
		if a.Vendor() == vendor {
			r.mu.RUnlock()
			_ = k
			r.mu.RLock()
			return a, true
		}
	}
	return nil, false
}

func (r *Registry) ListStatus(ctx context.Context) []*pb.AdapterStatus {
	r.mu.RLock()
	defer r.mu.RUnlock()
	var statuses []*pb.AdapterStatus
	for _, a := range r.adapters {
		status, err := a.HealthCheck(ctx)
		if err != nil {
			status = &pb.AdapterStatus{
				Vendor:  a.Vendor(),
				Protocol: a.Protocol(),
				Healthy: false,
				ErrorMsg: err.Error(),
			}
		}
		statuses = append(statuses, status)
	}
	return statuses
}
