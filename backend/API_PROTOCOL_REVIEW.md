# yuleDKCS Backend API Protocol Review

## Overview
This document provides a comprehensive review of the yuleDKCS (Digital Key Connectivity Stack) Backend API protocol definitions.

**Protocol Version**: 1.0.0  
**Base URL**: `/api/v1`  
**Review Date**: 2026-05-09

---

## 1. API Protocol Version

- **Current Version**: 1.0.0 (defined in swagger.yaml)
- **API Base Path**: `/api/v1`
- **Documentation**: OpenAPI 3.0.3 specification available at `/docs/api/swagger.yaml`

---

## 2. API Route Definitions

### 2.1 System/Health Endpoints (Public)
| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Health check (readiness) |
| GET | `/health/live` | Liveness probe (Kubernetes) |
| GET | `/health/ready` | Readiness probe (Kubernetes) |
| GET | `/metrics` | Prometheus metrics endpoint |
| GET | `/api/v1/ping` | Simple connectivity test |

### 2.2 Authentication Endpoints
| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| POST | `/api/v1/auth/register` | User registration | No |
| POST | `/api/v1/auth/login` | User login | No |
| POST | `/api/v1/auth/refresh` | Refresh JWT token | Yes |

### 2.3 User Endpoints
| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| GET | `/api/v1/user/profile` | Get user profile | Yes |

### 2.4 Vehicle Endpoints
| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| POST | `/api/v1/vehicles` | Register new vehicle | Yes |
| GET | `/api/v1/vehicles` | List user vehicles | Yes |
| GET | `/api/v1/vehicles/{id}` | Get vehicle details | Yes |
| GET | `/api/v1/vehicles/{id}/status` | Get vehicle status | Yes |
| POST | `/api/v1/vehicles/{id}/command` | Send control command | Yes |
| PUT | `/api/v1/vehicles/{id}/location` | Update vehicle location | Yes |
| PUT | `/api/v1/vehicles/{id}/status/update` | Vehicle status report | Yes |
| GET | `/api/v1/commands/{command_id}` | Get command result | Yes |

### 2.5 Digital Key Endpoints
| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| POST | `/api/v1/keys/issue` | Issue new key | Yes |
| GET | `/api/v1/keys` | List user keys | Yes |
| GET | `/api/v1/keys/{id}` | Get key details | Yes |
| POST | `/api/v1/keys/{id}/share` | Share key | Yes |
| DELETE | `/api/v1/keys/{id}` | Revoke key | Yes |
| PUT | `/api/v1/keys/{id}/permissions` | Update key permissions | Yes |
| GET | `/api/v1/keys/shared/list` | Get shared keys | Yes |
| GET | `/api/v1/keys/{id}/shares` | Get key shares | Yes |
| DELETE | `/api/v1/keys/shares/{share_id}` | Revoke share | Yes |

### 2.6 OTA (Over-The-Air) Endpoints
| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| POST | `/api/v1/ota/firmwares` | Create firmware | Admin |
| GET | `/api/v1/ota/firmwares` | List firmwares | Yes |
| GET | `/api/v1/ota/firmwares/{id}` | Get firmware details | Yes |
| PUT | `/api/v1/ota/firmwares/{id}` | Update firmware | Admin |
| DELETE | `/api/v1/ota/firmwares/{id}` | Delete firmware | Admin |
| PUT | `/api/v1/ota/firmwares/{id}/deactivate` | Deactivate firmware | Admin |
| POST | `/api/v1/ota/check` | Check for updates | Yes |
| GET | `/api/v1/ota/updates` | Check updates (query) | Yes |
| POST | `/api/v1/ota/firmwares/{id}/download` | Download firmware | Yes |
| GET | `/api/v1/vehicles/{id}/ota/status` | Get vehicle OTA status | Yes |
| PUT | `/api/v1/vehicles/{id}/ota/status` | Update OTA status | Yes |
| POST | `/api/v1/vehicles/{id}/ota/start` | Start OTA update | Yes |
| GET | `/api/v1/vehicles/{id}/ota/history` | Get OTA history | Yes |

### 2.7 WebSocket Endpoints
| Method | Path | Description |
|--------|------|-------------|
| GET | `/ws` | WebSocket connection endpoint |

