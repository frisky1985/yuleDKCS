.PHONY: test lint build clean coverage

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
