// Package bertlv 提供数字钥匙协议栈的 BERTLV 编解码功能
// 参考: cloud/protocol/encoding-rules.md v1.0.0
package bertlv

import (
	"errors"
	"fmt"
)

// ==================== 错误码 ====================

var (
	ErrInvalidTag      = errors.New("bertlv: 无效Tag (E001)")
	ErrInvalidLength   = errors.New("bertlv: 无效Length (E002)")
	ErrDataTruncated   = errors.New("bertlv: 数据截断 (E003)")
	ErrNestingTooDeep  = errors.New("bertlv: 嵌套深度超限 (E004)")
	ErrUnsupportedTag  = errors.New("bertlv: 不支持的Tag (E005)")
)

// MaxNestingDepth 最大嵌套深度限制
const MaxNestingDepth = 16

// ==================== TLV 结构体 ====================

// TLV 表示一个 BERTLV 编码的数据单元
type TLV struct {
	Tag    Tag
	Length int
	Value  []byte
	// Children 仅在构造类型(Constructed)时使用
	Children []*TLV
}

// ==================== Tag 定义 ====================

// Tag 表示 BERTLV 标签
type Tag struct {
	Class      uint8 // 00=universal, 01=application, 10=context-specific, 11=private
	Constructed bool // true=构造类型, false=原始类型
	Number     uint32
}

// ==================== 预定义 Tag 常量 ====================

// Universal 标签
const (
	TagBoolean     = uint8(0x01)
	TagInteger     = uint8(0x02)
	TagBitString   = uint8(0x03)
	TagOctetString = uint8(0x04)
	TagNULL        = uint8(0x05)
	TagUTF8String  = uint8(0x0C)
	TagNumeric     = uint8(0x12)
	TagAlphaNumeric = uint8(0x13)
	TagSequence    = uint8(0x30) // Constructed
)

// HUB-DKCS 消息头标签 (E1 xx)
const (
	TagVersion      = 0xE101
	TagTimestamp    = 0xE102
	TagMessageType  = 0xE103
	TagSequenceNo   = 0xE104
	TagDeviceId     = 0xE105
	TagSessionId    = 0xE106
	TagPriority     = 0xE107
	TagFlags        = 0xE108
	TagCorrelationId = 0xE109
	TagTrailer      = 0xE1FF
)

// HUB-DKCS 消息体标签 (A0 xx)
const (
	TagVehicleId    = 0xA001
	TagVin          = 0xA002
	TagDeviceIdBody = 0xA003
	TagUserId       = 0xA004
	TagVendor       = 0xA005
	TagProtocol     = 0xA006
	TagKeyType      = 0xA007
	TagAccessLevel  = 0xA008
	TagDevicePubkey = 0xA009
	TagDeviceCert   = 0xA00A
	TagValidFrom    = 0xA00B
	TagValidUntil   = 0xA00C
	TagMaxUses      = 0xA00D
	TagDistanceLimit = 0xA010
	TagKeyId        = 0xA011
	TagStatus       = 0xA012
	TagVehiclePubkey = 0xA013
	TagSharedSecret = 0xA014
	TagResultCode   = 0xA015
	TagReason       = 0xA016
	TagEmergency    = 0xA017
	TagAction       = 0xA018
	TagCmdId        = 0xA019
	TagParams       = 0xA01A
	TagSource       = 0xA01B
	TagWaitResponse = 0xA01C
	TagTimeoutMs    = 0xA01D
)

// DKCS-TCU 消息体标签 (B0 xx)
const (
	TagTCUKeyRef    = 0xB001
	TagTCUProtocol  = 0xB002
	TagTCUAccessType = 0xB003
	TagTCUAccessLevel = 0xB004
	TagTCUSeKeyRef  = 0xB005
	TagTCUVehicleKeyRef = 0xB006
	TagTCUBleChannel = 0xB007
	TagTCUBleMac    = 0xB008
	TagTCUUwbChannel = 0xB009
	TagTCUUwbPreamble = 0xB00A
	TagTCUTcuId     = 0xB011
	TagTCUFirmware  = 0xB012
	TagTCULockStatus = 0xB013
	TagTCUEngineStatus = 0xB014
	TagTCUDoorStatus = 0xB015
	TagTCUWindowStatus = 0xB016
	TagTCUBatteryVoltage = 0xB017
	TagTCUBatteryPct = 0xB018
	TagTCUInteriorTemp = 0xB019
	TagTCUOdometer  = 0xB01A
	TagTCUAlarmStatus = 0xB01B
	TagTCULatitude  = 0xB01C
	TagTCULongitude = 0xB01D
	TagTCUSignalStrength = 0xB01E
	TagTCULatencyMs = 0xB020
)

