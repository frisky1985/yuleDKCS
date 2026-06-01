module github.com/frisky1985/yuleDKCS/backend/dkcs

go 1.22

require (
	github.com/golang-jwt/jwt/v5 v5.3.1
	github.com/jmoiron/sqlx v1.4.0
	github.com/lib/pq v1.10.9
	github.com/redis/go-redis/v9 v9.5.1
	google.golang.org/grpc v1.63.2
)

require (
	github.com/cespare/xxhash/v2 v2.2.0 // indirect
	golang.org/x/net v0.24.0 // indirect
	golang.org/x/sys v0.19.0 // indirect
	golang.org/x/text v0.14.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240227224415-6ceb2ff114de // indirect
	google.golang.org/protobuf v1.33.0 // indirect
)

replace github.com/digitalkey/proto/dkcs => ./proto
