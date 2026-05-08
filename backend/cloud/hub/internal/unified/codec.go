package unified

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"reflect"
)

// MessageCodec 消息编解码器接口
// 每个协议实现自己的编解码器
type MessageCodec interface {
	// 编码
	Encode(msg *UnifiedMessage) ([]byte, error)
	// 解码
	Decode(data []byte) (*UnifiedMessage, error)
	// 获取协议类型
	Protocol() ProtocolType
}

// CodecRegistry 编解码器注册表
type CodecRegistry struct {
	codecs map[ProtocolType]MessageCodec
}

func NewCodecRegistry() *CodecRegistry {
	return &CodecRegistry{
		codecs: make(map[ProtocolType]MessageCodec),
	}
}

// Register 注册协议编解码器
func (r *CodecRegistry) Register(proto ProtocolType, codec MessageCodec) {
	r.codecs[proto] = codec
}

// Get 获取指定协议的编解码器
func (r *CodecRegistry) Get(proto ProtocolType) (MessageCodec, bool) {
	c, ok := r.codecs[proto]
	return c, ok
}

// UnifiedCodec 统一编解码器 - 支持自动检测协议并路由
type UnifiedCodec struct {
	registry *CodecRegistry
	buffers  map[ProtocolType][]byte  // 协议特定缓冲
}

// NewUnifiedCodec 创建统一编解码器
func NewUnifiedCodec(registry *CodecRegistry) *UnifiedCodec {
	return &UnifiedCodec{
		registry: registry,
		buffers:  make(map[ProtocolType][]byte),
	}
}

// Encode 使用指定协议编码消息
func (u *UnifiedCodec) Encode(proto ProtocolType, msg *UnifiedMessage) ([]byte, error) {
	codec, ok := u.registry.Get(proto)
	if !ok {
		return nil, fmt.Errorf("no codec registered for protocol: %s", proto)
	}
	return codec.Encode(msg)
}

// DecodeAuto 自动检测协议并解码
// data 的前几个字节用于协议识别
func (u *UnifiedCodec) DecodeAuto(data []byte) (*UnifiedMessage, error) {
	if len(data) < 2 {
		return nil, fmt.Errorf("data too short for protocol detection")
	}
	
	proto := u.detectProtocol(data)
	codec, ok := u.registry.Get(proto)
	if !ok {
		return nil, fmt.Errorf("no codec registered for detected protocol: %s", proto)
	}
	
	return codec.Decode(data)
}

// detectProtocol 根据数据内容自动检测协议
func (u *UnifiedCodec) detectProtocol(data []byte) ProtocolType {
	// 协议识别策略:
	// 1. ICCOA: TLV 格式, 首字节通常是 0x30 (SEQUENCE/CONSTRUCTED)
	// 2. CCC: 自定义帧格式, 首字节通常是固定值
	// 3. ICCE: BER-TLV, 首字节范围 0x80-0xBF
	
	if len(data) < 1 {
		return ProtocolUnspecified
	}
	
	first := data[0]
	
	// BER-TLV: 0x80-0xBF (constructed/private class)
	if first >= 0x80 && first <= 0xBF {
		// 可能是 ICCE 或 CCC
		if u.containsBERTLV(data) {
			return ProtocolICCE  // ICCE 使用标准 BER-TLV
		}
		return ProtocolCCC3
	}
	
	// ICCOA: 通常以 0x30 (ASN.1 SEQUENCE) 或自定义头
	if first == 0x30 || first == 0xA0 || first == 0xA1 {
		return ProtocolICCOA30
	}
	
	// ICCOA 4.0: 更多自定义格式
	if first == 0x5F || first == 0x7F {
		return ProtocolICCOA40
	}
	
	// ICCOA 变体
	if first >= 0xD0 && first <= 0xDF {
		return ProtocolICCOA40
	}
	
	// 默认 ICCOA 3.0
	return ProtocolICCOA30
}

// containsBERTLV 简单检查是否包含有效 BER-TLV
func (u *UnifiedCodec) containsBERTLV(data []byte) bool {
	if len(data) < 2 {
		return false
	}
	// 检查第二字节是否为合法的 length
	if data[1] <= 0x7F || data[1] == 0x81 || data[1] == 0x82 || data[1] == 0x83 {
		return true
	}
	return false
}

// ============================================================
// 具体协议编解码器实现
// ============================================================

// ICCOACodec ICCOA 协议编解码器 (DK3.0 / DK4.0)
// ICCOA 使用自定义 TLV 格式
type ICCOACodec struct {
	proto ProtocolType
}

