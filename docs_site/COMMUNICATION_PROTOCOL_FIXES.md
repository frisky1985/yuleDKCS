# yuleDKCS 通信协议修复建议文档

**项目**: yuleDKCS Digital Key Connectivity Stack  
**版本**: 1.0.0  
**生成日期**: 2026-05-09  
**状态**: 基于审查结果，待修复

---

## 1. 执行摘要

### 1.1 审查结果概览

| 审查项 | 状态 | 优先级 |
|--------|------|--------|
| Backend API 审查 | 完成 | - |
| Frontend 通信协议 | 严重问题 | P0 |
| Embedded SDK 协议 | 部分实现 | P1 |
| WebSocket 一致性 | 严重不一致 | P0 |

### 1.2 关键发现

1. **Frontend WebSocket 完全缺失** - 无法接收实时车辆状态更新
2. **API 路径不一致** - 前后端路由定义不匹配
3. **Embedded SDK 加密未实现** - AES-GCM 为 memcpy 占位
4. **Mobile SDK 无 WebSocket** - 车辆状态无法实时上报

### 1.3 修复优先级

| 优先级 | 描述 | 数量 |
|--------|------|------|
| P0 - Critical | 阻断性问题，必须立即修复 | 5 |
| P1 - High | 重要问题，影响功能完整性 | 6 |
| P2 - Medium | 中等问题，影响用户体验 | 4 |
| P3 - Low | 轻微问题，建议优化 | 3 |

---

## 2. 问题清单与修复方案

### 2.1 P0 - Critical 优先级

#### 问题 #1: Frontend WebSocket 客户端完全缺失

**位置**: `/home/admin/yuleDKCS/frontend/src/`  
**影响**: 无法接收车辆实时状态更新，无法接收命令执行结果  
**状态**: 严重缺失

**Backend 已提供**: WebSocket Hub 架构、心跳机制、消息类型定义  
**Frontend 缺失**: WebSocket 客户端代码、消息处理器、重连机制

**修复方案**:

1. 添加 WebSocket 依赖到 package.json
2. 创建 WebSocket 服务类
3. 集成到状态管理

**代码示例**:

```typescript
// frontend/src/services/websocket.ts
export class WebSocketService {
  private ws: WebSocket | null = null;
  private reconnectTimer: NodeJS.Timeout | null = null;
  private messageHandlers: Map<string, ((payload: any) => void)[]> = new Map();
  
  private readonly WS_BASE_URL = import.meta.env.VITE_WS_URL || 'ws://localhost:8080';
  private readonly RECONNECT_INTERVAL = 3000;
  private readonly MAX_RECONNECT_INTERVAL = 60000;
  private currentReconnectInterval = 3000;

  connect(vehicleId?: string) {
    const token = useAuthStore.getState().token;
    const url = vehicleId 
      ? `${this.WS_BASE_URL}/ws?vehicle_id=${vehicleId}`
      : `${this.WS_BASE_URL}/ws`;
    
    this.ws = new WebSocket(url);
    
    this.ws.onopen = () => {
      console.log('WebSocket connected');
      this.currentReconnectInterval = this.RECONNECT_INTERVAL;
      // 发送认证消息
      this.send('auth', { token });
    };
    
    this.ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      this.handleMessage(message);
    };
    
    this.ws.onclose = () => {
      console.log('WebSocket disconnected, reconnecting...');
      this.scheduleReconnect(vehicleId);
    };
    
    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };
  }

  private handleMessage(message: WSMessage) {
    const handlers = this.messageHandlers.get(message.type) || [];
    handlers.forEach(handler => handler(message.payload));
  }

  on(type: string, handler: (payload: any) => void) {
    if (!this.messageHandlers.has(type)) {
      this.messageHandlers.set(type, []);
    }
    this.messageHandlers.get(type)!.push(handler);
  }

  send(type: string, payload: any) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ type, payload, timestamp: new Date().toISOString() }));
    }
  }

  private scheduleReconnect(vehicleId?: string) {
    this.reconnectTimer = setTimeout(() => {
      this.connect(vehicleId);
      this.currentReconnectInterval = Math.min(
        this.currentReconnectInterval * 2,
        this.MAX_RECONNECT_INTERVAL
      );
    }, this.currentReconnectInterval);
  }

  disconnect() {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
    }
    this.ws?.close();
  }
}

export const wsService = new WebSocketService();

// 消息类型常量
export const WSMessageTypes = {
  PING: 'ping',
  PONG: 'pong',
  VEHICLE_STATUS: 'vehicle_status',
  VEHICLE_STATUS_UPDATE: 'vehicle_status_update',
  COMMAND: 'command',
  COMMAND_RESULT: 'command_result',
  NOTIFICATION: 'notification',
} as const;

interface WSMessage {
  type: string;
  timestamp: string;
  payload: any;
}
```

**负责人**: Frontend Team Lead  
**时间表**: 3-5 天  
**依赖**: Backend WebSocket 端点已就绪

---

#### 问题 #2: API 路径不一致

**位置**: Frontend 多个文件  
**影响**: 前后端无法正常通信，API 调用返回 404

**不一致列表**:

| 功能 | Frontend 路径 | Backend 路径 | 状态 |
|------|---------------|--------------|------|
| 登录 | `/api/auth/login` | `/api/v1/auth/login` | 不匹配 |
| 钥匙列表 | `/keys` | `/api/v1/keys` | 不匹配 |
| 用户资料 | `/user/me` | `/api/v1/user/profile` | 不匹配 |
| 车辆列表 | `/vehicles` | `/api/v1/vehicles` | 不匹配 |

**修复方案**:

1. 统一使用 `/api/v1/` 前缀
2. 修复 KeysPage.tsx 导入错误
3. 更新 useApi.ts 基础路径

**代码示例**:

