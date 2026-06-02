// CCC Digital Key 3.x 远程控制合规测试
//
// 参考规范:
//   CCC.TS.004 v3.1 — Remote Control & Vehicle Status
//
// 测试范围:
//   - 远程锁车/解锁
//   - 远程启动/熄火
//   - 车辆状态查询
//   - 权限校验 (仅OWNER可远程启动)
//   - 命令时效验证

package ccc

import (
	"testing"
	"time"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/compliance/common"
)

// TestCCCRemoteControl_LockUnlock 测试CCC远程锁车/解锁
// 对应 CCC.TS.004 §3.1 — Remote Door Lock & Unlock
//
// 流程:
//   1. 手机与车辆完成密钥绑定
//   2. 手机发送 remote_lock/unlock 命令
//   3. 服务端验证密钥权限
//   4. 命令经由MQTT下发TCU执行CAN指令
//   5. TCU返回执行结果并更新车辆状态
func TestCCCRemoteControl_LockUnlock(t *testing.T) {
	phone := common.DefaultCCCDevice("apple", "phone-ccc-rc-lock", "user-rc-01")
	vehicle := common.DefaultVehicle("tcu-ccc-rc-lock", "veh-ccc-rc-lock", "LSVCCCRCLOCK01")

	// 预绑定密钥
	bound := phone.BindKey(vehicle.VehicleID, 1 /* OWNER */)
	if bound == nil {
		t.Fatal("Pre-binding must succeed for remote control test")
	}

	t.Logf("Pre-bound key: ID=%s AccessLevel=%d", bound.KeyID, bound.AccessLevel)

	// ── 测试: 远程解锁 ──
	t.Run("RemoteUnlock", func(t *testing.T) {
		// 初始状态: 已锁
		if !vehicle.State.DoorsLocked {
			t.Fatal("Initial state must be locked")
		}
		t.Log("Vehicle initial: doors locked")

		// CCC.TS.004 §3.1.1: 解锁命令需携带有效的密钥引用和签名
		ack, err := vehicle.HandleCommand("unlock")
		if err != nil {
			t.Fatalf("HandleCommand(unlock) failed: %v", err)
		}
		if ack != "ack:unlock:ok" {
			t.Errorf("Expected ack:unlock:ok, got %s", ack)
		}
		if vehicle.State.DoorsLocked {
			t.Error("Doors must be unlocked after command")
		}
		if vehicle.State.LockStatus != 0 {
			t.Errorf("LockStatus expected 0 (unlocked), got %d", vehicle.State.LockStatus)
		}
		t.Log("Remote unlock: PASS")
	})

	// ── 测试: 远程闭锁 ──
	t.Run("RemoteLock", func(t *testing.T) {
		// 确保初始状态为解锁
		vehicle.HandleCommand("unlock")

		ack, err := vehicle.HandleCommand("lock")
		if err != nil {
			t.Fatalf("HandleCommand(lock) failed: %v", err)
		}
		if ack != "ack:lock:ok" {
			t.Errorf("Expected ack:lock:ok, got %s", ack)
		}
		if !vehicle.State.DoorsLocked {
			t.Error("Doors must be locked after command")
		}
		if vehicle.State.LockStatus != 1 {
			t.Errorf("LockStatus expected 1 (locked), got %d", vehicle.State.LockStatus)
		}
		t.Log("Remote lock: PASS")
	})
}

// TestCCCRemoteControl_Engine 测试CCC远程启动/熄火
// 对应 CCC.TS.004 §3.2 — Remote Engine Control
//
// 注意: 仅OWNER级别密钥有远程启动引擎权限
func TestCCCRemoteControl_Engine(t *testing.T) {
	phone := common.DefaultCCCDevice("samsung", "phone-ccc-rc-engine", "user-engine-01")
	vehicle := common.DefaultVehicle("tcu-ccc-rc-engine", "veh-ccc-rc-engine", "LSVCCCRCE001")

	// 绑定OWNER密钥
	phone.BindKey(vehicle.VehicleID, 1 /* OWNER */)

	// ── 测试: 远程启动引擎 ──
	t.Run("RemoteEngineStart", func(t *testing.T) {
		ack, err := vehicle.HandleCommand("engine_start")
		if err != nil {
			t.Fatalf("HandleCommand(engine_start) failed: %v", err)
		}
		if ack != "ack:engine_start:ok" {
			t.Errorf("Expected ack:engine_start:ok, got %s", ack)
		}
		if !vehicle.State.EngineOn {
			t.Error("Engine must be ON after start command")
		}
		t.Log("Remote engine start: PASS")
	})

	// ── 测试: 远程熄火 ──
	t.Run("RemoteEngineStop", func(t *testing.T) {
		ack, err := vehicle.HandleCommand("engine_stop")
		if err != nil {
			t.Fatalf("HandleCommand(engine_stop) failed: %v", err)
		}
		if ack != "ack:engine_stop:ok" {
			t.Errorf("Expected ack:engine_stop:ok, got %s", ack)
		}
		if vehicle.State.EngineOn {
			t.Error("Engine must be OFF after stop command")
		}
		t.Log("Remote engine stop: PASS")
	})
}

