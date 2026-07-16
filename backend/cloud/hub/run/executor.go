package run

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"time"
)

// DeviceProvider 设备提供者
// 负责连接/断开真实设备或模拟器，并执行具体测试步骤。
type DeviceProvider interface {
	// ListDevices 列出所有可用设备
	ListDevices(ctx context.Context) ([]TestDevice, error)

	// ConnectDevice 连接指定设备
	ConnectDevice(ctx context.Context, deviceID string) error

	// DisconnectDevice 断开指定设备
	DisconnectDevice(ctx context.Context, deviceID string) error

	// ExecuteStep 在指定设备上执行单步操作
	ExecuteStep(ctx context.Context, deviceID string, step TestStep) (*StepResult, error)
}

// ──  Mock 实现 ────────────────────────────────────────────────────────────

// MockDeviceProvider 内存模拟设备提供者
// 用于单元测试和离线压测验证，不依赖真实硬件。
type MockDeviceProvider struct {
	mu      sync.RWMutex
	devices map[string]*mockDeviceSession
}

type mockDeviceSession struct {
	device     TestDevice
	connected  bool
	failChance float64 // 0.0 ~ 1.0，模拟失败率
	latencyMin time.Duration
	latencyMax time.Duration

	// 攻击模拟参数
	rangingDistance  float64 // UWB 测距距离 (m)，默认 0.5m
	signalAmplLevel float64 // BLE 信号放大级别，1.0=正常
	usedNonces      map[string]bool // 已使用的 Nonce
	replayFail      bool    // 是否触发重放检测
	distanceFail    bool    // 是否触发距离检测拒绝
	amplificationFail bool // 是否触发信号放大检测拒绝
}

type mockConditionConfig struct {
	distanceThreshold   float64 // UWB 距离阈值 (m)，默认 2.0
	amplificationThreshold float64 // 信号放大阈值，默认 1.5
}

// NewMockDeviceProvider 创建默认 mock provider
func NewMockDeviceProvider() *MockDeviceProvider {
	return &MockDeviceProvider{
		devices: make(map[string]*mockDeviceSession),
	}
}

// AddDevice 注册一个 mock 设备
func newMockSession(device TestDevice, failChance float64) *mockDeviceSession {
	return &mockDeviceSession{
		device:            device,
		failChance:        failChance,
		latencyMin:        30 * time.Millisecond,
		latencyMax:        150 * time.Millisecond,
		rangingDistance:   0.5, // 默认近距离
		signalAmplLevel:   1.0, // 默认无放大
		usedNonces:        make(map[string]bool),
	}
}

func (m *MockDeviceProvider) AddDevice(device TestDevice, failChance float64) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.devices[device.DeviceID] = newMockSession(device, failChance)
}

// SetLatencyRange 设置模拟网络的延时范围
func (m *MockDeviceProvider) SetLatencyRange(deviceID string, min, max time.Duration) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return fmt.Errorf("device %s not registered", deviceID)
	}
	session.latencyMin = min
	session.latencyMax = max
	return nil
}

func (m *MockDeviceProvider) ListDevices(_ context.Context) ([]TestDevice, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	list := make([]TestDevice, 0, len(m.devices))
	for _, s := range m.devices {
		list = append(list, s.device)
	}
	return list, nil
}

func (m *MockDeviceProvider) ConnectDevice(_ context.Context, deviceID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return fmt.Errorf("device %s not found", deviceID)
	}
	session.connected = true
	return nil
}

func (m *MockDeviceProvider) DisconnectDevice(_ context.Context, deviceID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return fmt.Errorf("device %s not found", deviceID)
	}
	session.connected = false
	return nil
}

// SetRangingDistance 设置设备的 UWB 测距距离 (m)，用于模拟距离欺骗攻击
func (m *MockDeviceProvider) SetRangingDistance(deviceID string, distanceMeters float64) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return fmt.Errorf("device %s not registered", deviceID)
	}
	session.rangingDistance = distanceMeters
	return nil
}