```typescript
// frontend/src/api/client.ts
const apiClient: AxiosInstance = axios.create({
  baseURL: import.meta.env.VITE_API_URL || '/api/v1',  // 统一使用 /api/v1
  timeout: 30000,
  headers: {
    'Content-Type': 'application/json',
  },
});

// 统一导出
export default apiClient;
export const api = apiClient;  // 命名导出，兼容现有代码

// frontend/src/hooks/useApi.ts
const API_BASE = import.meta.env.VITE_API_URL || '/api/v1';  // 统一路径

// frontend/src/pages/KeysPage.tsx
// 修复导入
import apiClient from '../api/client';  // 正确导入
// 或
import { api } from '../api/client';  // 使用命名导出
```

**负责人**: Frontend Developer  
**时间表**: 1 天  
**验证**: 所有 API 调用返回 200

---

#### 问题 #3: Frontend 类型定义不匹配

**位置**: `/home/admin/yuleDKCS/frontend/src/types/index.ts`  
**影响**: 类型安全缺失，开发体验差

**问题**: 当前类型定义是充值卡系统遗留代码，与 Digital Key 业务不符

**修复方案**:

根据 Swagger 文档重新定义 TypeScript 类型

**代码示例**:

```typescript
// frontend/src/types/index.ts

// 车辆相关
export interface Vehicle {
  id: number;
  vin: string;
  name: string;
  brand: string;
  model: string;
  year: number;
  color: string;
  licensePlate: string;
  createdAt: string;
  updatedAt: string;
}

export interface VehicleStatus {
  vehicleId: number;
  status: 'online' | 'offline' | 'sleeping';
  lockState: 'locked' | 'unlocked' | 'unknown';
  engineState: 'running' | 'stopped' | 'unknown';
  batteryLevel: number;
  fuelLevel?: number;
  location?: {
    latitude: number;
    longitude: number;
    accuracy: number;
    timestamp: string;
  };
  lastUpdated: string;
}

// 数字钥匙相关
export interface DigitalKey {
  id: string;
  vehicleId: number;
  name: string;
  keyType: 'owner' | 'shared' | 'guest' | 'valet';
  status: 'active' | 'suspended' | 'revoked' | 'expired';
  permissions: KeyPermissions;
  validFrom: string;
  validUntil: string | null;
  sharedBy?: string;
  createdAt: string;
}

export interface KeyPermissions {
  unlock: boolean;
  lock: boolean;
  engineStart: boolean;
  engineStop: boolean;
  trunk: boolean;
  climate: boolean;
  share: boolean;
  revoke: boolean;
}

// 用户相关
export interface User {
  id: number;
  username: string;
  email: string;
  phone?: string;
  avatar?: string;
  createdAt: string;
  updatedAt: string;
}

// API 响应类型
export interface ApiResponse<T> {
  data: T;
  message?: string;
  code: number;
}

export interface PaginatedResponse<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
}
```

**负责人**: Frontend Developer  
**时间表**: 1-2 天

---

#### 问题 #4: Embedded SDK AES-GCM 加密未实现

**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c:398-403`  
**影响**: 消息传输不安全，仅使用 memcpy

**当前代码**:
```c
/* TODO: AES-128-GCM 加密载荷 */
memcpy(&encrypted_data[offset], payload, payload_len);
offset += payload_len;

/* MAC/Tag (占位) */
memset(&encrypted_data[offset], 0, 16);
```

**修复方案**:

集成 mbedtls 实现 AES-128-GCM 加密

**代码示例**:

```c
// src/ccc/protocol/ccc_crypto.c
#include "mbedtls/gcm.h"
#include "mbedtls/cipher.h"

error_t ccc_aes_gcm_encrypt(
    const uint8_t *key,           // 16 bytes
    const uint8_t *iv,            // 12 bytes
    const uint8_t *aad,           // Additional authenticated data
    size_t aad_len,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t *ciphertext,
    uint8_t *tag                  // 16 bytes output
) {
    mbedtls_gcm_context gcm;
    int ret;
    
    mbedtls_gcm_init(&gcm);
    
    ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return ERROR_CRYPTO_INIT_FAILED;
    }
    
    ret = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        plaintext_len,
        iv, 12,
        aad, aad_len,
        plaintext,
        ciphertext,
        16, tag
    );
    
    mbedtls_gcm_free(&gcm);
    
    return (ret == 0) ? ERROR_OK : ERROR_CRYPTO_ENCRYPT_FAILED;
}

error_t ccc_aes_gcm_decrypt(
    const uint8_t *key,
    const uint8_t *iv,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *tag,
    uint8_t *plaintext
) {
    mbedtls_gcm_context gcm;
    int ret;
    
    mbedtls_gcm_init(&gcm);
    
    ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return ERROR_CRYPTO_INIT_FAILED;
    }
    
    ret = mbedtls_gcm_auth_decrypt(
        &gcm,
        ciphertext_len,
        iv, 12,
        aad, aad_len,
        tag, 16,
        ciphertext,
        plaintext
    );
    
    mbedtls_gcm_free(&gcm);
    
    return (ret == 0) ? ERROR_OK : ERROR_CRYPTO_DECRYPT_FAILED;
}
```

**CMakeLists.txt 更新**:
```cmake
# 添加 mbedtls 依赖
find_package(mbedtls REQUIRED)
target_link_libraries(yuledkcs mbedtls::mbedtls)
```

**负责人**: Embedded SDK Team  
**时间表**: 3-5 天  
**依赖**: mbedtls 库集成

---

#### 问题 #5: Mobile SDK (Android/iOS) 无 WebSocket 支持

**位置**: 
- Android: `/home/admin/yuleDKCS/mobile/android/...`
- iOS: `/home/admin/yuleDKCS/mobile/ios/...`  
**影响**: 车辆状态无法实时上报到 Backend

**修复方案**:

为 Android 和 iOS 添加 WebSocket 客户端实现

**Android 代码示例** (Kotlin):

```kotlin
// mobile/android/src/main/java/com/yuledkcs/websocket/WebSocketManager.kt
package com.yuledkcs.websocket

