// decoder.go - BERTLV解码器
// 数字钥匙私有协议 - BERTLV解码实现
// 基于 cloud/protocol/encoding-rules.md 规范

package bertlv

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
)

// 解码错误定义
var (
	ErrInvalidTag      = errors.New("无效的Tag标签")
	ErrInvalidLength   = errors.New("无效的Length长度")
	ErrBufferTooShort  = errors.New("数据缓冲区太短")
	ErrUnexpectedEnd   = errors.New("数据意外结束")
	ErrInvalidEncoding = errors.New("无效的编码格式")
	ErrTagNotMatch     = errors.New("Tag不匹配")
)

// Decoder BERTLV解码器
type Decoder struct {
	reader *bytes.Reader
}

// TLV解析结果
type TLV struct {
	Tag    Tag
	Length int
	Value  []byte
	Raw    []byte // 原始数据（含Tag和Length）
}

// 解码错误详情
type DecodeError struct {
	Offset int
	Err    error
}

func (e *DecodeError) Error() string {
	return fmt.Sprintf("解码错误 at offset %d: %v", e.Offset, e.Err)
}

// NewDecoder 创建解码器
func NewDecoder(data []byte) *Decoder {
	return &Decoder{
		reader: bytes.NewReader(data),
	}
}

// Decode 解码单个TLV
func Decode(data []byte) (*TLV, error) {
	decoder := NewDecoder(data)
	return decoder.Decode()
}

// Decode 解码单个TLV
func (d *Decoder) Decode() (*TLV, error) {
	offset := d.reader.Size() - int64(d.reader.Len())
	
	// 解析Tag
	tag, err := d.readTag()
	if err != nil {
		return nil, &DecodeError{Offset: int(offset), Err: err}
	}
	
	// 解析Length
	length, err := d.readLength()
	if err != nil {
		return nil, &DecodeError{Offset: int(offset), Err: err}
	}
	
	// 检查数据长度
	if d.reader.Len() < length {
		return nil, &DecodeError{Offset: int(offset), Err: ErrBufferTooShort}
	}
	
	// 读取Value
	value := make([]byte, length)
	if _, err := d.reader.Read(value); err != nil {
		return nil, &DecodeError{Offset: int(offset), Err: ErrUnexpectedEnd}
	}
	
	// 计算Raw长度
	rawLen := len(d.intToBytes(tag, 0)) + d.lengthBytesLen(length) + length
	raw := make([]byte, 0, rawLen)
	raw = append(raw, d.intToBytes(tag, 0)...)
	raw = append(raw, d.encodeLength(length)...)
	raw = append(raw, value...)
	
	return &TLV{
		Tag:    tag,
		Length: length,
		Value:  value,
		Raw:    raw,
	}, nil
}

// DecodeAll 解码所有TLV
func DecodeAll(data []byte) ([]*TLV, error) {
	decoder := NewDecoder(data)
	var tlvs []*TLV
	
	for decoder.reader.Len() > 0 {
		tlv, err := decoder.Decode()
		if err != nil {
			return nil, err
		}
		tlvs = append(tlvs, tlv)
	}
	
	return tlvs, nil
}

// DecodeNested 解码嵌套TLV（Value中包含TLV）
func (t *TLV) DecodeNested() ([]*TLV, error) {
	return DecodeAll(t.Value)
}

// DecodeToUint8 解码为uint8
func (t *TLV) DecodeToUint8() (uint8, error) {
	if len(t.Value) < 1 {
		return 0, ErrBufferTooShort
	}
	return t.Value[0], nil
}

// DecodeToUint16 解码为uint16 (大端序)
func (t *TLV) DecodeToUint16() (uint16, error) {
	if len(t.Value) < 2 {
		return 0, ErrBufferTooShort
	}
	return binary.BigEndian.Uint16(t.Value), nil
}

// DecodeToUint32 解码为uint32 (大端序)
func (t *TLV) DecodeToUint32() (uint32, error) {
	if len(t.Value) < 4 {
		return 0, ErrBufferTooShort
	}
	return binary.BigEndian.Uint32(t.Value), nil
}

// DecodeToUint64 解码为uint64 (大端序)
func (t *TLV) DecodeToUint64() (uint64, error) {
	if len(t.Value) < 8 {
		return 0, ErrBufferTooShort
	}
	return binary.BigEndian.Uint64(t.Value), nil
}

// DecodeToInt8 解码为int8
func (t *TLV) DecodeToInt8() (int8, error) {
	val, err := t.DecodeToUint8()
	return int8(val), err
}

// DecodeToInt16 解码为int16 (大端序)
func (t *TLV) DecodeToInt16() (int16, error) {
	val, err := t.DecodeToUint16()
	return int16(val), err
}

// DecodeToInt32 解码为int32 (大端序)
func (t *TLV) DecodeToInt32() (int32, error) {
	val, err := t.DecodeToUint32()
	return int32(val), err
}

// DecodeToInt64 解码为int64 (大端序)
func (t *TLV) DecodeToInt64() (int64, error) {
	val, err := t.DecodeToUint64()
	return int64(val), err
}

// DecodeToString 解码为字符串
func (t *TLV) DecodeToString() (string, error) {
	return string(t.Value), nil
}

