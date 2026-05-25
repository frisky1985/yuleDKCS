// Package iccoacert ICCOA DK 4.0 证书存储管理
package iccoacert

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"time"

	"github.com/google/uuid"
)

// CertStore 证书存储接口
type CertStore interface {
	// CA 证书管理
	StoreCA(ctx context.Context, cert *ICCOACertificate) (string, error)
	GetCA(ctx context.Context, certID string) (*ICCOACertificate, error)
	GetCAByVehicleOemID(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error)
	ListCAs(ctx context.Context) ([]*ICCOACertificate, error)

	// 车证书管理
	StoreVehicleCert(ctx context.Context, cert *ICCOACertificate, vehicleID string) (string, error)
	GetVehicleCert(ctx context.Context, vehicleID string) (*ICCOACertificate, error)

	// 车主钥匙证书管理
	StoreOwnerKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, userID uint) (string, error)
	GetOwnerKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error)
	GetOwnerKeyCertByUserID(ctx context.Context, userID uint) ([]*ICCOACertificate, error)

	// 中间分享证书管理
	StoreMidShareCert(ctx context.Context, cert *ICCOACertificate, shareID string) (string, error)
	GetMidShareCert(ctx context.Context, shareID string) (*ICCOACertificate, error)

	// 好友钥匙证书管理
	StoreSharedKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, friendUserID uint) (string, error)
	GetSharedKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error)
	GetSharedKeyCertsByUserID(ctx context.Context, userID uint) ([]*ICCOACertificate, error)

	// 证书链管理
	StoreCertChain(ctx context.Context, chain *CertificateChain, entityID string) error
	GetCertChain(ctx context.Context, entityID string) (*CertificateChain, error)

	// 证书撤销
	RevokeCertificate(ctx context.Context, certID string, reason string) error
	IsRevoked(ctx context.Context, certID string) (bool, error)

	// 过期清理
	DeleteExpiredCerts(ctx context.Context, before time.Time) (int64, error)
}

// certStore 证书存储实现
type certStore struct {
	db *sql.DB
}

// CertRecord 证书数据库记录
type CertRecord struct {
	ID             string    `json:"id" db:"id"`
	Type           int       `json:"type" db:"type"`
	Mode           int       `json:"mode" db:"mode"`
	VehicleOemID   string    `json:"vehicle_oem_id" db:"vehicle_oem_id"`
	VehicleID      string    `json:"vehicle_id" db:"vehicle_id"`
	KeyID          string    `json:"key_id" db:"key_id"`
	UserID         uint      `json:"user_id" db:"user_id"`
	SerialNumber   string    `json:"serial_number" db:"serial_number"`
	Subject        string    `json:"subject" db:"subject"`
	Issuer         string    `json:"issuer" db:"issuer"`
	NotBefore      time.Time `json:"not_before" db:"not_before"`
	NotAfter       time.Time `json:"not_after" db:"not_after"`
	DERData        string    `json:"der_data" db:"der_data"`        // Base64 编码
	PEMData        string    `json:"pem_data" db:"pem_data"`
	ParentCertID   string    `json:"parent_cert_id" db:"parent_cert_id"`
	Status         string    `json:"status" db:"status"`
	RevokedAt      *time.Time `json:"revoked_at,omitempty" db:"revoked_at"`
	RevokeReason   string    `json:"revoke_reason,omitempty" db:"revoke_reason"`
	CreatedAt      time.Time `json:"created_at" db:"created_at"`
	UpdatedAt      time.Time `json:"updated_at" db:"updated_at"`
}

// NewCertStore 创建证书存储实例
func NewCertStore(db *sql.DB) CertStore {
	return &certStore{db: db}
}

