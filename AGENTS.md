# yuleDKCS Agent Instructions

## Project Overview

yuleDKCS is a multi-protocol Digital Key Connectivity System (Triple-End Architecture):
- **Cloud Backend**: Go (API + MQTT)
- **Mobile**: iOS SDK + Android SDK + Flutter App
- **Embedded**: C/C++ (KW47 MCU)

## Commands

### Backend (Go)
```bash
cd backend
go mod download
go build -o yuledkcs-api ./cmd/api
go test -v ./...
go test -race -coverprofile=coverage.out ./...
go tool cover -html=coverage.out -o coverage.html
golangci-lint run --timeout 5m
```

### Frontend (TypeScript/Vite)
```bash
cd frontend
npm install
npm run dev          # Dev server
npm run build        # Production build
npm run lint         # ESLint
npm run type-check   # TypeScript check
npm run test         # Unit tests
npm run test:coverage # With coverage
```

### Mobile
```bash
# iOS SDK
cd mobile/ios
swift build
swift test

# Android SDK
cd mobile/android
./gradlew build
./gradlew test

# Flutter App
cd mobile/flutter
flutter build ios
flutter build apk
```

### Embedded (KW47)
```bash
cd embedded
mkdir -p build && cd build
cmake ..
make
make test
```

### Docker
```bash
docker compose up -d              # Start all services
docker compose -f backend/docker-compose.yml up -d  # Backend only
docker build -t yuledkcs-backend ./backend
```

## Project Structure

```
yuleDKCS/
├── backend/          # Go API + MQTT services
│   ├── cmd/api/      # Main API entry
│   ├── adapters/     # External integrations
│   ├── database/     # DB models & migrations
│   └── config/       # Configuration
├── frontend/         # Web dashboard (Vite + React)
│   └── src/
├── mobile/           # Mobile SDKs & App
│   ├── ios/          # Swift SDK
│   ├── android/      # Kotlin SDK
│   └── flutter/      # Flutter reference app
├── embedded/         # KW47 MCU firmware
│   ├── src/          # Core implementation
│   ├── sdk/          # External SDKs
│   └── tests/        # Unit tests
├── docs/             # Architecture & design docs
├── tests/            # Integration & E2E tests
└── deploy/           # Deployment configs
```

## Testing Conventions

- **Unit tests**: Co-located with source (`*_test.go`, `*.test.ts`)
- **Integration tests**: `tests/` directory at project root
- **Coverage requirement**: ≥80% for backend, ≥70% for frontend
- **Mock external services**, never the database (use testcontainers)
- Run `go test -race` for Go code (concurrency safety)

## Code Style

### Go
- Follow [Effective Go](https://go.dev/doc/effective_go)
- Use `golangci-lint` with configured rules
- Error handling: wrap with context using `fmt.Errorf("context: %w", err)`
- No global state; dependency injection via constructors

### TypeScript
- Strict TypeScript configuration
- Functional components with hooks
- Prefer `async/await` over callbacks

### Embedded C/C++
- MISRA C:2012 compliance for safety-critical code
- Static analysis with cppcheck
- Unit tests with Unity/CMock framework

## Git Workflow

- Branch from `main`, PR back to `main`
- Commit messages: `type(scope): description` (conventional commits)
  - `feat(backend): add vehicle status endpoint`
  - `fix(embedded): correct BLE advertisement interval`
  - `docs(api): update authentication flow`
- One concern per PR
- Squash merge preferred

## Dependencies

### Backend
- Go 1.22+
- PostgreSQL 15
- Redis 7
- MQTT Broker (EMQX or Mosquitto)

### Frontend
- Node.js 20
- Vite 5
- React 18

### Mobile
- Xcode 15+ (iOS)
- Android Studio Hedgehog+ (Android)
- Flutter 3.19+

### Embedded
- ARM GCC toolchain
- CMake 3.20+
- J-Link or equivalent debugger

## Documentation

- `docs/architecture.md` - System architecture overview
- `docs/api/` - API documentation
- `docs/design/` - Design decisions & ADRs
- `DEPLOYMENT_PLAN.md` - Deployment procedures

## CI/CD Pipeline

GitHub Actions runs:
1. Lint & type check
2. Unit tests with coverage
3. Integration tests
4. Security scanning (Trivy)
5. Docker image build

## Boundaries

### Always Do
- Run tests before committing
- Check types/lint before pushing
- Update docs when changing architecture
- Add tests for new features
- Use structured logging (not fmt.Print)

### Ask First
- Adding new dependencies (check license)
- Changing database schema
- Modifying public API signatures
- Updating CI/CD configuration
- Adding new environment variables

### Never Do
- Skip or delete failing tests
- Commit secrets, tokens, or credentials
- Hardcode credentials in source code
- Modify files outside task scope
- Force push to main
- Bypass PR review (even for "urgent" fixes)

## Security

- API uses JWT authentication with short expiry
- MQTT requires TLS + client certificates
- Embedded uses secure boot + encrypted storage
- Follow OWASP Top 10 for web components
- Secrets in environment variables only (never in code)

## Multi-Agent Coordination

This project supports parallel development:
- Backend, Frontend, Mobile, Embedded can be worked on independently
- API contracts defined in `docs/api/`
- Use feature flags for partial implementations
- Coordinate through `TASK_STATUS.md` for cross-component work
