// Stub for github.com/digitalkey/proto/dkcs — generated from dkcs.proto
// Build with: protoc --go_out=. --go_opt=paths=source_relative dkcs.proto
//go:build !ignore
// +build !ignore

package dkcs

import (
	"context"
	"fmt"
)

// ─── Key Messages ─────────────────────────────────────────────

type CreateKeyRequest struct {
	VehicleId   string
	UserId      string
	KeyType     string
	Permissions []string
}

type CreateKeyResponse struct {
	KeyId     string
	Secret    string
	Status    string
	CreatedAt int64
}

type ActivateKeyRequest struct {
	KeyId string
}

type ActivateKeyResponse struct {
	KeyId       string
	Status      string
	ActivatedAt int64
}

type RevokeKeyRequest struct {
	KeyId  string
	Reason string
}

type RevokeKeyResponse struct {
	KeyId     string
	Status    string
	RevokedAt int64
}

type GetKeyRequest struct {
	KeyId string
}

type GetKeyResponse struct {
	KeyId       string
	VehicleId   string
	UserId      string
	KeyType     string
	Status      string
	Permissions []string
	CreatedAt   int64
	ExpiresAt   int64
}

type ListKeysRequest struct {
	UserId    string
	VehicleId string
	Limit     uint32
	Offset    uint32
}

type ListKeysResponse struct {
	Keys []*KeyInfo
}

type KeyInfo struct {
	KeyId       string
	VehicleId   string
	UserId      string
	KeyType     string
	Status      string
	Permissions []string
	CreatedAt   int64
	ExpiresAt   int64
}

type ShareKeyRequest struct {
	KeyId       string
	ToUserId    string
	Permissions []string
}

type ShareKeyResponse struct {
	SharedKeyId string
	ShareCode   string
	ExpiresAt   int64
}

// ─── Vehicle Messages ─────────────────────────────────────────

type RegisterVehicleRequest struct {
	VehicleId string
	Model     string
	Year      int32
}

type RegisterVehicleResponse struct {
	VehicleId string
	Status    string
}

// ─── Command Messages ─────────────────────────────────────────

type SendCommandRequest struct {
	KeyId     string
	VehicleId string
	Command   string
}

type SendCommandResponse struct {
	RequestId string
	Status    string
	Timestamp int64
}

// ─── Event Messages ──────────────────────────────────────────

type LogEventRequest struct {
	KeyId     string
	VehicleId string
	EventType string
	Metadata  map[string]string
}

type LogEventResponse struct {
	EventId   string
	Timestamp int64
}

// ─── KeyService gRPC Interface (protoc-gen-go-grpc) ─────────────

type UnimplementedKeyServiceServer struct{}

func (UnimplementedKeyServiceServer) Server() {}

type KeyServiceServer interface {
	CreateKey(context.Context, *CreateKeyRequest) (*CreateKeyResponse, error)
	ActivateKey(context.Context, *ActivateKeyRequest) (*ActivateKeyResponse, error)
	RevokeKey(context.Context, *RevokeKeyRequest) (*RevokeKeyResponse, error)
	GetKey(context.Context, *GetKeyRequest) (*GetKeyResponse, error)
	ListKeys(context.Context, *ListKeysRequest) (*ListKeysResponse, error)
	ShareKey(context.Context, *ShareKeyRequest) (*ShareKeyResponse, error)
}

type KeyServiceClient interface {
	CreateKey(context.Context, *CreateKeyRequest) (*CreateKeyResponse, error)
}

// Error helper
func Errorf(code int, format string, args ...interface{}) error {
	return fmt.Errorf(format, args...)
}
