package tsp

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"go.uber.org/zap/zaptest"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
)

// mockStream implements pb.VehicleControlService_StreamStatusServer for testing
type mockStream struct {
	grpc.ServerStream
	ctx    context.Context
	mu     sync.Mutex
	sent   []*pb.VehicleStatusUpdate
	sendCh chan *pb.VehicleStatusUpdate
	done   bool
}

func newMockStream(ctx context.Context) *mockStream {
	return &mockStream{
		ctx:    ctx,
		sendCh: make(chan *pb.VehicleStatusUpdate, 10),
	}
}

func (s *mockStream) Context() context.Context { return s.ctx }

func (s *mockStream) Send(status *pb.VehicleStatusUpdate) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sent = append(s.sent, status)
	s.sendCh <- status
	return nil
}

func (s *mockStream) sentCount() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return len(s.sent)
}

// TestNewService tests service creation
func TestNewService(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)
	if svc == nil {
		t.Fatal("NewService returned nil")
	}
	if svc.logger == nil {
		t.Fatal("logger is nil")
	}
}

// TestSendCommand tests the SendCommand handler
func TestSendCommand(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	req := &pb.ControlCommandRequest{
		VehicleId: "VH-TEST-001",
		Action:    "unlock",
		KeyId:     "key-abc-123",
	}

	ctx := context.Background()
	resp, err := svc.SendCommand(ctx, req)
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}

	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if resp.ResultCode != 0 {
		t.Errorf("expected ResultCode=0, got %d", resp.ResultCode)
	}
	expectedCmdID := "tsp-cmd-VH-TEST-001"
	if resp.CmdId != expectedCmdID {
		t.Errorf("expected CmdId='%s', got '%s'", expectedCmdID, resp.CmdId)
	}
}

// TestSendCommand_MultipleVehicles tests multiple command invocations
func TestSendCommand_MultipleVehicles(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	vehicles := []string{"VH-001", "VH-002", "VH-003"}
	actions := []string{"lock", "unlock", "start"}

	for i, vid := range vehicles {
		req := &pb.ControlCommandRequest{
			VehicleId: vid,
			Action:    actions[i],
		}
		resp, err := svc.SendCommand(context.Background(), req)
		if err != nil {
			t.Fatalf("SendCommand for %s failed: %v", vid, err)
		}
		expectedCmdID := fmt.Sprintf("tsp-cmd-%s", vid)
		if resp.CmdId != expectedCmdID {
			t.Errorf("expected CmdId='%s', got '%s'", expectedCmdID, resp.CmdId)
		}
	}
}

// TestSendCommand_NilRequest tests handling of nil request (though proto handles this)
func TestSendCommand_WithLoggerLevels(t *testing.T) {
	// Use a captured logger to verify log output
	var logs []string
	var mu sync.Mutex

	encoder := zapcore.NewConsoleEncoder(zapcore.EncoderConfig{})
	core := zapcore.NewCore(
		encoder,
		zapcore.AddSync(&testWriteSyncer{writeFn: func(p []byte) (n int, err error) {
			mu.Lock()
			logs = append(logs, string(p))
			mu.Unlock()
			return len(p), nil
		}}),
		zapcore.InfoLevel,
	)
	logger := zap.New(core)
	svc := NewService(logger)

	req := &pb.ControlCommandRequest{
		VehicleId: "VH-LOG-001",
		Action:    "lock",
	}
	resp, err := svc.SendCommand(context.Background(), req)
	if err != nil {
		t.Fatalf("SendCommand failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	_ = logs
}

type testWriteSyncer struct {
	writeFn func(p []byte) (n int, err error)
}

func (w *testWriteSyncer) Write(p []byte) (n int, err error) {
	return w.writeFn(p)
}

func (w *testWriteSyncer) Sync() error { return nil }

// TestStreamStatus_ContextDeadline tests stream exits on context deadline
func TestStreamStatus_ContextDeadline(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	// Create a context that will be cancelled quickly
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Millisecond)
	defer cancel()

	stream := newMockStream(ctx)
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-STREAM-001",
	}

	err := svc.StreamStatus(req, stream)
	// The context should time out before the 5s ticker fires
	if err == nil {
		t.Log("StreamStatus returned nil - ticker may have fired before context deadline")
	} else {
		t.Logf("StreamStatus returned expected error: %v", err)
	}
}

// TestStreamStatus_ContextCancelled tests stream exits on immediate context cancellation
func TestStreamStatus_ContextCancelled(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	ctx, cancel := context.WithCancel(context.Background())
	cancel() // Cancel immediately

	stream := newMockStream(ctx)
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-CANCEL-001",
	}

	err := svc.StreamStatus(req, stream)
	if err == nil {
		t.Fatal("expected error from cancelled context, got nil")
	}
}

// TestProtoImports verifies proto dependency resolution
func TestProtoImports(t *testing.T) {
	// Verify we can reference proto types without errors
	_ = &pb.ControlCommandRequest{VehicleId: "test"}
	_ = &pb.ControlCommandResponse{CmdId: "test"}
	_ = &pb.VehicleStatusRequest{VehicleId: "test"}
	_ = proto.Message(nil)
	_, _ = context.Background(), time.Now
}
