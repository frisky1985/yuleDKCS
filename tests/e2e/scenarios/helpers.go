package scenarios

import (
	"net"
	"os"
	"testing"
	"time"

	"github.com/yuleDKCS/tests/e2e/proto"
)

// getCarAddr returns the car simulator address, defaulting to localhost:18001.
func getCarAddr(t *testing.T) string {
	addr := os.Getenv("CARSIM_ADDR")
	if addr == "" {
		addr = "localhost:18001"
	}
	t.Logf("🎯 Car simulator address: %s", addr)
	return addr
}

// requireCarSim skips the test when no car simulator is reachable.
// All scenario tests are integration tests that need the carsim binary
// (tests/e2e/carsim) listening on CARSIM_ADDR (default localhost:18001).
// CI without the simulator should skip, not fail.
func requireCarSim(t *testing.T) {
	t.Helper()
	carAddr := getCarAddr(t)
	if os.Getenv("CARSIM_ADDR") == "" {
		conn, err := net.DialTimeout("tcp", carAddr, 500*time.Millisecond)
		if err != nil {
			t.Skipf("car simulator not reachable at %s — skipping integration test", carAddr)
		}
		conn.Close()
	}
}

// encodePayload wraps proto.EncodePayload for use by scenario test files.
func encodePayload(v interface{}) []byte { return proto.EncodePayload(v) }

// decodePayload wraps proto.DecodePayload for use by scenario test files.
func decodePayload(data []byte, v interface{}) error { return proto.DecodePayload(data, v) }
