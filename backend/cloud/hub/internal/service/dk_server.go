package service

import (
	"context"
	"fmt"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)

// ─── DK Server 接口 ───────────────────────────────────────────────────────
// 这是 DK Hub 调用车厂 DK Server 的契约接口。
// DK Server 可能部署在同进程(all-in-one)、同VPC(hub-only+gRPC)、或车厂内网。

// KeyRequest 请求 DK Server 签发一把钥匙
type KeyRequest struct {
	TokenID     string            // Hub 签发的 Token ID
	SubjectID   string            // 钥匙持有者（人或服务商）
	VehicleID   string            // 目标车辆
	Permissions []token.Permission // 权限位
	ExpiresAt   int64             // 过期时间戳
}

// KeyResponse DK Server 签发钥匙的结果
type KeyResponse struct {
	KeyID    string `json:"key_id"`
	Status   string `json:"status"` // issued | pending | rejected
	Message  string `json:"message,omitempty"`
}

// DKServer 是 Hub 与车厂 DK Server 之间的适配器接口
type DKServer interface {
	// IssueKey 请求 DK Server 签发一把离线可用的数字钥匙
	// Hub 调用此接口通知车厂 PKI/TSP 生成受限钥匙
	IssueKey(ctx context.Context, req *KeyRequest) (*KeyResponse, error)

	// RevokeKeyByToken 根据 Token ID 吊销已签发的钥匙
	// 车主吊销 Token 时，同时通知车端作废对应钥匙
	RevokeKeyByToken(ctx context.Context, tokenID string) error
}

// ─── 默认实现（同进程 all-in-one 模式） ────────────────────────────────────

// LocalDKServer 是 DK Hub 内嵌的简单 DK Server 实现
// 用于 all-in-one 部署模式，实际生产应替换为对接车厂 DK Server
type LocalDKServer struct{}

func NewLocalDKServer() *LocalDKServer {
	return &LocalDKServer{}
}

func (s *LocalDKServer) IssueKey(ctx context.Context, req *KeyRequest) (*KeyResponse, error) {
	// 同进程模式：直接返回模拟结果
	// 实现在此插入 DK Server / TSP / PKI 调用链
	keyID := fmt.Sprintf("key-%s-%d", req.VehicleID, time.Now().UnixMilli())

	return &KeyResponse{
		KeyID:   keyID,
		Status:  "issued",
		Message: "钥匙已通知车端",
	}, nil
}

func (s *LocalDKServer) RevokeKeyByToken(ctx context.Context, tokenID string) error {
	return nil // 同进程直接吊销
}


// ─── gRPC Server 实现（server-only 模式） ──────────────────────────────────

// GRPCDKServer 是 DK Server 的 gRPC 服务端
// 用于 server-only 模式：接受 Hub 的远程调用
type GRPCDKServer struct {
	// Unimplemented 将在 proto 编译后替换
	// pb.UnimplementedDKServerServer
	local *LocalDKServer
}

func NewGRPCDKServer() *GRPCDKServer {
	return &GRPCDKServer{local: NewLocalDKServer()}
}

// RegisterGRPCServer 将 DK Server 注册到 gRPC 服务器
func (s *GRPCDKServer) RegisterGRPCServer(srv interface{ RegisterService(desc, impl) }) {
	// TODO: proto 编译后取消注释
	// pb.RegisterDKServerServer(srv, s)
	_ = srv
}