import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import okhttp3.*
import okio.ByteString
import java.util.concurrent.TimeUnit
import org.json.JSONObject

class WebSocketManager private constructor() {
    private var webSocket: WebSocket? = null
    private var client: OkHttpClient = OkHttpClient.Builder()
        .pingInterval(30, TimeUnit.SECONDS)
        .build()
    
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()
    
    private val _messages = MutableSharedFlow<WebSocketMessage>()
    val messages: SharedFlow<WebSocketMessage> = _messages.asSharedFlow()
    
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var reconnectJob: Job? = null

    fun connect(url: String, token: String, vehicleId: String? = null) {
        disconnect()
        
        val wsUrl = buildString {
            append(url)
            if (vehicleId != null) {
                append("?vehicle_id=$vehicleId")
            }
        }
        
        val request = Request.Builder()
            .url(wsUrl)
            .header("Authorization", "Bearer $token")
            .build()
        
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(ws: WebSocket, response: Response) {
                _connectionState.value = ConnectionState.Connected
                reconnectJob?.cancel()
            }
            
            override fun onMessage(ws: WebSocket, text: String) {
                scope.launch {
                    _messages.emit(parseMessage(text))
                }
            }
            
            override fun onClosing(ws: WebSocket, code: Int, reason: String) {
                _connectionState.value = ConnectionState.Disconnecting
            }
            
            override fun onClosed(ws: WebSocket, code: Int, reason: String) {
                _connectionState.value = ConnectionState.Disconnected
                scheduleReconnect(url, token, vehicleId)
            }
            
            override fun onFailure(ws: WebSocket, t: Throwable, response: Response?) {
                _connectionState.value = ConnectionState.Error(t.message ?: "Unknown error")
                scheduleReconnect(url, token, vehicleId)
            }
        })
    }
    
    fun send(type: String, payload: JSONObject) {
        val message = JSONObject().apply {
            put("version", "1.0.0")
            put("type", type)
            put("timestamp", System.currentTimeMillis())
            put("payload", payload)
        }
        webSocket?.send(message.toString())
    }
    
    fun disconnect() {
        reconnectJob?.cancel()
        webSocket?.close(1000, "Client disconnecting")
        webSocket = null
    }
    
    private fun scheduleReconnect(url: String, token: String, vehicleId: String?) {
        reconnectJob = scope.launch {
            delay(5000)
            connect(url, token, vehicleId)
        }
    }
    
    private fun parseMessage(text: String): WebSocketMessage {
        val json = JSONObject(text)
        return WebSocketMessage(
            version = json.optString("version"),
            type = json.optString("type"),
            timestamp = json.optLong("timestamp"),
            payload = json.optJSONObject("payload")
        )
    }
    
    companion object {
        @Volatile
        private var instance: WebSocketManager? = null
        
        fun getInstance(): WebSocketManager {
            return instance ?: synchronized(this) {
                instance ?: WebSocketManager().also { instance = it }
            }
        }
    }
}

sealed class ConnectionState {
    object Disconnected : ConnectionState()
    object Connecting : ConnectionState()
    object Connected : ConnectionState()
    object Disconnecting : ConnectionState()
    data class Error(val message: String) : ConnectionState()
}

data class WebSocketMessage(
    val version: String,
    val type: String,
    val timestamp: Long,
    val payload: JSONObject?
)
```

**iOS 代码示例** (Swift):

```swift
// mobile/ios/Sources/YuleDKCS/WebSocket/WebSocketManager.swift
import Foundation
import Combine

public class WebSocketManager: NSObject, ObservableObject {
    public static let shared = WebSocketManager()
    
    @Published public var connectionState: ConnectionState = .disconnected
    
    private var webSocketTask: URLSessionWebSocketTask?
    private var reconnectTimer: Timer?
    private var messageHandlers: [String: [(WebSocketMessage) -> Void]] = [:]
    
    private let reconnectInterval: TimeInterval = 5.0
    private var currentReconnectInterval: TimeInterval = 5.0
    
    private override init() {}
    
    public func connect(url: URL, token: String, vehicleId: String? = nil) {
        disconnect()
        
        var components = URLComponents(url: url, resolvingAgainstBaseURL: true)
        if let vehicleId = vehicleId {
            components?.queryItems = [URLQueryItem(name: "vehicle_id", value: vehicleId)]
        }
        
        guard let wsURL = components?.url else { return }
        
        var request = URLRequest(url: wsURL)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        
        let session = URLSession(configuration: .default)
        webSocketTask = session.webSocketTask(with: request)
        webSocketTask?.delegate = self
        
        connectionState = .connecting
        webSocketTask?.resume()
        
        receiveMessage()
    }
    
    public func send(type: String, payload: [String: Any]) {
        let message: [String: Any] = [
            "version": "1.0.0",
            "type": type,
            "timestamp": ISO8601DateFormatter().string(from: Date()),
            "payload": payload
        ]
        
        do {
            let data = try JSONSerialization.data(withJSONObject: message)
            if let text = String(data: data, encoding: .utf8) {
                webSocketTask?.send(.string(text)) { error in
                    if let error = error {
                        print("WebSocket send error: \(error)")
                    }
                }
            }
        } catch {
            print("JSON encoding error: \(error)")
        }
    }
    
    public func on(_ type: String, handler: @escaping (WebSocketMessage) -> Void) {
        if messageHandlers[type] == nil {
            messageHandlers[type] = []
        }
        messageHandlers[type]?.append(handler)
    }
    
