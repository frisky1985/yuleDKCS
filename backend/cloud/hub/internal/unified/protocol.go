package unified

import (
	"context"
	"fmt"

	pb "github.com/digitalkey/hub/api/v1"
)

// Device 统一设备抽象 - 跨协议的设备模型
type Device struct {
	DeviceID   string            // 设备唯一标识 (UUID)
	Vendor     string            // 厂商: apple/samsung/xiaomi/oppo/vivo/huawei
	OS         string            // 操作系统: ios/android
	Protocol   ProtocolType       // 协商后的协议类型
	Capabilities *CapabilitySet   // 设备能力集
	
	// 协议特定字段 (按需填充)
	VendorDeviceID  string       // 厂商设备ID
	BLEMAC          string       // BLE MAC地址
	UWBID           string       // UWB ID
	NFCUID          string       // NFC UID
	SEID            string       // SE芯片ID
	
	// 协议协商中间数据
	SessionKey       []byte      // 会话密钥
	VehiclePubKey    []byte      // 车辆公钥
}

// ProtocolType 协议类型枚举
type ProtocolType int

const (
	ProtocolUnspecified ProtocolType = iota
	ProtocolCCC3           // CCC Digital Key 3.0
	ProtocolICCOA30        // ICCOA DK 3.0
	ProtocolICCOA40        // ICCOA DK 4.0
	ProtocolICCE            // ICCE (华为)
)

func (p ProtocolType) String() string {
	switch p {
	case ProtocolCCC3:
		return "ccc_dk3"
	case ProtocolICCOA30:
		return "iccoa_dk30"
	case ProtocolICCOA40:
		return "iccoa_dk40"
	case ProtocolICCE:
		return "icce"
	default:
		return "unknown"
	}
}

// ToProto 转换为 protobuf 枚举
func (p ProtocolType) ToProto() pb.Protocol {
	switch p {
	case ProtocolCCC3:
		return pb.Protocol_CCC_DK3
	case ProtocolICCOA30:
		return pb.Protocol_ICCOA_DK30
	case ProtocolICCOA40:
		return pb.Protocol_ICCOA_DK40
	case ProtocolICCE:
		return pb.Protocol_ICCE
	default:
		return pb.Protocol_PROTOCOL_UNSPECIFIED
	}
}

// FromProto 从 protobuf 枚举转换
func ProtocolFromProto(p pb.Protocol) ProtocolType {
	switch p {
	case pb.Protocol_CCC_DK3:
		return ProtocolCCC3
	case pb.Protocol_ICCOA_DK30:
		return ProtocolICCOA30
	case pb.Protocol_ICCOA_DK40:
		return ProtocolICCOA40
	case pb.Protocol_ICCE:
		return ProtocolICCE
	default:
		return ProtocolUnspecified
	}
}

// CapabilitySet 设备能力集
type CapabilitySet struct {
	BLE  bool   // BLE 支持
	UWB  bool   // UWB 定位支持
	NFC  bool   // NFC 支持
	SE   bool   // Secure Element 支持
	FiRa bool   // FiRa 协议栈 (UWB物理层)
	
	// 协议版本
	BLEVersion  string  // e.g. "5.0", "5.3"
	UWBVersion  string  // e.g. "FiRa 1.0"
	
	// 精度 (mm)
	UWBAccuracy int     // UWB 定位精度, 0=不支持
	
	// 距离限制 (mm), 0=无限制
	BLERange    int
	NFCRange    int
}

// String 实现 fmt.Stringer
func (c *CapabilitySet) String() string {
	return fmt.Sprintf("BLE=%t UWB=%t NFC=%t SE=%t FiRa=%t",
		c.BLE, c.UWB, c.NFC, c.SE, c.FiRa)
}

// ProtocolSpec 协议规范定义
type ProtocolSpec struct {
	Name         string
	MinVersion   string
	MaxVersion   string
	RequiredCaps *CapabilitySet
	OptionalCaps *CapabilitySet
}

// GetSpec 返回指定协议的规范
func GetSpec(p ProtocolType) *ProtocolSpec {
	specs := map[ProtocolType]*ProtocolSpec{
		ProtocolCCC3: {
			Name:       "CCC Digital Key 3.0",
			MinVersion: "3.0.0",
			MaxVersion: "3.1.0",
			RequiredCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: false, SE: true, FiRa: true,
			},
		},
		ProtocolICCOA30: {
			Name:       "ICCOA Digital Key 3.0",
			MinVersion: "3.0.0",
			MaxVersion: "3.0.9",
			RequiredCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: false, SE: true, FiRa: true,
			},
		},
		ProtocolICCOA40: {
			Name:       "ICCOA Digital Key 4.0",
			MinVersion: "4.0.0",
			MaxVersion: "4.0.9",
			RequiredCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true, FiRa: true,
			},
		},
		ProtocolICCE: {
			Name:       "ICCE Digital Key",
			MinVersion: "1.0.0",
			MaxVersion: "2.0.0",
			RequiredCaps: &CapabilitySet{
				BLE: true, UWB: true, NFC: true, SE: true, FiRa: true,
			},
		},
	}
	if spec, ok := specs[p]; ok {
		return spec
	}
	return nil
}