// DecodeToBytes 解码为字节数组
func (t *TLV) DecodeToBytes() ([]byte, error) {
	return t.Value, nil
}

// DecodeToBool 解码为布尔值
func (t *TLV) DecodeToBool() (bool, error) {
	if len(t.Value) < 1 {
		return false, ErrBufferTooShort
	}
	return t.Value[0] != 0, nil
}

// readTag 读取Tag
func (d *Decoder) readTag() (Tag, error) {
	// Tag为1-4字节
	var tag Tag = 0
	firstByte, err := d.reader.ReadByte()
	if err != nil {
		return 0, ErrBufferTooShort
	}
	
	tag = Tag(firstByte)
	
	// 检查后续字节 (高两位为11表示有多字节Tag)
	if firstByte&0x1F == 0x1F {
		for {
			if d.reader.Len() == 0 {
				return 0, ErrInvalidTag
			}
			nextByte, err := d.reader.ReadByte()
			if err != nil {
				return 0, ErrInvalidTag
			}
			tag = (tag << 8) | Tag(nextByte)
			// 高两位不为10则继续读取
			if nextByte&0x80 == 0 {
				break
			}
		}
	}
	
	return tag, nil
}

// readLength 读取Length
func (d *Decoder) readLength() (int, error) {
	firstByte, err := d.reader.ReadByte()
	if err != nil {
		return 0, ErrBufferTooShort
	}
	
	// 短格式: 0x00-0x7F
	if firstByte < 0x80 {
		return int(firstByte), nil
	}
	
	// 长格式: 0x81-0x84表示后续字节数
	lengthBytes := int(firstByte & 0x7F)
	if lengthBytes > 3 {
		return 0, ErrInvalidLength
	}
	
	if d.reader.Len() < lengthBytes {
		return 0, ErrBufferTooShort
	}
	
	length := 0
	for i := 0; i < lengthBytes; i++ {
		b, err := d.reader.ReadByte()
		if err != nil {
			return 0, ErrBufferTooShort
		}
		length = (length << 8) | int(b)
	}
	
	return length, nil
}

// lengthBytesLen 计算Length字段的字节数
func (d *Decoder) lengthBytesLen(length int) int {
	if length < 0x80 {
		return 1
	}
	if length < 0x100 {
		return 2
	}
	if length < 0x10000 {
		return 3
	}
	return 4
}

// intToBytes 将Tag转换为字节数组
func (d *Decoder) intToBytes(tag Tag, minBytes int) []byte {
	var bytes []byte
	t := uint32(tag)
	
	// 先计算需要多少字节
	if t < 0x100 {
		bytes = []byte{byte(t)}
	} else if t < 0x10000 {
		bytes = []byte{byte(t >> 8), byte(t)}
	} else if t < 0x1000000 {
		bytes = []byte{byte(t >> 16), byte(t >> 8), byte(t)}
	} else {
		bytes = []byte{byte(t >> 24), byte(t >> 16), byte(t >> 8), byte(t)}
	}
	
	// 补齐最小字节数
	for len(bytes) < minBytes {
		bytes = append([]byte{0}, bytes...)
	}
	
	return bytes
}

// encodeLength 编码Length字段
func (d *Decoder) encodeLength(length int) []byte {
	if length < 0x80 {
		return []byte{byte(length)}
	}
	
	var bytes []byte
	// 计算长度字节数
	if length < 0x100 {
		bytes = []byte{0x81, byte(length)}
	} else if length < 0x10000 {
		bytes = []byte{0x82, byte(length >> 8), byte(length)}
	} else {
		bytes = []byte{0x83, byte(length >> 16), byte(length >> 8), byte(length)}
	}
	
	return bytes
}

// Skip 跳过当前TLV
func (d *Decoder) Skip() error {
	tlv, err := d.Decode()
	if err != nil {
		return err
	}
	_ = tlv // tlv已被消费
	return nil
}

// PeekTag 预览下一个Tag（不消费数据）
func (d *Decoder) PeekTag() (Tag, error) {
	if d.reader.Len() < 1 {
		return 0, ErrBufferTooShort
	}
	
	// 保存当前位置
	pos := d.reader.Size() - int64(d.reader.Len())
	
	tag, err := d.readTag()
	
	// 恢复位置
	d.reader.Seek(pos, 0)
	
	return tag, err
}

// DecodeWithSchema 按Schema解码
func DecodeWithSchema(data []byte, schema []Tag) ([]*TLV, error) {
	decoder := NewDecoder(data)
	var result []*TLV
	
	for _, expectedTag := range schema {
		tlv, err := decoder.Decode()
		if err != nil {
			return nil, fmt.Errorf("期望Tag 0x%04X: %w", expectedTag, err)
		}
		if tlv.Tag != expectedTag {
			return nil, &DecodeError{
				Offset: 0,
				Err:    fmt.Errorf("%w: 期望 0x%04X, 实际 0x%04X", ErrTagNotMatch, expectedTag, tlv.Tag),
			}
		}
		result = append(result, tlv)
	}
	
	return result, nil
}

// DecodeMap 将TLV列表转换为Map（方便访问）
func DecodeMap(tlvs []*TLV) map[Tag]*TLV {
	result := make(map[Tag]*TLV)
	for _, tlv := range tlvs {
		result[tlv.Tag] = tlv
	}
	return result
}
