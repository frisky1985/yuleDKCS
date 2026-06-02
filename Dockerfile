FROM golang:1.25-alpine AS builder
WORKDIR /app
COPY backend/ ./backend/
COPY go.work go.sum go.mod ./
RUN cd backend/dkcs && go build -o /app/dkcs-server ./cmd/dkcs
RUN cd backend/cloud/hub && go build -o /app/hub-server ./cmd/hub

FROM alpine:3.19
RUN apk add --no-cache ca-certificates
COPY --from=builder /app/dkcs-server /usr/local/bin/
COPY --from=builder /app/hub-server /usr/local/bin/
EXPOSE 8080 9090
CMD ["/usr/local/bin/hub-server"]
