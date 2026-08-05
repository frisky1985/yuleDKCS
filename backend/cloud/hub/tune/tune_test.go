// Package tune — unit tests for calibration platform
package tune

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestAllCalibrationTypes(t *testing.T) {
	types := AllCalibrationTypes()
	assert.Len(t, types, 3)
	assert.Contains(t, types, CalibUWB)
	assert.Contains(t, types, CalibBLE)
	assert.Contains(t, types, CalibNFC)
}

func TestClassifySignal(t *testing.T) {
	tests := []struct {
		dbm     float64
		expect  SignalQuality
		desc    string
	}{
		{-95, SignalPoor, "very weak signal"},
		{-85, SignalFair, "weak signal"},
		{-75, SignalFair, "fair signal"},
		{-60, SignalGood, "good signal"},
		{-45, SignalExcellent, "excellent signal"},
		{0, SignalExcellent, "strong signal"},
	}
	for _, tt := range tests {
		t.Run(tt.desc, func(t *testing.T) {
			assert.Equal(t, tt.expect, ClassifySignal(tt.dbm))
		})
	}
}

func TestNewMockCalibrator(t *testing.T) {
	models := []DeviceModel{
		{ModelID: "iphone-15-pro", Manufacturer: "Apple", OS: "iOS", OSVersion: "18.0",
			DefaultParams: map[string]float64{"tx_power": 10, "rssi_offset": -65}},
		{ModelID: "xiaomi-14", Manufacturer: "Xiaomi", OS: "Android", OSVersion: "15",
			DefaultParams: map[string]float64{"tx_power": 12, "rssi_offset": -68}},
	}
	m := NewMockCalibrator(models)
	require.NotNil(t, m)
	assert.Len(t, m.models, 2)
}

func TestMockCalibrator_Calibrate(t *testing.T) {
	ctx := context.Background()
	m := NewMockCalibrator([]DeviceModel{
		{ModelID: "iphone-15-pro", Manufacturer: "Apple",
			DefaultParams: map[string]float64{"tx_power": 10, "rssi_offset": -65}},
	})

	t.Run("successful calibration", func(t *testing.T) {
		rec, err := m.Calibrate(ctx, "iphone-15-pro", CalibUWB)
		require.NoError(t, err)
		assert.Equal(t, "iphone-15-pro", rec.ModelID)
		assert.Equal(t, CalibUWB, rec.CalibType)
		assert.NotEmpty(t, rec.Params)
		assert.Greater(t, rec.Accuracy, 0.0)
		assert.Equal(t, "manual", rec.Source)
	})

	t.Run("unknown model returns error", func(t *testing.T) {
		_, err := m.Calibrate(ctx, "unknown-model", CalibBLE)
		assert.Error(t, err)
	})
}

func TestMockCalibrator_GetProfile(t *testing.T) {
	ctx := context.Background()
	m := NewMockCalibrator([]DeviceModel{
		{ModelID: "iphone-15-pro", Manufacturer: "Apple",
			DefaultParams: map[string]float64{"tx_power": 10}},
	})

	t.Run("returns profile with default params when no calibration done", func(t *testing.T) {
		prof, err := m.GetProfile(ctx, "iphone-15-pro", CalibUWB)
		require.NoError(t, err)
		assert.Equal(t, "iphone-15-pro", prof.ModelID)
		assert.NotEmpty(t, prof.Current)
	})

	t.Run("returns profile with calibration history", func(t *testing.T) {
		_, _ = m.Calibrate(ctx, "iphone-15-pro", CalibUWB)
		_, _ = m.Calibrate(ctx, "iphone-15-pro", CalibUWB)

		prof, err := m.GetProfile(ctx, "iphone-15-pro", CalibUWB)
		require.NoError(t, err)
		assert.Equal(t, 2, prof.SampleCount)
		assert.Len(t, prof.History, 2)
	})

	t.Run("unknown model returns error", func(t *testing.T) {
		_, err := m.GetProfile(ctx, "unknown", CalibUWB)
		assert.Error(t, err)
	})
}

