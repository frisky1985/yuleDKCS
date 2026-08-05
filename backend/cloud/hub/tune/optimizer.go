// yuleTUNE — 标定校准平台 (Calibration Hub)
// Optimizer — OTA 优化器接口与 Mock 实现。

package tune

import (
	"context"
	"fmt"
	"math"
	"math/rand"
	"sort"
	"sync"
	"time"
)

// Optimizer OTA 优化器
type Optimizer interface {
	// Analyze 分析标定数据，生成优化建议
	Analyze(ctx context.Context, modelID string, calibType CalibrationType) (*CalibrationRecommendation, error)

	// ApplyRecommendation 应用优化建议
	ApplyRecommendation(ctx context.Context, rec CalibrationRecommendation) error

	// BatchOptimize 批量优化所有型号
	BatchOptimize(ctx context.Context) ([]CalibrationRecommendation, error)
}

// ---------------------------------------------------------------------------
// MockOptimizer — 基于模拟标定数据的 OTA 优化 Mock

// MockOptimizer 从 MockCalibrator 读取历史数据，利用加权平均
// 推算出优化后的参数，并计算预期提升。
type MockOptimizer struct {
	calibrator *MockCalibrator
	rng        *rand.Rand

	mu        sync.Mutex
	applied   []CalibrationRecommendation
}

// NewMockOptimizer 创建 Mock 优化器，引用一个 MockCalibrator 作为数据源。
func NewMockOptimizer(cal *MockCalibrator) *MockOptimizer {
	return &MockOptimizer{
		calibrator: cal,
		rng:        rand.New(rand.NewSource(time.Now().UnixNano())),
		applied:    make([]CalibrationRecommendation, 0),
	}
}

func (o *MockOptimizer) Analyze(_ context.Context, modelID string, calibType CalibrationType) (*CalibrationRecommendation, error) {
	key := modelID + ":" + string(calibType)

	// 获取当前档案
	prof, ok := o.calibrator.profiles[key]
	if !ok || len(prof.History) < 3 {
		return nil, fmt.Errorf("tune: insufficient samples for %q (need >=3, got %d)", key, len(prof.History))
	}

	// 按信号质量加权计算新参数
	newParams := make(map[string]float64)
	weights := make(map[string]float64)

	for _, rec := range prof.History {
		w := signalWeight(rec.SignalStrength)
		for k, v := range rec.Params {
			newParams[k] += v * w
			weights[k] += w
		}
	}
	for k := range newParams {
		if weights[k] > 0 {
			newParams[k] /= weights[k]
			newParams[k] = math.Round(newParams[k]*10000) / 10000
		}
	}

	// 用出厂默认参数作为 base
	dm, ok := o.calibrator.models[modelID]
	if !ok {
		return nil, fmt.Errorf("tune: model %q not found", modelID)
	}
	oldParams := cloneParams(dm.DefaultParams)
	baseAccuracy := prof.AvgAccuracy

	// 估算新参数的预期精度
	improvementFactor := 0.85 + o.rng.Float64()*0.10 // 预期改善 5–15%
	expectedAccuracy := baseAccuracy * improvementFactor
	expectedImprovement := math.Round((baseAccuracy-expectedAccuracy)/baseAccuracy*100*100) / 100

	// 置信度随样本量递增
	confidence := math.Min(0.3+float64(len(prof.History))*0.05, 0.95)
	confidence = math.Round(confidence*100) / 100

	rec := CalibrationRecommendation{
		ModelID:             modelID,
		CalibType:           calibType,
		OldParams:           oldParams,
		NewParams:           newParams,
		ExpectedImprovement: expectedImprovement,
		Confidence:          confidence,
		Reason:              fmt.Sprintf("weighted from %d records, avg accuracy improved from %.1fcm to ~%.1fcm", len(prof.History), baseAccuracy, expectedAccuracy),
		CreatedAt:           time.Now(),
	}
	return &rec, nil
}

func (o *MockOptimizer) ApplyRecommendation(_ context.Context, rec CalibrationRecommendation) error {
	o.mu.Lock()
	defer o.mu.Unlock()

	dm, ok := o.calibrator.models[rec.ModelID]
	if !ok {
		return fmt.Errorf("tune: model %q not found", rec.ModelID)
	}
	dm.DefaultParams = cloneParams(rec.NewParams)

	o.applied = append(o.applied, rec)
	return nil
}

func (o *MockOptimizer) BatchOptimize(ctx context.Context) ([]CalibrationRecommendation, error) {
	type modelTypePair struct {
		modelID   string
		calibType CalibrationType
	}

	// 收集有足够样本的 model+type 组合
	pairs := make(map[modelTypePair]bool)
	for key := range o.calibrator.profiles {
		prof := o.calibrator.profiles[key]
		if prof != nil && len(prof.History) >= 3 {
			pairs[modelTypePair{modelID: prof.ModelID, calibType: prof.CalibType}] = true
		}
	}

	var recommendations []CalibrationRecommendation
	for pair := range pairs {
		rec, err := o.Analyze(ctx, pair.modelID, pair.calibType)
		if err != nil {
			continue // 忽略分析失败的组合
		}
		recommendations = append(recommendations, *rec)
	}

	// 按 ExpectedImprovement 降序排列
	sort.Slice(recommendations, func(i, j int) bool {
		return recommendations[i].ExpectedImprovement > recommendations[j].ExpectedImprovement
	})
	return recommendations, nil
}

// signalWeight 根据信号强度返回权重。
func signalWeight(dbm float64) float64 {
	switch ClassifySignal(dbm) {
	case SignalExcellent:
		return 1.0
	case SignalGood:
		return 0.8
	case SignalFair:
		return 0.5
	default:
		return 0.2
	}
}