// MessageType 统一消息类型
type MessageType int

const (
	MsgTypeUnspecified    MessageType = iota
	MsgTypeDeviceInfo          // 设备信息交换
	MsgTypeCapabilityNegotiate // 能力协商
	MsgTypeKeyBind             // 密钥绑定
	MsgTypeKeyUnbind           // 密钥解绑
	MsgTypeKeyShare            // 密钥分享
	MsgTypeKeyAcceptShare      // 接收分享
	MsgTypeVehicleStatus       // 车辆状态上报
	MsgTypeRemoteControl       // 远程控制
	MsgTypeKeyRevoke           // 密钥撤销
	MsgTypeKeyRenew            // 密钥续期
	MsgTypeHeartbeat           // 心跳保活
)

// String 实现 fmt.Stringer
func (m MessageType) String() string {
	return [...]string{
		"Unspecified", "DeviceInfo", "CapabilityNegotiate", "KeyBind",
		"KeyUnbind", "KeyShare", "KeyAcceptShare", "VehicleStatus",
		"RemoteControl", "KeyRevoke", "KeyRenew", "Heartbeat",
	}[m]
}

// UnifiedMessage 统一消息格式 - 所有协议消息的公共容器
type UnifiedMessage struct {
	Type     MessageType     // 消息类型
	Device   *Device        // 关联设备
	Sequence uint64          // 序列号
	
	// 协议原生消息体 (按需填充其中一个)
	Raw []byte  // 原始字节 (未解析)
	
	// 解析后的消息 (语义级别)
	DeviceInfo  *DeviceInfoMessage
	KeyBind     *KeyBindMessage
	KeyShare    *KeyShareMessage
	VehicleStatus *VehicleStatusMessage
	RemoteControl *RemoteControlMessage
	
	// 元数据
	TraceID   string  // 链路追踪ID
	Timestamp int64   // Unix毫秒时间戳
}

// DeviceInfoMessage 设备信息消息
type DeviceInfoMessage struct {
	DeviceID    string
	Vendor      string
	OS          string
	OSVersion   string
	AppVersion  string
	Capabilities *CapabilitySet
	Timestamp   int64
}

// KeyBindMessage 密钥绑定消息
type KeyBindMessage struct {
	VehicleID    string
	UserID       string
	KeyType      pb.KeyType
	AccessLevel  *pb.AccessLevel
	DevicePubKey []byte  // DER格式公钥
	ValidFrom    int64
	ValidUntil   int64
}

// KeyShareMessage 密钥分享消息
type KeyShareMessage struct {
	KeyID       string
	ShareID     string
	RecipientID string
	AccessLevel *pb.AccessLevel
	ValidUntil  int64
}

// VehicleStatusMessage 车辆状态消息
type VehicleStatusMessage struct {
	VehicleID    string
	KeyID        string
	DoorsLocked  bool
	EngineOn     bool
	Location     *Location
	BatteryLevel int
	Timestamp    int64
}

// Location 位置信息
type Location struct {
	Latitude  float64
	Longitude float64
	Altitude  float64
	Accuracy  float64  // 米
}

// RemoteControlMessage 远程控制消息
type RemoteControlMessage struct {
	KeyID     string
	VehicleID string
	Action    RemoteAction
	Timestamp int64
}

// RemoteAction 远程控制动作
type RemoteAction int

const (
	ActionUnspecified RemoteAction = iota
	ActionLock
	ActionUnlock
	ActionEngineStart
	ActionEngineStop
	ActionTrunkOpen
	ActionTrunkClose
	ActionFindCar
	ActionClimateOn
	ActionClimateOff
)

func (a RemoteAction) String() string {
	return [...]string{
		"Unspecified", "Lock", "Unlock", "EngineStart", "EngineStop",
		"TrunkOpen", "TrunkClose", "FindCar", "ClimateOn", "ClimateOff",
	}[a]
}

// ToProtoAction 转换为 protobuf 动作
func (a RemoteAction) ToProtoAction() pb.RemoteAction {
	switch a {
	case ActionLock:
		return pb.RemoteAction_LOCK
	case ActionUnlock:
		return pb.RemoteAction_UNLOCK
	case ActionEngineStart:
		return pb.RemoteAction_ENGINE_START
	case ActionEngineStop:
		return pb.RemoteAction_ENGINE_STOP
	case ActionTrunkOpen:
		return pb.RemoteAction_TRUNK_OPEN
	case ActionTrunkClose:
		return pb.RemoteAction_TRUNK_CLOSE
	case ActionFindCar:
		return pb.RemoteAction_FIND_CAR
	case ActionClimateOn:
		return pb.RemoteAction_CLIMATE_ON
	case ActionClimateOff:
		return pb.RemoteAction_CLIMATE_OFF
	default:
		return pb.RemoteAction_ACTION_UNSPECIFIED
	}
}
