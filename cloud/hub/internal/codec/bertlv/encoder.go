// Package bertlv 提供 BER-TLV 编解码功能
// 参考: ISO 7816-4, EMVco Book 3
package bertlv

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

// Encoder BERTLV 编码器
type Encoder struct {
	buf *bytes.Buffer
}

// NewEncoder 创建新的编码器
func NewEncoder() *Encoder {
	return &Encoder{
		buf: new(bytes.Buffer),
	}
}

// Encode 使用指定标签编码值
// 支持 int, uint, string, []byte 等基本类型
func Encode(tag Tag, value interface{}) ([]byte, error) {
	enc := NewEncoder()
	if err := enc.encodeValue(tag, value); err != nil {
		return nil, err
	}
	return enc.buf.Bytes(), nil
}

// EncodeTLV 使用指定标签、长度和值编码
func EncodeTLV(tag Tag, length int, value []byte) ([]byte, error) {
	enc := NewEncoder()
	if err := enc.writeTLV(tag, length, value); err != nil {
		return nil, err
	}
	return enc.buf.Bytes(), nil
}

// encodeValue 编码单个值
func (e *Encoder) encodeValue(tag Tag, value interface{}) error {
	switch v := value.(type) {
	case nil:
		// nil 值编码为空 bytes
		return e.writeTLV(tag, 0, nil)
	case int:
		return e.writeInt(tag, int64(v))
	case int8:
		return e.writeTLV(tag, 1, []byte{byte(v)})
	case int16:
		buf := make([]byte, 2)
		binary.BigEndian.PutUint16(buf, uint16(v))
		return e.writeTLV(tag, 2, buf)
	case int32:
		buf := make([]byte, 4)
		binary.BigEndian.PutUint32(buf, uint32(v))
		return e.writeTLV(tag, 4, buf)
	case int64:
		buf := make([]byte, 8)
		binary.BigEndian.PutUint64(buf, uint64(v))
		return e.writeTLV(tag, 8, buf)
	case uint:
		return e.writeUint(tag, uint64(v))
	case uint8:
		return e.writeTLV(tag, 1, []byte{v})
	case uint16:
		buf := make([]byte, 2)
		binary.BigEndian.PutUint16(buf, v)
		return e.writeTLV(tag, 2, buf)
	case uint32:
		buf := make([]byte, 4)
		binary.BigEndian.PutUint32(buf, v)
		return e.writeTLV(tag, 4, buf)
	case uint64:
		buf := make([]byte, 8)
		binary.BigEndian.PutUint64(buf, v)
		return e.writeTLV(tag, 8, buf)
	case string:
		return e.writeTLV(tag, len(v), []byte(v))
	case []byte:
		return e.writeTLV(tag, len(v), v)
	case [][]byte:
		// 构造类型: 嵌套多个 TLV
		return e.writeConstructed(tag, v)
	case *Encoder:
		// 嵌套编码器
		return e.writeTLV(tag, v.buf.Len(), v.buf.Bytes())
	default:
		return fmt.Errorf("%w: unsupported type %T", ErrCodeInvalidValue, value)
	}
}

// writeInt 编码有符号整数
func (e *Encoder) writeInt(tag Tag, value int64) error {
	// 计算需要多少字节
	bytesNeeded := intSize(value)
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, uint64(value))
	return e.writeTLV(tag, bytesNeeded, buf[8-bytesNeeded:])
}

// writeUint 编码无符号整数
func (e *Encoder) writeUint(tag Tag, value uint64) error {
	bytesNeeded := uintSize(value)
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, value)
	return e.writeTLV(tag, bytesNeeded, buf[8-bytesNeeded:])
}

// writeTLV 写入完整的 TLV
func (e *Encoder) writeTLV(tag Tag, length int, value []byte) error {
	// 写入 Tag
	if err := e.writeTag(tag); err != nil {
		return err
	}
	// 写入 Length
	if err := e.writeLength(length); err != nil {
		return err
	}
	// 写入 Value
	if length > 0 && value != nil {
		e.buf.Write(value)
	}
	return nil
}