// ==================== 编码函数 ====================

// EncodeTag 编码标签
func EncodeTag(tag Tag) []byte {
	firstByte := uint8(tag.Class<<6) | uint8(tag.Number&0x1F)
	if tag.Constructed {
		firstByte |= 0x20
	}

	if tag.Number < 0x1F {
		return []byte{firstByte}
	}

	// 多字节 Tag 编码
	result := []byte{firstByte | 0x1F}
	n := tag.Number
	bytes := make([]byte, 0)
	for n > 0 {
		bytes = append([]byte{uint8(n & 0x7F)}, bytes...)
		n >>= 7
	}
	// 设置后续字节的最高位（最后一个字节除外）
	for i := 0; i < len(bytes)-1; i++ {
		bytes[i] |= 0x80
	}
	result = append(result, bytes...)
	return result
}

// EncodeTagUint16 编码双字节标签 (如 E101, A001, B001)
func EncodeTagUint16(tag uint16) []byte {
	return []byte{uint8(tag >> 8), uint8(tag & 0xFF)}
}

// EncodeLength 编码长度字段
func EncodeLength(length int) []byte {
	if length < 0x80 {
		return []byte{uint8(length)}
	}
	if length <= 0xFF {
		return []byte{0x81, uint8(length)}
	}
	if length <= 0xFFFF {
		return []byte{0x82, uint8(length >> 8), uint8(length & 0xFF)}
	}
	return []byte{0x83, uint8(length >> 16), uint8((length >> 8) & 0xFF), uint8(length & 0xFF)}
}

// EncodeTLV 编码单个 TLV 元素
func EncodeTLV(tag Tag, value []byte) []byte {
	tagBytes := EncodeTag(tag)
	lengthBytes := EncodeLength(len(value))
	result := make([]byte, 0, len(tagBytes)+len(lengthBytes)+len(value))
	result = append(result, tagBytes...)
	result = append(result, lengthBytes...)
	result = append(result, value...)
	return result
}

// EncodeTLVUint16 使用 uint16 标签编码 TLV
func EncodeTLVUint16(tag uint16, value []byte) []byte {
	tagBytes := EncodeTagUint16(tag)
	lengthBytes := EncodeLength(len(value))
	result := make([]byte, 0, len(tagBytes)+len(lengthBytes)+len(value))
	result = append(result, tagBytes...)
	result = append(result, lengthBytes...)
	result = append(result, value...)
	return result
}

// EncodeConstructed 编码构造类型 TLV（包含子元素）
func EncodeConstructed(tag Tag, children ...*TLV) *TLV {
	tlv := &TLV{
		Tag:         tag,
		Constructed: true,
		Children:    children,
	}
	return tlv
}

// EncodeConstructedBytes 将构造类型序列化为字节
func EncodeConstructedBytes(tag Tag, children ...[]byte) []byte {
	// 计算子元素总长度
	totalLen := 0
	for _, child := range children {
		totalLen += len(child)
	}
	tagBytes := EncodeTag(tag)
	lengthBytes := EncodeLength(totalLen)
	result := make([]byte, 0, len(tagBytes)+len(lengthBytes)+totalLen)
	result = append(result, tagBytes...)
	result = append(result, lengthBytes...)
	for _, child := range children {
		result = append(result, child...)
	}
	return result
}

// EncodeConstructedUint16 使用 uint16 标签编码构造类型
func EncodeConstructedUint16(tag uint16, children ...[]byte) []byte {
	totalLen := 0
	for _, child := range children {
		totalLen += len(child)
	}
	tagBytes := EncodeTagUint16(tag)
	lengthBytes := EncodeLength(totalLen)
	result := make([]byte, 0, len(tagBytes)+len(lengthBytes)+totalLen)
	result = append(result, tagBytes...)
	result = append(result, lengthBytes...)
	for _, child := range children {
		result = append(result, child...)
	}
	return result
}

// ==================== 解码函数 ====================

// DecodeTLV 从字节切片解码 TLV
func DecodeTLV(data []byte) (*TLV, error) {
	return decodeTLVWithDepth(data, 0)
}

func decodeTLVWithDepth(data []byte, depth int) (*TLV, error) {
	if depth > MaxNestingDepth {
		return nil, ErrNestingTooDeep
	}
	if len(data) == 0 {
		return nil, ErrDataTruncated
	}

	pos := 0

	// 解析 Tag
	tag, bytesRead, err := decodeTag(data[pos:])
	if err != nil {
		return nil, fmt.Errorf("%w: %v", ErrInvalidTag, err)
	}
	pos += bytesRead

	// 解析 Length
	length, bytesRead, err := decodeLength(data[pos:])
	if err != nil {
		return nil, fmt.Errorf("%w: %v", ErrInvalidLength, err)
	}
	pos += bytesRead

	// 解析 Value
	if pos+length > len(data) {
		return nil, ErrDataTruncated
	}
	value := data[pos : pos+length]

	tlv := &TLV{
		Tag:    tag,
		Length: length,
		Value:  value,
	}

	// 如果是构造类型，递归解码子元素
	if tag.Constructed {
		children, err := decodeChildren(value, depth+1)
		if err != nil {
			return nil, err
		}
		tlv.Children = children
	}

	return tlv, nil
}

