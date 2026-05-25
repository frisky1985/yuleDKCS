/*
 * @file    key_tracking_test.go
 * @brief   KTS (Key Tracking Service) 单元测试
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-16
 */

package service

import (
	"context"
	"database/sql"
	"fmt"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/frisky1985/yuleDKCS/backend/internal/model"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"
)

func setupTestDB(t *testing.T) (*sql.DB, sqlmock.Sqlmock) {
	db, mock, err := sqlmock.New()
	require.NoError(t, err)
	return db, mock
}

func setupTestService(db *sql.DB) KeyTrackingService {
	logger := zap.NewNop()
	return NewKeyTrackingService(db, logger)
}

/******************************************************************************
 * 配置管理测试
 ******************************************************************************/
func TestKeyTrackingService_EnableTracking(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	config := &model.KeyTrackingConfig{
		Enabled:                 true,
		TrackLocation:           true,
		TrackUsage:              true,
		GeofenceEnabled:         false,
		AnomalyDetectionEnabled: true,
		MaxUsagePerHour:         50,
	}

	mock.ExpectExec("INSERT INTO key_tracking_configs").
		WithArgs(
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(),
		).
		WillReturnResult(sqlmock.NewResult(1, 1))

	err := svc.EnableTracking(ctx, keyID, config)
	assert.NoError(t, err)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetTrackingConfig(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"

	rows := sqlmock.NewRows([]string{
		"id", "key_id", "enabled", "track_location", "track_usage",
		"geofence_enabled", "geofence_center", "geofence_radius",
		"anomaly_detection_enabled", "max_usage_per_hour", "created_at", "updated_at",
	}).AddRow(
		1, keyID, true, true, true, false, nil, 100.0, true, 30,
		time.Now(), time.Now(),
	)

	mock.ExpectQuery(`SELECT \* FROM key_tracking_configs WHERE key_id = `).
		WithArgs(keyID).
		WillReturnRows(rows)

	config, err := svc.GetTrackingConfig(ctx, keyID)
	assert.NoError(t, err)
	assert.NotNil(t, config)
	assert.Equal(t, keyID, config.KeyID)
	assert.True(t, config.Enabled)
	assert.True(t, config.TrackLocation)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_DisableTracking(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"

	mock.ExpectExec("UPDATE key_tracking_configs SET enabled = false").
		WithArgs(sqlmock.AnyArg(), keyID).
		WillReturnResult(sqlmock.NewResult(1, 1))

	err := svc.DisableTracking(ctx, keyID)
	assert.NoError(t, err)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 状态跟踪测试
 ******************************************************************************/
func TestKeyTrackingService_RecordStatusChange(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	newStatus := model.KeyStatusActive
	reason := "Key activated"
	deviceID := "device_001"

	// Mock current status query
	mock.ExpectQuery("SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at").
		WithArgs(keyID).
		WillReturnError(sql.ErrNoRows)

	// Mock insert status record
	mock.ExpectExec("INSERT INTO key_status_records").
		WithArgs(
			keyID, newStatus, model.KeyStatus(""), sqlmock.AnyArg(),
			reason, deviceID, sqlmock.AnyArg(),
		).
		WillReturnResult(sqlmock.NewResult(1, 1))

	// Mock realtime status update (using unified upsert)
	mock.ExpectExec("INSERT INTO key_realtime_status").
		WithArgs(keyID, newStatus, sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnResult(sqlmock.NewResult(1, 1))

	err := svc.RecordStatusChange(ctx, keyID, newStatus, reason, deviceID, "admin")
	assert.NoError(t, err)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetCurrentStatus(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"

	rows := sqlmock.NewRows([]string{
		"id", "key_id", "status", "prev_status", "changed_by", "reason", "device_id", "created_at",
	}).AddRow(
		1, keyID, model.KeyStatusActive, model.KeyStatusPending, "system", "Activated", "device_001",
		time.Now(),
	)

	mock.ExpectQuery("SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at").
		WithArgs(keyID).
		WillReturnRows(rows)

	status, err := svc.GetCurrentStatus(ctx, keyID)
	assert.NoError(t, err)
	assert.NotNil(t, status)
	assert.Equal(t, model.KeyStatusActive, status.Status)
	assert.Equal(t, model.KeyStatusPending, status.PrevStatus)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetStatusHistory(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"

	rows := sqlmock.NewRows([]string{
		"id", "key_id", "status", "prev_status", "changed_by", "reason", "device_id", "created_at",
	}).
		AddRow(1, keyID, model.KeyStatusActive, model.KeyStatusPending, "system", "Activated", "device_001", time.Now()).
		AddRow(2, keyID, model.KeyStatusSuspended, model.KeyStatusActive, "admin", "Suspended", "", time.Now().Add(-time.Hour))

	mock.ExpectQuery("SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at").
		WithArgs(keyID, 10).
		WillReturnRows(rows)

	history, err := svc.GetStatusHistory(ctx, keyID, 10)
	assert.NoError(t, err)
	assert.Len(t, history, 2)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 使用记录测试
 ******************************************************************************/
func TestKeyTrackingService_RecordUsage(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	record := &model.KeyUsageRecord{
		KeyID:     "key_test_001",
		UsageType: model.KeyUsageTypeUnlock,
		VehicleID: "vehicle_001",
		DeviceID:  "device_001",
		UserID:    "user_001",
		Success:   true,
		Protocol:  "CCC",
	}

	mock.ExpectBegin()
	// Mock insert usage record
	mock.ExpectQuery("INSERT INTO key_usage_records").
		WithArgs(
			record.KeyID, record.UsageType, record.VehicleID, record.DeviceID, record.UserID,
			sqlmock.AnyArg(), record.Success, sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), record.Protocol, sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
		).
		WillReturnRows(sqlmock.NewRows([]string{"id"}).AddRow(1))

	// Mock counter update
	mock.ExpectExec("INSERT INTO key_usage_counters").
		WithArgs(sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnResult(sqlmock.NewResult(1, 1))

	// Mock realtime status upsert
	mock.ExpectExec("INSERT INTO key_realtime_status").
		WithArgs(sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnResult(sqlmock.NewResult(1, 1))

	// Mock tracking config query for anomaly detection
	configRows := sqlmock.NewRows([]string{
		"id", "key_id", "enabled", "track_location", "track_usage",
		"geofence_enabled", "geofence_center", "geofence_radius",
		"anomaly_detection_enabled", "max_usage_per_hour", "created_at", "updated_at",
	}).AddRow(
		1, record.KeyID, true, true, true, false, nil, 100.0, true, 30,
		time.Now(), time.Now(),
	)
	mock.ExpectQuery(`SELECT \* FROM key_tracking_configs WHERE key_id = `).
		WithArgs(record.KeyID).
		WillReturnRows(configRows)

	// Mock counter query for anomaly detection
	mock.ExpectQuery("SELECT usage_count FROM key_usage_counters").
		WithArgs(sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnError(sql.ErrNoRows)

	mock.ExpectCommit()

	err := svc.RecordUsage(ctx, record)
	assert.NoError(t, err)
	assert.Equal(t, int64(1), record.ID)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetUsageHistory(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	query := model.KeyUsageQuery{
		KeyID: "key_test_001",
		Limit: 10,
		Offset: 0,
	}

	// Mock count query
	mock.ExpectQuery(`SELECT COUNT\(\*\) FROM key_usage_records`).
		WithArgs(query.KeyID).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(2))

	// Mock select query
	rows := sqlmock.NewRows([]string{
		"id", "key_id", "usage_type", "vehicle_id", "device_id", "user_id", "location",
		"success", "error_code", "error_message", "session_id", "protocol",
		"ranging_distance", "metadata", "created_at",
	}).
		AddRow(1, query.KeyID, model.KeyUsageTypeUnlock, "vehicle_001", "device_001", "user_001", nil, true, 0, "", "session_1", "CCC", nil, nil, time.Now()).
		AddRow(2, query.KeyID, model.KeyUsageTypeLock, "vehicle_001", "device_001", "user_001", nil, true, 0, "", "session_2", "CCC", nil, nil, time.Now().Add(-time.Hour))

	mock.ExpectQuery("SELECT id, key_id, usage_type, vehicle_id, device_id, user_id, location").
		WithArgs(query.KeyID, query.Limit, query.Offset).
		WillReturnRows(rows)

	records, total, err := svc.GetUsageHistory(ctx, query)
	assert.NoError(t, err)
	assert.Equal(t, int64(2), total)
	assert.Len(t, records, 2)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 异常检测测试
 ******************************************************************************/
func TestCalculateDistance(t *testing.T) {
	// 测试哈弗辛公式距离计算
	loc1 := &model.Location{
		Latitude:  39.9042, // 北京
		Longitude: 116.4074,
	}
	loc2 := &model.Location{
		Latitude:  31.2304, // 上海
		Longitude: 121.4737,
	}

	distance := calculateDistance(loc1, loc2)
	// 北京到上海大约1067公里
	assert.Greater(t, distance, 1000000.0) // > 1000km
	assert.Less(t, distance, 1100000.0)    // < 1100km
}

/******************************************************************************
 * 实时状态测试
 ******************************************************************************/
/******************************************************************************
 * 配置管理补充测试
 ******************************************************************************/
func TestKeyTrackingService_UpdateTrackingConfig(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	config := &model.KeyTrackingConfig{
		Enabled:                 true,
		TrackLocation:           false,
		TrackUsage:              true,
		GeofenceEnabled:         true,
		AnomalyDetectionEnabled: true,
		MaxUsagePerHour:         80,
	}

	mock.ExpectExec("UPDATE key_tracking_configs SET").
		WithArgs(
			config.Enabled, config.TrackLocation, config.TrackUsage,
			config.GeofenceEnabled, config.GeofenceCenter, config.GeofenceRadius,
			config.AnomalyDetectionEnabled, config.MaxUsagePerHour,
			sqlmock.AnyArg(), // updated_at
			keyID,
		).
		WillReturnResult(sqlmock.NewResult(1, 1))

	err := svc.UpdateTrackingConfig(ctx, keyID, config)
	assert.NoError(t, err)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_UpdateTrackingConfig_DBError(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	config := &model.KeyTrackingConfig{
		Enabled: true,
	}

	mock.ExpectExec("UPDATE key_tracking_configs SET").
		WithArgs(sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(),
			sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), keyID).
		WillReturnError(fmt.Errorf("connection refused"))

	err := svc.UpdateTrackingConfig(ctx, keyID, config)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "connection refused")
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 使用统计测试
 ******************************************************************************/
func TestKeyTrackingService_GetUsageStatistics(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	days := 7

	// Mock usage statistics query
	usageRows := sqlmock.NewRows([]string{"usage_type", "protocol", "success", "count", "avg_distance"}).
		AddRow("unlock", "CCC", true, int64(20), float64(2.5)).
		AddRow("lock", "CCC", true, int64(10), float64(1.8)).
		AddRow("unlock", "ICCOA", false, int64(2), nil)

	mock.ExpectQuery("SELECT usage_type, protocol, success, COUNT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(usageRows)

	// Mock MostActiveHour query
	mock.ExpectQuery("SELECT EXTRACT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"hour", "cnt"}).AddRow(int64(14), int64(20)))

	// Mock ConsecutiveFailures query
	mock.ExpectQuery("SELECT consecutive_failures").
		WithArgs(keyID).
		WillReturnRows(sqlmock.NewRows([]string{"consecutive_failures"}).AddRow(int64(0)))

	// Mock AnomalyCount query
	mock.ExpectQuery("SELECT COUNT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(int64(1)))

	stats, err := svc.GetUsageStatistics(ctx, keyID, days)
	assert.NoError(t, err)
	assert.NotNil(t, stats)
	assert.Equal(t, keyID, stats.KeyID)
	assert.Equal(t, int64(32), stats.TotalUsage)
	assert.Equal(t, int64(30), stats.SuccessfulUsage)
	assert.Equal(t, int64(2), stats.FailedUsage)
	assert.Equal(t, int64(20), stats.UsageByType["unlock"])
	assert.Equal(t, int64(10), stats.UsageByType["lock"])
	assert.Equal(t, int64(30), stats.UsageByProtocol["CCC"])
	assert.Equal(t, int64(2), stats.UsageByProtocol["ICCOA"])
	assert.InDelta(t, 2.3, stats.AverageRangingDist, 0.1)
	assert.Equal(t, 14, stats.MostActiveHour)
	assert.Equal(t, 0, stats.ConsecutiveFailures)
	assert.Equal(t, 1, stats.AnomalyCount)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetUsageStatistics_EmptyResult(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	days := 7

	// Empty usage statistics
	mock.ExpectQuery("SELECT usage_type, protocol, success, COUNT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"usage_type", "protocol", "success", "count", "avg_distance"}))

	// MostActiveHour returns no rows
	mock.ExpectQuery("SELECT EXTRACT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnError(sql.ErrNoRows)

	// ConsecutiveFailures returns no rows
	mock.ExpectQuery("SELECT consecutive_failures").
		WithArgs(keyID).
		WillReturnError(sql.ErrNoRows)

	// AnomalyCount returns no rows
	mock.ExpectQuery("SELECT COUNT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnError(sql.ErrNoRows)

	stats, err := svc.GetUsageStatistics(ctx, keyID, days)
	assert.NoError(t, err)
	assert.NotNil(t, stats)
	assert.Equal(t, keyID, stats.KeyID)
	assert.Equal(t, int64(0), stats.TotalUsage)
	assert.Equal(t, int64(0), stats.SuccessfulUsage)
	assert.Equal(t, int64(0), stats.FailedUsage)
	assert.Equal(t, 0, stats.MostActiveHour)
	assert.Equal(t, 0, stats.ConsecutiveFailures)
	assert.Equal(t, 0, stats.AnomalyCount)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetUsageStatistics_DBError(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	days := 7

	mock.ExpectQuery("SELECT usage_type, protocol, success, COUNT").
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnError(fmt.Errorf("database is down"))

	stats, err := svc.GetUsageStatistics(ctx, keyID, days)
	assert.Error(t, err)
	assert.Nil(t, stats)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 异常事件测试
 ******************************************************************************/
func TestKeyTrackingService_GetAnomalyEvents(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	now := time.Now()
	query := model.AnomalyQuery{
		KeyID:       "key_test_001",
		MinSeverity: 3,
		Limit:       10,
		Offset:      0,
	}

	// Count query
	mock.ExpectQuery("SELECT COUNT").
		WithArgs(query.KeyID, query.MinSeverity).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(int64(2)))

	// Select query
	rows := sqlmock.NewRows([]string{
		"id", "key_id", "user_id", "anomaly_type", "severity", "description", "location",
		"evidence", "status", "resolved_by", "resolved_at", "resolution", "created_at",
	}).
		AddRow(int64(1), query.KeyID, "user_001", model.AnomalyTypeRapidUsage, 3,
			"Rapid usage detected", nil, model.JSONMap{"usage_count": 60},
			"new", nil, nil, "", now).
		AddRow(int64(2), query.KeyID, "user_001", model.AnomalyTypeOutsideGeofence, 4,
			"Outside geofence", &model.Location{Latitude: 40.0, Longitude: 116.0},
			model.JSONMap{"distance": 1500}, "new", nil, nil, "", now.Add(-time.Hour))

	mock.ExpectQuery("SELECT id, key_id, user_id, anomaly_type, severity, description, location").
		WithArgs(query.KeyID, query.MinSeverity, query.Limit, query.Offset).
		WillReturnRows(rows)

	events, total, err := svc.GetAnomalyEvents(ctx, query)
	assert.NoError(t, err)
	assert.Equal(t, int64(2), total)
	assert.Len(t, events, 2)
	assert.Equal(t, model.AnomalyTypeRapidUsage, events[0].AnomalyType)
	assert.Equal(t, model.AnomalyTypeOutsideGeofence, events[1].AnomalyType)
	assert.Equal(t, "new", events[0].Status)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetAnomalyEvents_EmptyResult(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	query := model.AnomalyQuery{
		KeyID: "nonexistent_key",
		Limit: 10,
	}

	mock.ExpectQuery("SELECT COUNT").
		WithArgs(query.KeyID).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(int64(0)))

	mock.ExpectQuery("SELECT id, key_id, user_id, anomaly_type, severity, description, location").
		WithArgs(query.KeyID, query.Limit, query.Offset).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "key_id", "user_id", "anomaly_type", "severity", "description", "location",
			"evidence", "status", "resolved_by", "resolved_at", "resolution", "created_at",
		}))

	events, total, err := svc.GetAnomalyEvents(ctx, query)
	assert.NoError(t, err)
	assert.Equal(t, int64(0), total)
	assert.Len(t, events, 0)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetAnomalyEvents_DBError(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	query := model.AnomalyQuery{KeyID: "key_test_001"}

	mock.ExpectQuery("SELECT COUNT").
		WithArgs(query.KeyID).
		WillReturnError(fmt.Errorf("connection timeout"))

	events, total, err := svc.GetAnomalyEvents(ctx, query)
	assert.Error(t, err)
	assert.Nil(t, events)
	assert.Equal(t, int64(0), total)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_ResolveAnomaly(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	anomalyID := int64(42)
	status := "resolved"
	resolution := "False alarm - user confirmed legitimate use"
	resolvedBy := "admin_001"

	mock.ExpectExec("UPDATE anomaly_events SET").
		WithArgs(status, resolvedBy, sqlmock.AnyArg(), resolution, anomalyID).
		WillReturnResult(sqlmock.NewResult(1, 1))

	err := svc.ResolveAnomaly(ctx, anomalyID, status, resolution, resolvedBy)
	assert.NoError(t, err)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_ResolveAnomaly_DBError(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	anomalyID := int64(999)

	mock.ExpectExec("UPDATE anomaly_events SET").
		WithArgs(sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), sqlmock.AnyArg(), anomalyID).
		WillReturnError(fmt.Errorf("anomaly event not found"))

	err := svc.ResolveAnomaly(ctx, anomalyID, "resolved", "fixed", "admin")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "anomaly event not found")
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 实时状态补充测试
 ******************************************************************************/
func TestKeyTrackingService_GetActiveKeysStatus(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	userID := "user_001"

	rows := sqlmock.NewRows([]string{
		"key_id", "current_status", "last_usage_time", "last_usage_type", "last_location",
		"today_usage_count", "total_usage_count", "consecutive_failures", "updated_at",
	}).
		AddRow("key_001", model.KeyStatusActive, time.Now(), model.KeyUsageTypeUnlock, nil,
			3, int64(50), 0, time.Now()).
		AddRow("key_002", model.KeyStatusActive, time.Now(), model.KeyUsageTypeLock, nil,
			1, int64(10), 0, time.Now())

	mock.ExpectQuery("SELECT r.key_id, r.current_status, r.last_usage_time, r.last_usage_type, r.last_location").
		WithArgs(userID).
		WillReturnRows(rows)

	statuses, err := svc.GetActiveKeysStatus(ctx, userID)
	assert.NoError(t, err)
	assert.Len(t, statuses, 2)
	assert.Equal(t, "key_001", statuses[0].KeyID)
	assert.Equal(t, model.KeyStatusActive, statuses[0].CurrentStatus)
	assert.Equal(t, 3, statuses[0].TodayUsageCount)
	assert.Equal(t, int64(50), statuses[0].TotalUsageCount)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetActiveKeysStatus_EmptyResult(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	userID := "user_no_keys"

	mock.ExpectQuery("SELECT r.key_id, r.current_status, r.last_usage_time, r.last_usage_type, r.last_location").
		WithArgs(userID).
		WillReturnRows(sqlmock.NewRows([]string{
			"key_id", "current_status", "last_usage_time", "last_usage_type", "last_location",
			"today_usage_count", "total_usage_count", "consecutive_failures", "updated_at",
		}))

	statuses, err := svc.GetActiveKeysStatus(ctx, userID)
	assert.NoError(t, err)
	assert.Len(t, statuses, 0)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetActiveKeysStatus_DBError(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	userID := "user_001"

	mock.ExpectQuery("SELECT r.key_id, r.current_status, r.last_usage_time, r.last_usage_type, r.last_location").
		WithArgs(userID).
		WillReturnError(fmt.Errorf("permission denied"))

	statuses, err := svc.GetActiveKeysStatus(ctx, userID)
	assert.Error(t, err)
	assert.Nil(t, statuses)
	assert.NoError(t, mock.ExpectationsWereMet())
}

func TestKeyTrackingService_GetRealtimeStatus(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"

	rows := sqlmock.NewRows([]string{
		"key_id", "current_status", "last_usage_time", "last_usage_type", "last_location",
		"today_usage_count", "total_usage_count", "consecutive_failures", "updated_at",
	}).AddRow(
		keyID, model.KeyStatusActive, time.Now(), model.KeyUsageTypeUnlock, nil,
		5, 100, 0, time.Now(),
	)

	mock.ExpectQuery("SELECT key_id, current_status, last_usage_time, last_usage_type, last_location").
		WithArgs(keyID).
		WillReturnRows(rows)

	status, err := svc.GetRealtimeStatus(ctx, keyID)
	assert.NoError(t, err)
	assert.NotNil(t, status)
	assert.Equal(t, keyID, status.KeyID)
	assert.Equal(t, model.KeyStatusActive, status.CurrentStatus)
	assert.Equal(t, 5, status.TodayUsageCount)
	assert.Equal(t, int64(100), status.TotalUsageCount)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 审计报告测试
 ******************************************************************************/
func TestKeyTrackingService_GenerateAuditReport(t *testing.T) {
	db, mock := setupTestDB(t)
	defer db.Close()
	svc := setupTestService(db)

	ctx := context.Background()
	keyID := "key_test_001"
	startTime := time.Now().AddDate(0, 0, -7)
	endTime := time.Now()

	// Mock status history
	statusRows := sqlmock.NewRows([]string{
		"id", "key_id", "status", "prev_status", "changed_by", "reason", "device_id", "created_at",
	}).
		AddRow(1, keyID, model.KeyStatusActive, model.KeyStatusPending, "system", "Activated", "device_001", time.Now().Add(-6*24*time.Hour))

	mock.ExpectQuery("SELECT id, key_id, status, prev_status, changed_by, reason, device_id, created_at").
		WithArgs(keyID, 100).
		WillReturnRows(statusRows)

	// Mock usage statistics
	mock.ExpectQuery(`SELECT usage_type, protocol, success, COUNT\(\*\), AVG\(ranging_distance\)`).
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"usage_type", "protocol", "success", "count", "avg_distance"}).
			AddRow("unlock", "CCC", true, 10, 2.5).
			AddRow("lock", "CCC", true, 10, nil))

	// Mock MostActiveHour query
	mock.ExpectQuery(`SELECT EXTRACT\(HOUR FROM created_at\)::int AS hour, COUNT\(\*\) AS cnt`).
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"hour", "cnt"}).AddRow(14, 20))

	// Mock ConsecutiveFailures query
	mock.ExpectQuery("SELECT consecutive_failures FROM key_realtime_status").
		WithArgs(keyID).
		WillReturnRows(sqlmock.NewRows([]string{"consecutive_failures"}).AddRow(0))

	// Mock AnomalyCount query
	mock.ExpectQuery(`SELECT COUNT\(\*\) FROM anomaly_events`).
		WithArgs(keyID, sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(0))

	// Mock anomaly events list for the report
	mock.ExpectQuery("SELECT COUNT\\(\\*\\) FROM anomaly_events").
		WithArgs(keyID, sqlmock.AnyArg(), sqlmock.AnyArg()).
		WillReturnRows(sqlmock.NewRows([]string{"count"}).AddRow(0))

	mock.ExpectQuery("SELECT id, key_id, user_id, anomaly_type, severity, description, location").
		WithArgs(keyID, sqlmock.AnyArg(), sqlmock.AnyArg(), 50, 0).
		WillReturnRows(sqlmock.NewRows([]string{
			"id", "key_id", "user_id", "anomaly_type", "severity", "description", "location",
			"evidence", "status", "resolved_by", "resolved_at", "resolution", "created_at",
		}))

	report, err := svc.GenerateAuditReport(ctx, keyID, startTime, endTime)
	assert.NoError(t, err)
	assert.NotNil(t, report)
	assert.Equal(t, keyID, report.KeyID)
	assert.NotNil(t, report.UsageSummary)
	assert.Equal(t, int64(20), report.UsageSummary.TotalUsage)
	assert.NoError(t, mock.ExpectationsWereMet())
}

/******************************************************************************
 * 串行化测试 (不使用mock，使用真实数据库连接)
 ******************************************************************************/
func TestKeyTrackingIntegration(t *testing.T) {
	// 注意: 这个测试需要真实的数据库连接
	// 在CI环境中应该被跳过
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	// 如果没有设置数据库连接，跳过
	dbURL := "postgres://admin:***@localhost/yuledkcs_test?sslmode=disable"
	db, err := sql.Open("pgx", dbURL)
	if err != nil {
		t.Skip("Database not available, skipping integration test")
	}
	defer db.Close()

	ctx := context.Background()
	logger := zap.NewNop()
	svc := NewKeyTrackingService(db, logger)

	keyID := "test_key_integration_" + time.Now().Format("20060102150405")

	// Test enable tracking
	err = svc.EnableTracking(ctx, keyID, nil)
	if err != nil {
		t.Skipf("Database error: %v, skipping integration test", err)
	}

	// Verify config
	config, err := svc.GetTrackingConfig(ctx, keyID)
	require.NoError(t, err)
	assert.True(t, config.Enabled)

	// Test record usage
	usage := &model.KeyUsageRecord{
		KeyID:     keyID,
		UsageType: model.KeyUsageTypeUnlock,
		VehicleID: "test_vehicle_001",
		DeviceID:  "test_device_001",
		UserID:    "test_user_001",
		Success:   true,
		Protocol:  "CCC",
	}
	err = svc.RecordUsage(ctx, usage)
	require.NoError(t, err)
	assert.NotZero(t, usage.ID)

	// Test get usage history
	query := model.KeyUsageQuery{
		KeyID:  keyID,
		Limit:  10,
		Offset: 0,
	}
	records, total, err := svc.GetUsageHistory(ctx, query)
	require.NoError(t, err)
	assert.Equal(t, int64(1), total)
	assert.Len(t, records, 1)

	// Cleanup
	svc.DisableTracking(ctx, keyID)
}
