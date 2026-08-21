module github.com/yuleDKCS/tests/security

go 1.25.0

require (
	github.com/golang-jwt/jwt/v5 v5.2.2
	github.com/yuleDKCS/tests/e2e/client v0.0.0
	github.com/yuleDKCS/tests/e2e/proto v0.0.0
)

replace (
	github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/codec/bertlv => ../../backend/cloud/hub
	github.com/yuleDKCS/tests/e2e/client => ../e2e/client
	github.com/yuleDKCS/tests/e2e/proto => ../e2e/proto
)