---

## 3. Request/Response Structure Definitions (DTOs)

### 3.1 Authentication DTOs

#### LoginRequest
```go
type LoginRequest struct {
    Username string `json:"username" binding:"required"`
    Password string `json:"password" binding:"required"`
}
```

#### RegisterRequest
```go
type RegisterRequest struct {
    Username string `json:"username" binding:"required,min=3,max=50"`
    Email    string `json:"email" binding:"required,email"`
    Password string `json:"password" binding:"required,min=6"`
}
```

### 3.2 Vehicle DTOs

#### VehicleRegisterRequest
```go
type VehicleRegisterRequest struct {
    Vin          string `json:"vin" binding:"required,len=17"`
    Brand        string `json:"brand" binding:"required"`
    Model        string `json:"model" binding:"required"`
    Year         int    `json:"year" binding:"required,min=1900,max=2100"`
    Color        string `json:"color"`
    Plate        string `json:"plate"`
    Name         string `json:"name" binding:"required"`
    BleMAC       string `json:"ble_mac"`
    UWBCapable   bool   `json:"uwb_capable"`
}
```

#### VehicleCommandRequest
```go
type VehicleCommandRequest struct {
    Command   VehicleCommand         `json:"command" binding:"required"`
    Params    map[string]interface{} `json:"params,omitempty"`
}
```

**Supported Commands**:
- `unlock` - Unlock vehicle
- `lock` - Lock vehicle
- `engine_start` - Start engine
- `engine_stop` - Stop engine
- `trunk` - Open trunk
- `windows_up` - Roll up windows
- `windows_down` - Roll down windows
- `find_my_car` - Find my car

#### VehicleLocationRequest
```go
type VehicleLocationRequest struct {
    Latitude  float64 `json:"latitude" binding:"required"`
    Longitude float64 `json:"longitude" binding:"required"`
    Altitude  float64 `json:"altitude"`
    Accuracy  float64 `json:"accuracy"`
}
```

#### VehicleResponse
```go
type VehicleResponse struct {
    ID              uint           `json:"id"`
    OwnerID         uint           `json:"owner_id"`
    Name            string         `json:"name"`
    Vin             string         `json:"vin"`
    Plate           string         `json:"plate,omitempty"`
    Brand           string         `json:"brand"`
    Model           string         `json:"model"`
    Year            int            `json:"year"`
    Color           string         `json:"color,omitempty"`
    Status          VehicleStatus  `json:"status"`
    LockStatus      string         `json:"lock_status"`
    EngineStatus    string         `json:"engine_status"`
    OnlineStatus    string         `json:"online_status"`
    BleMAC          string         `json:"ble_mac,omitempty"`
    UWBCapable      bool           `json:"uwb_capable"`
    UWBEnabled      bool           `json:"uwb_enabled"`
    LastSeenAt      *time.Time     `json:"last_seen_at,omitempty"`
    SoftwareVersion string         `json:"software_version,omitempty"`
    OTAAvailable    bool           `json:"ota_available"`
    CreatedAt       time.Time      `json:"created_at"`
    UpdatedAt       time.Time      `json:"updated_at"`
}
```

### 3.3 Digital Key DTOs

#### IssueKeyRequest
```go
type IssueKeyRequest struct {
    VehicleID     uint           `json:"vehicle_id" binding:"required"`
    Type          KeyType        `json:"type" binding:"required,oneof=CCC ICCE ICCOA"`
    Permissions   KeyPermissions `json:"permissions"`
    Name          string         `json:"name"`
    Description   string         `json:"description"`
    ExpiresAt     *time.Time     `json:"expires_at"`
}
```

**Key Types**:
- `CCC` - Car Connectivity Consortium
- `ICCE` - Intelligent Car Connectivity Ecosystem
- `ICCOA` - Interoperable Car Connectivity Open Alliance

**Key Permissions**:
```go
type KeyPermissions struct {
    Unlock      bool `json:"unlock"`
    Lock        bool `json:"lock"`
    StartEngine bool `json:"start_engine"`
    Trunk       bool `json:"trunk"`
    Windows     bool `json:"windows"`
}
```

