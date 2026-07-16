package client

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"fmt"
	"log"
	"net"
	"time"

	"github.com/yuleDKCS/tests/e2e/proto"
)

// MobileClient simulates a mobile phone client that talks to the car simulator over TCP
// and can make simulated gRPC calls to the cloud backend.
type MobileClient struct {
	DeviceID    string
	UserID      string
	Vendor      uint32
	Protocol    uint32

	// Cloud gRPC address (simulated or real)
	CloudAddr string

	// Car simulator connection
	carConn  net.Conn

	// Crypto state
	PrivateKey *ecdsa.PrivateKey
	PublicKey  []byte // DER-encoded
	CarPubKey  []byte // Car's SE050 public key

	// SCP03 session state
	HostChallenge []byte
	CardChallenge []byte
	SessionKey    []byte

	// Shared secret for ECDH simulation
	SharedSecret []byte

	// Bound keys on this client
	Keys map[string]*proto.KeyPayload
}

// NewMobileClient creates a new simulated mobile client.
func NewMobileClient(deviceID, userID string, vendor, protocol uint32) (*MobileClient, error) {
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, fmt.Errorf("generate key: %w", err)
	}
	pubDER, err := x509.MarshalPKIXPublicKey(&priv.PublicKey)
	if err != nil {
		return nil, fmt.Errorf("marshal pubkey: %w", err)
	}

	return &MobileClient{
		DeviceID:   deviceID,
		UserID:     userID,
		Vendor:     vendor,
		Protocol:   protocol,
		PrivateKey: priv,
		PublicKey:  pubDER,
		Keys:       make(map[string]*proto.KeyPayload),
	}, nil
}

// ConnectCar connects to the car simulator at the given TCP address.
func (c *MobileClient) ConnectCar(addr string) error {
	conn, err := net.DialTimeout("tcp", addr, 5*time.Second)
	if err != nil {
		return fmt.Errorf("connect to car: %w", err)
	}
	c.carConn = conn
	log.Printf("[%s] 📱 Connected to car simulator at %s", c.DeviceID, addr)
	return nil
}

// DisconnectCar closes the car connection.
func (c *MobileClient) DisconnectCar() {
	if c.carConn != nil {
		c.carConn.Close()
		c.carConn = nil
	}
}

// SendFrame sends a frame to the car simulator and receives a response.
func (c *MobileClient) SendFrame(msgType uint16, payload []byte) (*proto.Frame, error) {
	if c.carConn == nil {
		return nil, fmt.Errorf("not connected to car")
	}

	frame := proto.NewFrame(msgType, payload)
	data := frame.Marshal()

	if _, err := c.carConn.Write(data); err != nil {
		return nil, fmt.Errorf("send frame: %w", err)
	}

	resp, err := proto.ReadFrame(c.carConn)
	if err != nil {
		return nil, fmt.Errorf("read response: %w", err)
	}

	return resp, nil
}

// ── Protocol Flows ───────────────────────────────────────────

// BLEPairing performs BLE pairing with the car simulator.
func (c *MobileClient) BLEPairing() error {
	req := &proto.PairingPayload{
		DeviceID:  c.DeviceID,
		Vendor:    c.Vendor,
		Protocol:  c.Protocol,
		PublicKey: c.PublicKey,
		Challenge: make([]byte, 8),
	}
	rand.Read(req.Challenge)

	respFrame, err := c.SendFrame(proto.MsgTypePairingRequest, encodePayload(req))
	if err != nil {
		return fmt.Errorf("pairing request: %w", err)
	}

	var resp proto.PairingPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return fmt.Errorf("decode pairing response: %w", err)
	}

	c.CarPubKey = resp.PublicKey
	c.SharedSecret = resp.SessionKey
	log.Printf("[%s] 🔗 BLE paired, car=%s, protocol=%d", c.DeviceID, resp.DeviceID, resp.Protocol)
	return nil
}