// DecodeTLVList 从字节切片解码多个 TLV 元素
func DecodeTLVList(data []byte) ([]*TLV, error) {
	return decodeChildren(data, 0)
}

func decodeChildren(data []byte, depth int) ([]*TLV, error) {
	var result []*TLV
	pos := 0
	for pos < len(data) {
		tlv, err := decodeTLVWithDepth(data[pos:], depth)
		if err != nil {
			return nil, err
		}
		result = append(result, tlv)
		pos += totalTLVLength(tlv)
	}
	return result, nil
}

func totalTLVLength(tlv *TLV) int {
	// Tag 长度
	tagLen := len(EncodeTag(tlv.Tag))
	// Length 长度
	lengthLen := len(EncodeLength(tlv.Length))
	return tagLen + lengthLen + tlv.Length
}

func decodeTag(data []byte) (Tag, int, error) {
	if len(data) == 0 {
		return Tag{}, 0, fmt.Errorf("empty data")
	}
	firstByte := data[0]
	tag := Tag{
		Class:      (firstByte >> 6) & 0x03,
		Constructed: (firstByte & 0x20) != 0,
		Number:     uint32(firstByte & 0x1F),
	}
	bytesRead := 1

	if firstByte&0x1F == 0x1F {
		// 多字节 Tag
		for {
			if bytesRead >= len(data) {
				return Tag{}, 0, fmt.Errorf("multi-byte tag truncated")
			}
			b := data[bytesRead]
			bytesRead++
			tag.Number = (tag.Number << 7) | uint32(b&0x7F)
			if b&0x80 == 0 {
				break
			}
		}
	}
	return tag, bytesRead, nil
}

func decodeLength(data []byte) (int, int, error) {
	if len(data) == 0 {
		return 0, 0, fmt.Errorf("empty data")
	}
	firstByte := data[0]
	if firstByte < 0x80 {
		return int(firstByte), 1, nil
	}
	numBytes := int(firstByte & 0x7F)
	if numBytes == 0 {
		return 0, 0, fmt.Errorf("indefinite length not supported")
	}
	if numBytes > 4 {
		return 0, 0, fmt.Errorf("length too large: %d bytes", numBytes)
	}
	if len(data) < 1+numBytes {
		return 0, 0, fmt.Errorf("length bytes truncated")
	}
	length := 0
	for i := 0; i < numBytes; i++ {
		length = (length << 8) | int(data[1+i])
	}
	return length, 1 + numBytes, nil
}

// ==================== 辅助函数 ====================

// GetTagUint16 尝试将 TLV 的 Tag 解析为 uint16 (双字节标签)
func (t *TLV) GetTagUint16() uint16 {
	return uint16(t.Tag.Class<<14) | uint16(0x3F80&0) | uint16(t.Tag.Number)
}

// FindChild 在子元素中查找指定标签的 TLV
func FindChild(children []*TLV, tag Tag) *TLV {
	for _, child := range children {
		if child.Tag == tag {
			return child
		}
	}
	return nil
}

// String 返回 TLV 的可读字符串
func (t *TLV) String() string {
	classStr := []string{"Universal", "Application", "Context", "Private"}
	typeStr := "Primitive"
	if t.Tag.Constructed {
		typeStr = "Constructed"
	}
	if len(t.Children) > 0 {
		return fmt.Sprintf("TLV[Class=%s, %s, Tag=%02X, Len=%d, Children=%d]",
			classStr[t.Tag.Class], typeStr, t.Tag.Number, t.Length, len(t.Children))
	}
	return fmt.Sprintf("TLV[Class=%s, %s, Tag=%02X, Len=%d, Value=%X]",
		classStr[t.Tag.Class], typeStr, t.Tag.Number, t.Length, t.Value)
}

// FormatTLV 格式化 TLV 列表为可读字符串（用于调试）
func FormatTLV(data []byte) string {
	tlvs, err := DecodeTLVList(data)
	if err != nil {
		return fmt.Sprintf("解码失败: %v", err)
	}
	var result string
	for _, tlv := range tlvs {
		result += tlv.String() + "\n"
	}
	return result
}