#### ShareKeyRequest
```go
type ShareKeyRequest struct {
    UserID      uint           `json:"user_id" binding:"required"`
    Permissions KeyPermissions `json:"permissions"`
    ExpiresAt   *time.Time     `json:"expires_at"`
}
```

#### KeyResponse
```go
type KeyResponse struct {
    ID            uint           `json:"id"`
    UserID        uint           `json:"user_id"`
    VehicleID     uint           `json:"vehicle_id"`
    VehicleName   string         `json:"vehicle_name,omitempty"`
    Type          KeyType        `json:"type"`
    Status        KeyStatus      `json:"status"`
    Permissions   KeyPermissions `json:"permissions"`
    KeyIdentifier string         `json:"key_identifier"`
    Name          string         `json:"name"`
    Description   string         `json:"description"`
    ExpiresAt     *time.Time     `json:"expires_at,omitempty"`
    LastUsedAt    *time.Time     `json:"last_used_at,omitempty"`
    UsageCount    int            `json:"usage_count"`
    CreatedAt     time.Time      `json:"created_at"`
    UpdatedAt     time.Time      `json:"updated_at"`
    IsShared      bool           `json:"is_shared"`
    SharedBy      uint           `json:"shared_by,omitempty"`
}
```

**Key Status Values**:
- `active` - Key is active
- `inactive` - Key is inactive
- `revoked` - Key has been revoked
- `expired` - Key has expired
- `pending` - Key is pending activation

### 3.4 OTA DTOs

#### CheckUpdateRequest
```go
type CheckUpdateRequest struct {
    CurrentVersion     string `json:"current_version" binding:"required"`
    HardwareVersion    string `json:"hardware_version" binding:"required"`
    FirmwareType       string `json:"firmware_type" binding:"required"`
    VehicleID          uint   `json:"vehicle_id"`
}
```

**Firmware Types**:
- `ecu` - ECU firmware
- `ivi` - In-Vehicle Infotainment
- `tbox` - T-Box firmware
- `adas` - ADAS firmware
- `gateway` - Gateway firmware
- `bms` - Battery Management System

#### UpdateOTAStatusRequest
```go
type UpdateOTAStatusRequest struct {
    Status       OTAStatus `json:"status" binding:"required,oneof=pending downloading installing completed failed rolled_back"`
    Progress     int       `json:"progress" binding:"min=0,max=100"`
    ErrorMessage string    `json:"error_message"`
}
```

**OTA Status Values**:
- `pending` - Update pending
- `downloading` - Downloading firmware
- `installing` - Installing update
- `completed` - Update completed
- `failed` - Update failed
- `rolled_back` - Update rolled back

---

## 4. WebSocket Protocol Definition

### 4.1 WebSocket Connection Types
- **User Connection**: `/ws?vehicle_id={id}` - For receiving real-time vehicle updates
- **Vehicle Connection**: `/ws/vehicles/{id}` - For vehicle to report status
- **Admin Connection**: `/ws/admin` - For system monitoring

### 4.2 WebSocket Message Format
```go
type Message struct {
    Type      string      `json:"type"`
    Timestamp time.Time   `json:"timestamp"`
    Payload   interface{} `json:"payload"`
}
```

### 4.3 WebSocket Message Types

#### Client to Server (Inbound)
| Type | Description |
|------|-------------|
| `vehicle_status` | Vehicle status update |
| `command_result` | Command execution result |
| `ping` | Keep-alive ping |

#### Server to Client (Outbound)
| Type | Description |
|------|-------------|
| `command` | Send command to vehicle |
| `vehicle_status_update` | Broadcast vehicle status |
| `command_result` | Command execution result |
| `pong` | Keep-alive pong response |

### 4.4 WebSocket Configuration
- **Write Wait**: 10 seconds
- **Pong Wait**: 60 seconds
- **Ping Period**: 54 seconds (9/10 of pong wait)
- **Max Message Size**: 512KB

---

## 5. Authentication & Security

### 5.1 Authentication Method
- **Type**: JWT Bearer Token
- **Header Format**: `Authorization: Bearer <token>`
- **Token Expiry**: 24 hours (configurable)

