// Package bertlv provides BER-TLV encoding/decoding
// Reference: ISO 7816-4, EMVco Book 3
package bertlv

import "fmt"

// Error code constants
const (
	ErrCodeInvalidTag     string = "E001"
	ErrCodeInvalidLength  string = "E002"
	ErrCodeDataTruncated  string = "E003"
	ErrCodeDepthExceeded  string = "E004"
	ErrCodeUnsupportedTag string = "E005"
	ErrCodeInvalidValue   string = "E006"
	ErrCodeBufferOverflow  string = "E007"
)

// BERTLVError represents a BERTLV encoding/decoding error
type BERTLVError struct {
	Code    string
	Message string
	Offset  int
}

func (e *BERTLVError) Error() string {
	if e.Offset > 0 {
		return fmt.Sprintf("[%s] %s (offset: %d)", e.Code, e.Message, e.Offset)
	}
	return fmt.Sprintf("[%s] %s", e.Code, e.Message)
}

// NewError creates a new error
func NewError(code, message string, offset ...int) *BERTLVError {
	off := 0
	if len(offset) > 0 {
		off = offset[0]
	}
	return &BERTLVError{Code: code, Message: message, Offset: off}
}

// ErrDataTruncated data truncated error
func ErrDataTruncated(expected, actual int, offset int) *BERTLVError {
	return NewError(ErrCodeDataTruncated,
		fmt.Sprintf("data truncated: expected %d bytes, got %d", expected, actual), offset)
}

// ErrDepthExceeded nesting depth exceeded
func ErrDepthExceeded(depth, maxDepth int) *BERTLVError {
	return NewError(ErrCodeDepthExceeded,
		fmt.Sprintf("nesting depth %d exceeds maximum %d", depth, maxDepth))
}

// ErrUnsupportedTag unsupported tag error
func ErrUnsupportedTag(tag Tag) *BERTLVError {
	return NewError(ErrCodeUnsupportedTag, fmt.Sprintf("unsupported tag: 0x%02X", tag))
}

// ErrInvalidValue invalid value error
func ErrInvalidValue(reason string, offset int) *BERTLVError {
	return NewError(ErrCodeInvalidValue, reason, offset)
}

// ErrBufferOverflow buffer overflow error
func ErrBufferOverflow(required, available int) *BERTLVError {
	return NewError(ErrCodeBufferOverflow,
		fmt.Sprintf("buffer overflow: required %d bytes, available %d bytes", required, available))
}

// TLV represents a complete TLV element
type TLV struct {
	Tag     Tag
	Length  int
	Value   []byte
	RawData []byte
}

// String returns string representation
func (t *TLV) String() string {
	return fmt.Sprintf("TLV{Tag: %s (0x%02X), Length: %d, Value: %x}",
		t.Tag.String(), t.Tag, t.Length, t.Value)
}

// IsConstructed checks if the tag indicates a constructed type
func (t *TLV) IsConstructed() bool {
	return t.Tag.IsConstructed()
}

// Children decodes and returns child TLV elements
func (t *TLV) Children() []*TLV {
	if !t.IsConstructed() {
		return nil
	}
	children, _ := DecodeAll(t.Value)
	return children
}
