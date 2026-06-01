package middleware

import (
	"fmt"

	"github.com/golang-jwt/jwt/v5"
)

// Claims represents the JWT claims structure for authentication.
// Embedding jwt.RegisteredClaims provides standard fields (exp, iat, iss, sub, etc.).
type Claims struct {
	UserID string `json:"user_id"`
	Role   string `json:"role"`
	jwt.RegisteredClaims
}

// validateJWT parses and validates a JWT token string using HMAC-SHA256 with the provided secret.
// It returns the extracted Claims on success, or an error if validation fails.
// This implementation is compatible with tokens issued by the hub REST gateway (V-01).
func validateJWT(tokenString string, jwtSecret string) (*Claims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (interface{}, error) {
		// Only accept HMAC-signed tokens (HS256/HS384/HS512)
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(jwtSecret), nil
	})
	if err != nil {
		return nil, fmt.Errorf("token validation failed: %w", err)
	}

	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid token claims")
	}

	return claims, nil
}