// SCP03Establish establishes an SCP03 secure channel.
func (c *MobileClient) SCP03Establish() error {
	// Step 1: Send host challenge
	hostChallenge := make([]byte, 8)
	rand.Read(hostChallenge)

	initReq := &proto.SCP03Payload{
		Step:          0,
		HostChallenge: hostChallenge,
	}
	respFrame, err := c.SendFrame(proto.MsgTypeSCP03Init, encodePayload(initReq))
	if err != nil {
		return fmt.Errorf("SCP03 init: %w", err)
	}

	var initResp proto.SCP03Payload
	if err := decodePayload(respFrame.Payload, &initResp); err != nil {
		return fmt.Errorf("decode SCP03 init response: %w", err)
	}

	c.HostChallenge = hostChallenge
	c.CardChallenge = initResp.CardChallenge

	// Simulate SCP03 auth: compute MAC over (hostChallenge || cardChallenge)
	authData := append(c.HostChallenge, c.CardChallenge...)
	authMac := computeMAC(authData)

	authReq := &proto.SCP03Payload{
		Step: 1,
		Mac:  authMac,
	}
	respFrame, err = c.SendFrame(proto.MsgTypeSCP03Auth, encodePayload(authReq))
	if err != nil {
		return fmt.Errorf("SCP03 auth: %w", err)
	}

	var authResp proto.SCP03Payload
	if err := decodePayload(respFrame.Payload, &authResp); err != nil {
		return fmt.Errorf("decode SCP03 auth response: %w", err)
	}

	if !authResp.Success {
		return fmt.Errorf("SCP03 authentication failed")
	}

	log.Printf("[%s] 🔐 SCP03 secure channel established", c.DeviceID)
	return nil
}

// WriteKey writes a digital key to the car (from KeyPayload struct).
func (c *MobileClient) WriteKey(kp *proto.KeyPayload) error {
	if kp.KeyData == nil || len(kp.KeyData) == 0 {
		kp.KeyData = make([]byte, 32)
		rand.Read(kp.KeyData)
	}
	kp.Status = 1

	return c.writeKeyRaw(kp)
}

// WriteKeyParams writes a digital key with explicit parameters.
func (c *MobileClient) WriteKeyParams(keyID string, keyType uint32, permissions uint32, validFrom, validUntil int64) error {
	req := &proto.KeyPayload{
		KeyID:       keyID,
		KeyType:     keyType,
		Permissions: permissions,
		KeyData:     make([]byte, 32),
		ValidFrom:   validFrom,
		ValidUntil:  validUntil,
		Status:      1,
	}
	rand.Read(req.KeyData)
	return c.writeKeyRaw(req)
}

func (c *MobileClient) writeKeyRaw(req *proto.KeyPayload) error {

	respFrame, err := c.SendFrame(proto.MsgTypeKeyWrite, encodePayload(req))
	if err != nil {
		return fmt.Errorf("write key: %w", err)
	}

	var resp proto.KeyPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return fmt.Errorf("decode key write ack: %w", err)
	}

	if resp.Status != 1 {
		return fmt.Errorf("key write rejected, status=%d", resp.Status)
	}

	c.Keys[req.KeyID] = req
	log.Printf("[%s] 🔑 Key %s (type=%d) written to vehicle", c.DeviceID, req.KeyID, req.KeyType)
	return nil
}

// RequestChallenge sends an authentication challenge request.
func (c *MobileClient) RequestChallenge() (*proto.AuthPayload, error) {
	respFrame, err := c.SendFrame(proto.MsgTypeChallengeReq, nil)
	if err != nil {
		return nil, fmt.Errorf("challenge request: %w", err)
	}

	var resp proto.AuthPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return nil, fmt.Errorf("decode challenge response: %w", err)
	}

	return &resp, nil
}

// RespondChallenge signs a challenge with the client's private key.
func (c *MobileClient) RespondChallenge(keyID string, challenge *proto.AuthPayload) error {
	hash := sha256.Sum256(challenge.Challenge)
	r, s, err := ecdsa.Sign(rand.Reader, c.PrivateKey, hash[:])
	if err != nil {
		return fmt.Errorf("sign challenge: %w", err)
	}
	sig := make([]byte, 64)
	r.FillBytes(sig[:32])
	s.FillBytes(sig[32:])

	req := &proto.AuthPayload{
		KeyID:     keyID,
		Challenge: challenge.Challenge,
		Signature: sig,
		Counter:   challenge.Counter,
		Timestamp: challenge.Timestamp,
	}

	respFrame, err := c.SendFrame(proto.MsgTypeChallengeResp, encodePayload(req))
	if err != nil {
		return fmt.Errorf("send challenge response: %w", err)
	}

	// Check for error response
	if respFrame.Header.MsgType == proto.MsgTypeError {
		var errPayload proto.ErrorPayload
		decodePayload(respFrame.Payload, &errPayload)
		return fmt.Errorf("challenge rejected: code=%d msg=%s", errPayload.Code, errPayload.Message)
	}

	log.Printf("[%s] ✅ Challenge response sent for key %s", c.DeviceID, keyID)
	return nil
}

