# CLAUDE.md - AI Assistant Context for yuleDKCS

## Quick Reference

| Component | Language | Test Command | Lint Command |
|-----------|----------|--------------|--------------|
| Backend | Go 1.22 | `go test ./...` | `golangci-lint run` |
| Frontend | TS/Node 20 | `npm run test` | `npm run lint` |
| iOS SDK | Swift | `swift test` | `swiftlint` |
| Android | Kotlin | `./gradlew test` | `ktlint` |
| Embedded | C/C++ | `make test` | `cppcheck` |

## Critical Rules

1. **Test First**: Write or run tests before implementing changes
2. **Type Safety**: TypeScript strict mode, Go race detector, strict Swift
3. **No Secrets**: Never commit credentials; use env vars
4. **Race Safety**: Go code must pass `go test -race`
5. **Documentation**: Update `docs/` when architecture changes

## Common Patterns

### Go Backend
```go
// Constructor pattern with dependency injection
type VehicleService struct {
    repo VehicleRepository
    mqtt MQTTClient
}

func NewVehicleService(repo VehicleRepository, mqtt MQTTClient) *VehicleService {
    return &VehicleService{repo: repo, mqtt: mqtt}
}

// Error wrapping
if err != nil {
    return fmt.Errorf("failed to get vehicle: %w", err)
}
```

### TypeScript Frontend
```typescript
// Use strict types
interface VehicleStatus {
    id: string;
    connected: boolean;
    lastSeen: Date;
}

// Async data fetching with error handling
const fetchStatus = async (id: string): Promise<VehicleStatus> => {
    const response = await api.get(`/vehicles/${id}/status`);
    if (!response.ok) {
        throw new Error(`Failed to fetch: ${response.statusText}`);
    }
    return response.json();
};
```

### Embedded C
```c
// MISRA-compliant pattern
#include "digital_key.h"

static dk_status_t g_status = DK_STATUS_IDLE;  // Static for internal linkage

dk_error_t dk_init(const dk_config_t* config)
{
    if (config == NULL) {
        return DK_ERROR_INVALID_PARAM;
    }
    // Implementation
    return DK_OK;
}
```

## Testing Guidelines

- **Go**: Table-driven tests with subtests
- **TypeScript**: Vitest with `describe/it` pattern
- **Swift**: XCTest with given-when-then comments
- **Embedded**: Unity framework with mocked hardware

## Architecture Constraints

### Dependency Direction
```
API Handlers -> Services -> Repositories -> Database
     ↑            ↑           ↑
  Adapters --------┘           |
     ↑                        |
  External APIs ---------------┘
```

- Business logic lives in Services (no HTTP awareness)
- Repositories abstract database access
- Adapters handle external integrations

### API Design
- RESTful endpoints with consistent error format
- MQTT for real-time vehicle communication
- Protobuf for Mobile ↔ Backend communication
- JSON for Frontend ↔ Backend communication

## Multi-Platform Considerations

When modifying:
- **Backend API** → Check Mobile SDK consumers
- **MQTT Protocol** → Check Embedded + Mobile implementations
- **Database Schema** → Check migration files + rollback plan
- **Security Protocol** → Check all three platforms

## Code Review Checklist

- [ ] Tests pass (`go test -race`, `npm run test`, etc.)
- [ ] Linting passes
- [ ] No hardcoded secrets
- [ ] Error handling is comprehensive
- [ ] Documentation updated if needed
- [ ] Backwards compatibility considered