    public func disconnect() {
        reconnectTimer?.invalidate()
        webSocketTask?.cancel(with: .normalClosure, reason: nil)
        connectionState = .disconnected
    }
    
    private func receiveMessage() {
        webSocketTask?.receive { [weak self] result in
            switch result {
            case .success(let message):
                switch message {
                case .string(let text):
                    self?.handleMessage(text)
                case .data(let data):
                    if let text = String(data: data, encoding: .utf8) {
                        self?.handleMessage(text)
                    }
                default:
                    break
                }
                self?.receiveMessage()
                
            case .failure(let error):
                print("WebSocket receive error: \(error)")
                self?.connectionState = .error(error.localizedDescription)
                self?.scheduleReconnect()
            }
        }
    }
    
    private func handleMessage(_ text: String) {
        do {
            let data = text.data(using: .utf8)!
            let message = try JSONDecoder().decode(WebSocketMessage.self, from: data)
            
            DispatchQueue.main.async {
                self.messageHandlers[message.type]?.forEach { $0(message) }
            }
        } catch {
            print("Message parsing error: \(error)")
        }
    }
    
    private func scheduleReconnect() {
        reconnectTimer = Timer.scheduledTimer(withTimeInterval: currentReconnectInterval, repeats: false) { [weak self] _ in
            // Retry connection
        }
        currentReconnectInterval = min(currentReconnectInterval * 2, 60.0)
    }
}

extension WebSocketManager: URLSessionWebSocketDelegate {
    public func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didOpenWithProtocol protocol: String?) {
        DispatchQueue.main.async {
            self.connectionState = .connected
            self.currentReconnectInterval = 5.0
        }
    }
    
    public func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        DispatchQueue.main.async {
            self.connectionState = .disconnected
            self.scheduleReconnect()
        }
    }
}

public enum ConnectionState {
    case disconnected
    case connecting
    case connected
    case error(String)
}

public struct WebSocketMessage: Codable {
    public let version: String
    public let type: String
    public let timestamp: String
    public let payload: [String: AnyCodable]?
}
```

**负责人**: Mobile SDK Team (Android & iOS)  
**时间表**: 5-7 天  
**依赖**: Backend WebSocket 端点

---

### 2.2 P1 - High 优先级

#### 问题 #6: CCC 配对请求消息格式不完整

**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_core.c:172-174`  
**影响**: CCC 协议配对流程不完整

**当前代码**:
```c
/* 证书 (M1: 简化版本) */
request_data[offset++] = 0x00;
request_data[offset++] = 0x00;
```

**修复方案**:

实现完整的 X.509 证书编码

**代码示例**:

```c
// src/ccc/protocol/ccc_certificate.c

// 证书数据结构
typedef struct {
    uint8_t version;
    uint8_t issuer[64];
    uint8_t subject[64];
    uint32_t valid_from;
    uint32_t valid_until;
    uint8_t public_key[65];  // P-256 uncompressed
    uint8_t signature[64];   // ECDSA
} ccc_certificate_t;

error_t ccc_certificate_serialize(
    const ccc_certificate_t *cert,
    uint8_t *output,
    size_t *output_len
) {
    size_t offset = 0;
    
    // Version
    output[offset++] = cert->version;
    
    // Issuer length + data
    uint8_t issuer_len = (uint8_t)strlen((char*)cert->issuer);
    output[offset++] = issuer_len;
    memcpy(&output[offset], cert->issuer, issuer_len);
    offset += issuer_len;
    
    // Subject length + data
    uint8_t subject_len = (uint8_t)strlen((char*)cert->subject);
    output[offset++] = subject_len;
    memcpy(&output[offset], cert->subject, subject_len);
    offset += subject_len;
    
    // Validity period (8 bytes)
    output[offset++] = (cert->valid_from >> 24) & 0xFF;
    output[offset++] = (cert->valid_from >> 16) & 0xFF;
    output[offset++] = (cert->valid_from >> 8) & 0xFF;
    output[offset++] = cert->valid_from & 0xFF;
    output[offset++] = (cert->valid_until >> 24) & 0xFF;
    output[offset++] = (cert->valid_until >> 16) & 0xFF;
    output[offset++] = (cert->valid_until >> 8) & 0xFF;
    output[offset++] = cert->valid_until & 0xFF;
    
    // Public key (65 bytes uncompressed)
    memcpy(&output[offset], cert->public_key, 65);
    offset += 65;
    
    // Signature (64 bytes)
    memcpy(&output[offset], cert->signature, 64);
    offset += 64;
    
    *output_len = offset;
    return ERROR_OK;
}

error_t ccc_build_pairing_request(
    const uint8_t *ephemeral_pub_key,
    const uint8_t *challenge,
    const ccc_certificate_t *device_cert,
    uint8_t *request_data,
    size_t *request_len
) {
    size_t offset = 0;
    
    // Version (1 byte)
    request_data[offset++] = 0x03;  // CCC R3
    
    // Ephemeral Public Key (65 bytes)
    memcpy(&request_data[offset], ephemeral_pub_key, 65);
    offset += 65;
    
    // Challenge (32 bytes)
    memcpy(&request_data[offset], challenge, 32);
    offset += 32;
    
    // Device Certificate
    size_t cert_len = 0;
    ccc_certificate_serialize(device_cert, &request_data[offset + 2], &cert_len);
    
    // Certificate length (2 bytes, big-endian)
    request_data[offset++] = (cert_len >> 8) & 0xFF;
    request_data[offset++] = cert_len & 0xFF;
    offset += cert_len;
    
    *request_len = offset;
    return ERROR_OK;
}
```

**负责人**: Embedded SDK Team  
**时间表**: 3 天

---

#### 问题 #7: UWB 测距结果为模拟值

**位置**: `/home/admin/yuleDKCS/src/ccc/protocol/ccc_uwb.c:42-44`  
**影响**: UWB 测距功能无法用于生产环境