### 5.2 Security Headers Applied
- `X-Content-Type-Options: nosniff`
- `X-Frame-Options: DENY`
- `X-XSS-Protection: 1; mode=block`
- `Strict-Transport-Security: max-age=31536000`
- `Content-Security-Policy` (defined)
- `Referrer-Policy: strict-origin-when-cross-origin`
- `Permissions-Policy` (restrictive)

### 5.3 CORS Configuration
- Allowed Origins: `*` (all)
- Allowed Methods: `GET, POST, PUT, DELETE, OPTIONS`
- Allowed Headers: `Content-Type, Authorization`

---

## 6. Response Format Standards

### 6.1 Success Response (Key Handler Pattern)
```json
{
  "code": 200,
  "message": "操作成功",
  "data": { ... }
}
```

### 6.2 Error Response Format
```json
{
  "code": 400,
  "message": "错误描述",
  "error": "详细错误信息（可选）"
}
```

### 6.3 Pagination Response
```json
{
  "code": 200,
  "message": "获取成功",
  "data": {
    "list": [ ... ],
    "total": 100,
    "page": 1,
    "page_size": 10
  }
}
```

---

## 7. HTTP Status Codes

| Code | Usage |
|------|-------|
| 200 | Success (GET, PUT, DELETE) |
| 201 | Created (POST) |
| 202 | Accepted (Async command) |
| 400 | Bad Request (Validation error) |
| 401 | Unauthorized (Missing/Invalid auth) |
| 403 | Forbidden (No permission) |
| 404 | Not Found |
| 409 | Conflict (Duplicate resource) |
| 500 | Internal Server Error |
| 501 | Not Implemented |
| 503 | Service Unavailable (Readiness check) |

---

## 8. API Implementation Status

### 8.1 Implemented Endpoints
- Health check endpoints (`/health`, `/health/live`, `/health/ready`, `/metrics`)
- Ping endpoint (`/api/v1/ping`)

### 8.2 Partially Implemented
- Authentication handlers (placeholders in router)
- Key handlers (full implementation with service layer)
- OTA handlers (full implementation with service layer)
- Vehicle handlers (implementation in place)

### 8.3 TODO Items
- JWT middleware integration (commented out in router)
- WebSocket handler integration
- Full auth service implementation

---

## 9. Issues & Observations

### 9.1 Critical Issues
1. **JWT Middleware Disabled**: In `router.go`, JWT auth is commented out:
   ```go
   // TODO: 添加 JWT 中间件
   // authorized.Use(middleware.JWTAuth(cfg.JWT.Secret))
   ```

2. **Inconsistent Import Paths**: Some files use different import paths:
   - `yuleDKCS/backend/...` (some handlers)
   - `github.com/frisky1985/yuleDKCS/backend/...` (others)

### 9.2 Recommendations
1. **Enable JWT Authentication**: Uncomment and test JWT middleware
2. **Standardize Import Paths**: Use consistent module path across all files
3. **Complete WebSocket Integration**: Link WebSocket handlers to router
4. **Add API Version Header**: Consider adding `X-API-Version` header
5. **Rate Limiting**: Add rate limiting middleware for public endpoints

---

## 10. File Locations

### Key API Files
- `/home/admin/yuleDKCS/backend/internal/router/router.go` - Route definitions
- `/home/admin/yuleDKCS/backend/internal/handlers/*.go` - HTTP handlers
- `/home/admin/yuleDKCS/backend/internal/models/*.go` - Data models/DTOs
- `/home/admin/yuleDKCS/backend/internal/websocket/*.go` - WebSocket implementation
- `/home/admin/yuleDKCS/backend/internal/middleware/*.go` - Middleware
- `/home/admin/yuleDKCS/docs/api/swagger.yaml` - OpenAPI specification

---

## 11. Summary

The yuleDKCS Backend API provides a comprehensive set of endpoints for:
- **User Management**: Registration, login, profile
- **Vehicle Management**: Registration, status, commands, location
- **Digital Key Operations**: Issue, share, revoke, permissions
- **OTA Updates**: Firmware management, update checks, status tracking
- **Real-time Communication**: WebSocket for live updates

**Protocol Version**: 1.0.0  
**Total Endpoints**: 40+  
**Authentication**: JWT Bearer Token  
**Documentation**: OpenAPI 3.0.3 (Swagger)
