module github.com/frisky1985/yuleDKCS/backend/cloud/hub/tests/integration

go 1.25.0

require (
	github.com/frisky1985/yuleDKCS/backend/cloud/hub v0.0.0
	github.com/stretchr/testify v1.10.0
	google.golang.org/grpc v1.82.0
)

require (
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

replace github.com/frisky1985/yuleDKCS/backend/cloud/hub => ../../