// writeTag 写入标签
func (e *Encoder) writeTag(tag Tag) error {
	t := uint32(tag)
	// 单字节 tag (0x00-0x1F 或者 0x00)
	if t <= 0x1F {
		return e.buf.WriteByte(byte(t))
	}
	// 多字节 tag: 第一个字节的低5位为 0x1F, 表示后续有更多字节
	if t <= 0xFF {
		e.buf.Write([]byte{0x1F, byte(t)})
		return nil
	}
	if t <= 0xFFFF {
		e.buf.Write([]byte{0x1F, byte(t >> 8), byte(t)})
		return nil
	}
	// 4字节 tag
	e.buf.Write([]byte{
		0x1F,
		byte(t >> 16),
		byte(t >> 8),
		byte(t),
	})
	return nil
}

// writeLength 写入长度
func (e *Encoder) writeLength(length int) error {
	if length < 0 {
		return fmt.Errorf("%w: negative length", ErrCodeInvalidLength)
	}
	// 短格式: 0x00-0x7F
	if length <= 0x7F {
		return e.buf.WriteByte(byte(length))
	}
	// 长格式: 0x80 表示后续字节数
	if length <= 0xFF {
		e.buf.Write([]byte{0x81, byte(length)})
		return nil
	}
	if length <= 0xFFFF {
		buf := make([]byte, 3)
		buf[0] = 0x82
		binary.BigEndian.PutUint16(buf[1:], uint16(length))
		e.buf.Write(buf)
		return nil
	}
	// 3字节长度 (最多 0xFFFFFF)
	if length > 0xFFFFFF {
		return fmt.Errorf("%w: length %d exceeds maximum 16777215", ErrCodeInvalidLength, length)
	}
	buf := make([]byte, 4)
	buf[0] = 0x83
	buf[1] = byte(length >> 16)
	buf[2] = byte(length >> 8)
	buf[3] = byte(length)
	e.buf.Write(buf)
	return nil
}

// writeConstructed 写入构造类型 (嵌套 TLV)
func (e *Encoder) writeConstructed(tag Tag, tlvList [][]byte) error {
	// 先收集所有子元素的编码
	var subBuf bytes.Buffer
	for _, tlvData := range tlvList {
		subBuf.Write(tlvData)
	}
	// 写入构造类型的 TLV
	return e.writeTLV(tag, subBuf.Len(), subBuf.Bytes())
}

// intSize 计算有符号整数需要的字节数
func intSize(x int64) int {
	switch {
	case x >= -0x80 && x <= 0x7F:
		return 1
	case x >= -0x8000 && x <= 0x7FFF:
		return 2
	case x >= -0x800000 && x <= 0x7FFFFF:
		return 3
	case x >= -0x80000000 && x <= 0x7FFFFFFF:
		return 4
	case x >= -0x8000000000 && x <= 0x7FFFFFFFff:
		return 5
	case x >= -0x800000000000 && x <= 0x7FFFFFFFFFFF:
		return 6
	case x >= -0x80000000000000 && x <= 0x7FFFFFFFFFFFFF:
		return 7
	default:
		return 8
	}
}

// uintSize 计算无符号整数需要的字节数
func uintSize(x uint64) int {
	switch {
	case x <= 0x7F:
		return 1
	case x <= 0x7FFF:
		return 2
	case x <= 0x7FFFFF:
		return 3
	case x <= 0x7FFFFFFF:
		return 4
	case x <= 0x7FFFFFFFFF:
		return 5
	case x <= 0x7FFFFFFFFFFF:
		return 6
	case x <= 0x7FFFFFFFFFFFFF:
		return 7
	default:
		return 8
	}
}

// EncodeAll 批量编码 TLV 列表
func EncodeAll(tlvs []*TLV) ([]byte, error) {
	if len(tlvs) == 0 {
		return nil, nil
	}
	enc := NewEncoder()
	for _, tlv := range tlvs {
		if err := enc.writeTLV(tlv.Tag, tlv.Length, tlv.Value); err != nil {
			return nil, err
		}
	}
	return enc.buf.Bytes(), nil
}

// NewTLV 创建新的 TLV 结构
func NewTLV(tag Tag, value interface{}) (*TLV, error) {
	data, err := Encode(tag, value)
	if err != nil {
		return nil, err
	}
	return &TLV{
		Tag:    tag,
		Length: len(data),
		Value:  data,
	}, nil
}

// DeepCopy 深度拷贝 TLV
func (t *TLV) DeepCopy() *TLV {
	if t == nil {
		return nil
	}
	cp := make([]byte, len(t.Value))
	copy(cp, t.Value)
	return &TLV{
		Tag:    t.Tag,
		Length: t.Length,
		Value:  cp,
	}
}