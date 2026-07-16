// yuleTUNE — 标定校准平台 (Calibration Hub)
// Calibrator — 标定执行器接口与 Mock 实现。

package tune

import (
	"context"
	"fmt"
	"math"
	"math/rand"
	"time"
)

// Calibrator 标定执行器
type Calibrator interface {
	// Calibrate 执行一次标定
	Calibrate(ctx context.Context, modelID string, calibType CalibrationType) (*CalibrationRecord, error)

	// GetProfile 获取当前标定档案
	GetProfile(ctx context.Context, modelID string, calibType CalibrationType) (*CalibrationProfile, error)

	// ListModels 列出支持的手机型号
	ListModels(ctx context.Context, manufacturer string) ([]DeviceModel, error)
}

// ---------------------------------------------------------------------------
// MockCalibrator — 内存级 Mock 实现，用于开发与测试

// MockCalibrator 模拟标定器，基于出厂默认参数加入随机扰动。
type MockCalibrator struct {
	models   map[string]*DeviceModel
	profiles map[string]*CalibrationProfile // key: modelID + ":" + calibType
	records  []CalibrationRecord
	rng      *rand.Rand
}

// NewMockCalibrator 创建基于预置手机型号列表的 Mock 标定器。
func NewMockCalibrator(models []DeviceModel) *MockCalibrator {
	m := &MockCalibrator{
		models:   make(map[string]*DeviceModel),
		profiles: make(map[string]*CalibrationProfile),
		records:  make([]CalibrationRecord, 0),
		rng:      rand.New(rand.NewSource(time.Now().UnixNano())),
	}
	for i := range models {
		dm := models[i]
		m.models[dm.ModelID] = &dm
	}
	return m
}

func (m *MockCalibrator) Calibrate(_ context.Context, modelID string, calibType CalibrationType) (*CalibrationRecord, error) {
	dm, ok := m.models[modelID]
	if !ok {
		return nil, fmt.Errorf("tune: model %q not found", modelID)
	}

	// 从出厂默认参数生成带随机扰动的实际标定结果
	params := make(map[string]float64, len(dm.DefaultParams))
	for k, v := range dm.DefaultParams {
		noise := (m.rng.Float64() - 0.5) * 0.2 * math.Abs(v) // ±10%
		if v == 0 {
			noise = (m.rng.Float64() - 0.5) * 0.1
		}
		params[k] = v + noise
	}

	// 模拟测距精度：基准 ± 随机误差 (cm)
	accuracy := 5.0 + m.rng.Float64()*10.0
	temp := 20.0 + m.rng.Float64()*15.0 - 5.0 // 15–35°C
	signal := -40.0 - m.rng.Float64()*35.0     // -75 ~ -40 dBm

	rec := CalibrationRecord{
		RecordID:       fmt.Sprintf("rec_%s_%s_%d", modelID, calibType, time.Now().UnixNano()),
		ModelID:        modelID,
		CalibType:      calibType,
		Params:         params,
		Temperature:    math.Round(temp*10) / 10,
		SignalStrength: math.Round(signal*10) / 10,
		Accuracy:       math.Round(accuracy*100) / 100,
		CreatedAt:      time.Now(),
		Source:         "manual",
	}

	m.records = append(m.records, rec)

	// 更新档案
	key := modelID + ":" + string(calibType)
	prof, ok := m.profiles[key]
	if !ok {
		prof = &CalibrationProfile{
			ModelID:   modelID,
			CalibType: calibType,
			Current:   make(map[string]float64),
			History:   make([]CalibrationRecord, 0),
		}
	}
	for k, v := range params {
		prof.Current[k] = v
	}
	prof.History = append(prof.History, rec)

	// 重算统计
	n := len(prof.History)
	var sum float64
	for _, r := range prof.History {
		sum += r.Accuracy
	}
	prof.AvgAccuracy = math.Round(sum/float64(n)*100) / 100
	prof.SampleCount = n
	prof.LastUpdated = time.Now()
	m.profiles[key] = prof

	return &rec, nil
}

func (m *MockCalibrator) GetProfile(_ context.Context, modelID string, calibType CalibrationType) (*CalibrationProfile, error) {
	key := modelID + ":" + string(calibType)
	prof, ok := m.profiles[key]
	if !ok {
		// 返回空档案
		dm, dmOK := m.models[modelID]
		if !dmOK {
			return nil, fmt.Errorf("tune: model %q not found", modelID)
		}
		prof = &CalibrationProfile{
			ModelID:   modelID,
			CalibType: calibType,
			Current:   cloneParams(dm.DefaultParams),
			History:   make([]CalibrationRecord, 0),
		}
	}
	return prof, nil
}

func (m *MockCalibrator) ListModels(_ context.Context, manufacturer string) ([]DeviceModel, error) {
	var result []DeviceModel
	for _, dm := range m.models {
		if manufacturer == "" || dm.Manufacturer == manufacturer {
			result = append(result, *dm)
		}
	}
	if len(result) == 0 {
		return result, nil
	}
	return result, nil
}

// cloneParams 浅拷贝 map。
func cloneParams(src map[string]float64) map[string]float64 {
	dst := make(map[string]float64, len(src))
	for k, v := range src {
		dst[k] = v
	}
	return dst
}
