package tsp

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"go.uber.org/zap/zaptest"

	pb "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/v1"
	"google.golang.org/grpc"
)

// controllableMockStream allows controlling Send behavior (success or error)
type controllableMockStream struct {
	grpc.ServerStream
	ctx       context.Context
	mu        sync.Mutex
	sent      []*pb.VehicleStatusUpdate
	failAfter int // fail after this many successful sends (0 = never fail, -1 = fail immediately)
	sendCount int
}

func newControllableMockStream(ctx context.Context, failAfter int) *controllableMockStream {
	return &controllableMockStream{
		ctx:       ctx,
		failAfter: failAfter,
	}
}

func (s *controllableMockStream) Context() context.Context { return s.ctx }

func (s *controllableMockStream) Send(status *pb.VehicleStatusUpdate) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.sent = append(s.sent, status)
	s.sendCount++
	if s.failAfter >= 0 && s.sendCount > s.failAfter {
		return errors.New("simulated send error")
	}
	return nil
}

func (s *controllableMockStream) sentCount() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return len(s.sent)
}

// TestStreamStatus_TickerFires verifies the ticker path sends status updates
func TestStreamStatus_TickerFires(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	// Set a context timeout long enough for the 5s ticker to fire at least once
	ctx, cancel := context.WithTimeout(context.Background(), 5100*time.Millisecond)
	defer cancel()

	stream := newControllableMockStream(ctx, -1) // never fail
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-TICKER-001",
	}

	err := svc.StreamStatus(req, stream)
	if err == nil {
		t.Log("StreamStatus returned nil (context timed out)")
	} else {
		t.Logf("StreamStatus returned: %v", err)
	}

	// Verify at least one status was sent by the ticker
	count := stream.sentCount()
	if count == 0 {
		t.Error("expected at least 1 status update from ticker, got 0")
	}
	t.Logf("Ticker sent %d status updates", count)

	// Verify the status has the correct VehicleId
	if count > 0 {
		status := stream.sent[0]
		if status.VehicleId != "VH-TICKER-001" {
			t.Errorf("expected VehicleId='VH-TICKER-001', got '%s'", status.VehicleId)
		}
		if status.Timestamp <= 0 {
			t.Error("expected positive timestamp")
		}
	}
}

// TestStreamStatus_SendError verifies error handling when stream.Send fails
func TestStreamStatus_SendError(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	// Use a long context timeout; the error should terminate before the timeout
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	// Fail after first successful send (i.e., second send will fail)
	stream := newControllableMockStream(ctx, 1)
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-ERR-001",
	}

	err := svc.StreamStatus(req, stream)
	if err == nil {
		t.Fatal("expected error from stream.Send failure, got nil")
	}
	t.Logf("Got expected send error: %v", err)

	// Verify that the first send succeeded (observed before error)
	count := stream.sentCount()
	if count < 1 {
		t.Errorf("expected at least 1 successful send before error, got %d", count)
	}

	// Verify the status has the correct VehicleId
	if count >= 1 {
		status := stream.sent[0]
		if status.VehicleId != "VH-ERR-001" {
			t.Errorf("expected VehicleId='VH-ERR-001', got '%s'", status.VehicleId)
		}
	}
}

// TestStreamStatus_SendImmediateFail verifies error path when first Send fails
func TestStreamStatus_SendImmediateFail(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	// Fail immediately on first send (failAfter=0 means sendCount>0 after first send => fail on second tick)
	// Actually, failAfter=0 means sendCount>0 after 1 send, so
	// send 1: sendCount becomes 1, failAfter=0 => 1>0 is true => fail
	// So first send fails. But with failAfter=0, all sends fail.
	// Let me use failAfter=-1 with a different approach.
	// We need: first send to fail. failAfter=0 means fail when sendCount>0, so
	// sendCount starts at 0.
	// After we increment (sendCount++), sendCount=1.
	// failAfter=0 means fail if sendCount > 0 => 1>0 => true. So first send fails correctly.
	stream := newControllableMockStream(ctx, 0)
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-IMMEDIATE-ERR",
	}

	err := svc.StreamStatus(req, stream)
	if err == nil {
		t.Fatal("expected error from immediate stream.Send failure, got nil")
	}
	t.Logf("Got expected immediate send error: %v", err)
	_ = stream.sentCount()
}

// TestService_ConcurrentCreationAndLogging tests concurrent access patterns
func TestService_ConcurrentCreationAndLogging(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	var wg sync.WaitGroup
	for i := 0; i < 10; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			req := &pb.ControlCommandRequest{
				VehicleId: "VH-CONCURRENT",
				Action:    "lock",
			}
			resp, err := svc.SendCommand(context.Background(), req)
			if err != nil {
				t.Errorf("concurrent SendCommand %d failed: %v", n, err)
			}
			if resp == nil {
				t.Errorf("concurrent SendCommand %d returned nil response", n)
			}
		}(i)
	}
	wg.Wait()
}

// TestService_EmptyVehicleID tests edge case with empty vehicle ID
func TestService_EmptyVehicleID(t *testing.T) {
	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	req := &pb.ControlCommandRequest{
		VehicleId: "",
		Action:    "unlock",
	}
	resp, err := svc.SendCommand(context.Background(), req)
	if err != nil {
		t.Fatalf("SendCommand with empty VehicleId failed: %v", err)
	}
	if resp == nil {
		t.Fatal("expected non-nil response")
	}
	if resp.CmdId != "tsp-cmd-" {
		t.Errorf("expected CmdId='tsp-cmd-', got '%s'", resp.CmdId)
	}
}

// TestService_MultipleTickerCycles verifies StreamStatus sends multiple updates
func TestService_MultipleTickerCycles(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping long ticker test in short mode")
	}

	logger := zaptest.NewLogger(t)
	svc := NewService(logger)

	// Set context timeout for 2 ticker cycles (~10s + margin)
	ctx, cancel := context.WithTimeout(context.Background(), 11*time.Second)
	defer cancel()

	stream := newControllableMockStream(ctx, -1) // never fail
	req := &pb.VehicleStatusRequest{
		VehicleId: "VH-MULTI-001",
	}

	_ = svc.StreamStatus(req, stream)

	// Should have received at least 2 updates in ~11 seconds
	count := stream.sentCount()
	if count < 2 {
		t.Errorf("expected at least 2 status updates from 2 ticker cycles, got %d", count)
	}
	t.Logf("Multiple ticker cycles sent %d status updates", count)
}
