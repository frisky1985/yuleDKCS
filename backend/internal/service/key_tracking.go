/*
 * @file    key_tracking.go
 * @brief   KTS (Key Tracking Service) 服务层实现
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-16
 */

package service

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"math"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/internal/model"
	"github.com/google/uuid"
	"go.uber.org/zap"
)

/******************************************************************************
 * KTS 服务接口
 ******************************************************************************/
type KeyTrackingService interface {
	// 配置管理
	EnableTracking(ctx context.Context, keyID string, config *model.KeyTrackingConfig) error
	DisableTracking(ctx context.Context, keyID string) error
	GetTrackingConfig(ctx context.Context, keyID string) (*model.KeyTrackingConfig, error)
	UpdateTrackingConfig(ctx context.Context, keyID string, config *model.KeyTrackingConfig) error

	// 状态跟踪
	RecordStatusChange(ctx context.Context, keyID string, newStatus model.KeyStatus, reason string, deviceID string, changedBy string) error
	GetCurrentStatus(ctx context.Context, keyID string) (*model.KeyStatusRecord, error)
	GetStatusHistory(ctx context.Context, keyID string, limit int) ([]*model.KeyStatusRecord, error)

	// 使用记录
	RecordUsage(ctx context.Context, record *model.KeyUsageRecord) error
	GetUsageHistory(ctx context.Context, query model.KeyUsageQuery) ([]*model.KeyUsageRecord, int64, error)
	GetUsageStatistics(ctx context.Context, keyID string, days int) (*UsageStatistics, error)

	// 异常检测
	DetectAnomalies(ctx context.Context, record *model.KeyUsageRecord) ([]*model.AnomalyEvent, error)
	GetAnomalyEvents(ctx context.Context, query model.AnomalyQuery) ([]*model.AnomalyEvent, int64, error)
	ResolveAnomaly(ctx context.Context, anomalyID int64, status string, resolution string, resolvedBy string) error

	// 实时状态
	GetRealtimeStatus(ctx context.Context, keyID string) (*model.KeyRealtimeStatus, error)
	GetActiveKeysStatus(ctx context.Context, userID string) ([]*model.KeyRealtimeStatus, error)

	// 审计报告
	GenerateAuditReport(ctx context.Context, keyID string, startTime, endTime time.Time) (*AuditReport, error)
}

/******************************************************************************
 * 使用统计
 ******************************************************************************/
type UsageStatistics struct {
	KeyID               string                 `json:"key_id"`
	Period              string                 `json:"period"`
	TotalUsage          int64                  `json:"total_usage"`
	SuccessfulUsage     int64                  `json:"successful_usage"`
	FailedUsage         int64                  `json:"failed_usage"`
	UsageByType         map[string]int64       `json:"usage_by_type"`
	UsageByProtocol     map[string]int64       `json:"usage_by_protocol"`
	AverageRangingDist  float64                `json:"average_ranging_distance"`
	MostActiveHour      int                    `json:"most_active_hour"`
	ConsecutiveFailures int                    `json:"consecutive_failures"`
	AnomalyCount        int                    `json:"anomaly_count"`
}

/******************************************************************************
 * 审计报告
 ******************************************************************************/
type AuditReport struct {
	KeyID          string                  `json:"key_id"`
	ReportPeriod   string                  `json:"report_period"`
	GeneratedAt    time.Time               `json:"generated_at"`
	StatusHistory  []*model.KeyStatusRecord `json:"status_history"`
	UsageSummary   *UsageStatistics         `json:"usage_summary"`
	Anomalies      []*model.AnomalyEvent    `json:"anomalies"`
	GeofenceBreaches int                   `json:"geofence_breaches"`
	RiskAssessment string                  `json:"risk_assessment"`
}

/******************************************************************************
 * KTS 服务实现
 ******************************************************************************/
type keyTrackingService struct {
	db     *sql.DB
	logger *zap.Logger
}

// sqlQuerier 抽象出 *sql.DB 和 *sql.Tx 的公共方法，用于事务内复用
type sqlQuerier interface {
	ExecContext(ctx context.Context, query string, args ...interface{}) (sql.Result, error)
	QueryRowContext(ctx context.Context, query string, args ...interface{}) *sql.Row
	QueryContext(ctx context.Context, query string, args ...interface{}) (*sql.Rows, error)
}

// NewKeyTrackingService 创建 KTS 服务实例
func NewKeyTrackingService(db *sql.DB, logger *zap.Logger) KeyTrackingService {
	return &keyTrackingService{
		db:     db,
		logger: logger,
	}
}

