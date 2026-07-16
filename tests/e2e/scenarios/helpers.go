package scenarios

import (
	"os"
	"testing"

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

// encodePayload wraps proto.EncodePayload for use by scenario test files.
func encodePayload(v interface{}) []byte { return proto.EncodePayload(v) }

// decodePayload wraps proto.DecodePayload for use by scenario test files.
func decodePayload(data []byte, v interface{}) error { return proto.DecodePayload(data, v) }
