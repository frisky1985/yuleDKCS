module github.com/yuleDKCS/tests/e2e/scenarios

go 1.21

require github.com/yuleDKCS/tests/e2e/client v0.0.0

require github.com/yuleDKCS/tests/e2e/proto v0.0.0

replace (
	github.com/yuleDKCS/tests/e2e/client => ../client
	github.com/yuleDKCS/tests/e2e/proto => ../proto
)