/******************************************************************************
 * 配置管理
 ******************************************************************************/
// EnableTracking 启用钥匙跟踪，可配置跟踪参数
func (s *keyTrackingService) EnableTracking(ctx context.Context, keyID string, config *model.KeyTrackingConfig) error {
	if config == nil {
		config = &model.KeyTrackingConfig{
			Enabled:                 true,
			TrackLocation:           false,
			TrackUsage:              true,
			GeofenceEnabled:         false,
			AnomalyDetectionEnabled: true,
			MaxUsagePerHour:         30,
		}
	}

	config.KeyID = keyID
	config.CreatedAt = time.Now()
	config.UpdatedAt = time.Now()

	query := `
		INSERT INTO key_tracking_configs 
		(key_id, enabled, track_location, track_usage, geofence_enabled, geofence_center, 
		 geofence_radius, anomaly_detection_enabled, max_usage_per_hour, created_at, updated_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
		ON CONFLICT (key_id) DO UPDATE SET
			enabled = EXCLUDED.enabled,
			track_location = EXCLUDED.track_location,
			track_usage = EXCLUDED.track_usage,
			geofence_enabled = EXCLUDED.geofence_enabled,
			geofence_center = EXCLUDED.geofence_center,
			geofence_radius = EXCLUDED.geofence_radius,
			anomaly_detection_enabled = EXCLUDED.anomaly_detection_enabled,
			max_usage_per_hour = EXCLUDED.max_usage_per_hour,
			updated_at = EXCLUDED.updated_at
	`

	_, err := s.db.ExecContext(ctx, query,
		config.KeyID, config.Enabled, config.TrackLocation, config.TrackUsage,
		config.GeofenceEnabled, config.GeofenceCenter, config.GeofenceRadius,
		config.AnomalyDetectionEnabled, config.MaxUsagePerHour, config.CreatedAt, config.UpdatedAt)

	if err != nil {
		s.logger.Error("Failed to enable tracking", zap.String("key_id", keyID), zap.Error(err))
		return fmt.Errorf("failed to enable tracking: %w", err)
	}

	s.logger.Info("Tracking enabled", zap.String("key_id", keyID))
	return nil
}

// DisableTracking 禁用钥匙跟踪
func (s *keyTrackingService) DisableTracking(ctx context.Context, keyID string) error {
	query := `UPDATE key_tracking_configs SET enabled = false, updated_at = $1 WHERE key_id = $2`
	_, err := s.db.ExecContext(ctx, query, time.Now(), keyID)
	if err != nil {
		return fmt.Errorf("failed to disable tracking: %w", err)
	}
	return nil
}

// GetTrackingConfig 获取钥匙跟踪配置
func (s *keyTrackingService) GetTrackingConfig(ctx context.Context, keyID string) (*model.KeyTrackingConfig, error) {
	config := &model.KeyTrackingConfig{}
	query := `SELECT * FROM key_tracking_configs WHERE key_id = $1`
	row := s.db.QueryRowContext(ctx, query, keyID)
	err := row.Scan(&config.ID, &config.KeyID, &config.Enabled, &config.TrackLocation,
		&config.TrackUsage, &config.GeofenceEnabled, &config.GeofenceCenter,
		&config.GeofenceRadius, &config.AnomalyDetectionEnabled, &config.MaxUsagePerHour,
		&config.CreatedAt, &config.UpdatedAt)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("tracking config not found for key %s", keyID)
		}
		return nil, err
	}
	return config, nil
}

// UpdateTrackingConfig 更新钥匙跟踪配置
func (s *keyTrackingService) UpdateTrackingConfig(ctx context.Context, keyID string, config *model.KeyTrackingConfig) error {
	config.UpdatedAt = time.Now()
	query := `
		UPDATE key_tracking_configs SET
			enabled = $1,
			track_location = $2,
			track_usage = $3,
			geofence_enabled = $4,
			geofence_center = $5,
			geofence_radius = $6,
			anomaly_detection_enabled = $7,
			max_usage_per_hour = $8,
			updated_at = $9
		WHERE key_id = $10
	`
	_, err := s.db.ExecContext(ctx, query,
		config.Enabled, config.TrackLocation, config.TrackUsage,
		config.GeofenceEnabled, config.GeofenceCenter, config.GeofenceRadius,
		config.AnomalyDetectionEnabled, config.MaxUsagePerHour, config.UpdatedAt, keyID)
	return err
}

/******************************************************************************
 * 状态跟踪
 ******************************************************************************/
