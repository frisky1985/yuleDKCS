package run

import "time"

// ── 预置测试场景 ──────────────────────────────────────────────────────────
// 每种场景对应数字钥匙的典型使用流程。
// 可直接注入到 ScenarioRunner 中进行压测。

// BasicPKEScenario 基本 PKE 解锁/上锁
// BLE连接 → 解锁 → 等待 → 上锁 → 断连
func BasicPKEScenario() TestCase {
	return TestCase{
		ID:          "pke_basic_001",
		Name:        "基本PKE解锁/上锁",
		Description: "BLE连接后进行无钥匙进入解锁，等待后上锁并断开连接",
		Protocol:    "BLE",
		Timeout:     30 * time.Second,
		Expectation: "所有步骤通过，延时在标准范围内",
		Steps: []TestStep{
			{
				Order:      1,
				Action:     "connect",
				Expected:   "success",
				MaxLatency: 5000 * time.Millisecond,
			},
			{
				Order:      2,
				Action:     "unlock",
				Expected:   "success",
				MaxLatency: 2000 * time.Millisecond,
			},
			{
				Order:      3,
				Action:     "lock",
				Expected:   "success",
				MaxLatency: 2000 * time.Millisecond,
			},
			{
				Order:      4,
				Action:     "disconnect",
				Expected:   "success",
				MaxLatency: 1000 * time.Millisecond,
			},
		},
	}
}

// NFCTapScenario NFC 刷卡解锁
// NFC 靠近 → 解锁 → 验证
func NFCTapScenario() TestCase {
	return TestCase{
		ID:          "nfc_tap_001",
		Name:        "NFC刷卡解锁",
		Description: "手机NFC靠近车载读卡器，执行解锁并验证",
		Protocol:    "NFC",
		Timeout:     15 * time.Second,
		Expectation: "NFC握手成功后立即解锁，响应时间 < 500ms",
		Steps: []TestStep{
			{
				Order:      1,
				Action:     "nfc_tap",
				Expected:   "handshake_ok",
				MaxLatency: 1000 * time.Millisecond,
				Params:     map[string]any{"reader_id": "driver_door"},
			},
			{
				Order:      2,
				Action:     "unlock",
				Expected:   "success",
				MaxLatency: 500 * time.Millisecond,
			},
			{
				Order:      3,
				Action:     "verify",
				Expected:   "door_open",
				MaxLatency: 1000 * time.Millisecond,
			},
		},
	}
}

// RemoteControlScenario 远程控车
// MQTT → 解锁 → 关窗 → 启动
func RemoteControlScenario() TestCase {
	return TestCase{
		ID:          "remote_ctrl_001",
		Name:        "远程控车",
		Description: "通过MQTT远程下发解锁、关窗、启动指令",
		Protocol:    "MQTT",
		Timeout:     60 * time.Second,
		Expectation: "远程指令均在 5s 内送达并执行成功",
		Steps: []TestStep{
			{
				Order:      1,
				Action:     "mqtt_connect",
				Expected:   "connected",
				MaxLatency: 3000 * time.Millisecond,
			},
			{
				Order:      2,
				Action:     "remote_unlock",
				Expected:   "success",
				MaxLatency: 5000 * time.Millisecond,
			},
			{
				Order:      3,
				Action:     "remote_window_close",
				Expected:   "success",
				MaxLatency: 5000 * time.Millisecond,
			},
			{
				Order:      4,
				Action:     "remote_start",
				Expected:   "engine_on",
				MaxLatency: 5000 * time.Millisecond,
			},
		},
	}
}

// KeySharingScenario 钥匙分享
// 创建分享 → 接收 → 使用 → 吊销
func KeySharingScenario() TestCase {
	return TestCase{
		ID:          "sharing_001",
		Name:        "钥匙分享全流程",
		Description: "创建数字钥匙分享链接、接收端激活、使用分享钥匙、回收权限",
		Protocol:    "BLE",
		Timeout:     120 * time.Second,
		Expectation: "分享链路完整，吊销后分享钥匙不可用",
		Steps: []TestStep{
			{
				Order:      1,
				Action:     "create_share",
				Expected:   "share_link_created",
				MaxLatency: 3000 * time.Millisecond,
				Params:     map[string]any{"target_user": "guest_01", "duration_hours": 24},
			},
			{
				Order:      2,
				Action:     "receive_share",
				Expected:   "key_activated",
				MaxLatency: 5000 * time.Millisecond,
			},
			{
				Order:      3,
				Action:     "unlock",
				Expected:   "success",
				MaxLatency: 2000 * time.Millisecond,
			},
			{
				Order:      4,
				Action:     "revoke_share",
				Expected:   "key_revoked",
				MaxLatency: 3000 * time.Millisecond,
			},
			{
				Order:      5,
				Action:     "unlock",
				Expected:   "failure", // 吊销后应失败
				MaxLatency: 2000 * time.Millisecond,
			},
		},
	}
}

// StressTestScenario 压力测试
// 连续解锁 100 次，测量稳定性和延时分布
func StressTestScenario() TestCase {
	steps := make([]TestStep, 0, 100)
	for i := 0; i < 100; i++ {
		steps = append(steps, TestStep{
			Order:      i + 1,
			Action:     "unlock",
			Expected:   "success",
			MaxLatency: 3000 * time.Millisecond,
		})
	}
	return TestCase{
		ID:          "stress_001",
		Name:        "压力测试（连续解锁100次）",
		Description: "对同一设备连续执行100次解锁操作，评估系统在高频调用下的稳定性",
		Protocol:    "BLE",
		Timeout:     600 * time.Second,
		Expectation: "通过率 >= 99%，单次延时无显著抖动",
		Steps:       steps,
	}
}

// ConcurrentTestScenario 并发测试
// 多设备同时尝试解锁，模拟多用户近距离同时控车
func ConcurrentTestScenario(deviceCount int) TestCase {
	actions := []string{"unlock", "lock", "start", "unlock"}
	steps := make([]TestStep, 0, len(actions)*deviceCount)
	order := 1
	for i := 0; i < deviceCount; i++ {
		for _, action := range actions {
			steps = append(steps, TestStep{
				Order:      order,
				Action:     action,
				Expected:   "success",
				MaxLatency: 5000 * time.Millisecond,
			})
			order++
		}
	}
	return TestCase{
		ID:          "concurrent_001",
		Name:        "并发测试（多设备同时控车）",
		Description: "多台设备同时执行解锁/上锁/启动操作，验证系统在并发场景下的正确性和性能",
		Protocol:    "BLE",
		Timeout:     300 * time.Second,
		Expectation: "所有操作正确完成，无死锁或数据竞争",
		Steps:       steps,
	}
}

// AllPresetScenarios 返回所有预置测试场景
func AllPresetScenarios() []TestCase {
	return []TestCase{
		BasicPKEScenario(),
		NFCTapScenario(),
		RemoteControlScenario(),
		KeySharingScenario(),
		StressTestScenario(),
		ConcurrentTestScenario(3),
	}
}