**当前代码**:
```c
/* 模拟测距结果 */
*distance_m = 2.5f;
*accuracy_m = 0.1f;
```

**修复方案**:

接入真实 UWB 芯片驱动 (Qorvo DW3000/DW3720 或 NXP NCJ29D5)

**代码示例**:

```c
// src/hal/uwb/qorvo_dw3000.c

#include "uwb_hal.h"
#include "dw3000.h"  // Qorvo driver

typedef struct {
    dwt_config_t config;
    bool initialized;
    uint8_t sts_key[16];
    uint8_t sts_iv[8];
} dw3000_context_t;

static dw3000_context_t g_dw3000_ctx = {0};

error_t uwb_hal_init_qorvo(void *spi_handle, void *gpio_handle) {
    // Initialize DW3000
    if (!dwt_checkidlerc()) {
        return ERROR_UWB_INIT_FAILED;
    }
    
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        return ERROR_UWB_INIT_FAILED;
    }
    
    // Configure for CCC R3
    dwt_config_t config = {
        .chan = 9,                    // Channel 9 (8.0 GHz)
        .txPreambLength = DWT_PLEN_64,
        .rxPAC = DWT_PAC8,
        .txCode = 9,
        .rxCode = 9,
        .sfdType = DWT_SFD_DW8,
        .dataRate = DWT_BR_6M8,
        .phrMode = DWT_PHRMODE_STD,
        .phrRate = DWT_PHRRATE_STD,
        .sfdTO = (64 + 1 + 8 - 8),
        .stsMode = DWT_STS_MODE_1,
        .stsLength = DWT_STS_LEN_64,
        .pdoaMode = DWT_PDOA_M0
    };
    
    if (dwt_configure(&config) == DWT_ERROR) {
        return ERROR_UWB_CONFIG_FAILED;
    }
    
    g_dw3000_ctx.config = config;
    g_dw3000_ctx.initialized = true;
    
    return ERROR_OK;
}

error_t uwb_hal_start_ranging(uint8_t session_id, const uint8_t *sts_key, const uint8_t *sts_iv) {
    if (!g_dw3000_ctx.initialized) {
        return ERROR_UWB_NOT_INITIALIZED;
    }
    
    // Configure STS
    memcpy(g_dw3000_ctx.sts_key, sts_key, 16);
    memcpy(g_dw3000_ctx.sts_iv, sts_iv, 8);
    
    dwt_configurestskey(g_dw3000_ctx.sts_key);
    dwt_configurestsnvic(g_dw3000_ctx.sts_iv);
    
    // Enable STS
    dwt_configureframefilter(DWT_FF_ENABLE_802_15_4, DWT_FF_COARSE);
    
    return ERROR_OK;
}

error_t uwb_hal_get_measurement(float *distance_m, float *accuracy_m) {
    if (!g_dw3000_ctx.initialized) {
        return ERROR_UWB_NOT_INITIALIZED;
    }
    
    // Read ranging result from DW3000
    int32_t tof_dtu;
    if (dwt_readrangingresult(&tof_dtu) != DWT_SUCCESS) {
        return ERROR_UWB_MEASUREMENT_FAILED;
    }
    
    // Convert to distance (DTU to meters)
    // 1 DTU = 1/499.2e6/128 seconds
    // Distance = (speed of light * tof) / 2
    float tof_sec = (float)tof_dtu / (499.2e6 * 128.0);
    *distance_m = (299792458.0 * tof_sec) / 2.0;
    
    // Estimate accuracy based on signal quality
    uint16_t cir_pwr;
    dwt_readcirpower(&cir_pwr);
    *accuracy_m = 0.1 + (1000.0 / cir_pwr);  // Simplified estimation
    
    return ERROR_OK;
}
```

**负责人**: Embedded SDK Team (Hardware Integration)  
**时间表**: 5-7 天  
**依赖**: UWB 芯片硬件/驱动

---

#### 问题 #8: SE 接口未实现

**位置**: `/home/admin/yuleDKCS/src/common/dkcs.c:408-414`  
**影响**: 安全元件操作无法执行

**当前代码**:
```c
/* 创建简化的 SE 接口 */
ccc_se_interface_t ccc_se = {0};
/* 设置基本回调函数 */
ret = ccc_init(&ccc_se);
```

**修复方案**:

实现 SE050/SCP03/TEE 适配器

**代码示例**:

