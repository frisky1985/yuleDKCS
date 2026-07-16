// yuleTUNE — 标定校准平台 (Calibration Hub)
// Core type definitions.

package tune

import "time"

// CalibrationType 标定类型
type CalibrationType string

const (
	CalibUWB  CalibrationType = "uwb_ranging"          // UWB 测距标定
	CalibBLE  CalibrationType = "ble_rssi"              // BLE RSSI 标定
	CalibNFC  CalibrationType = "nfc_field_strength"    // NFC 场强标定
)

// AllCalibrationTypes 返回所有支持的标定类型。
func AllCalibrationTypes() []CalibrationType {
	return []CalibrationType{CalibUWB, CalibBLE, CalibNFC}
}

// DeviceModel 手机型号配置
type DeviceModel struct {
	ModelID       string             `json:"model_id"`
	Manufacturer  string             `json:"manufacturer"`   // Apple, Xiaomi, OPPO, vivo, Huawei...
	OS            string             `json:"os"`             // iOS, Android
	OSVersion     string             `json:"os_version"`
	ReleasedAt    time.Time          `json:"released_at"`
	DefaultParams map[string]float64 `json:"default_params"` // 出厂默认标定参数
}

// CalibrationRecord 单次标定记录
type CalibrationRecord struct {
	RecordID       string             `json:"record_id"`
	ModelID        string             `json:"model_id"`
	CalibType      CalibrationType    `json:"calib_type"`
	Params         map[string]float64 `json:"params"`          // 标定后的参数
	Temperature    float64            `json:"temperature"`     // 标定时环境温度 (°C)
	SignalStrength float64            `json:"signal_strength"` // 信号强度 (dBm / 归一化)
	Accuracy       float64            `json:"accuracy"`        // 测距精度 (cm)
	CreatedAt      time.Time          `json:"created_at"`
	Source         string             `json:"source"`          // "factory", "ota", "manual"
}

// CalibrationProfile 手机型号标定档案
type CalibrationProfile struct {
	ModelID     string                       `json:"model_id"`
	CalibType   CalibrationType              `json:"calib_type"`
	Current     map[string]float64           `json:"current_params"`
	History     []CalibrationRecord          `json:"history"`
	AvgAccuracy float64                      `json:"avg_accuracy"`
	SampleCount int                          `json:"sample_count"`
	LastUpdated time.Time                    `json:"last_updated"`
}

// CalibrationRecommendation OTA 优化建议
type CalibrationRecommendation struct {
	ModelID             string             `json:"model_id"`
	CalibType           CalibrationType    `json:"calib_type"`
	OldParams           map[string]float64 `json:"old_params"`
	NewParams           map[string]float64 `json:"new_params"`
	ExpectedImprovement float64            `json:"expected_improvement"`   // 预期精度提升 (%)
	Confidence          float64            `json:"confidence"`             // 置信度 0-1
	Reason              string             `json:"reason"`
	CreatedAt           time.Time          `json:"created_at"`
}

// SignalQuality 信号质量枚举（用于标定数据筛选）
type SignalQuality string

const (
	SignalPoor     SignalQuality = "poor"
	SignalFair     SignalQuality = "fair"
	SignalGood     SignalQuality = "good"
	SignalExcellent SignalQuality = "excellent"
)

// ClassifySignal 根据信号强度对质量进行定性分级。
func ClassifySignal(dbm float64) SignalQuality {
	switch {
	case dbm < -90:
		return SignalPoor
	case dbm < -70:
		return SignalFair
	case dbm < -50:
		return SignalGood
	default:
		return SignalExcellent
	}
}