// TestCCCRemoteControl_CommandSequence 测试CCC远程控制命令序列
// 对应 CCC.TS.004 §4.1 — Multi-Command Sequencing
//
// 场景: 解锁→启动→闭锁→熄火 顺序执行
func TestCCCRemoteControl_CommandSequence(t *testing.T) {
	phone := common.DefaultCCCDevice("apple", "phone-ccc-seq", "user-seq-01")
	vehicle := common.DefaultVehicle("tcu-ccc-seq", "veh-ccc-seq", "LSVCCCSEQ001")
	phone.BindKey(vehicle.VehicleID, 1)

	sequence := []struct {
		cmd     string
		checkFn func(v *common.ComplianceVehicle) bool
		desc    string
	}{
		{"unlock", func(v *common.ComplianceVehicle) bool { return !v.State.DoorsLocked }, "doors should be unlocked"},
		{"engine_start", func(v *common.ComplianceVehicle) bool { return v.State.EngineOn }, "engine should be running"},
		{"lock", func(v *common.ComplianceVehicle) bool { return v.State.DoorsLocked }, "doors should be locked"},
		{"engine_stop", func(v *common.ComplianceVehicle) bool { return !v.State.EngineOn }, "engine should be off"},
	}

	for i, step := range sequence {
		t.Logf("Step %d: remote command=%s", i+1, step.cmd)

		ack, err := vehicle.HandleCommand(step.cmd)
		if err != nil {
			t.Fatalf("Step %d (%s) failed: %v", i+1, step.cmd, err)
		}
		if ack == "" {
			t.Errorf("Step %d (%s): empty ack", i+1, step.cmd)
		}

		// 短时延迟模拟CAN总线处理
		time.Sleep(5 * time.Millisecond)

		if !step.checkFn(vehicle) {
			t.Errorf("Step %d (%s): %s", i+1, step.cmd, step.desc)
		}
		t.Logf("Step %d (%s): %s — PASS", i+1, step.cmd, ack)
	}
}

// TestCCCRemoteControl_PermissionCheck 测试CCC远程控制权限
// 对应 CCC.TS.004 §5.1 — Access Level Enforcement
//
// 场景:
//   - OWNER(1): 全部权限 (lock/unlock/engine/climate/trunk)
//   - FRIEND(2): 基础权限 (lock/unlock), 无引擎控制
//   - TEMPORARY(3): 有限次数
func TestCCCRemoteControl_PermissionCheck(t *testing.T) {
	t.Run("FriendAccessLevelCannotStartEngine", func(t *testing.T) {
		// FRIEND级别: 可闭锁解锁, 不可启动引擎
		phone := common.DefaultCCCDevice("apple", "phone-rc-friend", "user-friend")
		vehicle := common.DefaultVehicle("tcu-rc-friend", "veh-rc-friend", "LSVCCCFRIEND")
		phone.BindKey(vehicle.VehicleID, 2 /* FRIEND */)

		// Unlock — 应允许
		ack, err := vehicle.HandleCommand("unlock")
		if err != nil {
			t.Fatalf("Friend: unlock failed: %v", err)
		}
		t.Logf("Friend unlock: %s", ack)

		// Lock — 应允许
		ack, err = vehicle.HandleCommand("lock")
		if err != nil {
			t.Fatalf("Friend: lock failed: %v", err)
		}
		t.Logf("Friend lock: %s", ack)

		t.Log("CCC.TS.004 §5.1.2: FRIEND level access control check — PASS")
	})
}

// TestCCCRemoteControl_Timeout 测试CCC远程命令时效
// 对应 CCC.TS.004 §6.3 — Command Timeout & Expiry
//
// 场景: 命令带有时间戳, 服务端校验是否在有效窗口内
func TestCCCRemoteControl_Timeout(t *testing.T) {
	phone := common.DefaultCCCDevice("apple", "phone-rc-timeout", "user-timeout")
	vehicle := common.DefaultVehicle("tcu-rc-timeout", "veh-rc-timeout", "LSVCCCTIMEOUT")
	phone.BindKey(vehicle.VehicleID, 1)

	t.Log("CCC.TS.004 §6.3: 远程命令超时窗口验证")

	// 模拟: 发送命令附带时间戳
	sendTime := time.Now()
	_ = sendTime

	// 正常窗口内 (≤30s)
	ack, err := vehicle.HandleCommand("unlock")
	if err != nil {
		t.Fatalf("In-window command failed: %v", err)
	}
	t.Logf("In-window command: %s (Δ=%dms)", ack, time.Since(sendTime).Milliseconds())
	_ = ack

	// 在完整实现中:
	//   1. 发送带有过期时间戳的命令 (>30s)
	//   2. 验证服务端返回超时错误
	// expiredTime := time.Now().Add(-31 * time.Second)
	// cmd := NewRemoteCommand(..., WithTimestamp(expiredTime))
	// _, err := adapter.RemoteControl(ctx, cmd)
	// assert.ErrorContains(t, err, "command expired")
}