// SetSignalAmplification 设置 BLE 信号放大级别，用于模拟信号放大攻击
// 1.0 = 正常信号，> 1.0 = 信号放大
func (m *MockDeviceProvider) SetSignalAmplification(deviceID string, level float64) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return fmt.Errorf("device %s not registered", deviceID)
	}
	session.signalAmplLevel = level
	return nil
}

// UseNonce 记录一个 Nonce 已被使用，若重复使用则触发重放检测
func (m *MockDeviceProvider) UseNonce(deviceID string, nonce string) (bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	session, ok := m.devices[deviceID]
	if !ok {
		return false, fmt.Errorf("device %s not registered", deviceID)
	}
	if session.usedNonces[nonce] {
		// 重复 Nonce → 重放攻击
		session.replayFail = true
		return false, nil
	}
	session.usedNonces[nonce] = true
	return true, nil
}

// checkConditionalRejection 基于条件做攻击检测
// 返回 true 表示应拒绝（安全机制触发）
func checkConditionalRejection(session *mockDeviceSession, step TestStep) (bool, string) {
	// 1. 距离欺骗检测: PE-SHALL-NOT-01
	// 系统 SHALL NOT 在 UWB 测距距离 > 2m 时执行解锁指令
	if step.Action == "unlock" && session.rangingDistance > 2.0 {
		session.distanceFail = true
		return true, fmt.Sprintf("距离欺骗拒绝: UWB测距 %.1fm > 2.0m 阈值", session.rangingDistance)
	}

	// 2. 信号放大检测: RA-SHALL-05
	// BLE RSSI + UWB 多因子交叉验证
	if step.Action == "unlock" && session.signalAmplLevel > 1.5 {
		session.amplificationFail = true
		return true, fmt.Sprintf("信号放大拒绝: BLE RSSI放大级别 %.1fx > 1.5x 阈值", session.signalAmplLevel)
	}

	// 3. 重放攻击检测: RA-SHALL-04, RA-SHALL-NOT-02
	if step.Action == "unlock" && session.replayFail {
		return true, "重放攻击拒绝: Nonce已被使用"
	}

	return false, ""
}

func (m *MockDeviceProvider) ExecuteStep(_ context.Context, deviceID string, step TestStep) (*StepResult, error) {
	m.mu.RLock()
	session, ok := m.devices[deviceID]
	m.mu.RUnlock()
	if !ok {
		return nil, fmt.Errorf("device %s not found", deviceID)
	}
	if !session.connected {
		return nil, fmt.Errorf("device %s not connected", deviceID)
	}

	startedAt := time.Now()

	// 模拟执行延时
	latency := session.latencyMin
	if session.latencyMax > session.latencyMin {
		latency += time.Duration(rand.Int63n(int64(session.latencyMax - session.latencyMin)))
	}
	time.Sleep(latency)

	// 条件性攻击检测（优先于随机失败）
	rejected, rejectReason := checkConditionalRejection(session, step)
	passed := !rejected
	completedAt := time.Now()
	latencyMs := completedAt.Sub(startedAt).Milliseconds()

	result := &StepResult{
		Passed:      passed,
		LatencyMs:   latencyMs,
		StartedAt:   startedAt,
		CompletedAt: completedAt,
	}
	if !passed {
		result.Error = rejectReason
	}

	// 若条件检测未触发, 回退到随机失败模式
	if passed && session.failChance > 0 {
		passed = rand.Float64() >= session.failChance
		result.Passed = passed
		if !passed {
			result.Error = fmt.Sprintf("step %s (%s) failed: simulated error", step.Action, step.Expected)
		}
	}

	// 校验 MaxLatency
	if step.MaxLatency > 0 && time.Duration(latencyMs)*time.Millisecond > step.MaxLatency {
		result.Passed = false
		result.Error = fmt.Sprintf("latency %dms exceeds max %v", latencyMs, step.MaxLatency)
	}

	return result, nil
}
