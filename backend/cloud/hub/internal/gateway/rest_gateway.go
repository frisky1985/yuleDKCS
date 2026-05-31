package gateway

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"

	pb "github.com/digitalkey/hub/api/v1"
)

// RESTGateway REST API → gRPC 转换网关
// 手机App通过HTTPS访问，网关转为gRPC调用HUB内部服务
type RESTGateway struct {
	grpcSrv   *grpc.Server
	logger    *zap.Logger
	httpSrv   *http.Server
	jwtSecret string
	grpcConn  *grpc.ClientConn
}

func NewRESTGateway(grpcSrv *grpc.Server, logger *zap.Logger) *RESTGateway {
	return &RESTGateway{
		grpcSrv: grpcSrv,
		logger:  logger,
	}
}

// WithJWTSecret sets the JWT secret for auth middleware
func (g *RESTGateway) WithJWTSecret(secret string) *RESTGateway {
	g.jwtSecret = secret
	return g
}

// WithGRPCConn sets a gRPC client connection for forwarding to HUB services
func (g *RESTGateway) WithGRPCConn(conn *grpc.ClientConn) *RESTGateway {
	g.grpcConn = conn
	return g
}

func (g *RESTGateway) Serve(addr string) error {
	r := gin.New()
	r.Use(gin.Recovery())
	r.Use(g.loggerMiddleware())

	// ── 健康检查 (公开) ──
	r.GET("/health", func(c *gin.Context) {
		c.JSON(200, gin.H{"status": "ok"})
	})

	r.POST("/api/v1/auth/login", g.login) // 登录获取token

	// ── API v1 (需要认证) ──
	v1 := r.Group("/api/v1")
	v1.Use(g.authMiddleware())
	{
		// 密钥管理
		keys := v1.Group("/keys")
		{
			keys.POST("", g.bindKey)            // 绑定密钥
			keys.DELETE("/:keyId", g.unbindKey) // 解绑密钥
			keys.PUT("/:keyId/suspend", g.suspendKey)
			keys.PUT("/:keyId/resume", g.resumeKey)
			keys.PUT("/:keyId/revoke", g.revokeKey)
			keys.PUT("/:keyId/renew", g.renewKey)
			keys.GET("/:keyId", g.getKey)
			keys.GET("", g.listKeys)
		}

		// 密钥分享
		shares := v1.Group("/shares")
		{
			shares.POST("", g.createShare)
			shares.POST("/accept", g.acceptShare)
			shares.DELETE("/:shareId", g.cancelShare)
			shares.GET("/:shareId", g.getShare)
		}

		// 车辆控制
		vehicles := v1.Group("/vehicles")
		{
			vehicles.POST("/:vehicleId/command", g.sendCommand)
			vehicles.GET("/:vehicleId/status", g.streamStatus)
		}

		// HUB管理
		hub := v1.Group("/hub")
		{
			hub.GET("/adapters", g.listAdapters)
			hub.GET("/health", g.hubHealthCheck)
		}
	}

	g.httpSrv = &http.Server{
		Addr:         addr,
		Handler:      r,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 30 * time.Second,
	}

	return g.httpSrv.ListenAndServe()
}

func (g *RESTGateway) Shutdown(ctx context.Context) error {
	if g.httpSrv != nil {
		return g.httpSrv.Shutdown(ctx)
	}
	return nil
}

func (g *RESTGateway) loggerMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		c.Next()
		g.logger.Info("HTTP",
			zap.String("method", c.Request.Method),
			zap.String("path", c.Request.URL.Path),
			zap.Int("status", c.Writer.Status()),
			zap.Duration("latency", time.Since(start)),
			zap.String("client_ip", c.ClientIP()),
		)
	}
}

// authMiddleware validates JWT Bearer token from Authorization header
func (g *RESTGateway) authMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "missing authorization header"})
			return
		}

		token := strings.TrimPrefix(authHeader, "Bearer ")
		if token == "" || token == authHeader {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid token format"})
			return
		}

		// Simple JWT-like validation (placeholder for full JWT lib)
		// In production, replace with github.com/golang-jwt/jwt/v5 validation
		userID, role, err := g.validateToken(token)
		if err != nil {
			g.logger.Warn("Auth failed", zap.String("token_prefix", token[:min(8, len(token))]), zap.Error(err))
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid or expired token"})
			return
		}

		// Inject user info into gin context
		c.Set("user_id", userID)
		c.Set("user_role", role)

		// Also inject into gRPC metadata for downstream calls
		md := metadata.Pairs(
			"user_id", userID,
			"user_role", role,
		)
		c.Request = c.Request.WithContext(metadata.NewOutgoingContext(c.Request.Context(), md))

		c.Next()
	}
}

