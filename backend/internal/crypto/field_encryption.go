package crypto

import (
	"context"
	"database/sql/driver"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"sync"

	"gorm.io/gorm"
	"gorm.io/gorm/schema"
)

// FieldEncryptionPlugin GORM字段加密插件
// 符合 ISO/SAE 21434 标准 - 敏感数据保护要求
// WP-08: 数据加密

type FieldEncryptionPlugin struct {
	encryptionService *EncryptionService
	encryptedFields   map[string]bool // 需要加密的字段映射
	mu                sync.RWMutex
}

// EncryptedField 加密字段接口
type EncryptedField interface {
	Encrypt(es *EncryptionService) error
	Decrypt(es *EncryptionService) error
}

// EncryptedString 加密字符串类型
type EncryptedString struct {
	Plaintext  string `json:"-" gorm:"-"` // 明文字段（内存中）
	Ciphertext string `json:"-"`           // 密文字段（数据库中）
	encrypted  bool   // 标记是否已加密
}

// NewFieldEncryptionPlugin 创建字段加密插件
func NewFieldEncryptionPlugin(es *EncryptionService) *FieldEncryptionPlugin {
	return &FieldEncryptionPlugin{
		encryptionService: es,
		encryptedFields:   make(map[string]bool),
	}
}

// RegisterEncryptedField 注册需要加密的字段
func (p *FieldEncryptionPlugin) RegisterEncryptedField(tableName, fieldName string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	
	key := fmt.Sprintf("%s.%s", tableName, fieldName)
	p.encryptedFields[key] = true
}

// Name 插件名称
func (p *FieldEncryptionPlugin) Name() string {
	return "field_encryption"
}

// Initialize 初始化插件
func (p *FieldEncryptionPlugin) Initialize(db *gorm.DB) error {
	// 注册回调函数
	if err := db.Callback().Create().Before("gorm:create").Register("field_encryption:before_create", p.beforeCreate); err != nil {
		return err
	}
	if err := db.Callback().Create().After("gorm:create").Register("field_encryption:after_create", p.afterCreate); err != nil {
		return err
	}
	if err := db.Callback().Update().Before("gorm:update").Register("field_encryption:before_update", p.beforeUpdate); err != nil {
		return err
	}
	if err := db.Callback().Update().After("gorm:update").Register("field_encryption:after_update", p.afterUpdate); err != nil {
		return err
	}
	if err := db.Callback().Query().After("gorm:query").Register("field_encryption:after_query", p.afterQuery); err != nil {
		return err
	}
	
	return nil
}

// beforeCreate 创建前加密
func (p *FieldEncryptionPlugin) beforeCreate(db *gorm.DB) {
	if db.Error != nil || db.Statement.Schema == nil {
		return
	}
	
	p.encryptFields(db.Statement.ReflectValue.Interface())
}

// afterCreate 创建后解密
func (p *FieldEncryptionPlugin) afterCreate(db *gorm.DB) {
	if db.Error != nil {
		return
	}
	
	p.decryptFields(db.Statement.ReflectValue.Interface())
}

// beforeUpdate 更新前加密
func (p *FieldEncryptionPlugin) beforeUpdate(db *gorm.DB) {
	if db.Error != nil || db.Statement.Schema == nil {
		return
	}
	
	p.encryptFields(db.Statement.ReflectValue.Interface())
}

// afterUpdate 更新后解密
func (p *FieldEncryptionPlugin) afterUpdate(db *gorm.DB) {
	if db.Error != nil {
		return
	}
	
	p.decryptFields(db.Statement.ReflectValue.Interface())
}

// afterQuery 查询后解密
func (p *FieldEncryptionPlugin) afterQuery(db *gorm.DB) {
	if db.Error != nil || db.Statement.Schema == nil {
		return
	}
	
	// 处理单条记录
	if db.Statement.ReflectValue.Kind() == reflect.Ptr && !db.Statement.ReflectValue.IsNil() {
		p.decryptFields(db.Statement.ReflectValue.Interface())
		return
	}
	
	// 处理切片
	if db.Statement.ReflectValue.Kind() == reflect.Slice {
		for i := 0; i < db.Statement.ReflectValue.Len(); i++ {
			elem := db.Statement.ReflectValue.Index(i)
			if elem.Kind() == reflect.Ptr && !elem.IsNil() {
				p.decryptFields(elem.Interface())
			} else if elem.CanAddr() {
				p.decryptFields(elem.Addr().Interface())
			}
		}
	}
}