// RecordStatusChange 记录钥匙状态变更
func (s *keyTrackingService) RecordStatusChange(ctx context.Context, keyID string, newStatus model.KeyStatus, reason string, deviceID string, changedBy string) error {
	// 获取当前状态
	currentStatus, _ := s.GetCurrentStatus(ctx, keyID)
	
	prevStatus := model.KeyStatus("")
	if currentStatus != nil {
		prevStatus = currentStatus.Status
	}

	record := &model.KeyStatusRecord{
		KeyID:      keyID,
		Status:     newStatus,
		PrevStatus: prevStatus,
		ChangedBy:  changedBy,
		Reason:     reason,
		DeviceID:   deviceID,
		CreatedAt:  time.Now(),
	}

	query := `
		INSERT INTO key_status_records 
		(key_id, status, prev_status, changed_by, reason, device_id, created_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7)
	`
	_, err := s.db.ExecContext(ctx, query,
		record.KeyID, record.Status, record.PrevStatus, record.ChangedBy,
		record.Reason, record.DeviceID, record.CreatedAt)

	if err != nil {
		return fmt.Errorf("failed to record status change: %w", err)
	}

	// 更新实时状态缓存（合并统一方法）
	s.upsertRealtimeStatus(ctx, s.db, keyID, newStatus, nil, "", nil, -1, 0, 0)

	s.logger.Info("Status changed",
		zap.String("key_id", keyID),
		zap.String("from", string(prevStatus)),
		zap.String("to", string(newStatus)),
	)

	return nil
}

// GetCurrentStatus 获取钥匙当前状态
func (s *keyTrackingService) GetCurrentStatus(ctx context.Context, keyID string) (*model.KeyStatusRecord, error) {
	record := &model.KeyStatusRecord{}
	query := `
		SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at
		FROM key_status_records
		WHERE key_id = $1
		ORDER BY created_at DESC
		LIMIT 1
	`
	err := s.db.QueryRowContext(ctx, query, keyID).Scan(
		&record.ID, &record.KeyID, &record.Status, &record.PrevStatus,
		&record.ChangedBy, &record.Reason, &record.DeviceID, &record.CreatedAt)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, nil
		}
		return nil, err
	}
	return record, nil
}

// GetStatusHistory 获取钥匙状态变更历史
func (s *keyTrackingService) GetStatusHistory(ctx context.Context, keyID string, limit int) ([]*model.KeyStatusRecord, error) {
	if limit <= 0 || limit > 100 {
		limit = 50
	}

	query := `
		SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at
		FROM key_status_records
		WHERE key_id = $1
		ORDER BY created_at DESC
		LIMIT $2
	`
	rows, err := s.db.QueryContext(ctx, query, keyID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var records []*model.KeyStatusRecord
	for rows.Next() {
		record := &model.KeyStatusRecord{}
		err := rows.Scan(&record.ID, &record.KeyID, &record.Status, &record.PrevStatus,
			&record.ChangedBy, &record.Reason, &record.DeviceID, &record.CreatedAt)
		if err != nil {
			s.logger.Error("Failed to scan status history row", zap.Error(err))
			continue
		}
		records = append(records, record)
	}

	return records, nil
}

/******************************************************************************
 * 使用记录
 ******************************************************************************/
// RecordUsage 记录钥匙使用事件
func (s *keyTrackingService) RecordUsage(ctx context.Context, record *model.KeyUsageRecord) error {
	if record.SessionID == "" {
		record.SessionID = uuid.New().String()
	}
	record.CreatedAt = time.Now()

	// 使用事务包裹所有写入操作
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		s.logger.Error("Failed to begin transaction", zap.Error(err))
		return fmt.Errorf("failed to begin transaction: %w", err)
	}
	defer tx.Rollback()

	query := `
		INSERT INTO key_usage_records 
		(key_id, usage_type, vehicle_id, device_id, user_id, location, success, 
		 error_code, error_message, session_id, protocol, ranging_distance, metadata, created_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
		RETURNING id
	`
	var id int64
	err = tx.QueryRowContext(ctx, query,
		record.KeyID, record.UsageType, record.VehicleID, record.DeviceID, record.UserID,
		record.Location, record.Success, record.ErrorCode, record.ErrorMessage,
		record.SessionID, record.Protocol, record.RangingDistance, record.Metadata, record.CreatedAt,
	).Scan(&id)

	if err != nil {
		s.logger.Error("Failed to record usage", zap.Error(err))
		return fmt.Errorf("failed to record usage: %w", err)
	}

	record.ID = id

	// 更新计数器 (事务内)
	s.updateUsageCounter(ctx, tx, record)

	// 更新实时状态 (事务内)
	consecutiveFailures := 0
	if !record.Success {
		currentStatus, _ := s.GetRealtimeStatus(ctx, record.KeyID)
		if currentStatus != nil {
			consecutiveFailures = currentStatus.ConsecutiveFailures + 1
		} else {
			consecutiveFailures = 1
		}
	}
	s.upsertRealtimeStatus(ctx, tx, record.KeyID, "", &record.CreatedAt, record.UsageType,
		record.Location, consecutiveFailures, 1, 1)

	// 异常检测 (事务内)
	if anomalies, err := s.detectAnomaliesWithQuerier(ctx, tx, record); err == nil && len(anomalies) > 0 {
		for _, anomaly := range anomalies {
			s.logger.Warn("Anomaly detected",
				zap.String("key_id", record.KeyID),
				zap.String("type", string(anomaly.AnomalyType)),
			)
		}
	}

	// 提交事务
	if err := tx.Commit(); err != nil {
		s.logger.Error("Failed to commit transaction", zap.Error(err))
		return fmt.Errorf("failed to commit transaction: %w", err)
	}

	return nil
}

