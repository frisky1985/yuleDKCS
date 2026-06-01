package gateway

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/service"
	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/token"
)


// ── Token 相关路由 ──

// issueToken POST /api/v1/tokens — 车主签发 Token
func (g *RESTGateway) issueToken(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED"})
		return
	}

	var req struct {
		SubjectID  string   `json:"subject_id" binding:"required"`
		VehicleID  string   `json:"vehicle_id" binding:"required"`
		Permissions []string `json:"permissions"` // "lock","engine","trunk",...
		Duration   string   `json:"duration"`     // "2h", "30m", "7d"...
		MaxUses    int32    `json:"max_uses"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "BAD_REQUEST", "message": "invalid request"})
		return
	}

	perms := parsePermissions(req.Permissions)
	dur, _ := time.ParseDuration(req.Duration)
	if dur == 0 {
		dur = 24 * time.Hour // 默认24h
	}

	tok, err := g.tokenSvc.Issue(userID, req.SubjectID, req.VehicleID, perms, dur, req.MaxUses)
	if err != nil {
		g.logger.Error("issueToken failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "ISSUE_FAILED"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token_id":    tok.ID,
		"expires_at":  tok.ExpiresAt,
		"signature":   tok.Signature,
	})
}

// verifyToken GET /api/v1/tokens/:tokenId — 验证 Token
func (g *RESTGateway) verifyToken(c *gin.Context) {
	tokenID := c.Param("tokenId")

	tok, err := g.tokenSvc.Verify(tokenID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{
			"valid":   false,
			"message": err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"valid":       true,
		"owner_id":    tok.OwnerID,
		"subject_id":  tok.SubjectID,
		"vehicle_id":  tok.VehicleID,
		"permissions": formatPermissions(tok.Perms),
		"expires_at":  tok.ExpiresAt,
		"use_count":   tok.UseCount,
	})
}

// revokeToken DELETE /api/v1/tokens/:tokenId — 吊销 Token
func (g *RESTGateway) revokeToken(c *gin.Context) {
	userID, _, err := g.extractAuth(c)
	if err != nil {
		c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "AUTH_FAILED"})
		return
	}

	tokenID := c.Param("tokenId")
	if err := g.tokenSvc.Revoke(tokenID, userID); err != nil {
		c.JSON(http.StatusForbidden, gin.H{"error": "REVOKE_FAILED", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"token_id": tokenID, "status": "revoked"})
}

// ── 辅助 ──

func parsePermissions(strs []string) []token.Permission {
	m := map[string]token.Permission{
		"lock":    token.PermLock,
		"engine":  token.PermEngineStart,
		"trunk":   token.PermTrunk,
		"window":  token.PermWindow,
		"climate": token.PermClimate,
		"seat":    token.PermSeat,
		"fuel":    token.PermFuel,
		"share":   token.PermShare,
	}
	var perms []token.Permission
	for _, s := range strs {
		if p, ok := m[s]; ok {
			perms = append(perms, p)
		}
	}
	if len(perms) == 0 {
		return []token.Permission{token.PermLock} // 默认只有解锁
	}
	return perms
}

func formatPermissions(perms []token.Permission) []string {
	r := make([]string, 0, len(perms))
	for _, p := range perms {
		switch p {
		case token.PermLock:
			r = append(r, "lock")
		case token.PermEngineStart:
			r = append(r, "engine")
		case token.PermTrunk:
			r = append(r, "trunk")
		case token.PermWindow:
			r = append(r, "window")
		case token.PermClimate:
			r = append(r, "climate")
		case token.PermSeat:
			r = append(r, "seat")
		case token.PermFuel:
			r = append(r, "fuel")
		case token.PermShare:
			r = append(r, "share")
		}
	}
	return r
}

// exchangeToken POST /api/v1/tokens/:tokenId/exchange
// 用 Token 换一把离线可用的真钥匙
// Hub 验证 Token → 通知 DK Server → DK Server 签发钥匙 → 返回
func (g *RESTGateway) exchangeToken(c *gin.Context) {
	tokenID := c.Param("tokenId")

	// 1. 验证 Token
	tok, err := g.tokenSvc.Verify(tokenID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{
			"exchanged": false,
			"error":     err.Error(),
		})
		return
	}

	// 2. 检查是否已换过钥匙
	if tok.UseCount > 1 {
		c.JSON(http.StatusConflict, gin.H{
			"exchanged": false,
			"error":     "key already issued for this token",
		})
		return
	}

	// 3. 通知 DK Server 签发钥匙
	ctx := c.Request.Context()
	keyReq := &service.KeyRequest{
		TokenID:     tokenID,
		SubjectID:   tok.SubjectID,
		VehicleID:   tok.VehicleID,
		Permissions: tok.Perms,
		ExpiresAt:   tok.ExpiresAt,
	}

	resp, err := g.dkServer.IssueKey(ctx, keyReq)
	if err != nil {
		g.logger.Error("exchangeToken: DK Server rejected", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{
			"exchanged": false,
			"error":     "key issuance rejected: " + err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"exchanged":  true,
		"token_id":   tokenID,
		"key_id":     resp.KeyID,
		"subject":    tok.SubjectID,
		"vehicle":    tok.VehicleID,
		"note":       "钥匙已签发，请等待设备端同步",
	})
}