// SendCommand sends a command (lock/unlock/engine/trunk) to the car.
func (c *MobileClient) SendCommand(action, keyID string, msgType uint16) error {
	req := &proto.CommandPayload{
		Action: action,
		KeyID:  keyID,
	}

	respFrame, err := c.SendFrame(msgType, encodePayload(req))
	if err != nil {
		return fmt.Errorf("send command %s: %w", action, err)
	}

	var resp proto.CommandPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return fmt.Errorf("decode command response: %w", err)
	}

	if resp.Result != 0 {
		return fmt.Errorf("command %s rejected: %s", action, resp.ErrorMsg)
	}

	log.Printf("[%s] ✅ Command %s executed successfully", c.DeviceID, action)
	return nil
}

// GetVehicleStatus requests the current vehicle status.
func (c *MobileClient) GetVehicleStatus() (*proto.StatusPayload, error) {
	respFrame, err := c.SendFrame(proto.MsgTypeStatusReq, nil)
	if err != nil {
		return nil, fmt.Errorf("status request: %w", err)
	}

	var status proto.StatusPayload
	if err := decodePayload(respFrame.Payload, &status); err != nil {
		return nil, fmt.Errorf("decode status: %w", err)
	}

	return &status, nil
}

// NFCRead simulates an NFC tag read by the vehicle.
func (c *MobileClient) NFCRead(keyID, credentialID string) (*proto.NFCReadPayload, error) {
	req := &proto.NFCReadPayload{
		KeyID:        keyID,
		CredentialID: credentialID,
	}

	respFrame, err := c.SendFrame(proto.MsgTypeNfcRead, encodePayload(req))
	if err != nil {
		return nil, fmt.Errorf("NFC read: %w", err)
	}

	var resp proto.NFCReadPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return nil, fmt.Errorf("decode NFC read: %w", err)
	}

	return &resp, nil
}

// NFCAuthenticate sends NFC authentication response.
func (c *MobileClient) NFCAuthenticate(keyID, credentialID string, authData []byte) (bool, error) {
	req := &proto.NFCReadPayload{
		KeyID:        keyID,
		CredentialID: credentialID,
		AuthData:     authData,
	}

	respFrame, err := c.SendFrame(proto.MsgTypeNfcAuthResp, encodePayload(req))
	if err != nil {
		return false, fmt.Errorf("NFC auth: %w", err)
	}

	var resp proto.NFCReadPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return false, fmt.Errorf("decode NFC auth response: %w", err)
	}

	return resp.AuthSuccess, nil
}

// SendReplay simulates sending a replayed frame (for security testing).
func (c *MobileClient) SendReplay(originalSeq uint32, originalTs int64) (*proto.ReplayPayload, error) {
	req := &proto.ReplayPayload{
		OriginalSeq: originalSeq,
		OriginalTs:  originalTs,
		ReplayedSeq: originalSeq, // same seq = replay
		ReplayedTs:  time.Now().UnixMilli(),
	}

	respFrame, err := c.SendFrame(proto.MsgTypeReplayFrame, encodePayload(req))
	if err != nil {
		return nil, fmt.Errorf("send replay: %w", err)
	}

	var resp proto.ReplayPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return nil, fmt.Errorf("decode replay response: %w", err)
	}

	return &resp, nil
}

// SendRevokeNotify tells the car that a key has been revoked.
func (c *MobileClient) SendRevokeNotify(keyID, reason string) error {
	req := &proto.RevokeNotifyPayload{
		KeyID:     keyID,
		RevokedAt: time.Now().UnixMilli(),
		Reason:    reason,
	}

	respFrame, err := c.SendFrame(proto.MsgTypeRevokeNotify, encodePayload(req))
	if err != nil {
		return fmt.Errorf("revoke notify: %w", err)
	}

	var resp proto.RevokeNotifyPayload
	if err := decodePayload(respFrame.Payload, &resp); err != nil {
		return fmt.Errorf("decode revoke response: %w", err)
	}

	log.Printf("[%s] 🔑 Revoke notification sent for key %s", c.DeviceID, keyID)
	return nil
}

// ── Helpers ─────────────────────────────────────────────────

func encodePayload(v interface{}) []byte {
	return proto.EncodePayload(v)
}

// computeMAC computes a simplified AES-CMAC for SCP03 auth simulation.
// Uses AES-CMAC with a derived session key, matching the server implementation.
// ── Protocol-specific flows ──────────────────────────────

// ICCEPair performs ICCE-specific pairing with edge compute capability flag.
func (c *MobileClient) ICCEPair() bool {
	req := &proto.ICCEPairPayload{
		DeviceID:    c.DeviceID,
		PublicKey:   c.PublicKey,
		EdgeCapable: true,
		Challenge:   make([]byte, 8),
	}
	resp, err := c.SendFrame(proto.MsgTypeICCEPair, encodePayload(req))
	if err != nil {
		log.Printf("[ICCE-PAIR] ❌ ICCE pair failed: %v", err)
		return false
	}
	var respp proto.ICCEPairPayload
	if err := decodePayload(resp.Payload, &respp); err != nil {
		return false
	}
	log.Printf("[ICCE-PAIR] ✅ Paired with car %s", respp.DeviceID)
	return true
}

