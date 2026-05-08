package repository

import "errors"

// Repository-level sentinel errors
var (
	ErrKeyNotFound    = errors.New("key not found")
	ErrVehicleNotFound = errors.New("vehicle not found")
	ErrEventNotFound  = errors.New("event not found")
)