// validateToken validates a bearer token and returns (userID, role, error)
// Placeholder: in production, decode and verify JWT with jwtSecret
func (g *RESTGateway) validateToken(token string) (string, string, error) {
	// TODO: Replace with proper JWT validation
	if token == "" {
		return "", "", status.Error(codes.Unauthenticated, "empty token")
	}
	// For now, use token as user_id placeholder
	// TODO: JWT claims extraction
	return "user:" + token[:min(16, len(token))], "user", nil
}

// login provides a simple token endpoint (placeholder)
func (g *RESTGateway) login(c *gin.Context) {
	var req struct {
		UserID   string `json:"user_id"`
		Password string `json:"password"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}

	// TODO: Validate credentials against user service
	// TODO: Generate proper JWT with expiry
	token := "placeholder-token-" + req.UserID + "-" + time.Now().Format(time.RFC3339)
	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"token_type": "Bearer",
		"expires_in": 3600,
	})
}

// ── gRPC Client Helpers ──

func (g *RESTGateway) keyManagementClient() pb.KeyManagementServiceClient {
	return pb.NewKeyManagementServiceClient(g.grpcConn)
}

func (g *RESTGateway) keyShareClient() pb.KeyShareServiceClient {
	return pb.NewKeyShareServiceClient(g.grpcConn)
}

func (g *RESTGateway) vehicleControlClient() pb.VehicleControlServiceClient {
	return pb.NewVehicleControlServiceClient(g.grpcConn)
}

func (g *RESTGateway) hubTransportClient() pb.HubTransportServiceClient {
	return pb.NewHubTransportServiceClient(g.grpcConn)
}

// ── Helper Methods ──

// checkGRPCConn checks whether the gRPC client connection is available.
// Returns true if conn is usable, false and sends 503 if nil.
func (g *RESTGateway) checkGRPCConn(c *gin.Context, conn *grpc.ClientConn) bool {
	if conn == nil {
		g.logger.Warn("gRPC connection not available")
		c.AbortWithStatusJSON(http.StatusServiceUnavailable, gin.H{
			"error": "hub service temporarily unavailable",
		})
		return false
	}
	return true
}

// handleGRPCError maps gRPC status codes to HTTP status codes and responds
// with a JSON error body.
func (g *RESTGateway) handleGRPCError(c *gin.Context, err error) {
	st, _ := status.FromError(err)
	httpStatus := http.StatusBadGateway

	switch st.Code() {
	case codes.NotFound:
		httpStatus = http.StatusNotFound
	case codes.PermissionDenied:
		httpStatus = http.StatusForbidden
	case codes.Unauthenticated:
		httpStatus = http.StatusUnauthorized
	case codes.Internal:
		httpStatus = http.StatusInternalServerError
	case codes.InvalidArgument:
		httpStatus = http.StatusBadRequest
	case codes.Unavailable:
		httpStatus = http.StatusServiceUnavailable
	case codes.DeadlineExceeded:
		httpStatus = http.StatusGatewayTimeout
	case codes.ResourceExhausted:
		httpStatus = http.StatusTooManyRequests
	default:
		httpStatus = http.StatusBadGateway
	}

	g.logger.Warn("gRPC forwarding error",
		zap.String("path", c.Request.URL.Path),
		zap.String("grpc_code", st.Code().String()),
		zap.String("grpc_message", st.Message()),
	)

	c.AbortWithStatusJSON(httpStatus, gin.H{
		"error":   st.Message(),
		"code":    st.Code().String(),
		"details": st.Proto().GetDetails(),
	})
}

// replyProto serializes a protobuf message to JSON and sends it as the HTTP response.
func (g *RESTGateway) replyProto(c *gin.Context, resp proto.Message) {
	raw, err := protojson.Marshal(resp)
	if err != nil {
		g.logger.Error("failed to marshal proto response", zap.Error(err))
		c.AbortWithStatusJSON(http.StatusInternalServerError, gin.H{"error": "internal serialization error"})
		return
	}
	c.Data(http.StatusOK, "application/json", raw)
}

// parseBody reads the JSON request body and unmarshals it into the given protobuf message.
func (g *RESTGateway) parseBody(c *gin.Context, req proto.Message) error {
	body, err := io.ReadAll(c.Request.Body)
	if err != nil {
		return fmt.Errorf("failed to read request body: %w", err)
	}
	if err := protojson.Unmarshal(body, req); err != nil {
		return fmt.Errorf("failed to unmarshal request: %w", err)
	}
	return nil
}

// ── 1. Key Management (8 handlers) ──

// bindKey POST /api/v1/keys — 绑定密钥
func (g *RESTGateway) bindKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.BindKeyRequest
	if err := g.parseBody(c, &req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	// Set user_id from auth context if not provided in body
	if req.UserId == "" {
		req.UserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().BindKey(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("bindKey",
		zap.String("user_id", userIDStr),
		zap.String("vehicle_id", req.VehicleId),
		zap.String("key_id", resp.GetKey().GetKeyId()),
	)
	g.replyProto(c, resp)
}

// unbindKey DELETE /api/v1/keys/:keyId — 解绑密钥
func (g *RESTGateway) unbindKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.UnbindKeyRequest{
		KeyId:   keyID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().UnbindKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("unbindKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// suspendKey PUT /api/v1/keys/:keyId/suspend — 挂起密钥
func (g *RESTGateway) suspendKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")
	userID, _ := c.Get("user_id")

	// Allow optional reason from body
	var body struct {
		Reason string `json:"reason"`
	}
	_ = c.ShouldBindJSON(&body)

	req := &pb.SuspendKeyRequest{
		KeyId:   keyID,
		Reason:  body.Reason,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().SuspendKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("suspendKey",
		zap.String("key_id", keyID),
		zap.String("user_id", fmt.Sprint(userID)),
	)
	g.replyProto(c, resp)
}

// resumeKey PUT /api/v1/keys/:keyId/resume — 恢复密钥
func (g *RESTGateway) resumeKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.ResumeKeyRequest{
		KeyId:   keyID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().ResumeKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("resumeKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// revokeKey PUT /api/v1/keys/:keyId/revoke — 撤销密钥
func (g *RESTGateway) revokeKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")
	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var body struct {
		Reason string `json:"reason"`
	}
	_ = c.ShouldBindJSON(&body)

	req := &pb.RevokeKeyRequest{
		KeyId:   keyID,
		Reason:  body.Reason,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().RevokeKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("revokeKey",
		zap.String("key_id", keyID),
		zap.String("user_id", userIDStr),
	)
	g.replyProto(c, resp)
}

// renewKey PUT /api/v1/keys/:keyId/renew — 续期密钥
func (g *RESTGateway) renewKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	var body struct {
		ValidUntil int64 `json:"valid_until"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request body"})
		return
	}

	req := &pb.RenewKeyRequest{
		KeyId:      keyID,
		ValidUntil: body.ValidUntil,
		TraceId:    c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().RenewKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("renewKey",
		zap.String("key_id", keyID),
		zap.Int64("valid_until", body.ValidUntil),
	)
	g.replyProto(c, resp)
}

// getKey GET /api/v1/keys/:keyId — 查询单个密钥
func (g *RESTGateway) getKey(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	keyID := c.Param("keyId")

	req := &pb.GetKeyRequest{
		KeyId: keyID,
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().GetKey(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("getKey", zap.String("key_id", keyID))
	g.replyProto(c, resp)
}

// listKeys GET /api/v1/keys — 查询密钥列表
func (g *RESTGateway) listKeys(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	req := &pb.ListKeysRequest{}

	if uid := c.Query("user_id"); uid != "" {
		req.UserId = uid
	}
	if vid := c.Query("vehicle_id"); vid != "" {
		req.VehicleId = vid
	}
	if limitStr := c.Query("limit"); limitStr != "" {
		if limit, err := strconv.Atoi(limitStr); err == nil && limit > 0 {
			req.PageSize = int32(limit)
		}
	}
	if offsetStr := c.Query("offset"); offsetStr != "" {
		req.PageToken = offsetStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyManagementClient().ListKeys(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("listKeys",
		zap.String("user_id", req.UserId),
		zap.String("vehicle_id", req.VehicleId),
	)
	g.replyProto(c, resp)
}

// ── 2. Key Sharing (4 handlers) ──

// createShare POST /api/v1/shares — 创建密钥分享
func (g *RESTGateway) createShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.CreateShareRequest
	if err := g.parseBody(c, &req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	if req.FromUserId == "" {
		req.FromUserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().CreateShare(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("createShare",
		zap.String("from_user", userIDStr),
		zap.String("share_id", resp.GetShareId()),
	)
	g.replyProto(c, resp)
}

// acceptShare POST /api/v1/shares/accept — 接受密钥分享
func (g *RESTGateway) acceptShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var req pb.AcceptShareRequest
	if err := g.parseBody(c, &req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	if req.UserId == "" {
		req.UserId = userIDStr
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().AcceptShare(ctx, &req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("acceptShare", zap.String("user_id", userIDStr))
	g.replyProto(c, resp)
}

// cancelShare DELETE /api/v1/shares/:shareId — 取消密钥分享
func (g *RESTGateway) cancelShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	shareID := c.Param("shareId")

	req := &pb.CancelShareRequest{
		ShareId: shareID,
		TraceId: c.GetHeader("X-Trace-Id"),
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().CancelShare(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("cancelShare", zap.String("share_id", shareID))
	g.replyProto(c, resp)
}

// getShare GET /api/v1/shares/:shareId — 查询分享信息
func (g *RESTGateway) getShare(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	shareID := c.Param("shareId")

	req := &pb.GetShareRequest{
		ShareId: shareID,
	}

	ctx := c.Request.Context()
	resp, err := g.keyShareClient().GetShare(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("getShare", zap.String("share_id", shareID))
	g.replyProto(c, resp)
}

// ── 3. Vehicle Control (2 handlers) ──

// sendCommand POST /api/v1/vehicles/:vehicleId/command — 发送车辆控制指令
func (g *RESTGateway) sendCommand(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	vehicleID := c.Param("vehicleId")
	userID, _ := c.Get("user_id")
	userIDStr := fmt.Sprint(userID)

	var body struct {
		Action  string          `json:"action"`
		KeyID   string          `json:"key_id"`
		Params  json.RawMessage `json:"params"`
		Source  int32           `json:"source"`
		TraceID string          `json:"trace_id"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request body"})
		return
	}

	req := &pb.ControlCommandRequest{
		VehicleId: vehicleID,
		UserId:    userIDStr,
		KeyId:     body.KeyID,
		Action:    body.Action,
		Params:    []byte(body.Params),
		Source:    body.Source,
		TraceId:   body.TraceID,
	}

	ctx := c.Request.Context()
	resp, err := g.vehicleControlClient().SendCommand(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("sendCommand",
		zap.String("vehicle_id", vehicleID),
		zap.String("action", req.Action),
		zap.String("user_id", userIDStr),
	)
	g.replyProto(c, resp)
}

// streamStatus GET /api/v1/vehicles/:vehicleId/status — SSE 流式车辆状态推送
func (g *RESTGateway) streamStatus(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	vehicleID := c.Param("vehicleId")

	req := &pb.VehicleStatusRequest{
		VehicleId: vehicleID,
	}

	ctx := c.Request.Context()
	stream, err := g.vehicleControlClient().StreamStatus(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	// Only set SSE headers after gRPC stream is established successfully
	c.Header("Content-Type", "text/event-stream")
	c.Header("Cache-Control", "no-cache")
	c.Header("Connection", "keep-alive")

	g.logger.Info("streamStatus started",
		zap.String("vehicle_id", vehicleID),
	)

	c.Stream(func(w io.Writer) bool {
		update, err := stream.Recv()
		if err != nil {
			if err == io.EOF || status.Code(err) == codes.Canceled {
				g.logger.Info("streamStatus closed",
					zap.String("vehicle_id", vehicleID),
				)
			} else {
				g.logger.Warn("streamStatus recv error",
					zap.String("vehicle_id", vehicleID),
					zap.Error(err),
				)
			}
			return false
		}

		data, err := protojson.Marshal(update)
		if err != nil {
			g.logger.Error("streamStatus marshal error", zap.Error(err))
			return false
		}

		_, writeErr := fmt.Fprintf(w, "data: %s\n\n", string(data))
		if writeErr != nil {
			g.logger.Warn("streamStatus write error",
				zap.String("vehicle_id", vehicleID),
				zap.Error(writeErr),
			)
			return false
		}
		return true
	})
}

// ── 4. HUB Management (2 handlers) ──

// listAdapters GET /api/v1/hub/adapters — 列出所有适配器状态
func (g *RESTGateway) listAdapters(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	ctx := c.Request.Context()
	req := &pb.HealthCheckRequest{}
	resp, err := g.hubTransportClient().HealthCheck(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("listAdapters",
		zap.Int("adapter_count", len(resp.GetAdapters())),
	)

	g.replyProto(c, resp)
}

// hubHealthCheck GET /api/v1/hub/health — HUB健康检查
func (g *RESTGateway) hubHealthCheck(c *gin.Context) {
	if !g.checkGRPCConn(c, g.grpcConn) {
		return
	}

	ctx := c.Request.Context()
	req := &pb.HealthCheckRequest{}
	resp, err := g.hubTransportClient().HealthCheck(ctx, req)
	if err != nil {
		g.handleGRPCError(c, err)
		return
	}

	g.logger.Info("hubHealthCheck",
		zap.Bool("healthy", resp.GetHealthy()),
	)

	g.replyProto(c, resp)
}