func (s *keyTrackingService) updateUsageCounter(ctx context.Context, q sqlQuerier, record *model.KeyUsageRecord) {
	hourWindow := record.CreatedAt.Truncate(time.Hour)

	query := `
		INSERT INTO key_usage_counters (key_id, hour_window, usage_count, success_count, fail_count, created_at, updated_at)
		VALUES ($1, $2, 1, $3, $4, $5, $5)
		ON CONFLICT (key_id, hour_window) DO UPDATE SET
			usage_count = key_usage_counters.usage_count + 1,
			success_count = key_usage_counters.success_count + EXCLUDED.success_count,
			fail_count = key_usage_counters.fail_count + EXCLUDED.fail_count,
			updated_at = EXCLUDED.updated_at
	`

	successCount := 0
	failCount := 0
	if record.Success {
		successCount = 1
	} else {
		failCount = 1
	}

	_, err := q.ExecContext(ctx, query, record.KeyID, hourWindow, successCount, failCount, time.Now())
	if err != nil {
		s.logger.Error("Failed to update usage counter", zap.Error(err))
	}
}

// GetUsageHistory 获取钥匙使用历史，支持分页和过滤
func (s *keyTrackingService) GetUsageHistory(ctx context.Context, query model.KeyUsageQuery) ([]*model.KeyUsageRecord, int64, error) {
	// 构建查询
	whereClause := "WHERE 1=1"
	var args []interface{}
	argIdx := 1

	if query.KeyID != "" {
		whereClause += fmt.Sprintf(" AND key_id = $%d", argIdx)
		args = append(args, query.KeyID)
		argIdx++
	}
	if query.UserID != "" {
		whereClause += fmt.Sprintf(" AND user_id = $%d", argIdx)
		args = append(args, query.UserID)
		argIdx++
	}
	if query.VehicleID != "" {
		whereClause += fmt.Sprintf(" AND vehicle_id = $%d", argIdx)
		args = append(args, query.VehicleID)
		argIdx++
	}
	if query.SuccessOnly {
		whereClause += fmt.Sprintf(" AND success = $%d", argIdx)
		args = append(args, true)
		argIdx++
	}
	if query.StartTime != nil {
		whereClause += fmt.Sprintf(" AND created_at >= $%d", argIdx)
		args = append(args, *query.StartTime)
		argIdx++
	}
	if query.EndTime != nil {
		whereClause += fmt.Sprintf(" AND created_at <= $%d", argIdx)
		args = append(args, *query.EndTime)
		argIdx++
	}

	// 计数
	countQuery := fmt.Sprintf("SELECT COUNT(*) FROM key_usage_records %s", whereClause)
	var total int64
	if err := s.db.QueryRowContext(ctx, countQuery, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	// 查询记录
	if query.Limit <= 0 || query.Limit > 1000 {
		query.Limit = 100
	}
	selectQuery := fmt.Sprintf(`
		SELECT id, key_id, usage_type, vehicle_id, device_id, user_id, location, 
		       success, error_code, error_message, session_id, protocol, 
		       ranging_distance, metadata, created_at
		FROM key_usage_records
		%s
		ORDER BY created_at DESC
		LIMIT $%d OFFSET $%d
	`, whereClause, argIdx, argIdx+1)
	args = append(args, query.Limit, query.Offset)

	rows, err := s.db.QueryContext(ctx, selectQuery, args...)
	if err != nil {
		return nil, 0, err
	}
	defer rows.Close()

	var records []*model.KeyUsageRecord
	for rows.Next() {
		record := &model.KeyUsageRecord{}
		err := rows.Scan(&record.ID, &record.KeyID, &record.UsageType, &record.VehicleID,
			&record.DeviceID, &record.UserID, &record.Location, &record.Success,
			&record.ErrorCode, &record.ErrorMessage, &record.SessionID, &record.Protocol,
			&record.RangingDistance, &record.Metadata, &record.CreatedAt)
		if err != nil {
			s.logger.Error("Failed to scan usage history row", zap.Error(err))
			continue
		}
		records = append(records, record)
	}

	return records, total, nil
}

/******************************************************************************
 * 异常检测
 ******************************************************************************/
// DetectAnomalies 检测钥匙使用异常
func (s *keyTrackingService) DetectAnomalies(ctx context.Context, record *model.KeyUsageRecord) ([]*model.AnomalyEvent, error) {
	return s.detectAnomaliesWithQuerier(ctx, s.db, record)
}

func (s *keyTrackingService) detectAnomaliesWithQuerier(ctx context.Context, q sqlQuerier, record *model.KeyUsageRecord) ([]*model.AnomalyEvent, error) {
	config, err := s.GetTrackingConfig(ctx, record.KeyID)
	if err != nil || !config.AnomalyDetectionEnabled {
		return nil, nil
	}

	var anomalies []*model.AnomalyEvent

	// 1. 检测频繁使用
	if config.MaxUsagePerHour > 0 {
		hourWindow := time.Now().Truncate(time.Hour)
		counter := &model.KeyUsageCounter{}
		query := `SELECT usage_count FROM key_usage_counters WHERE key_id = $1 AND hour_window = $2`
		err := q.QueryRowContext(ctx, query, record.KeyID, hourWindow).Scan(&counter.UsageCount)
		if err == nil && counter.UsageCount > config.MaxUsagePerHour {
			anomaly := s.createAnomalyEvent(record, model.AnomalyTypeRapidUsage, 3,
				fmt.Sprintf("Key used %d times in the last hour (limit: %d)", counter.UsageCount, config.MaxUsagePerHour))
			anomalies = append(anomalies, anomaly)
		}
	}

	// 2. 检测地理围栏违规
	if config.GeofenceEnabled && config.GeofenceCenter != nil && record.Location != nil {
		distance := calculateDistance(config.GeofenceCenter, record.Location)
		if distance > config.GeofenceRadius {
			anomaly := s.createAnomalyEvent(record, model.AnomalyTypeOutsideGeofence, 4,
				fmt.Sprintf("Key used outside geofence (distance: %.2f m, limit: %.2f m)", distance, config.GeofenceRadius))
			anomalies = append(anomalies, anomaly)
		}
	}

	// 3. 检测连续失败
	realtimeStatus, _ := s.GetRealtimeStatus(ctx, record.KeyID)
	if realtimeStatus != nil && realtimeStatus.ConsecutiveFailures >= 3 {
		anomaly := s.createAnomalyEvent(record, model.AnomalyTypeUnauthorizedUse, 4,
			fmt.Sprintf("Key has %d consecutive failures", realtimeStatus.ConsecutiveFailures))
		anomalies = append(anomalies, anomaly)
	}

	// 保存异常事件
	for _, anomaly := range anomalies {
		s.saveAnomalyEvent(ctx, q, anomaly)
	}

	return anomalies, nil
}

func (s *keyTrackingService) createAnomalyEvent(record *model.KeyUsageRecord, anomalyType model.AnomalyType, severity int, description string) *model.AnomalyEvent {
	return &model.AnomalyEvent{
		KeyID:       record.KeyID,
		UserID:      record.UserID,
		AnomalyType: anomalyType,
		Severity:    severity,
		Description: description,
		Location:    record.Location,
		Evidence: model.JSONMap{
			"usage_type":    record.UsageType,
			"device_id":     record.DeviceID,
			"vehicle_id":    record.VehicleID,
			"protocol":      record.Protocol,
			"session_id":    record.SessionID,
			"error_message": record.ErrorMessage,
		},
		Status:    "new",
		CreatedAt: time.Now(),
	}
}

func (s *keyTrackingService) saveAnomalyEvent(ctx context.Context, q sqlQuerier, event *model.AnomalyEvent) error {
	query := `
		INSERT INTO anomaly_events 
		(key_id, user_id, anomaly_type, severity, description, location, evidence, status, created_at)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
		RETURNING id
	`
	err := q.QueryRowContext(ctx, query,
		event.KeyID, event.UserID, event.AnomalyType, event.Severity,
		event.Description, event.Location, event.Evidence, event.Status, event.CreatedAt,
	).Scan(&event.ID)
	return err
}

func calculateDistance(loc1, loc2 *model.Location) float64 {
	// 使用Haversine公式计算两点间距离
	const R = 6371000 // 地球半径(米)

	lat1Rad := loc1.Latitude * math.Pi / 180
	lat2Rad := loc2.Latitude * math.Pi / 180
	deltaLat := (loc2.Latitude - loc1.Latitude) * math.Pi / 180
	deltaLon := (loc2.Longitude - loc1.Longitude) * math.Pi / 180

	a := math.Sin(deltaLat/2)*math.Sin(deltaLat/2) +
		math.Cos(lat1Rad)*math.Cos(lat2Rad)*
			math.Sin(deltaLon/2)*math.Sin(deltaLon/2)
	c := 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))

	return R * c
}

