/*
 * @file    key_tracking.go
 * @brief   KTS (Key Tracking Service) 数据模型
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-16
 *
 * @note    符合 CCC Digital Key R3 KTS (Key Tracking Service) 规范
 *          实现钥匙状态跟踪、使用记录、审计日志功能
 */

package model

import (
	"database/sql/driver"
	"encoding/json"
	"time"
)

/******************************************************************************
 * KTS 错误码
 ******************************************************************************/

type KTSErrorCode int

const (
	KTS_SUCCESS KTSErrorCode = 0
	KTS_ERROR_INVALID_PARAM KTSErrorCode = -700
	KTS_ERROR_KEY_NOT_FOUND KTSErrorCode = -701
	KTS_ERROR_TRACKING_DISABLED KTSErrorCode = -702
	KTS_ERROR_STORAGE_FAILED KTSErrorCode = -703
	KTS_ERROR_INVALID_STATE KTSErrorCode = -704
	KTS_ERROR_PERMISSION_DENIED KTSErrorCode = -705
)

/******************************************************************************
 * 钥匙状态定义
 ******************************************************************************/

type KeyStatus string

const (
	KeyStatusActive      KeyStatus = "active"      // 正常使用中
	KeyStatusSuspended   KeyStatus = "suspended"   // 已暂停
	KeyStatusRevoked     KeyStatus = "revoked"     // 已撤销
	KeyStatusExpired     KeyStatus = "expired"     // 已过期
	KeyStatusPending     KeyStatus = "pending"     // 待激活
)

/******************************************************************************
 * 钥匙使用类型
 ******************************************************************************/

type KeyUsageType string

const (
	KeyUsageTypeUnlock      KeyUsageType = "unlock"      // 解锁
	KeyUsageTypeLock        KeyUsageType = "lock"        // 上锁
	KeyUsageTypeStartEngine KeyUsageType = "start"       // 启动发动机
	KeyUsageTypeShare       KeyUsageType = "share"       // 分享钥匙
	KeyUsageTypeRevoke      KeyUsageType = "revoke"      // 撤销钥匙
	KeyUsageTypeUpdate      KeyUsageType = "update"      // 更新钥匙
	KeyUsageTypePairing     KeyUsageType = "pairing"     // 配对
	KeyUsageTypeRanging     KeyUsageType = "ranging"     // UWB测距
)

/******************************************************************************
 * 异常行为类型
 ******************************************************************************/

type AnomalyType string

const (
	AnomalyTypeRapidUsage       AnomalyType = "rapid_usage"       // 频繁使用
	AnomalyTypeUnusualLocation  AnomalyType = "unusual_location"  // 异常位置
	AnomalyTypeUnauthorizedUse  AnomalyType = "unauthorized_use"  // 未授权使用
	AnomalyTypeReplayAttack     AnomalyType = "replay_attack"     // 重放攻击
	AnomalyTypeOutsideGeofence  AnomalyType = "outside_geofence"  // 越出地理围栏
)

/******************************************************************************
 * 钥匙跟踪配置
 ******************************************************************************/
type KeyTrackingConfig struct {
	ID              int64     `json:"id" db:"id"`
	KeyID           string    `json:"key_id" db:"key_id"`
	Enabled         bool      `json:"enabled" db:"enabled"`
	TrackLocation   bool      `json:"track_location" db:"track_location"`
	TrackUsage      bool      `json:"track_usage" db:"track_usage"`
	GeofenceEnabled bool      `json:"geofence_enabled" db:"geofence_enabled"`
	GeofenceCenter  *Location `json:"geofence_center,omitempty" db:"geofence_center"`
	GeofenceRadius  float64   `json:"geofence_radius" db:"geofence_radius"` // 米
	AnomalyDetectionEnabled bool `json:"anomaly_detection_enabled" db:"anomaly_detection_enabled"`
	MaxUsagePerHour int       `json:"max_usage_per_hour" db:"max_usage_per_hour"`
	CreatedAt       time.Time `json:"created_at" db:"created_at"`
	UpdatedAt       time.Time `json:"updated_at" db:"updated_at"`
}

/******************************************************************************
 * 地理位置
 ******************************************************************************/
type Location struct {
	Latitude  float64 `json:"latitude" db:"latitude"`
	Longitude float64 `json:"longitude" db:"longitude"`
	Accuracy  float64 `json:"accuracy" db:"accuracy"` // 精度(米)
	Timestamp int64   `json:"timestamp" db:"timestamp"`
}

func (l Location) Value() (driver.Value, error) {
	return json.Marshal(l)
}

func (l *Location) Scan(value interface{}) error {
	if value == nil {
		return nil
	}
	bytes, ok := value.([]byte)
	if !ok {
		return nil
	}
	return json.Unmarshal(bytes, l)
}

/******************************************************************************
 * 钥匙状态记录
 ******************************************************************************/
type KeyStatusRecord struct {
	ID          int64      `json:"id" db:"id"`
	KeyID       string     `json:"key_id" db:"key_id"`
	Status      KeyStatus  `json:"status" db:"status"`
	PrevStatus  KeyStatus  `json:"prev_status" db:"prev_status"`
	ChangedBy   string     `json:"changed_by" db:"changed_by"` // user_id, system, admin
	Reason      string     `json:"reason" db:"reason"`
	DeviceID    string     `json:"device_id" db:"device_id"`
	CreatedAt   time.Time  `json:"created_at" db:"created_at"`
}