// StoreCA 存储 CA 证书
func (s *certStore) StoreCA(ctx context.Context, cert *ICCOACertificate) (string, error) {
	certID := uuid.New().String()
	record := &CertRecord{
		ID:           certID,
		Type:         int(cert.Type),
		Mode:         int(cert.Mode),
		VehicleOemID: cert.VehicleOemID,
		SerialNumber: cert.X509Cert.SerialNumber.String(),
		Subject:      cert.X509Cert.Subject.String(),
		Issuer:       cert.X509Cert.Issuer.String(),
		NotBefore:    cert.X509Cert.NotBefore,
		NotAfter:     cert.X509Cert.NotAfter,
		DERData:      base64.StdEncoding.EncodeToString(cert.DERData),
		PEMData:      string(cert.PEMData),
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}

	query := `
		INSERT INTO iccoa_certificates (
			id, type, mode, vehicle_oem_id, serial_number, subject, issuer,
			not_before, not_after, der_data, pem_data, status, created_at, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := s.db.ExecContext(ctx, query,
		record.ID, record.Type, record.Mode, record.VehicleOemID,
		record.SerialNumber, record.Subject, record.Issuer,
		record.NotBefore, record.NotAfter, record.DERData,
		record.PEMData, record.Status, record.CreatedAt, record.UpdatedAt,
	)
	if err != nil {
		return "", fmt.Errorf("failed to store CA certificate: %w", err)
	}

	return certID, nil
}

// GetCA 获取 CA 证书
func (s *certStore) GetCA(ctx context.Context, certID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, serial_number, subject, issuer,
		       not_before, not_after, der_data, pem_data, status
		FROM iccoa_certificates
		WHERE id = ? AND type = ? AND status = 'active'
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, certID, int(CertTypeVehicleOemCA)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("CA certificate not found: %s", certID)
		}
		return nil, fmt.Errorf("failed to get CA certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// GetCAByVehicleOemID 通过车企 ID 获取 CA 证书
func (s *certStore) GetCAByVehicleOemID(ctx context.Context, vehicleOemID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, serial_number, subject, issuer,
		       not_before, not_after, der_data, pem_data, status
		FROM iccoa_certificates
		WHERE vehicle_oem_id = ? AND type = ? AND status = 'active'
		ORDER BY created_at DESC
		LIMIT 1
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, vehicleOemID, int(CertTypeVehicleOemCA)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("CA certificate not found for vehicle OEM ID: %s", vehicleOemID)
		}
		return nil, fmt.Errorf("failed to get CA certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// ListCAs 列出所有 CA 证书
func (s *certStore) ListCAs(ctx context.Context) ([]*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, serial_number, subject, issuer,
		       not_before, not_after, der_data, pem_data, status
		FROM iccoa_certificates
		WHERE type = ? AND status = 'active'
		ORDER BY created_at DESC
	`

	rows, err := s.db.QueryContext(ctx, query, int(CertTypeVehicleOemCA))
	if err != nil {
		return nil, fmt.Errorf("failed to list CA certificates: %w", err)
	}
	defer rows.Close()

	var certs []*ICCOACertificate
	for rows.Next() {
		var record CertRecord
		var derData string
		err := rows.Scan(
			&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
			&record.SerialNumber, &record.Subject, &record.Issuer,
			&record.NotBefore, &record.NotAfter, &derData,
			&record.PEMData, &record.Status,
		)
		if err != nil {
			continue
		}

		derBytes, err := base64.StdEncoding.DecodeString(derData)
		if err != nil {
			continue
		}

		cert, err := ParseICCOACertificate(derBytes)
		if err != nil {
			continue
		}

		certs = append(certs, cert)
	}

	return certs, nil
}