```c
// src/security/se050_adapter.c

#include "se050.h"
#include "ccc.h"

static sm_session_t se050_session;

error_t se050_adapter_init(void) {
    sss_status_t status;
    sss_session_t session;
    
    // Open session to SE050
    status = sss_session_open(&session, 
                              kType_SSS_SE_SE05x,
                              0,  // session ID
                              kSE05x_AccessMode_Plain,
                              NULL);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_INIT_FAILED;
    }
    
    memcpy(&se050_session, &session, sizeof(session));
    
    return ERROR_OK;
}

error_t se050_generate_key_pair(uint8_t *pub_key, uint8_t *priv_key_id) {
    sss_status_t status;
    sss_object_t key_object;
    uint32_t key_id = SE050_KEY_ID_BASE + 1;
    
    status = sss_key_object_init(&key_object, &se050_session);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_KEY_GEN_FAILED;
    }
    
    status = sss_key_object_allocate_handle(&key_object,
                                            key_id,
                                            kSSS_KeyPart_Pair,
                                            kSSS_CipherType_EC_NIST_P,
                                            256,
                                            kKeyObject_Mode_Persistent);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_KEY_GEN_FAILED;
    }
    
    status = sss_key_store_generate_key(&se050_session, &key_object, 256, NULL);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_KEY_GEN_FAILED;
    }
    
    // Export public key
    size_t pub_key_len = 65;
    status = sss_key_store_get_key(&se050_session, &key_object, pub_key, &pub_key_len, &pub_key_len, NULL);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_KEY_GEN_FAILED;
    }
    
    *priv_key_id = key_id;
    
    return ERROR_OK;
}

error_t se050_sign(const uint8_t *data, size_t len, uint8_t key_id, uint8_t *signature) {
    sss_status_t status;
    sss_object_t key_object;
    sss_asymmetric_t ctx;
    
    status = sss_key_object_init(&key_object, &se050_session);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_SIGN_FAILED;
    }
    
    status = sss_key_object_get_handle(&key_object, key_id);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_SIGN_FAILED;
    }
    
    status = sss_asymmetric_context_init(&ctx, &se050_session, &key_object, kAlgorithm_SSS_ECDSA_SHA256, kMode_SSS_Sign);
    if (status != kStatus_SSS_Success) {
        return ERROR_SE_SIGN_FAILED;
    }
    
    size_t sig_len = 64;
    status = sss_asymmetric_sign_digest(&ctx, data, len, signature, &sig_len);
    
    sss_asymmetric_context_free(&ctx);
    
    return (status == kStatus_SSS_Success) ? ERROR_OK : ERROR_SE_SIGN_FAILED;
}

// CCC SE 接口实现
ccc_se_interface_t se050_ccc_interface = {
    .init = se050_adapter_init,
    .generate_key_pair = se050_generate_key_pair,
    .sign = se050_sign,
    // ... 其他函数
};
```

**负责人**: Embedded SDK Team (Security)  
**时间表**: 7-10 天  
**依赖**: SE050 SDK/驱动

---

#### 问题 #9: WebSocket URL 路径不一致

**位置**: Backend 路由 vs Swagger 定义  
**影响**: 集成困难

**不一致**:
- Backend: `/ws/vehicles/{id}`
- Swagger: `/ws/vehicle/{id}` (单复数差异)

**修复方案**:

统一使用 `/ws/v1/` 前缀和复数形式

```go
// backend/internal/router/router.go
func SetupWebSocketRoutes(r *gin.Engine, hub *websocket.Hub) {
    ws := r.Group("/ws/v1")
    {
        // User connection
        ws.GET("/user", func(c *gin.Context) {
            vehicleId := c.Query("vehicle_id")
            handlers.HandleUserWS(c, hub, vehicleId)
        })
        
        // Vehicle connection (复数形式)
        ws.GET("/vehicles/:id", func(c *gin.Context) {
            vehicleId := c.Param("id")
            handlers.HandleVehicleWS(c, hub, vehicleId)
        })
        
        // Admin connection
        ws.GET("/admin", func(c *gin.Context) {
            handlers.HandleAdminWS(c, hub)
        })
    }
}
```

**Swagger 更新**:
```yaml
# docs/api/swagger.yaml
/ws/v1/user:
  get:
    tags:
      - WebSocket
    summary: 用户 WebSocket 连接
    parameters:
      - name: vehicle_id
        in: query
        schema:
          type: string
        description: 可选的车辆ID订阅

/ws/v1/vehicles/{id}:
  get:
    tags:
      - WebSocket
    summary: 车辆 WebSocket 连接
    parameters:
      - name: id
        in: path
        required: true
        schema:
          type: string
```

**负责人**: Backend Team  
**时间表**: 0.5 天

---

#### 问题 #10: WebSocket 消息类型命名不一致

**位置**: Backend vs Swagger  
**影响**: 文档与实现不同步

**不一致列表**:
- Backend: `ping` vs Swagger: `heartbeat`
- Backend: `vehicle_status_update` vs Swagger: `status`

**修复方案**:

统一命名规范:

| 消息类型 | 统一命名 | 说明 |
|----------|----------|------|
| 心跳请求 | `ping` | 客户端 -> 服务端 |
| 心跳响应 | `pong` | 服务端 -> 客户端 |
| 车辆状态上报 | `vehicle_status` | 车辆 -> 服务端 |
| 车辆状态广播 | `vehicle_status_update` | 服务端 -> 客户端 |
| 命令发送 | `command` | 服务端 -> 车辆 |
| 命令结果 | `command_result` | 车辆 -> 服务端 -> 客户端 |
| 系统通知 | `notification` | 服务端 -> 客户端 |

**Backend 更新**:
```go
// backend/internal/websocket/message.go
const (
    MsgTypePing               = "ping"
    MsgTypePong               = "pong"
    MsgTypeVehicleStatus      = "vehicle_status"
    MsgTypeVehicleStatusUpdate = "vehicle_status_update"
    MsgTypeCommand            = "command"
    MsgTypeCommandResult      = "command_result"
    MsgTypeNotification       = "notification"
)
```

**负责人**: Backend Team  
**时间表**: 0.5 天

---

#### 问题 #11: 缺少 WebSocket 子协议版本

**位置**: WebSocket 握手  
**影响**: 协议演化困难

**修复方案**:

添加 `Sec-WebSocket-Protocol` 头

```go
// backend/internal/handlers/ws_handler.go
func HandleWebSocket(hub *websocket.Hub) gin.HandlerFunc {
    return func(c *gin.Context) {
        // 检查子协议
        protocols := c.Request.Header.Get("Sec-WebSocket-Protocol")
        if !strings.Contains(protocols, "yuledkcs.ws.v1") {
            c.JSON(400, gin.H{"error": "Unsupported subprotocol"})
            return
        }
        
        upgrader := websocket.Upgrader{
            Subprotocols: []string{"yuledkcs.ws.v1"},
            CheckOrigin: func(r *http.Request) bool {
                return true
            },
        }
        
        conn, err := upgrader.Upgrade(c.Writer, c.Request, http.Header{
            "Sec-WebSocket-Protocol": []string{"yuledkcs.ws.v1"},
        })
        if err != nil {
            return
        }
        
        // ... 后续处理
    }
}
```

