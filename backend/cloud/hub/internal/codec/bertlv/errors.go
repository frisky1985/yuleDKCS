// Package bertlv 提供 BER-TLV 编解码功能
// 参考: ISO 7816-4, EMVco Book 3
package bertlv

import "fmt"

// 错误代码定义
const (
	ErrCodeInvalidTag     string = "E001" // 无效Tag
	ErrCodeInvalidLength  string = "E002" // 无效Length
	ErrCodeDataTruncated  string = "E003" // 数据截断
	ErrCodeDepthExceeded  string = "E004" // 嵌套深度超限
	ErrCodeUnsupportedTag string = "E005" // 不支持的Tag
	ErrCodeInvalidValue   string = "E006" // 无效Value
	ErrCodeBufferOverflow string = "E007" // 缓冲区溢出
)

// BERTLVError 表示 BERTLV 编解码过程中的错误
type BERTLVError struct {
	Code    string
	Message string
	Offset  int // 错误发生的位置
}

func (e *BERTLVError) Error() string {
	if e.Offset > 0 {
		return fmt.Sprintf("[%s] %s (offset: %d)", e.Code, e.Message, e.Offset)
	}
	return fmt.Sprintf("[%s] %s", e.Code, e.Message)
}

// NewError 创建新的错误
func NewError(code, message string, offset ...int) *BERTLVError {
	off := 0
	if len(offset) > 0 {
		off = offset[0]
	}
	return &BERTLVError{
		Code:    code,
		Message: message,
		Offset:  off,
	}
}

// ErrInvalidTag 无效Tag错误
func ErrInvalidTag(tag Tag, offset int) *BERTLVError {
	return NewError(ErrCodeInvalidTag, fmt.Sprintf("invalid tag: 0x%02X", tag), offset)
}

// ErrInvalidLength 无效Length错误
func ErrInvalidLength(length int, offset int) *BERTLVError {
	return NewError(ErrCodeInvalidLength, fmt.Sprintf("invalid length: %d", length), offset)
}

// ErrDataTruncated 数据截断错误
func ErrDataTruncated(expected, actual int, offset int) *BERTLVError {
	return NewError(ErrCodeDataTruncated,
		fmt.Sprintf("data truncated: expected %d bytes, got %d", expected, actual), offset)
}

// ErrDepthExceeded 嵌套深度超限错误
func ErrDepthExceeded(depth, maxDepth int) *BERTLVError {
	return NewError(ErrCodeDepthExceeded,
		fmt.Sprintf("nesting depth %d exceeds maximum %d", depth, maxDepth))
}

// ErrUnsupportedTag 不支持Tag错误
func ErrUnsupportedTag(tag Tag) *BERTLVError {
	return NewError(ErrCodeUnsupportedTag, fmt.Sprintf("unsupported tag: 0x%02X", tag))
}

// ErrInvalidValue 无效Value错误
func ErrInvalidValue(reason string, offset int) *BERTLVError {
	return NewError(ErrCodeInvalidValue, reason, offset)
}

// ErrBufferOverflow 缓冲区溢出错误
func ErrBufferOverflow(required, available int) *BERTLVError {
	return NewError(ErrCodeBufferOverflow,
		fmt.Sprintf("buffer overflow: required %d bytes, available %d bytes", required, available))
}

// TLV 表示一个完整的 TLV 元素
type TLV struct {
	Tag     Tag     // 标签
	Length  int     // 长度
	Value   []byte  // 值
	RawData []byte  // 原始数据 (可选,用于调试)
}

// String 返回 TLV 的字符串表示
func (t *TLV) String() string {
	return fmt.Sprintf("TLV{Tag: %s (0x%02X), Length: %d, Value: %x}",
		t.Tag.String(), t.Tag, t.Length, t.Value)
}

// IsConstructed 判断是否为构造类型
func (t *TLV) IsConstructed() bool {
	return t.Tag.IsConstructed()
}

// Children 获取子元素 (仅对构造类型有效)
func (t *TLV) Children() []*TLV {
	if !t.IsConstructed() {
		return nil
	}
	// 递归解码子元素
	children, _ := DecodeAll(t.Value)
	return children
}