/******************************************************************************
 * 实时状态（合并统一方法，使用 COALESCE 避免并行覆盖）
 ******************************************************************************/
func (s *keyTrackingService) upsertRealtimeStatus(ctx context.Context, q sqlQuerier, keyID string,
	currentStatus model.KeyStatus,
	lastUsageTime *time.Time, lastUsageType model.KeyUsageType, lastLocation *model.Location,
	consecutiveFailures int, todayUsageCount int, totalUsageCount int) {

	query := `
		INSERT INTO key_realtime_status
		(key_id, current_status, last_usage_time, last_usage_type, last_location,
		 today_usage_count, total_usage_count, consecutive_failures, updated_at)
		VALUES ($1,
			CASE WHEN $2::varchar != '' THEN $2 ELSE 'active' END,
			$3,
			CASE WHEN $4::varchar != '' THEN $4 ELSE NULL END,
			$5,
			$6, $7,
			CASE WHEN $8 >= 0 THEN $8 ELSE 0 END,
			$9)
		ON CONFLICT (key_id) DO UPDATE SET
			current_status = CASE WHEN $2::varchar != '' THEN $2 ELSE key_realtime_status.current_status END,
			last_usage_time = COALESCE($3, key_realtime_status.last_usage_time),
			last_usage_type = CASE WHEN $4::varchar != '' THEN $4 ELSE key_realtime_status.last_usage_type END,
			last_location = COALESCE($5, key_realtime_status.last_location),
			today_usage_count = key_realtime_status.today_usage_count + $6,
			total_usage_count = key_realtime_status.total_usage_count + $7,
			consecutive_failures = CASE WHEN $8 >= 0 THEN $8 ELSE key_realtime_status.consecutive_failures END,
			updated_at = EXCLUDED.updated_at
	`
	_, err := q.ExecContext(ctx, query,
		keyID, currentStatus, lastUsageTime, lastUsageType, lastLocation,
		todayUsageCount, totalUsageCount, consecutiveFailures, time.Now())
	if err != nil {
		s.logger.Error("Failed to upsert realtime status", zap.Error(err))
	}
}

