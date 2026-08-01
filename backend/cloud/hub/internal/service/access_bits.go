package service

import pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"

// ── 权限位掩码 (ICCOA_PERM 语义) ────────────────────────────
//
// 与 proto AccessLevel 的字段顺序一一对应, 使用位掩码便于持久化
// 为单个整数 (数据库 access_bits 列) 以及跨协议透传。
const (
	PermBitLock    uint32 = 1 << 0 // 锁车
	PermBitUnlock  uint32 = 1 << 1 // 解锁
	PermBitEngine  uint32 = 1 << 2 // 引擎启停
	PermBitTrunk   uint32 = 1 << 3 // 后备箱
	PermBitWindow  uint32 = 1 << 4 // 车窗
	PermBitClimate uint32 = 1 << 5 // 空调
	PermBitFind    uint32 = 1 << 6 // 寻车
	PermBitSeat    uint32 = 1 << 7 // 座椅
)

// PermBitAll 全部权限位。
const PermBitAll uint32 = PermBitLock | PermBitUnlock | PermBitEngine | PermBitTrunk |
	PermBitWindow | PermBitClimate | PermBitFind | PermBitSeat

// accessLevelToBits 将 proto AccessLevel 转换为权限位掩码。
// 传入 nil 时按全权限处理 (与协议默认行为一致)。
func accessLevelToBits(l *pb.AccessLevel) uint32 {
	if l == nil {
		return PermBitAll
	}
	var bits uint32
	if l.Lock {
		bits |= PermBitLock
	}
	if l.Unlock {
		bits |= PermBitUnlock
	}
	if l.Engine {
		bits |= PermBitEngine
	}
	if l.Trunk {
		bits |= PermBitTrunk
	}
	if l.Window {
		bits |= PermBitWindow
	}
	if l.Climate {
		bits |= PermBitClimate
	}
	if l.Find {
		bits |= PermBitFind
	}
	if l.Seat {
		bits |= PermBitSeat
	}
	return bits
}

// bitsToAccessLevel 将权限位掩码转换为 proto AccessLevel。
func bitsToAccessLevel(bits uint32) *pb.AccessLevel {
	return &pb.AccessLevel{
		Lock:    bits&PermBitLock != 0,
		Unlock:  bits&PermBitUnlock != 0,
		Engine:  bits&PermBitEngine != 0,
		Trunk:   bits&PermBitTrunk != 0,
		Window:  bits&PermBitWindow != 0,
		Climate: bits&PermBitClimate != 0,
		Find:    bits&PermBitFind != 0,
		Seat:    bits&PermBitSeat != 0,
	}
}

// actionRequiresBit 返回执行 action 所需的权限位。
// 第二个返回值 false 表示 action 不识别 (无效指令)。
func actionRequiresBit(action string) (uint32, bool) {
	switch action {
	case "lock":
		return PermBitLock, true
	case "unlock":
		return PermBitUnlock, true
	case "engine_on", "engine_start", "engine_off", "engine_stop":
		return PermBitEngine, true
	case "trunk_open", "trunk_close", "trunk":
		return PermBitTrunk, true
	case "window_open", "window_close", "window":
		return PermBitWindow, true
	case "climate_on", "climate_off", "climate":
		return PermBitClimate, true
	case "find", "vehicle_find":
		return PermBitFind, true
	case "seat_adjust", "seat":
		return PermBitSeat, true
	default:
		return 0, false
	}
}
