package testutils

import (
	"context"
	"database/sql"
	"testing"
	"time"

	"github.com/DATA-DOG/go-sqlmock"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// MockDB 包含 SQL mock 和 GORM DB 实例
type MockDB struct {
	DB   *gorm.DB
	Mock sqlmock.Sqlmock
	SQL  *sql.DB
}

// NewMockDB 创建一个新的 Mock 数据库
func NewMockDB(t *testing.T) *MockDB {
	sqlDB, mock, err := sqlmock.New()
	if err != nil {
		t.Fatalf("创建 sqlmock 失败: %v", err)
	}

	dialector := postgres.New(postgres.Config{
		Conn:       sqlDB,
		DriverName: "postgres",
	})

	gormDB, err := gorm.Open(dialector, &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		t.Fatalf("创建 GORM DB 失败: %v", err)
	}

	return &MockDB{
		DB:   gormDB,
		Mock: mock,
		SQL:  sqlDB,
	}
}

// Close 关闭数据库连接
func (m *MockDB) Close() {
	if m.SQL != nil {
		m.SQL.Close()
	}
}

// ExpectQuery 期望执行查询
func (m *MockDB) ExpectQuery(query string) *sqlmock.ExpectedQuery {
	return m.Mock.ExpectQuery(query)
}

// ExpectExec 期望执行 Exec
func (m *MockDB) ExpectExec(query string) *sqlmock.ExpectedExec {
	return m.Mock.ExpectExec(query)
}

// ExpectBegin 期望开始事务
func (m *MockDB) ExpectBegin() *sqlmock.ExpectedBegin {
	return m.Mock.ExpectBegin()
}

// ExpectCommit 期望提交事务
func (m *MockDB) ExpectCommit() *sqlmock.ExpectedCommit {
	return m.Mock.ExpectCommit()
}

// ExpectRollback 期望回滚事务
func (m *MockDB) ExpectRollback() *sqlmock.ExpectedRollback {
	return m.Mock.ExpectRollback()
}

// ExpectationsWereMet 检查所有期望是否都被满足
func (m *MockDB) ExpectationsWereMet() error {
	return m.Mock.ExpectationsWereMet()
}

// MatchExpectationsInOrder 设置是否按顺序匹配期望
func (m *MockDB) MatchExpectationsInOrder(inOrder bool) {
	m.Mock.MatchExpectationsInOrder(inOrder)
}

// TestContext 返回带超时的测试上下文
func TestContext() (context.Context, context.CancelFunc) {
	return context.WithTimeout(context.Background(), 5*time.Second)
}

// AnyTime 匹配任意时间值的 sqlmock 参数
func AnyTime() interface{} {
	return sqlmock.AnyArg()
}

// AnyString 匹配任意字符串的 sqlmock 参数
func AnyString() interface{} {
	return sqlmock.AnyArg()
}

// AnyUint 匹配任意 uint 的 sqlmock 参数
func AnyUint() interface{} {
	return sqlmock.AnyArg()
}
