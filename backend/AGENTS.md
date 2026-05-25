# Backend Agent Instructions

## Commands

```bash
# Development
go run ./cmd/api                    # Run API server
go run ./cmd/api --config=./config.local.yaml

# Testing
go test ./...                       # All tests
go test -v ./...                    # Verbose
go test -race ./...                 # Race detection
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out -o coverage.html

# Build
CGO_ENABLED=0 GOOS=linux go build -ldflags="-w -s" -o yuledkcs-api ./cmd/api

# Linting
golangci-lint run --timeout 5m
gofmt -w .                          # Format
goimports -w .                      # Fix imports

# Database migrations
# (using golang-migrate)
migrate -path ./database/migrations -database "postgres://..." up
migrate -path ./database/migrations -database "postgres://..." down
```

## Structure

```
backend/
├── cmd/
│   └── api/              # Main API entry point
├── adapters/           # External service integrations
│   ├── mqtt/           # MQTT client
│   └── vehicle/        # Vehicle adapter
├── database/           # Database layer
│   ├── migrations/     # SQL migrations
│   └── models/         # GORM models
├── config/             # Configuration structs
├── internal/           # Private packages
└── pkg/                # Public packages
```

## Patterns

### Handler (HTTP)
```go
func (h *VehicleHandler) GetStatus(c *gin.Context) {
    id := c.Param("id")
    status, err := h.service.GetStatus(c.Request.Context(), id)
    if err != nil {
        c.JSON(http.StatusInternalServerError, ErrorResponse{Error: err.Error()})
        return
    }
    c.JSON(http.StatusOK, status)
}
```

### Service (Business Logic)
```go
type VehicleService struct {
    repo VehicleRepository
    mqtt MQTTClient
}

func (s *VehicleService) GetStatus(ctx context.Context, id string) (*VehicleStatus, error) {
    // Business logic here - no HTTP awareness
}
```

### Repository (Data Access)
```go
type VehicleRepository interface {
    GetByID(ctx context.Context, id string) (*Vehicle, error)
    UpdateStatus(ctx context.Context, id string, status *VehicleStatus) error
}
```

## Testing

- Unit tests: `*_test.go` next to source
- Integration tests: `tests/integration/`
- Use `testify/assert` for assertions
- Mock interfaces with `mockery` or hand-written mocks
- Test database with `testcontainers-go`

## Database Migrations

1. Create new migration:
   ```bash
   migrate create -ext sql -dir database/migrations -seq add_vehicle_column
   ```
2. Write up/down SQL
3. Test locally: `migrate up`
4. Include in PR with rollback plan

## Configuration

- Config file: `config.yaml` (see `config.example.yaml`)
- Environment variables override file values
- Secrets in env vars only
- Local dev: `config.local.yaml` (gitignored)

## API Standards

- RESTful endpoints
- Consistent error format: `{"error": "message", "code": "ERROR_CODE"}`
- Pagination: `?page=1&page_size=20`
- Sorting: `?sort=-created_at` (minus for DESC)
- Filtering: `?status=active&vehicle_type=car`
