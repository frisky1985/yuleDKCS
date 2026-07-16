package oms

import (
	"context"
	"errors"
	"time"
)

// ── 生命周期管理器接口 ──────────────────────────────────────────────────────
// 对标银基 OMS: 钥匙终态管理核心模块
// 负责从创建到删除的全生命周期控制

var (
	ErrKeyNotFound         = errors.New("oms: key not found")
	ErrInvalidTransition   = errors.New("oms: invalid state transition")
	ErrKeyAlreadyExists    = errors.New("oms: key already exists")
	ErrTerminalState       = errors.New("oms: key is in terminal state, no transitions allowed")
)

// LifecycleManager 生命周期管理器
// 提供数字钥匙的创建、状态转换、查询和列表能力。
type LifecycleManager interface {
	// CreateKey 创建一个新的数字钥匙记录
	CreateKey(ctx context.Context, key KeyRecord) error

	// TransitionState 执行安全的状态转换
	// from: 期望的当前状态（乐观锁），to: 目标状态
	// 如果实际状态与 from 不匹配，返回 ErrInvalidTransition
	TransitionState(ctx context.Context, keyID string, from, to KeyLifecycleState) error

	// GetKey 查询单个钥匙的完整生命周期记录
	GetKey(ctx context.Context, keyID string) (*KeyRecord, error)

	// ListKeys 按筛选条件查询钥匙列表
	ListKeys(ctx context.Context, filter KeyFilter) ([]KeyRecord, error)
}

// ── 状态转换辅助函数 ──────────────────────────────────────────────────────

// EnsureTransition 验证并执行状态转换的安全检查
// 这是生命周期管理的核心安全检查逻辑，所有实现都应调用此函数。
func EnsureTransition(key *KeyRecord, from, to KeyLifecycleState) error {
	if key == nil {
		return ErrKeyNotFound
	}

	// 检查当前状态是否匹配期望状态（乐观锁）
	if key.State != from {
		return ErrInvalidTransition
	}

	// 检查是否为合法的状态转换
	if !IsValidNextState(from, to) {
		return ErrInvalidTransition
	}

	// 检查转向终态的合法性
	if IsTerminal(to) && to != StateDeleted {
		// Revoked 需要记录吊销时间
		now := time.Now()
		key.RevokedAt = &now
	}

	return nil
}