// StoreVehicleCert 存储车证书
func (s *certStore) StoreVehicleCert(ctx context.Context, cert *ICCOACertificate, vehicleID string) (string, error) {
	certID := uuid.New().String()
	record := &CertRecord{
		ID:           certID,
		Type:         int(cert.Type),
		Mode:         int(cert.Mode),
		VehicleOemID: cert.VehicleOemID,
		VehicleID:    vehicleID,
		SerialNumber: cert.X509Cert.SerialNumber.String(),
		Subject:      cert.X509Cert.Subject.String(),
		Issuer:       cert.X509Cert.Issuer.String(),
		NotBefore:    cert.X509Cert.NotBefore,
		NotAfter:     cert.X509Cert.NotAfter,
		DERData:      base64.StdEncoding.EncodeToString(cert.DERData),
		PEMData:      string(cert.PEMData),
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}

	query := `
		INSERT INTO iccoa_certificates (
			id, type, mode, vehicle_oem_id, vehicle_id, serial_number, subject, issuer,
			not_before, not_after, der_data, pem_data, status, created_at, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := s.db.ExecContext(ctx, query,
		record.ID, record.Type, record.Mode, record.VehicleOemID, record.VehicleID,
		record.SerialNumber, record.Subject, record.Issuer,
		record.NotBefore, record.NotAfter, record.DERData,
		record.PEMData, record.Status, record.CreatedAt, record.UpdatedAt,
	)
	if err != nil {
		return "", fmt.Errorf("failed to store vehicle certificate: %w", err)
	}

	return certID, nil
}

// GetVehicleCert 获取车证书
func (s *certStore) GetVehicleCert(ctx context.Context, vehicleID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id, serial_number, subject, issuer,
		       not_before, not_after, der_data, pem_data, status
		FROM iccoa_certificates
		WHERE vehicle_id = ? AND type = ? AND status = 'active'
		ORDER BY created_at DESC
		LIMIT 1
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, vehicleID, int(CertTypeVehicle)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID, &record.VehicleID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("vehicle certificate not found: %s", vehicleID)
		}
		return nil, fmt.Errorf("failed to get vehicle certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// StoreOwnerKeyCert 存储车主钥匙证书
func (s *certStore) StoreOwnerKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, userID uint) (string, error) {
	certID := uuid.New().String()
	record := &CertRecord{
		ID:           certID,
		Type:         int(cert.Type),
		Mode:         int(cert.Mode),
		VehicleOemID: cert.VehicleOemID,
		VehicleID:    cert.VehicleID,
		KeyID:        keyID,
		UserID:       userID,
		SerialNumber: cert.X509Cert.SerialNumber.String(),
		Subject:      cert.X509Cert.Subject.String(),
		Issuer:       cert.X509Cert.Issuer.String(),
		NotBefore:    cert.X509Cert.NotBefore,
		NotAfter:     cert.X509Cert.NotAfter,
		DERData:      base64.StdEncoding.EncodeToString(cert.DERData),
		PEMData:      string(cert.PEMData),
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}

	query := `
		INSERT INTO iccoa_certificates (
			id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
			serial_number, subject, issuer, not_before, not_after,
			der_data, pem_data, status, created_at, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := s.db.ExecContext(ctx, query,
		record.ID, record.Type, record.Mode, record.VehicleOemID,
		record.VehicleID, record.KeyID, record.UserID,
		record.SerialNumber, record.Subject, record.Issuer,
		record.NotBefore, record.NotAfter, record.DERData,
		record.PEMData, record.Status, record.CreatedAt, record.UpdatedAt,
	)
	if err != nil {
		return "", fmt.Errorf("failed to store owner key certificate: %w", err)
	}

	return certID, nil
}

// GetOwnerKeyCert 获取车主钥匙证书
func (s *certStore) GetOwnerKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
		       serial_number, subject, issuer, not_before, not_after,
		       der_data, pem_data, status
		FROM iccoa_certificates
		WHERE key_id = ? AND type = ? AND status = 'active'
		ORDER BY created_at DESC
		LIMIT 1
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, keyID, int(CertTypeOwnerDK)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
		&record.VehicleID, &record.KeyID, &record.UserID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("owner key certificate not found: %s", keyID)
		}
		return nil, fmt.Errorf("failed to get owner key certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// GetOwnerKeyCertByUserID 通过用户 ID 获取车主钥匙证书
func (s *certStore) GetOwnerKeyCertByUserID(ctx context.Context, userID uint) ([]*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
		       serial_number, subject, issuer, not_before, not_after,
		       der_data, pem_data, status
		FROM iccoa_certificates
		WHERE user_id = ? AND type = ? AND status = 'active'
		ORDER BY created_at DESC
	`

	rows, err := s.db.QueryContext(ctx, query, userID, int(CertTypeOwnerDK))
	if err != nil {
		return nil, fmt.Errorf("failed to list owner key certificates: %w", err)
	}
	defer rows.Close()

	var certs []*ICCOACertificate
	for rows.Next() {
		var record CertRecord
		var derData string
		err := rows.Scan(
			&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
			&record.VehicleID, &record.KeyID, &record.UserID,
			&record.SerialNumber, &record.Subject, &record.Issuer,
			&record.NotBefore, &record.NotAfter, &derData,
			&record.PEMData, &record.Status,
		)
		if err != nil {
			continue
		}

		derBytes, err := base64.StdEncoding.DecodeString(derData)
		if err != nil {
			continue
		}

		cert, err := ParseICCOACertificate(derBytes)
		if err != nil {
			continue
		}

		certs = append(certs, cert)
	}

	return certs, nil
}

// StoreMidShareCert 存储中间分享证书
func (s *certStore) StoreMidShareCert(ctx context.Context, cert *ICCOACertificate, shareID string) (string, error) {
	certID := uuid.New().String()
	record := &CertRecord{
		ID:           certID,
		Type:         int(cert.Type),
		Mode:         int(cert.Mode),
		VehicleOemID: cert.VehicleOemID,
		VehicleID:    cert.VehicleID,
		SerialNumber: cert.X509Cert.SerialNumber.String(),
		Subject:      cert.X509Cert.Subject.String(),
		Issuer:       cert.X509Cert.Issuer.String(),
		NotBefore:    cert.X509Cert.NotBefore,
		NotAfter:     cert.X509Cert.NotAfter,
		DERData:      base64.StdEncoding.EncodeToString(cert.DERData),
		PEMData:      string(cert.PEMData),
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}

	query := `
		INSERT INTO iccoa_certificates (
			id, type, mode, vehicle_oem_id, vehicle_id,
			serial_number, subject, issuer, not_before, not_after,
			der_data, pem_data, status, created_at, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := s.db.ExecContext(ctx, query,
		record.ID, record.Type, record.Mode, record.VehicleOemID, record.VehicleID,
		record.SerialNumber, record.Subject, record.Issuer,
		record.NotBefore, record.NotAfter, record.DERData,
		record.PEMData, record.Status, record.CreatedAt, record.UpdatedAt,
	)
	if err != nil {
		return "", fmt.Errorf("failed to store mid-share certificate: %w", err)
	}

	return certID, nil
}

// GetMidShareCert 获取中间分享证书
func (s *certStore) GetMidShareCert(ctx context.Context, shareID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id,
		       serial_number, subject, issuer, not_before, not_after,
		       der_data, pem_data, status
		FROM iccoa_certificates
		WHERE id = ? AND type = ? AND status = 'active'
		LIMIT 1
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, shareID, int(CertTypeMidShare)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID, &record.VehicleID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("mid-share certificate not found: %s", shareID)
		}
		return nil, fmt.Errorf("failed to get mid-share certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// StoreSharedKeyCert 存储好友钥匙证书
func (s *certStore) StoreSharedKeyCert(ctx context.Context, cert *ICCOACertificate, keyID string, friendUserID uint) (string, error) {
	certID := uuid.New().String()
	record := &CertRecord{
		ID:           certID,
		Type:         int(cert.Type),
		Mode:         int(cert.Mode),
		VehicleOemID: cert.VehicleOemID,
		VehicleID:    cert.VehicleID,
		KeyID:        keyID,
		UserID:       friendUserID,
		SerialNumber: cert.X509Cert.SerialNumber.String(),
		Subject:      cert.X509Cert.Subject.String(),
		Issuer:       cert.X509Cert.Issuer.String(),
		NotBefore:    cert.X509Cert.NotBefore,
		NotAfter:     cert.X509Cert.NotAfter,
		DERData:      base64.StdEncoding.EncodeToString(cert.DERData),
		PEMData:      string(cert.PEMData),
		Status:       "active",
		CreatedAt:    time.Now(),
		UpdatedAt:    time.Now(),
	}

	query := `
		INSERT INTO iccoa_certificates (
			id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
			serial_number, subject, issuer, not_before, not_after,
			der_data, pem_data, status, created_at, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	`

	_, err := s.db.ExecContext(ctx, query,
		record.ID, record.Type, record.Mode, record.VehicleOemID,
		record.VehicleID, record.KeyID, record.UserID,
		record.SerialNumber, record.Subject, record.Issuer,
		record.NotBefore, record.NotAfter, record.DERData,
		record.PEMData, record.Status, record.CreatedAt, record.UpdatedAt,
	)
	if err != nil {
		return "", fmt.Errorf("failed to store shared key certificate: %w", err)
	}

	return certID, nil
}

// GetSharedKeyCert 获取好友钥匙证书
func (s *certStore) GetSharedKeyCert(ctx context.Context, keyID string) (*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
		       serial_number, subject, issuer, not_before, not_after,
		       der_data, pem_data, status
		FROM iccoa_certificates
		WHERE key_id = ? AND (type = ? OR type = ?) AND status = 'active'
		ORDER BY created_at DESC
		LIMIT 1
	`

	var record CertRecord
	var derData string
	err := s.db.QueryRowContext(ctx, query, keyID, int(CertTypeSharedDK), int(CertTypeSharedDKV2)).Scan(
		&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
		&record.VehicleID, &record.KeyID, &record.UserID,
		&record.SerialNumber, &record.Subject, &record.Issuer,
		&record.NotBefore, &record.NotAfter, &derData,
		&record.PEMData, &record.Status,
	)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("shared key certificate not found: %s", keyID)
		}
		return nil, fmt.Errorf("failed to get shared key certificate: %w", err)
	}

	derBytes, err := base64.StdEncoding.DecodeString(derData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode DER data: %w", err)
	}

	return ParseICCOACertificate(derBytes)
}

// GetSharedKeyCertsByUserID 通过用户 ID 获取好友钥匙证书
func (s *certStore) GetSharedKeyCertsByUserID(ctx context.Context, userID uint) ([]*ICCOACertificate, error) {
	query := `
		SELECT id, type, mode, vehicle_oem_id, vehicle_id, key_id, user_id,
		       serial_number, subject, issuer, not_before, not_after,
		       der_data, pem_data, status
		FROM iccoa_certificates
		WHERE user_id = ? AND (type = ? OR type = ?) AND status = 'active'
		ORDER BY created_at DESC
	`

	rows, err := s.db.QueryContext(ctx, query, userID, int(CertTypeSharedDK), int(CertTypeSharedDKV2))
	if err != nil {
		return nil, fmt.Errorf("failed to list shared key certificates: %w", err)
	}
	defer rows.Close()

	var certs []*ICCOACertificate
	for rows.Next() {
		var record CertRecord
		var derData string
		err := rows.Scan(
			&record.ID, &record.Type, &record.Mode, &record.VehicleOemID,
			&record.VehicleID, &record.KeyID, &record.UserID,
			&record.SerialNumber, &record.Subject, &record.Issuer,
			&record.NotBefore, &record.NotAfter, &derData,
			&record.PEMData, &record.Status,
		)
		if err != nil {
			continue
		}

		derBytes, err := base64.StdEncoding.DecodeString(derData)
		if err != nil {
			continue
		}

		cert, err := ParseICCOACertificate(derBytes)
		if err != nil {
			continue
		}

		certs = append(certs, cert)
	}

	return certs, nil
}

// StoreCertChain 存储证书链
func (s *certStore) StoreCertChain(ctx context.Context, chain *CertificateChain, entityID string) error {
	// 将证书链序列化为 JSON
	chainData := make(map[string]interface{})

	if chain.RootCA != nil {
		chainData["root_ca"] = base64.StdEncoding.EncodeToString(chain.RootCA.DERData)
	}

	intermediates := make([]string, 0, len(chain.Intermediate))
	for _, cert := range chain.Intermediate {
		intermediates = append(intermediates, base64.StdEncoding.EncodeToString(cert.DERData))
	}
	chainData["intermediates"] = intermediates

	if chain.EndEntity != nil {
		chainData["end_entity"] = base64.StdEncoding.EncodeToString(chain.EndEntity.DERData)
	}

	jsonData, err := json.Marshal(chainData)
	if err != nil {
		return fmt.Errorf("failed to marshal certificate chain: %w", err)
	}

	query := `
		INSERT INTO iccoa_cert_chains (entity_id, chain_data, created_at, updated_at)
		VALUES (?, ?, ?, ?)
		ON DUPLICATE KEY UPDATE
		chain_data = VALUES(chain_data), updated_at = VALUES(updated_at)
	`

	_, err = s.db.ExecContext(ctx, query, entityID, string(jsonData), time.Now(), time.Now())
	if err != nil {
		return fmt.Errorf("failed to store certificate chain: %w", err)
	}

	return nil
}

// GetCertChain 获取证书链
func (s *certStore) GetCertChain(ctx context.Context, entityID string) (*CertificateChain, error) {
	query := `
		SELECT chain_data FROM iccoa_cert_chains WHERE entity_id = ?
	`

	var chainData string
	err := s.db.QueryRowContext(ctx, query, entityID).Scan(&chainData)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, fmt.Errorf("certificate chain not found: %s", entityID)
		}
		return nil, fmt.Errorf("failed to get certificate chain: %w", err)
	}

	var chainMap map[string]interface{}
	if err := json.Unmarshal([]byte(chainData), &chainMap); err != nil {
		return nil, fmt.Errorf("failed to unmarshal certificate chain: %w", err)
	}

	chain := &CertificateChain{}

	// 解析根 CA
	if rootCAData, ok := chainMap["root_ca"].(string); ok {
		derBytes, err := base64.StdEncoding.DecodeString(rootCAData)
		if err == nil {
			chain.RootCA, _ = ParseICCOACertificate(derBytes)
		}
	}

	// 解析中间证书
	if intermediates, ok := chainMap["intermediates"].([]interface{}); ok {
		for _, intermediateData := range intermediates {
			if data, ok := intermediateData.(string); ok {
				derBytes, err := base64.StdEncoding.DecodeString(data)
				if err == nil {
					cert, err := ParseICCOACertificate(derBytes)
					if err == nil {
						chain.Intermediate = append(chain.Intermediate, cert)
					}
				}
			}
		}
	}

	// 解析终端实体证书
	if endEntityData, ok := chainMap["end_entity"].(string); ok {
		derBytes, err := base64.StdEncoding.DecodeString(endEntityData)
		if err == nil {
			chain.EndEntity, _ = ParseICCOACertificate(derBytes)
		}
	}

	return chain, nil
}

// RevokeCertificate 撤销证书
func (s *certStore) RevokeCertificate(ctx context.Context, certID string, reason string) error {
	query := `
		UPDATE iccoa_certificates
		SET status = 'revoked', revoked_at = ?, revoke_reason = ?, updated_at = ?
		WHERE id = ?
	`

	now := time.Now()
	_, err := s.db.ExecContext(ctx, query, now, reason, now, certID)
	if err != nil {
		return fmt.Errorf("failed to revoke certificate: %w", err)
	}

	return nil
}

// IsRevoked 检查证书是否已撤销
func (s *certStore) IsRevoked(ctx context.Context, certID string) (bool, error) {
	query := `
		SELECT status FROM iccoa_certificates WHERE id = ?
	`

	var status string
	err := s.db.QueryRowContext(ctx, query, certID).Scan(&status)
	if err != nil {
		if err == sql.ErrNoRows {
			return false, nil
		}
		return false, fmt.Errorf("failed to check certificate status: %w", err)
	}

	return status == "revoked", nil
}

// DeleteExpiredCerts 删除过期证书
func (s *certStore) DeleteExpiredCerts(ctx context.Context, before time.Time) (int64, error) {
	query := `
		DELETE FROM iccoa_certificates
		WHERE not_after < ? AND status != 'revoked'
	`

	result, err := s.db.ExecContext(ctx, query, before)
	if err != nil {
		return 0, fmt.Errorf("failed to delete expired certificates: %w", err)
	}

	rowsAffected, err := result.RowsAffected()
	if err != nil {
		return 0, fmt.Errorf("failed to get rows affected: %w", err)
	}

	return rowsAffected, nil
}
