// Package-level documentation is in doc.go.
// This file defines key lifecycle types, records, and filter structures.
package oms

import "time"

// ── 钥匙生命周期状态 ──────────────────────────────────────────────────────
// 对标 银基 OMS: 全生命周期覆盖 售前 → 售中 → 售后

// KeyLifecycleState 钥匙生命周期状态
type KeyLifecycleState string

const (
	StateCreated   KeyLifecycleState = "created"     // 售前: 创建（数字钥匙在系统中注册）
	StatePrePaired KeyLifecycleState = "pre_paired"  // 售中: 预配对（完成蓝牙握手，等待用户确认）
	StatePaired    KeyLifecycleState = "paired"      // 售中: 配对完成（设备与钥匙绑定）
	StateActive    KeyLifecycleState = "active"      // 售后: 激活使用（钥匙可正常使用）
	StateSuspended KeyLifecycleState = "suspended"   // 售后: 暂停（临时禁用，不可恢复）
	StateRevoked   KeyLifecycleState = "revoked"     // 售后: 吊销（永久作废）
	StateDeleted   KeyLifecycleState = "deleted"     // 售后: 删除（记录保留，数据不可复现）
)

// ValidTransitions 定义允许的状态转换
// key: current state, value: set of allowed next states
var ValidTransitions = map[KeyLifecycleState]map[KeyLifecycleState]bool{
	StateCreated:   {StatePrePaired: true},
	StatePrePaired: {StatePaired: true, StateCreated: true}, // 预配对可回退到创建
	StatePaired:    {StateActive: true, StateRevoked: true}, // 配对后可激活或吊销
	StateActive:    {StateSuspended: true, StateRevoked: true},
	StateSuspended: {StateActive: true, StateRevoked: true}, // 暂停后可恢复或吊销
	StateRevoked:   {},                                       // 终态，不可逆转
	StateDeleted:   {},                                       // 终态，不可逆转
}

// IsValidNextState 检查状态转换是否合法
func IsValidNextState(from, to KeyLifecycleState) bool {
	nexts, ok := ValidTransitions[from]
	if !ok {
		return false
	}
	return nexts[to]
}

// IsTerminal 判断是否为终态
func IsTerminal(state KeyLifecycleState) bool {
	return state == StateRevoked || state == StateDeleted
}

// ── 数字钥匙记录 ──────────────────────────────────────────────────────────

// KeyRecord 数字钥匙完整生命周期记录
type KeyRecord struct {
	KeyID       string            `json:"key_id"`
	DeviceID    string            `json:"device_id"`              // 绑定的设备
	OwnerID     string            `json:"owner_id"`               // 车主/用户标识
	State       KeyLifecycleState `json:"state"`                  // 当前状态
	CreatedAt   time.Time         `json:"created_at"`             // 创建时间
	ActivatedAt *time.Time        `json:"activated_at,omitempty"` // 首次激活时间
	SuspendedAt *time.Time        `json:"suspended_at,omitempty"` // 最近暂停时间
	RevokedAt   *time.Time        `json:"revoked_at,omitempty"`   // 吊销时间
	DeletedAt   *time.Time        `json:"deleted_at,omitempty"`   // 删除时间
	Metadata    map[string]string `json:"metadata,omitempty"`     // 扩展属性（OEM 自定义）
}

// KeyFilter 钥匙查询筛选条件
type KeyFilter struct {
	OwnerID *string           `json:"owner_id,omitempty"`
	DeviceID *string          `json:"device_id,omitempty"`
	State   *KeyLifecycleState `json:"state,omitempty"`
	OemID   *string           `json:"oem_id,omitempty"` // 按 OEM 筛选
	Limit   int               `json:"limit,omitempty"`
	Offset  int               `json:"offset,omitempty"`
}

// ── 预置相关 ──────────────────────────────────────────────────────────────

// ProvisioningJob 预置任务
type ProvisioningJob struct {
	JobID     string             `json:"job_id"`
	KeyID     string             `json:"key_id"`
	OemID     string             `json:"oem_id"`
	ModelID   string             `json:"model_id"`    // 车型
	Status    ProvisioningStatus `json:"status"`      // 预置状态
	CreatedAt time.Time          `json:"created_at"`
	UpdatedAt time.Time          `json:"updated_at"`
	ExpiresAt *time.Time         `json:"expires_at,omitempty"` // 预置有效期
	Error     string             `json:"error,omitempty"`
	Metadata  map[string]string  `json:"metadata,omitempty"`
}

// ProvisioningStatus 预置任务状态
type ProvisioningStatus string

const (
	ProvPending    ProvisioningStatus = "pending"     // 等待预置
	ProvInProgress ProvisioningStatus = "in_progress" // 预置中
	ProvCompleted  ProvisioningStatus = "completed"   // 预置完成
	ProvFailed     ProvisioningStatus = "failed"      // 预置失败
	ProvExpired    ProvisioningStatus = "expired"     // 已过期
)

// ── 部署相关 ──────────────────────────────────────────────────────────────

// DeploymentRecord 部署记录
// 用于跟踪 OEM 侧钥匙系统升级/配置变更的发布过程
type DeploymentRecord struct {
	DeployID    string     `json:"deploy_id"`
	OemID       string     `json:"oem_id"`
	ModelID     string     `json:"model_id"`     // 车型
	Version     string     `json:"version"`      // 部署版本
	Status      string     `json:"status"`       // "planning", "in_progress", "completed", "rolled_back"
	StartedAt   time.Time  `json:"started_at"`
	CompletedAt *time.Time `json:"completed_at,omitempty"`
	Description string     `json:"description,omitempty"`
	RolloutPct  int        `json:"rollout_pct"`   // 灰度比例 0-100
	Metadata    map[string]string `json:"metadata,omitempty"`
}

// ── 监控相关 ──────────────────────────────────────────────────────────────

// UsageRecord 钥匙使用记录
type UsageRecord struct {
	RecordID   string    `json:"record_id"`
	KeyID      string    `json:"key_id"`
	DeviceID   string    `json:"device_id"`
	Action     string    `json:"action"`      // "unlock", "lock", "start", "trunk_open"
	Result     string    `json:"result"`      // "success", "failure"
	Timestamp  time.Time `json:"timestamp"`
	DurationMs int64     `json:"duration_ms"` // 操作耗时(毫秒)
	Extra      map[string]string `json:"extra,omitempty"`
}

// UsageStats 使用统计
type UsageStats struct {
	KeyID         string            `json:"key_id"`
	TotalActions  int64             `json:"total_actions"`
	SuccessCount  int64             `json:"success_count"`
	FailureCount  int64             `json:"failure_count"`
	ByAction      map[string]int64  `json:"by_action"`       // 按操作类型聚合
	AvgDurationMs float64           `json:"avg_duration_ms"` // 平均耗时
	FirstUsed     *time.Time        `json:"first_used,omitempty"`
	LastUsed      *time.Time        `json:"last_used,omitempty"`
	PeriodStart   time.Time         `json:"period_start"`
	PeriodEnd     time.Time         `json:"period_end"`
}
