// Package bertlv 提供 BER-TLV 编解码功能
// 参考: ISO 7816-4, EMVco Book 3
package bertlv

// Tag 表示 BERTLV 标签类型
type Tag uint32

// 标签类别定义
const (
	TagClassUniversal     Tag = 0x00 // 通用类
	TagClassApplication  Tag = 0x40 // 应用类
	TagClassContextSpec   Tag = 0x80 // 上下文特定类
	TagClassPrivate       Tag = 0xC0 // 私有类
)

// 构造类型标志
const (
	TagConstructed uint8 = 0x20 // 构造类型标志
)

// 常用标签定义 (私有协议)
const (
	// 身份标识 (0xE0-0xEF)
	TagDeviceID     Tag = 0xE0 // 设备ID
	TagVehicleID    Tag = 0xE1 // 车辆ID
	TagKeyID        Tag = 0xE2 // 钥匙ID
	TagCommandID    Tag = 0xE3 // 命令ID
	TagStatusCode   Tag = 0xE4 // 状态码
	TagTimestamp    Tag = 0xE5 // 时间戳
	TagSignature    Tag = 0xE6 // 签名
	TagPublicKey    Tag = 0xE7 // 公钥
	TagCertificate  Tag = 0xE8 // 证书

	// 协议消息 (0xD0-0xDF)
	TagCCCMsg  Tag = 0xD0 // CCC (Car Connectivity Consortium) 消息
	TagICCOAMsg Tag = 0xD1 // ICCOA (ICCE for OA) 消息
	TagICCMsg  Tag = 0xD2 // ICCE (ICCE for OEM) 消息
	// 通用数据 (0xA0-0xAF)
	TagBleMac     Tag = 0xA0 // BLE MAC 地址
	TagUwbChannel Tag = 0xA1 // UWB 通道
	TagKeyType    Tag = 0xA2 // 钥匙类型
	TagAccessLevel Tag = 0xA3 // 访问级别

	// 消息头部 (0xF0-0xFF)
	TagVersion    Tag = 0xF0 // 版本
	TagMessageType Tag = 0xF1 // 消息类型
	TagSequenceNo Tag = 0xF2 // 序列号
)

// TagString 返回标签的字符串表示
func (t Tag) String() string {
	switch t {
	case TagDeviceID:
		return "DeviceID"
	case TagVehicleID:
		return "VehicleID"
	case TagKeyID:
		return "KeyID"
	case TagCommandID:
		return "CommandID"
	case TagStatusCode:
		return "StatusCode"
	case TagTimestamp:
		return "Timestamp"
	case TagSignature:
		return "Signature"
	case TagPublicKey:
		return "PublicKey"
	case TagCertificate:
		return "Certificate"
	case TagCCCMsg:
		return "CCCMsg"
	case TagICCOAMsg:
		return "ICCOAMsg"
	case TagICCE Msg:
		return "ICCMsg"
	case TagBleMac:
		return "BleMac"
	case TagUwbChannel:
		return "UwbChannel"
	case TagKeyType:
		return "KeyType"
	case TagAccessLevel:
		return "AccessLevel"
	case TagVersion:
		return "Version"
	case TagMessageType:
		return "MessageType"
	case TagSequenceNo:
		return "SequenceNo"
	default:
		return "Unknown"
	}
}

// IsConstructed 判断是否为构造类型
func (t Tag) IsConstructed() bool {
	return uint32(t)&uint32(TagConstructed) != 0
}

// GetClass 获取标签类别
func (t Tag) GetClass() Tag {
	return Tag(uint32(t) & 0xC0)
}

// GetTagNumber 获取标签编号
func (t Tag) GetTagNumber() uint32 {
	// 去掉类别位和构造位
	return uint32(t) & 0x1F
}