// GetRealtimeStatus 获取钥匙实时状态
func (s *keyTrackingService) GetRealtimeStatus(ctx context.Context, keyID string) (*model.KeyRealtimeStatus, error) {
	status := &model.KeyRealtimeStatus{}
	query := `
		SELECT key_id, current_status, last_usage_time, last_usage_type, last_location,
		       today_usage_count, total_usage_count, consecutive_failures, updated_at
		FROM key_realtime_status
		WHERE key_id = $1
	`
	err := s.db.QueryRowContext(ctx, query, keyID).Scan(
		&status.KeyID, &status.CurrentStatus, &status.LastUsageTime, &status.LastUsageType,
		&status.LastLocation, &status.TodayUsageCount, &status.TotalUsageCount,
		&status.ConsecutiveFailures, &status.UpdatedAt)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, nil
		}
		return nil, err
	}
	return status, nil
}

// GetActiveKeysStatus 获取用户所有活跃钥匙的状态
func (s *keyTrackingService) GetActiveKeysStatus(ctx context.Context, userID string) ([]*model.KeyRealtimeStatus, error) {
	query := `
		SELECT r.key_id, r.current_status, r.last_usage_time, r.last_usage_type, r.last_location,
		       r.today_usage_count, r.total_usage_count, r.consecutive_failures, r.updated_at
		FROM key_realtime_status r
		JOIN keys k ON r.key_id = k.key_id
		WHERE k.user_id = $1 AND k.status = 'active'
	`
	rows, err := s.db.QueryContext(ctx, query, userID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var statuses []*model.KeyRealtimeStatus
	for rows.Next() {
		status := &model.KeyRealtimeStatus{}
		err := rows.Scan(&status.KeyID, &status.CurrentStatus, &status.LastUsageTime,
			&status.LastUsageType, &status.LastLocation, &status.TodayUsageCount,
			&status.TotalUsageCount, &status.ConsecutiveFailures, &status.UpdatedAt)
		if err != nil {
			s.logger.Error("Failed to scan active key status row", zap.Error(err))
			continue
		}
		statuses = append(statuses, status)
	}

	return statuses, nil
}

/******************************************************************************
 * 审计报告
 ******************************************************************************/
// GenerateAuditReport 生成钥匙审计报告
func (s *keyTrackingService) GenerateAuditReport(ctx context.Context, keyID string, startTime, endTime time.Time) (*AuditReport, error) {
	report := &AuditReport{
		KeyID:        keyID,
		ReportPeriod: fmt.Sprintf("%s to %s", startTime.Format("2006-01-02"), endTime.Format("2006-01-02")),
		GeneratedAt:  time.Now(),
	}

	// 获取状态历史
	statusHistory, err := s.GetStatusHistory(ctx, keyID, 100)
	if err == nil {
		report.StatusHistory = statusHistory
	}

	// 获取使用统计
	days := int(endTime.Sub(startTime).Hours() / 24)
	if days <= 0 {
		days = 7
	}
	stats, err := s.GetUsageStatistics(ctx, keyID, days)
	if err == nil {
		report.UsageSummary = stats
	}

	// 获取异常事件
	anomalies, _, err := s.GetAnomalyEvents(ctx, model.AnomalyQuery{
		KeyID:     keyID,
		StartTime: &startTime,
		EndTime:   &endTime,
	})
	if err == nil {
		report.Anomalies = anomalies
	}

	// 风险评估
	report.RiskAssessment = s.assessRisk(report)

	return report, nil
}

func (s *keyTrackingService) assessRisk(report *AuditReport) string {
	// 简单的风险评估逻辑
	if len(report.Anomalies) > 5 {
		return "high"
	} else if len(report.Anomalies) > 0 {
		return "medium"
	}
	return "low"
}

// GetUsageStatistics 获取钥匙使用统计数据
func (s *keyTrackingService) GetUsageStatistics(ctx context.Context, keyID string, days int) (*UsageStatistics, error) {
	startTime := time.Now().AddDate(0, 0, -days)
	
	stats := &UsageStatistics{
		KeyID:           keyID,
		Period:          fmt.Sprintf("last %d days", days),
		UsageByType:     make(map[string]int64),
		UsageByProtocol: make(map[string]int64),
	}

	query := `
		SELECT usage_type, protocol, success, COUNT(*), AVG(ranging_distance)
		FROM key_usage_records
		WHERE key_id = $1 AND created_at >= $2
		GROUP BY usage_type, protocol, success
	`
	rows, err := s.db.QueryContext(ctx, query, keyID, startTime)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var totalDist float64
	var distCount int
	for rows.Next() {
		var usageType, protocol string
		var success bool
		var count int64
		var avgDist sql.NullFloat64
		if err := rows.Scan(&usageType, &protocol, &success, &count, &avgDist); err != nil {
			s.logger.Error("Failed to scan usage statistics row", zap.Error(err))
			continue
		}

		stats.TotalUsage += count
		if success {
			stats.SuccessfulUsage += count
		} else {
			stats.FailedUsage += count
		}
		stats.UsageByType[usageType] += count
		stats.UsageByProtocol[protocol] += count

		if avgDist.Valid {
			totalDist += avgDist.Float64
			distCount++
		}
	}

	if distCount > 0 {
		stats.AverageRangingDist = totalDist / float64(distCount)
	}

	// 查询最活跃小时 (MostActiveHour)
	activeHourQuery := `
		SELECT EXTRACT(HOUR FROM created_at)::int AS hour, COUNT(*) AS cnt
		FROM key_usage_records
		WHERE key_id = $1 AND created_at >= $2
		GROUP BY hour
		ORDER BY cnt DESC
		LIMIT 1
	`
	var mostActiveHour sql.NullInt64
	if err := s.db.QueryRowContext(ctx, activeHourQuery, keyID, startTime).Scan(&mostActiveHour); err == nil && mostActiveHour.Valid {
		stats.MostActiveHour = int(mostActiveHour.Int64)
	}

	// 查询连续失败次数 (ConsecutiveFailures)
	consecFailQuery := `
		SELECT consecutive_failures
		FROM key_realtime_status
		WHERE key_id = $1
	`
	var consecFail sql.NullInt64
	if err := s.db.QueryRowContext(ctx, consecFailQuery, keyID).Scan(&consecFail); err == nil && consecFail.Valid {
		stats.ConsecutiveFailures = int(consecFail.Int64)
	}

	// 查询异常事件数 (AnomalyCount)
	anomalyCountQuery := `
		SELECT COUNT(*) FROM anomaly_events
		WHERE key_id = $1 AND created_at >= $2
	`
	var anomalyCount sql.NullInt64
	if err := s.db.QueryRowContext(ctx, anomalyCountQuery, keyID, startTime).Scan(&anomalyCount); err == nil && anomalyCount.Valid {
		stats.AnomalyCount = int(anomalyCount.Int64)
	}

	return stats, nil
}

// GetAnomalyEvents 获取异常事件列表，支持分页和过滤
func (s *keyTrackingService) GetAnomalyEvents(ctx context.Context, query model.AnomalyQuery) ([]*model.AnomalyEvent, int64, error) {
	whereClause := "WHERE 1=1"
	var args []interface{}
	argIdx := 1

	if query.KeyID != "" {
		whereClause += fmt.Sprintf(" AND key_id = $%d", argIdx)
		args = append(args, query.KeyID)
		argIdx++
	}
	if query.UserID != "" {
		whereClause += fmt.Sprintf(" AND user_id = $%d", argIdx)
		args = append(args, query.UserID)
		argIdx++
	}
	if query.Status != "" {
		whereClause += fmt.Sprintf(" AND status = $%d", argIdx)
		args = append(args, query.Status)
		argIdx++
	}
	if query.MinSeverity > 0 {
		whereClause += fmt.Sprintf(" AND severity >= $%d", argIdx)
		args = append(args, query.MinSeverity)
		argIdx++
	}
	if query.StartTime != nil {
		whereClause += fmt.Sprintf(" AND created_at >= $%d", argIdx)
		args = append(args, *query.StartTime)
		argIdx++
	}
	if query.EndTime != nil {
		whereClause += fmt.Sprintf(" AND created_at <= $%d", argIdx)
		args = append(args, *query.EndTime)
		argIdx++
	}

	countQuery := fmt.Sprintf("SELECT COUNT(*) FROM anomaly_events %s", whereClause)
	var total int64
	if err := s.db.QueryRowContext(ctx, countQuery, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	if query.Limit <= 0 || query.Limit > 100 {
		query.Limit = 50
	}
	selectQuery := fmt.Sprintf(`
		SELECT id, key_id, user_id, anomaly_type, severity, description, location, 
		       evidence, status, resolved_by, resolved_at, resolution, created_at
		FROM anomaly_events
		%s
		ORDER BY created_at DESC
		LIMIT $%d OFFSET $%d
	`, whereClause, argIdx, argIdx+1)
	args = append(args, query.Limit, query.Offset)

	rows, err := s.db.QueryContext(ctx, selectQuery, args...)
	if err != nil {
		return nil, 0, err
	}
	defer rows.Close()

	var events []*model.AnomalyEvent
	for rows.Next() {
		event := &model.AnomalyEvent{}
		err := rows.Scan(&event.ID, &event.KeyID, &event.UserID, &event.AnomalyType,
			&event.Severity, &event.Description, &event.Location, &event.Evidence,
			&event.Status, &event.ResolvedBy, &event.ResolvedAt, &event.Resolution,
			&event.CreatedAt)
		if err != nil {
			s.logger.Error("Failed to scan anomaly event row", zap.Error(err))
			continue
		}
		events = append(events, event)
	}

	return events, total, nil
}

// ResolveAnomaly 处理异常事件
func (s *keyTrackingService) ResolveAnomaly(ctx context.Context, anomalyID int64, status string, resolution string, resolvedBy string) error {
	query := `
		UPDATE anomaly_events SET
			status = $1,
			resolved_by = $2,
			resolved_at = $3,
			resolution = $4
		WHERE id = $5
	`
	_, err := s.db.ExecContext(ctx, query, status, resolvedBy, time.Now(), resolution, anomalyID)
	return err
}
