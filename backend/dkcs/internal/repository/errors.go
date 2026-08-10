package repository

import "errors"

// Repository-level sentinel errors
var (
	// User errors
	ErrUserNotFound  = errors.New("user not found")
	ErrUserExists    = errors.New("user already exists")
	ErrInvalidUserID = errors.New("invalid user ID")

	// Vehicle errors
	ErrVehicleNotFound  = errors.New("vehicle not found")
	ErrVehicleExists    = errors.New("vehicle already exists")
	ErrInvalidVehicleID = errors.New("invalid vehicle ID")
	ErrInvalidVIN       = errors.New("invalid VIN")

	// Key errors
	ErrKeyNotFound  = errors.New("key not found")
	ErrKeyExists    = errors.New("key already exists")
	ErrInvalidKeyID = errors.New("invalid key ID")
	ErrKeyExpired   = errors.New("key has expired")
	ErrKeyRevoked   = errors.New("key has been revoked")
	ErrKeyInactive  = errors.New("key is not active")

	// Event errors
	ErrEventNotFound    = errors.New("event not found")
	ErrInvalidEventType = errors.New("invalid event type")

	// Permission errors
	ErrPermissionNotFound    = errors.New("permission not found")
	ErrPermissionDenied      = errors.New("permission denied")
	ErrInvalidPermissionType = errors.New("invalid permission type")

	// Transaction errors
	ErrTransactionFailed   = errors.New("transaction failed")
	ErrTransactionRollback = errors.New("transaction rollback")

	// Database errors
	ErrDatabaseConnection = errors.New("database connection error")
	ErrDuplicateEntry     = errors.New("duplicate entry")
	ErrInvalidQuery       = errors.New("invalid query")
)