func NewICCOACodec(proto ProtocolType) *ICCOACodec {
	return &ICCOACodec{proto: proto}
}

func (c *ICCOACodec) Protocol() ProtocolType { return c.proto }

func (c *ICCOACodec) Encode(msg *UnifiedMessage) ([]byte, error) {
	switch msg.Type {
	case MsgTypeKeyBind:
		return c.encodeKeyBind(msg)
	case MsgTypeKeyShare:
		return c.encodeKeyShare(msg)
	case MsgTypeRemoteControl:
		return c.encodeRemoteControl(msg)
	case MsgTypeVehicleStatus:
		return c.encodeVehicleStatus(msg)
	default:
		return nil, fmt.Errorf("unsupported message type: %s", msg.Type)
	}
}

func (c *ICCOACodec) Decode(data []byte) (*UnifiedMessage, error) {
	if len(data) < 3 {
		return nil, fmt.Errorf("data too short")
	}
	
	msgType := c.detectMessageType(data)
	msg := &UnifiedMessage{
		Type: msgType,
		Raw:  data,
	}
	
	switch msgType {
	case MsgTypeKeyBind:
		return c.decodeKeyBind(msg, data)
	case MsgTypeKeyShare:
		return c.decodeKeyShare(msg, data)
	case MsgTypeRemoteControl:
		return c.decodeRemoteControl(msg, data)
	case MsgTypeVehicleStatus:
		return c.decodeVehicleStatus(msg, data)
	default:
		return msg, nil
	}
}

func (c *ICCOACodec) detectMessageType(data []byte) MessageType {
	if len(data) < 1 {
		return MsgTypeUnspecified
	}
	first := data[0]
	switch {
	case first >= 0x01 && first <= 0x10:
		return MsgTypeDeviceInfo
	case first >= 0x11 && first <= 0x20:
		return MsgTypeKeyBind
	case first >= 0x21 && first <= 0x30:
		return MsgTypeKeyShare
	case first >= 0x31 && first <= 0x40:
		return MsgTypeRemoteControl
	default:
		return MsgTypeUnspecified
	}
}

func (c *ICCOACodec) encodeKeyBind(msg *UnifiedMessage) ([]byte, error) {
	kb := msg.KeyBind
	var buf bytes.Buffer
	
	// 帧头: 0xA0 (ICCOA frame type)
	buf.WriteByte(0xA0)
	
	// 消息类型: 0x11 (bind request)
	buf.WriteByte(0x11)
	
	// 序列号 (2 bytes)
	binary.BigEndian.PutUint16(make([]byte, 2), uint16(msg.Sequence))
	
	// TLV 编码各个字段
	if kb.VehicleID != "" {
		buf.Write(c.encodeTLV(0x01, []byte(kb.VehicleID)))
	}
	if kb.UserID != "" {
		buf.Write(c.encodeTLV(0x02, []byte(kb.UserID)))
	}
	if kb.DevicePubKey != nil {
		buf.Write(c.encodeTLV(0x03, kb.DevicePubKey))
	}
	
	// 时间戳 (8 bytes)
	binary.BigEndian.PutUint64(make([]byte, 8), uint64(kb.ValidFrom))
	
	return buf.Bytes(), nil
}

func (c *ICCOACodec) encodeKeyShare(msg *UnifiedMessage) ([]byte, error) {
	ks := msg.KeyShare
	var buf bytes.Buffer
	buf.WriteByte(0xA0)
	buf.WriteByte(0x21)  // share request
	binary.BigEndian.PutUint16(make([]byte, 2), uint16(msg.Sequence))
	
	buf.Write(c.encodeTLV(0x01, []byte(ks.KeyID)))
	if ks.RecipientID != "" {
		buf.Write(c.encodeTLV(0x02, []byte(ks.RecipientID)))
	}
	
	return buf.Bytes(), nil
}

func (c *ICCOACodec) encodeRemoteControl(msg *UnifiedMessage) ([]byte, error) {
	rc := msg.RemoteControl
	var buf bytes.Buffer
	buf.WriteByte(0xA0)
	buf.WriteByte(0x31)  // control request
	
	action := uint8(0)
	switch rc.Action {
	case ActionLock:
		action = 0x01
	case ActionUnlock:
		action = 0x02
	case ActionEngineStart:
		action = 0x03
	case ActionEngineStop:
		action = 0x04
	case ActionTrunkOpen:
		action = 0x05
	case ActionFindCar:
		action = 0x06
	}
	
	buf.WriteByte(action)
	binary.BigEndian.PutUint64(make([]byte, 8), uint64(rc.Timestamp))
	return buf.Bytes(), nil
}