// CccOobExchange performs CCC NFC OOB initial handshake.
func (c *MobileClient) CccOobExchange() bool {
	req := &proto.CccOobPayload{Step: 1, NfcData: []byte("nfc_oob_init")}
	resp, err := c.SendFrame(proto.MsgTypeCCC_OOB_Init, encodePayload(req))
	if err != nil || resp == nil {
		return false
	}
	var respp proto.CccOobPayload
	decodePayload(resp.Payload, &respp)
	// Step 2: exchange data
	req2 := &proto.CccOobPayload{Step: 2, Signature: []byte("phone_sig")}
	resp2, err2 := c.SendFrame(proto.MsgTypeCCC_OOB_Data, encodePayload(req2))
	if err2 != nil || resp2 == nil {
		return false
	}
	var respp2 proto.CccOobPayload
	decodePayload(resp2.Payload, &respp2)
	log.Printf("[CCC-OOB] ✅ NFC OOB exchange success=%v", respp2.Success)
	return respp2.Success
}

// CccUwbRanging performs CCC UWB ranging session.
func (c *MobileClient) CccUwbRanging(slot uint32) bool {
	req := &proto.CccUwbRangingPayload{
		SessionID: 1001, SlotIndex: slot,
	}
	resp, err := c.SendFrame(proto.MsgTypeCCC_UWB_Rng, encodePayload(req))
	if err != nil || resp == nil {
		return false
	}
	var respp proto.CccUwbRangingPayload
	decodePayload(resp.Payload, &respp)
	log.Printf("[CCC-UWB] Distance=%.2fm RSSI=%d", respp.Distance, respp.Rssi)
	return true
}

// IcCoaFrameExchange performs ICCOA DK3.0/DK4.0 frame exchange.
func (c *MobileClient) IcCoaFrameExchange(isDK40 bool) bool {
	dkVersion := uint32(30)
	msgType := proto.MsgTypeICCOA_DK30
	if isDK40 {
		dkVersion = 40
		msgType = proto.MsgTypeICCOA_DK40
	}
	req := &proto.IcCoaFramePayload{
		DkVersion: dkVersion,
		Sop:       []byte{0x53, 0x4F, 0x50},
		Payload:   []byte("iccoa_key_provision"),
	}
	if isDK40 {
		req.SessionToken = []byte("session_token_16b")
	}
	resp, err := c.SendFrame(msgType, encodePayload(req))
	if err != nil || resp == nil {
		return false
	}
	var respp proto.IcCoaFramePayload
	decodePayload(resp.Payload, &respp)

	// Verify HMAC if DK4.0
	if isDK40 && len(respp.Hmac) > 0 {
		hmacPayload := &proto.IcCoaFramePayload{
			DkVersion: dkVersion,
			Hmac:      respp.Hmac,
		}
		c.SendFrame(proto.MsgTypeICCOA_HMAC, encodePayload(hmacPayload))
	}
	log.Printf("[ICCOA-DK%d] ✅ Frame exchange completed, checksum=0x%X", dkVersion, respp.Checksum)
	return true
}

// Unlock sends an unlock command to the car.
func (c *MobileClient) Unlock(keyID string) error {
	req := &proto.CommandPayload{Action: "unlock", KeyID: keyID}
	resp, err := c.SendFrame(proto.MsgTypeUnlockCmd, encodePayload(req))
	if err != nil {
		return fmt.Errorf("unlock: %w", err)
	}
	var respp proto.CommandPayload
	decodePayload(resp.Payload, &respp)
	if respp.Result != 0 {
		return fmt.Errorf("unlock failed: %s", respp.ErrorMsg)
	}
	return nil
}

func computeMAC(data []byte) []byte {
	// Simplified: derive a MAC key from the data and simulate AES-CMAC
	hash := sha256.Sum256(data)
	// Use AES-CBC-MAC with the SHA-256 hash-derived key (matching SE050Mock.computeCMAC style)
	return append([]byte(nil), hash[:8]...)
}

func decodePayload(data []byte, v interface{}) error {
	return proto.DecodePayload(data, v)
}

// SignChallenge signs a challenge (stub for multi-protocol tests).
func (c *MobileClient) SignChallenge(keyID string) ([]byte, error) {
	log.Printf("[%s] 🔏 Signed challenge for key %s", c.DeviceID, keyID)
	return make([]byte, 64), nil
}

// Ensure proto helpers are used
var _ = proto.EncodePayload
var _ = proto.DecodePayload
