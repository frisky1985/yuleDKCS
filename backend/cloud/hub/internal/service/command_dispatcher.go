package service

import (
	"context"
	"sync"

	"go.uber.org/zap"
)

// ControlCommand 一条待下发的远程控车指令。
type ControlCommand struct {
	CmdID     string // 命令唯一 id
	VehicleID string // 车辆 id
	KeyID     string // 使用的数字钥匙 id
	Action    string // 指令动作 (unlock/lock/engine_on/...)
	Params    []byte // 附加参数 (JSON)
	Source    int32  // 1=NFC 2=BLE 3=UWB 4=Remote 5=Edge
	TraceID   string // 链路追踪 id
	Timestamp int64  // 下发时间 unix millis
}

// CommandDispatcher 指令下发通道接口。
//
// 将远程控车指令下发到车端 TCU。生产实现可基于 MQTT
// (topic: vehicle/{vehicle_id}/command), 当前提供 Noop/Mock 实现,
// 通过 WithCommandDispatcher 注入, 不引入外部 MQTT 依赖。
type CommandDispatcher interface {
	// Send 下发一条指令到车端。返回 error 表示下发失败。
	Send(ctx context.Context, cmd *ControlCommand) error
}

// NoopCommandDispatcher 空实现 — 记录日志但不实际下发。
// 作为 dev/test 默认实现, 生产环境应注入真实 MQTT 实现。
type NoopCommandDispatcher struct {
	logger *zap.Logger
}

// NewNoopCommandDispatcher 创建空实现下发通道。
func NewNoopCommandDispatcher(logger *zap.Logger) *NoopCommandDispatcher {
	return &NoopCommandDispatcher{logger: logger.With(zap.String("component", "command_dispatcher"))}
}

// Send 记录日志并返回 nil (模拟下发成功)。
func (d *NoopCommandDispatcher) Send(_ context.Context, cmd *ControlCommand) error {
	d.logger.Info("command dispatched (noop)",
		zap.String("cmd_id", cmd.CmdID),
		zap.String("vehicle_id", cmd.VehicleID),
		zap.String("action", cmd.Action),
	)
	return nil
}

// MockCommandDispatcher 测试用实现 — 记录所有下发的指令, 可注入错误。
type MockCommandDispatcher struct {
	mu       sync.Mutex
	Commands []*ControlCommand // 已下发的指令列表 (并发安全)
	Err      error             // 非 nil 时 Send 返回该错误 (模拟下发失败)
}

// NewMockCommandDispatcher 创建测试用下发通道。
func NewMockCommandDispatcher() *MockCommandDispatcher {
	return &MockCommandDispatcher{}
}

// Send 记录指令; 若设置了 Err 则返回它。
func (d *MockCommandDispatcher) Send(_ context.Context, cmd *ControlCommand) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.Commands = append(d.Commands, cmd)
	return d.Err
}

// Count 返回已下发的指令数。
func (d *MockCommandDispatcher) Count() int {
	d.mu.Lock()
	defer d.mu.Unlock()
	return len(d.Commands)
}
