// yuleTUNE — 标定校准平台 (Calibration Hub)
// ProfileManager — 标定档案管理器接口、Mock 实现与预置手机型号。

package tune

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// ProfileManager 手机型号标定档案管理器
type ProfileManager interface {
	// RegisterModel 注册新手机型号
	RegisterModel(ctx context.Context, model DeviceModel) error

	// UpdateDefaultParams 更新出厂默认参数
	UpdateDefaultParams(ctx context.Context, modelID string, params map[string]float64) error

	// GetDefaultParams 获取设备型号出厂默认参数
	GetDefaultParams(ctx context.Context, modelID string) (*DeviceModel, error)
}

// ---------------------------------------------------------------------------
// MockProfileManager

// MockProfileManager 内存级档案管理器 Mock。
type MockProfileManager struct {
	mu     sync.Mutex
	models map[string]*DeviceModel
}

// NewMockProfileManager 创建 MockProfileManager，可传入预置型号列表。
func NewMockProfileManager(models []DeviceModel) *MockProfileManager {
	m := &MockProfileManager{
		models: make(map[string]*DeviceModel),
	}
	for i := range models {
		dm := models[i]
		m.models[dm.ModelID] = &dm
	}
	return m
}

func (m *MockProfileManager) RegisterModel(_ context.Context, model DeviceModel) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if _, exists := m.models[model.ModelID]; exists {
		return fmt.Errorf("tune: model %q already registered", model.ModelID)
	}
	m.models[model.ModelID] = &model
	return nil
}

func (m *MockProfileManager) UpdateDefaultParams(_ context.Context, modelID string, params map[string]float64) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	dm, ok := m.models[modelID]
	if !ok {
		return fmt.Errorf("tune: model %q not found", modelID)
	}
	dm.DefaultParams = cloneParams(params)
	return nil
}

func (m *MockProfileManager) GetDefaultParams(_ context.Context, modelID string) (*DeviceModel, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	dm, ok := m.models[modelID]
	if !ok {
		return nil, fmt.Errorf("tune: model %q not found", modelID)
	}
	clone := *dm
	clone.DefaultParams = cloneParams(dm.DefaultParams)
	return &clone, nil
}

// ---------------------------------------------------------------------------
// 预置手机型号模板