func (c *ICCOACodec) encodeVehicleStatus(msg *UnifiedMessage) ([]byte, error) {
	vs := msg.VehicleStatus
	var buf bytes.Buffer
	buf.WriteByte(0xA0)
	buf.WriteByte(0x41)  // status report
	
	locked := byte(0)
	if vs.DoorsLocked {
		locked = 0x01
	}
	buf.WriteByte(locked)
	
	engine := byte(0)
	if vs.EngineOn {
		engine = 0x01
	}
	buf.WriteByte(engine)
	
	buf.WriteByte(byte(vs.BatteryLevel))
	return buf.Bytes(), nil
}

func (c *ICCOACodec) encodeTLV(tag uint8, value []byte) []byte {
	length := len(value)
	var header []byte
	if length <= 127 {
		header = []byte{tag, byte(length)}
	} else if length <= 255 {
		header = []byte{tag, 0x81, byte(length)}
	} else {
		header = []byte{tag, 0x82, byte(length >> 8), byte(length)}
	}
	return append(header, value...)
}

func (c *ICCOACodec) decodeKeyBind(msg *UnifiedMessage, data []byte) (*UnifiedMessage, error) {
	msg.KeyBind = &KeyBindMessage{}
	offset := 2  // skip header
	for offset < len(data) {
		tag := data[offset]
		offset++
		length, n := c.decodeLength(data[offset:])
		offset += n
		value := data[offset : offset+length]
		offset += length
		
		switch tag {
		case 0x01:
			msg.KeyBind.VehicleID = string(value)
		case 0x02:
			msg.KeyBind.UserID = string(value)
		case 0x03:
			msg.KeyBind.DevicePubKey = value
		}
	}
	return msg, nil
}

func (c *ICCOACodec) decodeKeyShare(msg *UnifiedMessage, data []byte) (*UnifiedMessage, error) {
	msg.KeyShare = &KeyShareMessage{}
	return msg, nil
}

func (c *ICCOACodec) decodeRemoteControl(msg *UnifiedMessage, data []byte) (*UnifiedMessage, error) {
	msg.RemoteControl = &RemoteControlMessage{}
	if len(data) < 3 {
		return msg, nil
	}
	switch data[2] {
	case 0x01:
		msg.RemoteControl.Action = ActionLock
	case 0x02:
		msg.RemoteControl.Action = ActionUnlock
	case 0x03:
		msg.RemoteControl.Action = ActionEngineStart
	case 0x04:
		msg.RemoteControl.Action = ActionEngineStop
	case 0x05:
		msg.RemoteControl.Action = ActionTrunkOpen
	case 0x06:
		msg.RemoteControl.Action = ActionFindCar
	}
	return msg, nil
}

func (c *ICCOACodec) decodeVehicleStatus(msg *UnifiedMessage, data []byte) (*UnifiedMessage, error) {
	msg.VehicleStatus = &VehicleStatusMessage{}
	if len(data) < 4 {
		return msg, nil
	}
	msg.VehicleStatus.DoorsLocked = data[2] == 0x01
	msg.VehicleStatus.EngineOn = data[3] == 0x01
	return msg, nil
}

func (c *ICCOACodec) decodeLength(data []byte) (int, int) {
	if data[0] <= 0x7F {
		return int(data[0]), 1
	}
	if data[0] == 0x81 {
		return int(data[1]), 2
	}
	if data[0] == 0x82 {
		return int(data[1])<<8 | int(data[2]), 3
	}
	return 0, 1
}

// ICCECodec ICCE 协议编解码器 - 基于 BER-TLV
type ICCECodec struct{}

func NewICCECodec() *ICCECodec {
	return &ICCECodec{}
}

func (c *ICCECodec) Protocol() ProtocolType { return ProtocolICCE }

// Encode 实现 MessageCodec 接口
func (c *ICCECodec) Encode(msg *UnifiedMessage) ([]byte, error) {
	var buf bytes.Buffer
	switch msg.Type {
	case MsgTypeKeyBind:
		kb := msg.KeyBind
		buf.Write(c.encodeTag(0x9F01))  // ICCE_MSG_BIND
		// device ID
		if kb.DevicePubKey != nil {
			buf.Write(c.encodeTLV(0x9F02, kb.DevicePubKey))
		}
		// user ID
		if kb.UserID != "" {
			buf.Write(c.encodeTLV(0x9F03, []byte(kb.UserID)))
		}
		return buf.Bytes(), nil
		
	case MsgTypeRemoteControl:
		rc := msg.RemoteControl
		action := c.actionToTag(rc.Action)
		buf.Write(c.encodeTag(0x9F10))  // ICCE_MSG_CONTROL
		buf.Write(c.encodeTLV(0x9F11, []byte{action}))
		return buf.Bytes(), nil
		
	case MsgTypeVehicleStatus:
		vs := msg.VehicleStatus
		buf.Write(c.encodeTag(0x9F20))  // ICCE_MSG_STATUS
		locked := byte(0)
		if vs.DoorsLocked {
			locked = 0x01
		}
		buf.Write(c.encodeTLV(0x9F21, []byte{locked}))
		return buf.Bytes(), nil
		
	default:
		return nil, fmt.Errorf("unsupported ICCE message type: %s", msg.Type)
	}
}