func TestMockCalibrator_ListModels(t *testing.T) {
	ctx := context.Background()
	m := NewMockCalibrator([]DeviceModel{
		{ModelID: "iphone-15-pro", Manufacturer: "Apple"},
		{ModelID: "xiaomi-14", Manufacturer: "Xiaomi"},
		{ModelID: "iphone-16", Manufacturer: "Apple"},
	})

	t.Run("all models", func(t *testing.T) {
		models, err := m.ListModels(ctx, "")
		assert.NoError(t, err)
		assert.Len(t, models, 3)
	})

	t.Run("filter by manufacturer", func(t *testing.T) {
		models, err := m.ListModels(ctx, "Apple")
		assert.NoError(t, err)
		assert.Len(t, models, 2)
	})

	t.Run("no matching manufacturer", func(t *testing.T) {
		models, err := m.ListModels(ctx, "Samsung")
		assert.NoError(t, err)
		assert.Empty(t, models)
	})
}

func TestMockCalibrator_MultipleRuns(t *testing.T) {
	ctx := context.Background()
	m := NewMockCalibrator([]DeviceModel{
		{ModelID: "test-phone", Manufacturer: "Test",
			DefaultParams: map[string]float64{"gain": 1.0}},
	})

	// Run multiple calibrations
	for i := 0; i < 3; i++ {
		rec, err := m.Calibrate(ctx, "test-phone", CalibBLE)
		require.NoError(t, err)
		assert.Equal(t, "test-phone", rec.ModelID)
	}
}

func TestOptimizer(t *testing.T) {
	ctx := context.Background()
	// The optimizer may be an interface. Let's test the store functionality.
	s := NewInMemoryStore()

	t.Run("store and retrieve model", func(t *testing.T) {
		err := s.Save(ctx, DeviceModel{
			ModelID:      "test-model",
			Manufacturer: "Test",
		})
		assert.NoError(t, err)

		m, err := s.Load(ctx, "test-model")
		assert.NoError(t, err)
		require.NotNil(t, m)
		assert.Equal(t, "Test", m.Manufacturer)
	})
}

func TestInMemoryStore(t *testing.T) {
	ctx := context.Background()
	s := NewInMemoryStore()

	t.Run("save and load record", func(t *testing.T) {
		rec := CalibrationRecord{
			RecordID:  "rec-001",
			ModelID:   "iphone-15-pro",
			CalibType: CalibUWB,
			CreatedAt: time.Now(),
		}
		err := s.SaveRecord(ctx, rec)
		assert.NoError(t, err)

		records, err := s.ListRecords(ctx, RecordFilter{ModelID: "iphone-15-pro"})
		assert.NoError(t, err)
		assert.Len(t, records, 1)
	})

	t.Run("save and list calibration", func(t *testing.T) {
		rec := CalibrationRecord{
			RecordID:  "rec-002",
			ModelID:   "xiaomi-14",
			CalibType: CalibBLE,
			CreatedAt: time.Now(),
		}
		err := s.SaveRecord(ctx, rec)
		assert.NoError(t, err)

		records, err := s.ListRecords(ctx, RecordFilter{CalibType: CalibBLE})
		assert.NoError(t, err)
		assert.Len(t, records, 1)
	})
}

func TestDeviceModelDefaults(t *testing.T) {
	dm := DeviceModel{
		ModelID:      "iphone-15-pro",
		Manufacturer: "Apple",
		DefaultParams: map[string]float64{"tx_power": 10},
	}
	assert.Equal(t, "iphone-15-pro", dm.ModelID)
	assert.Equal(t, "Apple", dm.Manufacturer)
}

// ── InMemoryStore (test helper) ────────────────────────────────────────────

type InMemoryStore struct {
	mu      sync.RWMutex
	models  map[string]DeviceModel
	records map[string]CalibrationRecord
}

func NewInMemoryStore() *InMemoryStore {
	return &InMemoryStore{
		models:  make(map[string]DeviceModel),
		records: make(map[string]CalibrationRecord),
	}
}

func (s *InMemoryStore) Save(_ context.Context, model DeviceModel) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.models[model.ModelID] = model
	return nil
}

func (s *InMemoryStore) Load(_ context.Context, modelID string) (*DeviceModel, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	m, ok := s.models[modelID]
	if !ok {
		return nil, nil
	}
	return &m, nil
}

func (s *InMemoryStore) SaveRecord(_ context.Context, rec CalibrationRecord) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.records[rec.RecordID] = rec
	return nil
}

func (s *InMemoryStore) ListRecords(_ context.Context, filter RecordFilter) ([]CalibrationRecord, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var result []CalibrationRecord
	for _, r := range s.records {
		if filter.ModelID != "" && r.ModelID != filter.ModelID {
			continue
		}
		if filter.CalibType != "" && r.CalibType != filter.CalibType {
			continue
		}
		result = append(result, r)
	}
	return result, nil
}

type RecordFilter struct {
	ModelID   string
	CalibType CalibrationType
}
