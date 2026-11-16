package gateway

import (
	"encoding/base64"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"

	"github.com/frisky1985/yuleDKCS/backend/cloud/hub/internal/revocation"
)

// WithRevocation attaches the signed CRL service. When unset, the CRL endpoint
// returns 503 instead of serving an unsigned/empty list.
func (g *RESTGateway) WithRevocation(svc *revocation.Service) *RESTGateway {
	g.revocation = svc
	return g
}

// getRevocationList serves the signed v2 CRL consumed by vehicle firmware.
// The content is public but signed and only contains hashed tokens.
func (g *RESTGateway) getRevocationList(c *gin.Context) {
	if g.revocation == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "revocation service not configured"})
		return
	}

	blob, epoch, err := g.revocation.Current(c.Request.Context())
	if err != nil {
		g.logger.Error("build signed CRL failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "CRL unavailable"})
		return
	}

	c.Header("Cache-Control", "no-store")
	c.Header("X-CRL-Epoch", strconv.FormatUint(uint64(epoch), 10))
	c.Data(http.StatusOK, "application/octet-stream", blob)
}

type revokeEntryRequest struct {
	Type string `json:"type"` // device_id | cert_serial | pubkey
	Data string `json:"data"` // base64 raw identifier
}

// addRevocationEntry revokes one identifier. Admin role required: a user token
// must not be able to revoke other users' keys.
func (g *RESTGateway) addRevocationEntry(c *gin.Context) {
	if g.revocation == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{"error": "revocation service not configured"})
		return
	}

	_, role, err := g.extractAuth(c)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": errAuthFailed.Error()})
		return
	}
	if role != "admin" {
		c.JSON(http.StatusForbidden, gin.H{"error": "admin role required"})
		return
	}

	var req revokeEntryRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid JSON body"})
		return
	}

	typ, err := revocation.ParseEntryType(req.Type)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	data, err := base64.StdEncoding.DecodeString(req.Data)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "data must be base64"})
		return
	}
	if _, err := revocation.TokenFor(typ, data); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	epoch, err := g.revocation.Revoke(c.Request.Context(), typ, data)
	if err != nil {
		g.logger.Error("persist revocation entry failed", zap.Error(err))
		c.JSON(http.StatusInternalServerError, gin.H{"error": "revocation failed"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status": "revoked",
		"type":   typ.String(),
		"epoch":  epoch,
	})
}