// PresetModels 返回主流手机型号出厂标定参数。
func PresetModels() []DeviceModel {
	return []DeviceModel{
		// ---- Apple ----
		{
			ModelID:      "iPhone16ProMax",
			Manufacturer: "Apple",
			OS:           "iOS",
			OSVersion:    "20",
			ReleasedAt:   time.Date(2025, 9, 15, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        12.0,
				"uwb_rx_sensitivity":  -82.0,
				"rssi_offset_2g4":     0.0,
				"rssi_offset_5g":      1.5,
				"nfc_field_gain":      1.0,
			},
		},
		{
			ModelID:      "iPhone16",
			Manufacturer: "Apple",
			OS:           "iOS",
			OSVersion:    "20",
			ReleasedAt:   time.Date(2025, 9, 15, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        10.0,
				"uwb_rx_sensitivity":  -80.0,
				"rssi_offset_2g4":     0.0,
				"rssi_offset_5g":      2.0,
				"nfc_field_gain":      0.95,
			},
		},
		{
			ModelID:      "iPhone15Pro",
			Manufacturer: "Apple",
			OS:           "iOS",
			OSVersion:    "19",
			ReleasedAt:   time.Date(2024, 9, 20, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        12.0,
				"uwb_rx_sensitivity":  -81.0,
				"rssi_offset_2g4":     0.2,
				"rssi_offset_5g":      1.8,
				"nfc_field_gain":      1.0,
			},
		},
		// ---- Xiaomi ----
		{
			ModelID:      "Xiaomi15Ultra",
			Manufacturer: "Xiaomi",
			OS:           "Android",
			OSVersion:    "16",
			ReleasedAt:   time.Date(2025, 3, 1, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        11.0,
				"uwb_rx_sensitivity":  -79.0,
				"rssi_offset_2g4":     0.5,
				"rssi_offset_5g":      2.5,
				"nfc_field_gain":      0.9,
			},
		},
		{
			ModelID:      "Xiaomi15Pro",
			Manufacturer: "Xiaomi",
			OS:           "Android",
			OSVersion:    "15",
			ReleasedAt:   time.Date(2024, 10, 30, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        10.5,
				"uwb_rx_sensitivity":  -78.0,
				"rssi_offset_2g4":     0.8,
				"rssi_offset_5g":      2.2,
				"nfc_field_gain":      0.85,
			},
		},
		// ---- OPPO ----
		{
			ModelID:      "OPPOFindX8Pro",
			Manufacturer: "OPPO",
			OS:           "Android",
			OSVersion:    "16",
			ReleasedAt:   time.Date(2025, 4, 15, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        11.0,
				"uwb_rx_sensitivity":  -80.0,
				"rssi_offset_2g4":     0.3,
				"rssi_offset_5g":      1.2,
				"nfc_field_gain":      0.95,
			},
		},
		// ---- vivo ----
		{
			ModelID:      "vivoX200Pro",
			Manufacturer: "vivo",
			OS:           "Android",
			OSVersion:    "16",
			ReleasedAt:   time.Date(2025, 3, 15, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        10.0,
				"uwb_rx_sensitivity":  -79.0,
				"rssi_offset_2g4":     0.4,
				"rssi_offset_5g":      1.8,
				"nfc_field_gain":      0.9,
			},
		},
		// ---- Huawei ----
		{
			ModelID:      "HuaweiMate70Pro",
			Manufacturer: "Huawei",
			OS:           "HarmonyOS",
			OSVersion:    "5.0",
			ReleasedAt:   time.Date(2025, 11, 1, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        12.5,
				"uwb_rx_sensitivity":  -83.0,
				"rssi_offset_2g4":     -0.2,
				"rssi_offset_5g":      1.0,
				"nfc_field_gain":      1.05,
			},
		},
		// ---- Honor ----
		{
			ModelID:      "HonorMagic7Pro",
			Manufacturer: "Honor",
			OS:           "Android",
			OSVersion:    "16",
			ReleasedAt:   time.Date(2025, 6, 1, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        11.5,
				"uwb_rx_sensitivity":  -81.0,
				"rssi_offset_2g4":     0.1,
				"rssi_offset_5g":      1.5,
				"nfc_field_gain":      0.95,
			},
		},
		// ---- Samsung ----
		{
			ModelID:      "SamsungS25Ultra",
			Manufacturer: "Samsung",
			OS:           "Android",
			OSVersion:    "16",
			ReleasedAt:   time.Date(2025, 1, 22, 0, 0, 0, 0, time.UTC),
			DefaultParams: map[string]float64{
				"uwb_center_freq":     6.5,
				"uwb_bandwidth":       500.0,
				"uwb_tx_power":        12.0,
				"uwb_rx_sensitivity":  -82.0,
				"rssi_offset_2g4":     0.0,
				"rssi_offset_5g":      1.2,
				"nfc_field_gain":      1.0,
			},
		},
	}
}

// PresetModelsByManufacturer 按厂商分组返回预置型号。
func PresetModelsByManufacturer() map[string][]DeviceModel {
	all := PresetModels()
	group := make(map[string][]DeviceModel)
	for _, m := range all {
		group[m.Manufacturer] = append(group[m.Manufacturer], m)
	}
	return group
}

// ShortHandlers 文档辅助：列出所有预设型号的 ModelID。
func PresetModelIDs() []string {
	all := PresetModels()
	ids := make([]string, len(all))
	for i, m := range all {
		ids[i] = m.ModelID
	}
	return ids
}
