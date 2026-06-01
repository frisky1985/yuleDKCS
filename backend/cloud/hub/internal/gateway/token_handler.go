package gateway

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
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
	// TODO: map string → Permission
	return nil
}

func formatPermissions(perms []token.Permission) []string {
	// TODO: map Permission → string
	return nil
}