// Decode 实现 MessageCodec 接口
func (c *ICCECodec) Decode(data []byte) (*UnifiedMessage, error) {
	if len(data) < 2 {
		return nil, fmt.Errorf("ICCE data too short")
	}
	
	msg := &UnifiedMessage{
		Raw: data,
		Type: c.detectICCEType(data),
	}
	
	switch msg.Type {
	case MsgTypeKeyBind:
		msg.KeyBind = &KeyBindMessage{}
		offset := 0
		for offset < len(data) {
			tag, tagLen := c.decodeTag(data[offset:])
			offset += tagLen
			length, lenLen := c.decodeLength(data[offset:])
			offset += lenLen
			value := data[offset : offset+length]
			offset += length
			
			switch tag {
			case 0x9F02:
				msg.KeyBind.DevicePubKey = value
			case 0x9F03:
				msg.KeyBind.UserID = string(value)
			}
		}
	case MsgTypeRemoteControl:
		msg.RemoteControl = &RemoteControlMessage{}
		// Simple single-byte decode
		if len(data) >= 3 {
			msg.RemoteControl.Action = c.tagToAction(data[2])
		}
	}
	
	return msg, nil
}

func (c *ICCECodec) detectICCEType(data []byte) MessageType {
	if len(data) < 1 {
		return MsgTypeUnspecified
	}
	first := data[0]
	switch {
	case first == 0x9F || (first >= 0x80 && first <= 0x9F):
		return MsgTypeDeviceInfo
	case first == 0x9F01 || first == 0x9F02:
		return MsgTypeKeyBind
	case first == 0x9F10:
		return MsgTypeRemoteControl
	case first == 0x9F20:
		return MsgTypeVehicleStatus
	default:
		return MsgTypeUnspecified
	}
}

func (c *ICCECodec) encodeTag(tag uint16) []byte {
	if tag <= 0x1F {
		return []byte{byte(tag)}
	}
	if tag >= 0x9F00 && tag <= 0x9FFF {
		return []byte{byte(tag >> 8), byte(tag)}
	}
	// Multi-byte
	return []byte{byte(tag >> 8), byte(tag)}
}

func (c *ICCECodec) decodeTag(data []byte) (uint16, int) {
	if len(data) < 1 {
		return 0, 0
	}
	if data[0] == 0x9F && len(data) >= 2 {
		return uint16(data[0])<<8 | uint16(data[1]), 2
	}
	return uint16(data[0]), 1
}

func (c *ICCECodec) encodeTLV(tag uint16, value []byte) []byte {
	length := len(value)
	var header []byte
	if length <= 127 {
		header = []byte{byte(tag), byte(length)}
	} else if length <= 255 {
		header = []byte{byte(tag), 0x81, byte(length)}
	} else {
		header = []byte{byte(tag), 0x82, byte(length >> 8), byte(length)}
	}
	return append(header, value...)
}

func (c *ICCECodec) decodeLength(data []byte) (int, int) {
	if data[0] <= 0x7F {
		return int(data[0]), 1
	}
	if data[0] == 0x81 {
		return int(data[1]), 2
	}
	if data[0] == 0x82 {
		return int(data[1])<<8 | int(data[2]), 3
	}
	return 0, 1
}

func (c *ICCECodec) actionToTag(a RemoteAction) uint8 {
	switch a {
	case ActionLock:
		return 0x01
	case ActionUnlock:
		return 0x02
	case ActionEngineStart:
		return 0x03
	case ActionEngineStop:
		return 0x04
	case ActionTrunkOpen:
		return 0x05
	case ActionFindCar:
		return 0x06
	default:
		return 0x00
	}
}

