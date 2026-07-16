.PHONY: test lint build clean coverage vet migrate docker-up docker-down docker-prod-up docker-prod-down init-db test-integration

test:
	go test -cover ./backend/dkcs/... ./backend/cloud/hub/...

build:
	go build ./backend/dkcs/... ./backend/cloud/hub/...

lint:
	golangci-lint run ./backend/... 2>&1 || true

coverage:
	go test -coverprofile=coverage.out ./backend/dkcs/... ./backend/cloud/hub/...
	go tool cover -func=coverage.out

vet:
	go vet ./backend/...

clean:
	rm -f coverage.out

# ---- Migrations ----
migrate:
	go run backend/hub/migrations/run_migrations.go

init-db:
	bash scripts/init-db.sh

# ---- Docker development targets ----

docker-up:
	docker compose up -d redis zookeeper kafka

docker-down:
	docker compose down

# ---- Docker production targets (docker-compose.prod.yml) ----
docker-prod-up:
	docker compose -f docker-compose.prod.yml up -d

docker-prod-down:
	docker compose -f docker-compose.prod.yml down

# Run integration tests with Docker dependencies
# Usage: make test-integration [TEST_FLAGS="-run TestIntegration"]
test-integration: docker-up
	go test -tags=integration -v -count=1 -cover $(TEST_FLAGS) ./backend/dkcs/... ./backend/cloud/hub/...

# Run only the dkcs integration tests
# These currently use mocks (miniredis, mock kafka), so they run without Docker.
# Add real Docker-backed tests gradually by marking them with //go:build integration

