package relay

// CCC Mailbox API 错误码
// Relay Server 使用自有错误码（非 TLV 设备间错误码 Table 11-27）
const (
	// 通用错误
	ErrCodeInvalidRequest    = "INVALID_REQUEST"
	ErrCodeInternalError     = "INTERNAL_ERROR"

	// Mailbox 查找/状态错误
	ErrCodeMailboxNotFound   = "MAILBOX_NOT_FOUND"
	ErrCodeMailboxExpired    = "MAILBOX_EXPIRED"
	ErrCodeMailboxCompleted  = "MAILBOX_COMPLETED"
	ErrCodeMailboxCancelled  = "MAILBOX_CANCELLED"

	// 更新错误
	ErrCodeInvalidTransition = "INVALID_TRANSITION"
	ErrCodeUpdateLimitExceeded = "UPDATE_LIMIT_EXCEEDED"
)