**Frontend 更新**:
```typescript
const ws = new WebSocket(url, ['yuledkcs.ws.v1']);
```

**负责人**: Backend + Frontend Teams  
**时间表**: 0.5 天

---

### 2.3 P2 - Medium 优先级

#### 问题 #12: HTTP 客户端双重实现

**位置**: `client.ts` 和 `useApi.ts`  
**影响**: 代码重复，维护困难

**修复方案**:

统一使用 Axios，移除 fetch API 实现

```typescript
// frontend/src/hooks/useApi.ts - 重构后
import { useQuery, useMutation } from '@tanstack/react-query';
import apiClient from '../api/client';

export function useVehicles() {
  return useQuery({
    queryKey: ['vehicles'],
    queryFn: async () => {
      const response = await apiClient.get('/vehicles');
      return response.data;
    },
  });
}

export function useVehicleStatus(vehicleId: string) {
  return useQuery({
    queryKey: ['vehicle', vehicleId, 'status'],
    queryFn: async () => {
      const response = await apiClient.get(`/vehicles/${vehicleId}/status`);
      return response.data;
    },
    refetchInterval: 30000,  // 30秒轮询，直到 WebSocket 就绪
  });
}
```

**负责人**: Frontend Developer  
**时间表**: 1 天

---

#### 问题 #13: Backend 缺少请求/响应日志

**位置**: Backend HTTP 处理  
**影响**: 调试困难

**修复方案**:

添加 Gin 中间件

```go
// backend/internal/middleware/logger.go
func Logger() gin.HandlerFunc {
    return func(c *gin.Context) {
        start := time.Now()
        path := c.Request.URL.Path
        raw := c.Request.URL.RawQuery
        
        // Process request
        c.Next()
        
        // Log after request
        latency := time.Since(start)
        clientIP := c.ClientIP()
        method := c.Request.Method
        statusCode := c.Writer.Status()
        
        if raw != "" {
            path = path + "?" + raw
        }
        
        log.Info().
            Str("client_ip", clientIP).
            Str("method", method).
            Str("path", path).
            Int("status", statusCode).
            Dur("latency", latency).
            Str("user_agent", c.Request.UserAgent()).
            Msg("HTTP Request")
    }
}
```

**负责人**: Backend Team  
**时间表**: 0.5 天

---

#### 问题 #14: Frontend Token 存储安全风险

**位置**: `store/auth.ts`  
**影响**: XSS 攻击风险

**修复方案**:

使用 httpOnly cookie 或内存存储

```typescript
// frontend/src/store/auth.ts - 改进版
import { create } from 'zustand';

// 使用内存存储，避免 localStorage XSS 风险
interface AuthState {
  token: string | null;
  user: User | null;
  setAuth: (token: string, user: User) => void;
  clearAuth: () => void;
}

export const useAuthStore = create<AuthState>((set) => ({
  token: null,
  user: null,
  setAuth: (token, user) => set({ token, user }),
  clearAuth: () => set({ token: null, user: null }),
}));

// 或者使用 httpOnly cookie（推荐）
// 由后端设置，前端无法通过 JS 访问
```

**负责人**: Frontend Developer  
**时间表**: 0.5 天

---

#### 问题 #15: Backend 大多数 Handler 返回 501

**位置**: Backend handlers  
**影响**: API 功能不完整

需要实现的 Handler:
- `POST /api/v1/auth/login`
- `POST /api/v1/auth/register`
- `GET /api/v1/user/profile`
- `GET /api/v1/vehicles`
- `GET /api/v1/keys`
- `POST /api/v1/vehicles/{id}/command`

**负责人**: Backend Team  
**时间表**: 5-7 天

---

### 2.4 P3 - Low 优先级

#### 问题 #16: 缺少请求重试机制

**修复方案**:

```typescript
// frontend/src/api/client.ts
import axiosRetry from 'axios-retry';

axiosRetry(apiClient, {
  retries: 3,
  retryDelay: axiosRetry.exponentialDelay,
  retryCondition: (error) => {
    return axiosRetry.isNetworkOrIdempotentRequestError(error) ||
           error.response?.status === 429;
  },
});
```

**负责人**: Frontend Developer  
**时间表**: 0.5 天

---

#### 问题 #17: 缺少 Token 自动刷新

**修复方案**:

实现 refresh token 机制

**负责人**: Backend + Frontend Teams  
**时间表**: 1-2 天

---

#### 问题 #18: 协议文档不完整

**修复方案**:

补充 Swagger WebSocket 消息结构定义

```yaml
# docs/api/swagger.yaml 新增
components:
  schemas:
    WebSocketMessage:
      type: object
      required:
        - version
        - type
        - timestamp
      properties:
        version:
          type: string
          example: "1.0.0"
        type:
          type: string
          enum: [ping, pong, vehicle_status, vehicle_status_update, command, command_result, notification]
        timestamp:
          type: string
          format: date-time
        id:
          type: string
          format: uuid
        payload:
          type: object
```

**负责人**: Backend Team Lead  
**时间表**: 1 天

---

## 3. 实施时间表

### Phase 1: 紧急修复 (Week 1)

| 天数 | 任务 | 负责人 | 产出 |
|------|------|--------|------|
| Day 1 | 修复 API 路径不一致 | Frontend Dev | 统一 `/api/v1/` 前缀 |
| Day 1 | 修复 KeysPage.tsx 导入错误 | Frontend Dev | 编译通过 |
| Day 2-3 | 重新定义 TypeScript 类型 | Frontend Dev | types/index.ts 更新 |
| Day 2-5 | 实现 Frontend WebSocket 客户端 | Frontend Lead | WebSocketService.ts |
| Day 3-5 | 实现 Mobile SDK WebSocket (Android) | Android Dev | WebSocketManager.kt |
| Day 3-5 | 实现 Mobile SDK WebSocket (iOS) | iOS Dev | WebSocketManager.swift |