/******************************************************************************
 * 钥匙使用记录
 ******************************************************************************/
type KeyUsageRecord struct {
	ID              int64        `json:"id" db:"id"`
	KeyID           string       `json:"key_id" db:"key_id"`
	UsageType       KeyUsageType `json:"usage_type" db:"usage_type"`
	VehicleID       string       `json:"vehicle_id" db:"vehicle_id"`
	DeviceID        string       `json:"device_id" db:"device_id"`
	UserID          string       `json:"user_id" db:"user_id"`
	Location        *Location    `json:"location,omitempty" db:"location"`
	Success         bool         `json:"success" db:"success"`
	ErrorCode       int          `json:"error_code,omitempty" db:"error_code"`
	ErrorMessage    string       `json:"error_message,omitempty" db:"error_message"`
	SessionID       string       `json:"session_id" db:"session_id"`
	Protocol        string       `json:"protocol" db:"protocol"` // CCC/ICCOA/ICCE
	RangingDistance float64      `json:"ranging_distance,omitempty" db:"ranging_distance"` // 测距结果
	Metadata        JSONMap      `json:"metadata,omitempty" db:"metadata"`
	CreatedAt       time.Time    `json:"created_at" db:"created_at"`
}

/******************************************************************************
 * 异常事件记录
 ******************************************************************************/
type AnomalyEvent struct {
	ID            int64       `json:"id" db:"id"`
	KeyID         string      `json:"key_id" db:"key_id"`
	UserID        string      `json:"user_id" db:"user_id"`
	AnomalyType   AnomalyType `json:"anomaly_type" db:"anomaly_type"`
	Severity      int         `json:"severity" db:"severity"` // 1-5
	Description   string      `json:"description" db:"description"`
	Location      *Location   `json:"location,omitempty" db:"location"`
	Evidence      JSONMap     `json:"evidence,omitempty" db:"evidence"` // 证据数据
	Status        string      `json:"status" db:"status"` // new, investigating, resolved, false_positive
	ResolvedBy    string      `json:"resolved_by,omitempty" db:"resolved_by"`
	ResolvedAt    *time.Time  `json:"resolved_at,omitempty" db:"resolved_at"`
	Resolution    string      `json:"resolution,omitempty" db:"resolution"`
	CreatedAt     time.Time   `json:"created_at" db:"created_at"`
}

/******************************************************************************
 * 钥匙计数器 (用于异常检测)
 ******************************************************************************/
type KeyUsageCounter struct {
	ID          int64     `json:"id" db:"id"`
	KeyID       string    `json:"key_id" db:"key_id"`
	HourWindow  time.Time `json:"hour_window" db:"hour_window"` // 小时窗口
	UsageCount  int       `json:"usage_count" db:"usage_count"`
	SuccessCount int      `json:"success_count" db:"success_count"`
	FailCount   int       `json:"fail_count" db:"fail_count"`
	CreatedAt   time.Time `json:"created_at" db:"created_at"`
	UpdatedAt   time.Time `json:"updated_at" db:"updated_at"`
}

/******************************************************************************
 * 钥匙实时状态 (缓存)
 ******************************************************************************/
type KeyRealtimeStatus struct {
	KeyID           string      `json:"key_id" db:"key_id"`
	CurrentStatus   KeyStatus   `json:"current_status" db:"current_status"`
	LastUsageTime   *time.Time  `json:"last_usage_time,omitempty" db:"last_usage_time"`
	LastUsageType   KeyUsageType `json:"last_usage_type,omitempty" db:"last_usage_type"`
	LastLocation    *Location   `json:"last_location,omitempty" db:"last_location"`
	TodayUsageCount int         `json:"today_usage_count" db:"today_usage_count"`
	TotalUsageCount int64       `json:"total_usage_count" db:"total_usage_count"`
	ConsecutiveFailures int     `json:"consecutive_failures" db:"consecutive_failures"`
	UpdatedAt       time.Time   `json:"updated_at" db:"updated_at"`
}

/******************************************************************************
 * JSONMap 类型 (用于 PostgreSQL jsonb)
 ******************************************************************************/
type JSONMap map[string]interface{}

func (m JSONMap) Value() (driver.Value, error) {
	if m == nil {
		return nil, nil
	}
	return json.Marshal(m)
}

func (m *JSONMap) Scan(value interface{}) error {
	if value == nil {
		*m = nil
		return nil
	}
	bytes, ok := value.([]byte)
	if !ok {
		return nil
	}
	return json.Unmarshal(bytes, m)
}

/******************************************************************************
 * 查询参数
 ******************************************************************************/
type KeyUsageQuery struct {
	KeyID       string
	UserID      string
	VehicleID   string
	UsageTypes  []KeyUsageType
	StartTime   *time.Time
	EndTime     *time.Time
	SuccessOnly bool
	Limit       int
	Offset      int
}

type AnomalyQuery struct {
	KeyID       string
	UserID      string
	AnomalyTypes []AnomalyType
	Status      string
	MinSeverity int
	StartTime   *time.Time
	EndTime     *time.Time
	Limit       int
	Offset      int
}
