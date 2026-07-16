// Package security — Security penetration test helpers
package security

import (
	"os"
	"testing"
)

// getCarAddr returns the car simulator TCP address for security tests.
func getCarAddr(t *testing.T) string {
	addr := os.Getenv("CARSIM_ADDR")
	if addr == "" {
		addr = "localhost:18001"
	}
	t.Logf("🎯 Car simulator address: %s", addr)
	return addr
}

// getAPIGateway returns the API gateway URL for security tests.
func getAPIGateway(t *testing.T) string {
	addr := os.Getenv("API_GATEWAY")
	if addr == "" {
		addr = "http://localhost:8080"
	}
	t.Logf("🎯 API Gateway: %s", addr)
	return addr
}

// getJWTSecret returns the JWT secret for security tests.
func getJWTSecret() string {
	secret := os.Getenv("JWT_SECRET")
	if secret == "" {
		secret = "test-jwt-secret-for-pentest-2026"
	}
	return secret
}

// getAdminCreds returns admin credentials for testing.
func getAdminCreds() (string, string) {
	user := os.Getenv("ADMIN_USERNAME")
	if user == "" {
		user = "admin"
	}
	pass := os.Getenv("ADMIN_PASSWORD")
	if pass == "" {
		pass = "admin123"
	}
	return user, pass
}