func (c *ICCECodec) tagToAction(b byte) RemoteAction {
	switch b {
	case 0x01:
		return ActionLock
	case 0x02:
		return ActionUnlock
	case 0x03:
		return ActionEngineStart
	case 0x04:
		return ActionEngineStop
	case 0x05:
		return ActionTrunkOpen
	case 0x06:
		return ActionFindCar
	default:
		return ActionUnspecified
	}
}

// CCCCodec CCC 协议编解码器 - 基于 FiRa HCP
type CCCCodec struct{}

func NewCCCCodec() *CCCCodec {
	return &CCCCodec{}
}

func (c *CCCCodec) Protocol() ProtocolType { return ProtocolCCC3 }

func (c *CCCCodec) Encode(msg *UnifiedMessage) ([]byte, error) {
	var buf bytes.Buffer
	// CCC 使用 FiRa HCP (Host Controller Protocol)
	// 简化版: 自定义帧格式
	buf.WriteByte(0x5C)  // CCC frame marker
	
	switch msg.Type {
	case MsgTypeKeyBind:
		buf.WriteByte(0x10)  // bind
		if msg.KeyBind != nil {
			buf.Write(c.encodeCCCValue(msg.KeyBind.VehicleID))
			buf.Write(c.encodeCCCValue(msg.KeyBind.UserID))
		}
	case MsgTypeRemoteControl:
		buf.WriteByte(0x30)  // control
		if msg.RemoteControl != nil {
			buf.WriteByte(byte(msg.RemoteControl.Action))
		}
	case MsgTypeVehicleStatus:
		buf.WriteByte(0x40)  // status
		if msg.VehicleStatus != nil {
			status := byte(0)
			if msg.VehicleStatus.DoorsLocked {
				status |= 0x01
			}
			buf.WriteByte(status)
		}
	default:
		return nil, fmt.Errorf("unsupported CCC message type")
	}
	
	return buf.Bytes(), nil
}

func (c *CCCCodec) Decode(data []byte) (*UnifiedMessage, error) {
	if len(data) < 2 || data[0] != 0x5C {
		return nil, fmt.Errorf("invalid CCC frame")
	}
	
	msg := &UnifiedMessage{
		Raw: data,
	}
	
	switch data[1] {
	case 0x10:
		msg.Type = MsgTypeKeyBind
		msg.KeyBind = &KeyBindMessage{}
	case 0x30:
		msg.Type = MsgTypeRemoteControl
		msg.RemoteControl = &RemoteControlMessage{}
		if len(data) >= 3 {
			msg.RemoteControl.Action = RemoteAction(data[2])
		}
	case 0x40:
		msg.Type = MsgTypeVehicleStatus
		msg.VehicleStatus = &VehicleStatusMessage{}
		if len(data) >= 3 {
			msg.VehicleStatus.DoorsLocked = data[2]&0x01 != 0
		}
	}
	
	return msg, nil
}

func (c *CCCCodec) encodeCCCValue(s string) []byte {
	length := len(s)
	return append([]byte{byte(length)}, []byte(s)...)
}

// GetCodecForProtocol 根据协议类型获取编解码器实例
func GetCodecForProtocol(proto ProtocolType) MessageCodec {
	switch proto {
	case ProtocolICCOA30, ProtocolICCOA40:
		return NewICCOACodec(proto)
	case ProtocolICCE:
		return NewICCECodec()
	case ProtocolCCC3:
		return NewCCCCodec()
	default:
		return nil
	}
}

// ---- Helpers ----

func encodeField(buf *bytes.Buffer, tag byte, val interface{}) {
	switch v := val.(type) {
	case string:
		buf.WriteByte(tag)
		buf.WriteByte(byte(len(v)))
		buf.WriteString(v)
	case []byte:
		buf.WriteByte(tag)
		buf.WriteByte(byte(len(v)))
		buf.Write(v)
	case uint64:
		b := make([]byte, 8)
		binary.BigEndian.PutUint64(b, v)
		buf.WriteByte(tag)
		buf.WriteByte(byte(len(b)))
		buf.Write(b)
	case int64:
		b := make([]byte, 8)
		binary.BigEndian.PutUint64(b, uint64(v))
		buf.WriteByte(tag)
		buf.WriteByte(byte(len(b)))
		buf.Write(b)
	}
}

// toUnifiedMessage 将 adapter 层 pb 模型转换为统一消息
func ToUnifiedMessage(req interface{}) *UnifiedMessage {
	msg := &UnifiedMessage{}
	
	switch r := req.(type) {
	case interface{ GetDeviceId() string }:
		if dev := r; dev != nil {
			msg.Device = &Device{DeviceID: dev.GetDeviceId()}
		}
	}
	
	_ = reflect.TypeOf(req)
	return msg
}