// encryptFields 加密结构体中的敏感字段
func (p *FieldEncryptionPlugin) encryptFields(model interface{}) {
	if model == nil {
		return
	}
	
	val := reflect.ValueOf(model)
	if val.Kind() == reflect.Ptr {
		val = val.Elem()
	}
	
	if val.Kind() != reflect.Struct {
		return
	}
	
	typ := val.Type()
	for i := 0; i < val.NumField(); i++ {
		field := val.Field(i)
		fieldType := typ.Field(i)
		
		// 检查字段标签
		if tag := fieldType.Tag.Get("encrypt"); tag == "true" {
			p.encryptField(field)
		}
		
		// 递归处理嵌套结构体
		if field.Kind() == reflect.Struct {
			p.encryptFields(field.Addr().Interface())
		}
	}
}

// decryptFields 解密结构体中的敏感字段
func (p *FieldEncryptionPlugin) decryptFields(model interface{}) {
	if model == nil {
		return
	}
	
	val := reflect.ValueOf(model)
	if val.Kind() == reflect.Ptr {
		val = val.Elem()
	}
	
	if val.Kind() != reflect.Struct {
		return
	}
	
	typ := val.Type()
	for i := 0; i < val.NumField(); i++ {
		field := val.Field(i)
		fieldType := typ.Field(i)
		
		// 检查字段标签
		if tag := fieldType.Tag.Get("encrypt"); tag == "true" {
			p.decryptField(field)
		}
		
		// 递归处理嵌套结构体
		if field.Kind() == reflect.Struct {
			p.decryptFields(field.Addr().Interface())
		}
	}
}

// encryptField 加密单个字段
func (p *FieldEncryptionPlugin) encryptField(field reflect.Value) {
	if !field.IsValid() || !field.CanSet() {
		return
	}
	
	switch field.Kind() {
	case reflect.String:
		plaintext := field.String()
		if plaintext != "" {
			ciphertext, err := p.encryptionService.EncryptString(plaintext)
			if err == nil {
				field.SetString(ciphertext)
			}
		}
	case reflect.Ptr:
		if !field.IsNil() && field.Elem().Kind() == reflect.String {
			plaintext := field.Elem().String()
			if plaintext != "" {
				ciphertext, err := p.encryptionService.EncryptString(plaintext)
				if err == nil {
					field.Elem().SetString(ciphertext)
				}
			}
		}
	}
}

// decryptField 解密单个字段
func (p *FieldEncryptionPlugin) decryptField(field reflect.Value) {
	if !field.IsValid() || !field.CanSet() {
		return
	}
	
	switch field.Kind() {
	case reflect.String:
		ciphertext := field.String()
		if ciphertext != "" && isEncrypted(ciphertext) {
			plaintext, err := p.encryptionService.DecryptString(ciphertext)
			if err == nil {
				field.SetString(plaintext)
			}
		}
	case reflect.Ptr:
		if !field.IsNil() && field.Elem().Kind() == reflect.String {
			ciphertext := field.Elem().String()
			if ciphertext != "" && isEncrypted(ciphertext) {
				plaintext, err := p.encryptionService.DecryptString(ciphertext)
				if err == nil {
					field.Elem().SetString(plaintext)
				}
			}
		}
	}
}

// isEncrypted 检查字符串是否是加密格式
func isEncrypted(s string) bool {
	// 简单检查：加密后的字符串通常是base64格式
	if len(s) < 20 {
		return false
	}
	// 检查是否包含非打印字符或特定加密标记
	return strings.ContainsAny(s, "+/=") && len(s)%4 == 0
}

// Value 实现driver.Valuer接口（用于GORM）
func (es EncryptedString) Value() (driver.Value, error) {
	if es.Ciphertext != "" {
		return es.Ciphertext, nil
	}
	return es.Plaintext, nil
}

// Scan 实现sql.Scanner接口（用于GORM）
func (es *EncryptedString) Scan(value interface{}) error {
	if value == nil {
		return nil
	}
	
	switch v := value.(type) {
	case string:
		es.Ciphertext = v
	case []byte:
		es.Ciphertext = string(v)
	default:
		return errors.New("cannot scan type into EncryptedString")
	}
	
	return nil
}