### Phase 2: 协议实现 (Week 2)

| 天数 | 任务 | 负责人 | 产出 |
|------|------|--------|------|
| Day 1-3 | 实现 AES-GCM 加密 | Embedded Dev | ccc_crypto.c |
| Day 1-3 | 实现 CCC 证书序列化 | Embedded Dev | ccc_certificate.c |
| Day 3-5 | 接入 UWB 芯片驱动 | Hardware Dev | qorvo_dw3000.c |
| Day 4-5 | 统一 WebSocket 协议 | Backend Dev | 消息类型统一 |

### Phase 3: 安全与完善 (Week 3)

| 天数 | 任务 | 负责人 | 产出 |
|------|------|--------|------|
| Day 1-5 | 实现 SE 适配器 | Security Dev | se050_adapter.c |
| Day 1-3 | 实现 Backend API Handlers | Backend Dev | 完整 API 实现 |
| Day 3-5 | 安全审计与优化 | Security Lead | 安全报告 |
| Day 5 | 协议文档更新 | Tech Writer | 更新 swagger.yaml |

### Phase 4: 集成测试 (Week 4)

| 天数 | 任务 | 负责人 | 产出 |
|------|------|--------|------|
| Day 1-3 | 端到端集成测试 | QA Team | 测试报告 |
| Day 2-3 | 性能测试 | QA Team | 性能报告 |
| Day 3-5 | 安全测试 | Security Team | 安全测试报告 |
| Day 5 | 发布准备 | DevOps | 发布检查清单 |

---

## 4. 团队分工

### 4.1 负责人分配

| 团队/人员 | 负责问题 | 工作量 |
|-----------|----------|--------|
| **Frontend Team Lead** | #1 (WebSocket) | 3-5 天 |
| **Frontend Developer** | #2, #3, #12, #14, #16 | 3-4 天 |
| **Android Developer** | #5 (Android) | 3-4 天 |
| **iOS Developer** | #5 (iOS) | 3-4 天 |
| **Backend Team Lead** | #9, #10, #11, #18 | 2-3 天 |
| **Backend Developer** | #13, #15 | 5-7 天 |
| **Embedded SDK Lead** | #4, #6, #8 | 10-15 天 |
| **Hardware Integration** | #7 | 5-7 天 |
| **Security Lead** | #17 | 1-2 天 |

### 4.2 依赖关系图

```
Week 1:
  [Frontend API Path Fix] -> [Frontend WebSocket]
  [TypeScript Types] -> [Frontend WebSocket]
  [Backend WebSocket Fix] -> [Frontend WebSocket] -> [Mobile WebSocket]

Week 2:
  [AES-GCM] -> [CCC Certificate]
  [UWB Driver] -> [Ranging Test]
  [SE Adapter] -> [Security Test]

Week 3:
  [All Above] -> [Integration Test]
```

---

## 5. 验证检查清单

### 5.1 Frontend 验证

- [ ] API 调用返回 200
- [ ] WebSocket 连接成功
- [ ] 心跳消息正常收发
- [ ] 车辆状态实时更新
- [ ] 命令结果正确接收
- [ ] 断线重连工作正常
- [ ] TypeScript 编译无错误

### 5.2 Mobile SDK 验证

- [ ] Android WebSocket 连接成功
- [ ] iOS WebSocket 连接成功
- [ ] 车辆状态上报正常
- [ ] 命令接收与执行正确

### 5.3 Backend 验证

- [ ] 所有 API 端点返回正确
- [ ] WebSocket 消息类型一致
- [ ] 认证/授权正常工作
- [ ] 日志记录完整

### 5.4 Embedded SDK 验证

- [ ] AES-GCM 加密/解密正确
- [ ] CCC 配对流程完整
- [ ] UWB 测距数据真实
- [ ] SE 操作正常

### 5.5 集成验证

- [ ] 三端通信正常
- [ ] 并发连接稳定
- [ ] 长连接无内存泄漏
- [ ] 安全审计通过

---

## 6. 风险评估

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| UWB 芯片驱动延期 | 中 | 高 | 先使用模拟数据，预留接口 |
| SE050 SDK 集成问题 | 中 | 高 | 准备软件 SE 备选方案 |
| 跨团队协调问题 | 低 | 中 | 每日站会同步进度 |
| 性能不达标 | 低 | 中 | 预留优化时间 |

---

## 7. 参考文档

### 7.1 审查报告

- `/home/admin/yuleDKCS/docs/embedded_sdk_protocol_review.md`
- `/home/admin/yuleDKCS/websocket_protocol_comparison.md`
- `/home/admin/yuleDKCS/frontend_communication_review.md`
- `/home/admin/yuleDKCS/docs/protocol_comparison.md`

### 7.2 技术规范

- CCC Digital Key Release 3 Specification 2.0.0r2
- IEEE 802.15.4z HRP UWB
- ICCE-DK-001 数字钥匙技术规范
- ICCOA-DK-001 数字钥匙技术规范
- OpenAPI 3.0.3 Specification

### 7.3 关键源代码

- `frontend/src/api/client.ts`
- `frontend/src/hooks/useApi.ts`
- `backend/internal/websocket/*.go`
- `backend/internal/router/router.go`
- `src/ccc/protocol/ccc_core.c`
- `src/ccc/protocol/ccc_uwb.c`
- `include/dkcs.h`

---

## 8. 变更记录

| 版本 | 日期 | 作者 | 变更内容 |
|------|------|------|----------|
| 1.0.0 | 2026-05-09 | Protocol Review Team | 初始版本，汇总所有修复建议 |

---

*文档结束*
