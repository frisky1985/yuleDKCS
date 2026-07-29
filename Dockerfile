FROM golang:1.26-alpine AS builder

WORKDIR /app
COPY backend/ ./backend/
COPY go.work go.work.sum go.mod go.sum ./

# Build both binaries
RUN cd backend/dkcs && CGO_ENABLED=0 go build -o /app/dkcs-server ./cmd/dkcs && \
    cd /app/backend/cloud/hub && CGO_ENABLED=0 go build -o /app/hub-server ./cmd/hub

FROM alpine:3.19

RUN apk add --no-cache ca-certificates tzdata && \
    adduser -D -u 1001 -s /bin/sh -h /home/app appuser

COPY --from=builder /app/dkcs-server /usr/local/bin/
COPY --from=builder /app/hub-server /usr/local/bin/

EXPOSE 8080 9090

USER appuser
WORKDIR /home/app

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:8080/healthz || exit 1

# Default: dkcs-server. Override with CMD ["hub-server"] for hub-only mode
ENTRYPOINT ["/usr/local/bin/dkcs-server"]