// SensitiveFieldEncryptor 敏感字段加密器
type SensitiveFieldEncryptor struct {
	es *EncryptionService
}

// NewSensitiveFieldEncryptor 创建敏感字段加密器
func NewSensitiveFieldEncryptor(es *EncryptionService) *SensitiveFieldEncryptor {
	return &SensitiveFieldEncryptor{es: es}
}

// EncryptVIN 加密VIN码
func (e *SensitiveFieldEncryptor) EncryptVIN(vin string) (string, error) {
	if vin == "" {
		return "", nil
	}
	return e.es.EncryptString(vin)
}

// DecryptVIN 解密VIN码
func (e *SensitiveFieldEncryptor) DecryptVIN(encryptedVIN string) (string, error) {
	if encryptedVIN == "" {
		return "", nil
	}
	return e.es.DecryptString(encryptedVIN)
}

// EncryptPhone 加密手机号
func (e *SensitiveFieldEncryptor) EncryptPhone(phone string) (string, error) {
	if phone == "" {
		return "", nil
	}
	return e.es.EncryptString(phone)
}

// DecryptPhone 解密手机号
func (e *SensitiveFieldEncryptor) DecryptPhone(encryptedPhone string) (string, error) {
	if encryptedPhone == "" {
		return "", nil
	}
	return e.es.DecryptString(encryptedPhone)
}

// EncryptEmail 加密邮箱
func (e *SensitiveFieldEncryptor) EncryptEmail(email string) (string, error) {
	if email == "" {
		return "", nil
	}
	return e.es.EncryptString(email)
}

// DecryptEmail 解密邮箱
func (e *SensitiveFieldEncryptor) DecryptEmail(encryptedEmail string) (string, error) {
	if encryptedEmail == "" {
		return "", nil
	}
	return e.es.DecryptString(encryptedEmail)
}

// EncryptBLEKey 加密BLE密钥
func (e *SensitiveFieldEncryptor) EncryptBLEKey(key string) (string, error) {
	if key == "" {
		return "", nil
	}
	return e.es.EncryptString(key)
}

// DecryptBLEKey 解密BLE密钥
func (e *SensitiveFieldEncryptor) DecryptBLEKey(encryptedKey string) (string, error) {
	if encryptedKey == "" {
		return "", nil
	}
	return e.es.DecryptString(encryptedKey)
}

// EncryptUWBKey 加密UWB密钥
func (e *SensitiveFieldEncryptor) EncryptUWBKey(key string) (string, error) {
	if key == "" {
		return "", nil
	}
	return e.es.EncryptString(key)
}

// DecryptUWBKey 解密UWB密钥
func (e *SensitiveFieldEncryptor) DecryptUWBKey(encryptedKey string) (string, error) {
	if encryptedKey == "" {
		return "", nil
	}
	return e.es.DecryptString(encryptedKey)
}

// SetupEncryptionForModel 为模型设置加密
func SetupEncryptionForModel(db *gorm.DB, es *EncryptionService, model interface{}, encryptedFields ...string) error {
	plugin := NewFieldEncryptionPlugin(es)
	
	// 获取表名
	stmt := &gorm.Statement{DB: db}
	if err := stmt.Parse(model); err != nil {
		return err
	}
	tableName := stmt.Schema.Table
	
	// 注册加密字段
	for _, field := range encryptedFields {
		plugin.RegisterEncryptedField(tableName, field)
	}
	
	return db.Use(plugin)
}

// EncryptContext 加密上下文键
type EncryptContext struct {
	es *EncryptionService
}

// ContextKey 上下文键类型
type ContextKey string

const (
	// EncryptionContextKey 加密服务上下文键
	EncryptionContextKey ContextKey = "encryption_service"
)

// WithEncryption 将加密服务添加到上下文
func WithEncryption(ctx context.Context, es *EncryptionService) context.Context {
	return context.WithValue(ctx, EncryptionContextKey, es)
}

// GetEncryptionFromContext 从上下文获取加密服务
func GetEncryptionFromContext(ctx context.Context) (*EncryptionService, bool) {
	es, ok := ctx.Value(EncryptionContextKey).(*EncryptionService)
	return es, ok
}
