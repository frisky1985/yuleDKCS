package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"sync"
	"syscall"

	"github.com/yuleDKCS/tests/e2e/proto"
)

const (
	defaultPort = 18001
)

// NeedCarSimProtoInit is a compile-time check that proto is imported (triggers init).
var NeedCarSimProtoInit = proto.EncodePayload

func main() {
	port := defaultPort
	if p := os.Getenv("CARSIM_PORT"); p != "" {
		fmt.Sscanf(p, "%d", &port)
	}

	addr := fmt.Sprintf(":%d", port)
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on %s: %v", addr, err)
	}
	defer listener.Close()

	log.Printf("🚗 Car Simulator listening on TCP %s", addr)
	log.Printf("   Vehicle: SIM_CAR_001 (Shanghai, 31.2304, 121.4737)")
	log.Printf("   Protocols: ICCE, CCC, ICCOA")
	log.Printf("   SE050: ✅ initialized with P256 keypair")

	// Initialize vehicle state and SE050
	vehicle := NewVehicleState()
	se050 := NewSE050Mock()
	handler := NewHandler(se050, vehicle)

	var wg sync.WaitGroup
	connCount := 0

	// Handle graceful shutdown
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigCh
		log.Println("Shutting down car simulator...")
		listener.Close()
	}()

	for {
		conn, err := listener.Accept()
		if err != nil {
			if opErr, ok := err.(*net.OpError); ok && opErr.Temporary() {
				continue
			}
			break
		}

		connCount++
		wg.Add(1)
		go handleConnection(conn, handler, vehicle, connCount)
	}

	wg.Wait()
	log.Println("Car simulator stopped")
}

func handleConnection(conn net.Conn, handler *Handler, vehicle *VehicleState, id int) {
	defer conn.Close()
	defer func() {
		log.Printf("[Conn-%d] Closed", id)
	}()

	log.Printf("[Conn-%d] 📱 Client connected from %s", id, conn.RemoteAddr())

	for {
		frame, err := proto.ReadFrame(conn)
		if err != nil {
			log.Printf("[Conn-%d] Read error: %v", id, err)
			return
		}

		log.Printf("[Conn-%d] ➡️ Received msg_type=0x%04X seq=%d len=%d",
			id, frame.Header.MsgType, frame.Header.SeqNum, len(frame.Payload))

		resp, err := handler.Handle(frame)
		if err != nil {
			log.Printf("[Conn-%d] Handle error: %v", id, err)
			continue
		}

		if resp != nil {
			data := resp.Marshal()
			if _, err := conn.Write(data); err != nil {
				log.Printf("[Conn-%d] Write error: %v", id, err)
				return
			}
			log.Printf("[Conn-%d] ⬅️ Sent msg_type=0x%04X seq=%d len=%d",
				id, resp.Header.MsgType, resp.Header.SeqNum, len(resp.Payload))
		}
	}
}

// StatusPayload is needed by handler — defined in state.go
var _ = StatusPayload{}
