package main

import (
	"log"

	"github.com/frisky1985/yuleDKCS/backend/cmd/api"
)

func main() {
	if err := api.Run(); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}