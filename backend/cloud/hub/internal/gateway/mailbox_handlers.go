package gateway

import (
	"encoding/json"
	"io"
	"net/http"

	"github.com/gin-gonic/gin"
	pb_relay "github.com/frisky1985/yuleDKCS/backend/cloud/hub/api/relay/v1"
)

// ── gRPC Client Helper ──

func (g *RESTGateway) relayClient() pb_relay.RelayServiceClient {
	return pb_relay.NewRelayServiceClient(g.grpcConn)
}

// ── Mailbox REST Handlers ──
//
// Mailbox API 是公开的（无需 JWT auth）。
// 安全依赖:
//   1. mailbox_id 是随机 UUID（不可猜测）
//   2. Payload 端到端加密（密钥来自 URL fragment）
//   3. Fragment 永不发送到服务器

// createMailbox POST /api/v1/mailbox — 发送方创建分享邮箱
func (g *RESTGateway) createMailbox(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	var req struct {
		Payload            []byte `json:"payload"`
		DisplayInfo        []byte `json:"displayInfo"`
		NotificationToken  string `json:"notificationToken"`
		SenderDeviceID     string `json:"senderDeviceId"`
		SenderVendor       string `json:"senderVendor"`
		ExpirationSeconds  int64  `json:"expirationSeconds"`
		MaxUpdates         int32  `json:"maxUpdates"`
		DeviceAttestation  []byte `json:"deviceAttestation"`
		TraceID            string `json:"traceId"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "BAD_REQUEST",
			"message": "invalid request body",
			"detail":  err.Error(),
		})
		return
	}

	grpcReq := &pb_relay.CreateMailboxRequest{
		Payload:           req.Payload,
		DisplayInfo:       req.DisplayInfo,
		NotificationToken: req.NotificationToken,
		SenderDeviceId:    req.SenderDeviceID,
		SenderVendor:      req.SenderVendor,
		DeviceAttestation: req.DeviceAttestation,
		TraceId:           req.TraceID,
	}
	if req.ExpirationSeconds > 0 || req.MaxUpdates > 0 {
		grpcReq.Config = &pb_relay.MailboxConfig{
			AccessRights:      pb_relay.AccessRights(4), // RWD
			ExpirationSeconds: req.ExpirationSeconds,
			MaxUpdates:        req.MaxUpdates,
		}
	}

	resp, err := g.relayClient().CreateMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "createMailbox", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"mailboxId":  resp.MailboxId,
		"sharingUrl": resp.SharingUrl,
		"expiresAt":  resp.ExpiresAt,
	})
}

// readMailboxDisplay GET /api/v1/mailbox/:id/display — 读取展示信息
func (g *RESTGateway) readMailboxDisplay(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	mailboxID := c.Param("id")
	grpcReq := &pb_relay.ReadDisplayInformationFromMailboxRequest{
		MailboxId: mailboxID,
		TraceId:   c.GetHeader("X-Trace-Id"),
	}

	resp, err := g.relayClient().ReadDisplayInformationFromMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "readMailboxDisplay", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"displayInfo": resp.DisplayInfo,
		"version":     resp.Version,
	})
}

// readMailboxContent GET /api/v1/mailbox/:id/content — 读取加密内容
func (g *RESTGateway) readMailboxContent(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	mailboxID := c.Param("id")
	grpcReq := &pb_relay.ReadSecureContentFromMailboxRequest{
		MailboxId: mailboxID,
		TraceId:   c.GetHeader("X-Trace-Id"),
	}

	resp, err := g.relayClient().ReadSecureContentFromMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "readMailboxContent", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"payload":   resp.Payload,
		"version":   resp.Version,
		"errorCode": resp.ErrorCode,
		"errorMsg":  resp.ErrorMsg,
	})
}

// updateMailbox PUT /api/v1/mailbox/:id — 更新邮箱（KeySigning/Import/Cancel）
func (g *RESTGateway) updateMailbox(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	mailboxID := c.Param("id")

	body, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "BAD_REQUEST",
			"message": "cannot read body",
		})
		return
	}

	var req struct {
		Payload           []byte `json:"payload"`
		SharingDataType   int32  `json:"sharingDataType"`
		NotificationToken string `json:"notificationToken"`
		UpdaterDeviceID   string `json:"updaterDeviceId"`
		TraceID           string `json:"traceId"`
	}
	if err := json.Unmarshal(body, &req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "BAD_REQUEST",
			"message": "invalid JSON",
		})
		return
	}

	grpcReq := &pb_relay.UpdateMailboxRequest{
		MailboxId:         mailboxID,
		Payload:           req.Payload,
		SharingDataType:   req.SharingDataType,
		NotificationToken: req.NotificationToken,
		UpdaterDeviceId:   req.UpdaterDeviceID,
		TraceId:           req.TraceID,
	}

	resp, err := g.relayClient().UpdateMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "updateMailbox", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":    resp.Status.String(),
		"version":   resp.Version,
		"errorCode": resp.ErrorCode,
		"errorMsg":  resp.ErrorMsg,
	})
}

// deleteMailbox DELETE /api/v1/mailbox/:id — 删除邮箱
func (g *RESTGateway) deleteMailbox(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	mailboxID := c.Param("id")

	var req struct {
		Reason          string `json:"reason"`
		DeleterDeviceID string `json:"deleterDeviceId"`
		TraceID         string `json:"traceId"`
	}
	_ = c.ShouldBindJSON(&req)

	grpcReq := &pb_relay.DeleteMailboxRequest{
		MailboxId:       mailboxID,
		Reason:          req.Reason,
		DeleterDeviceId: req.DeleterDeviceID,
		TraceId:         req.TraceID,
	}

	resp, err := g.relayClient().DeleteMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "deleteMailbox", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success":   resp.Success,
		"errorCode": resp.ErrorCode,
	})
}

// relinquishMailbox POST /api/v1/mailbox/:id/relinquish — 转移邮箱
func (g *RESTGateway) relinquishMailbox(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	mailboxID := c.Param("id")

	var req struct {
		FromDeviceID string `json:"fromDeviceId"`
		ToDeviceID   string `json:"toDeviceId"`
		TraceID      string `json:"traceId"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "BAD_REQUEST",
			"message": "invalid request body",
		})
		return
	}

	grpcReq := &pb_relay.RelinquishMailboxRequest{
		MailboxId:     mailboxID,
		FromDeviceId:  req.FromDeviceID,
		ToDeviceId:    req.ToDeviceID,
		TraceId:       req.TraceID,
	}

	resp, err := g.relayClient().RelinquishMailbox(c.Request.Context(), grpcReq)
	if err != nil {
		g.handleGRPCError(c, "relinquishMailbox", err)
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success":   resp.Success,
		"errorCode": resp.ErrorCode,
		"errorMsg":  resp.ErrorMsg,
	})
}
