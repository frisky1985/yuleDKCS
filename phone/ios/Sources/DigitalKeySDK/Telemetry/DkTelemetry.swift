// DkTelemetry.swift - 统一埋点接口 - iOS SDK端
// 三端统一埋点规范

import Foundation
import UIKit

// MARK: - Event Type

/// 事件类型
public enum DkEventType {
    // App事件
    public static let appLaunch = "app_launch"
    public static let appBackground = "app_background"
    public static let sdkInit = "sdk_init"
    public static let sdkError = "sdk_error"
    
    // 用户事件
    public static let userLogin = "user_login"
    public static let userLogout = "user_logout"
    
    // 设备事件
    public static let deviceBindStart = "device_bind_start"
    public static let deviceBindSuccess = "device_bind_success"
    public static let deviceBindFailed = "device_bind_failed"
    public static let deviceUnbind = "device_unbind"
    
    // 密钥事件
    public static let keyCreate = "key_create"
    public static let keyDelete = "key_delete"
    public static let keyRefresh = "key_refresh"
    public static let keyUse = "key_use"
    public static let keyExpired = "key_expired"
    
    // 车辆事件
    public static let vehicleUnlock = "vehicle_unlock"
    public static let vehicleLock = "vehicle_lock"
    public static let vehicleStart = "vehicle_start"
    public static let vehicleStop = "vehicle_stop"
    
    // 分享事件
    public static let shareCreate = "share_create"
    public static let shareAccept = "share_accept"
    public static let shareRevoke = "share_revoke"
    
    // 通道事件
    public static let bleScan = "ble_scan"
    public static let bleConnect = "ble_connect"
    public static let bleDisconnect = "ble_disconnect"
    public static let nfcTap = "nfc_tap"
    public static let uwbRanging = "uwb_ranging"
    public static let channelSwitch = "channel_switch"
    
    // 协议事件
    public static let protocolMsg = "protocol_msg"
    
    // 安全事件
    public static let securityAlert = "security_alert"
}

// MARK: - Event Source

/// 事件来源
public enum DkEventSource: String {
    case sdk = "SDK"
    case tcu = "TCU"
    case cloud = "CLOUD"
}

// MARK: - Telemetry Event

/// 埋点事件
public struct DkTelemetryEvent: Codable {
    public let eventId: String
    public let eventType: String
    public let timestamp: Int64
    public let source: String
    public let sessionId: String?
    public let userId: String?
    public let deviceId: String?
    public let vehicleId: String?
    public let keyId: String?
    public let traceId: String?
    public let properties: [String: AnyCodable]
    public let context: [String: AnyCodable]
    
    public init(eventType: String, source: DkEventSource = .sdk, sessionId: String?, userId: String?, deviceId: String?, vehicleId: String?, keyId: String?, traceId: String?, properties: [String: Any], context: [String: Any] = [:]) {
        self.eventId = UUID().uuidString
        self.eventType = eventType
        self.timestamp = Int64(Date().timeIntervalSince1970 * 1000)
        self.source = source.rawValue
        self.sessionId = sessionId
        self.userId = userId
        self.deviceId = deviceId
        self.vehicleId = vehicleId
        self.keyId = keyId
        self.traceId = traceId
        self.properties = properties.mapValues { AnyCodable($0) }
        self.context = context.mapValues { AnyCodable($0) }
    }
    
    public func toDictionary() -> [String: Any] {
        var dict: [String: Any] = [
            "event_id": eventId,
            "event_type": eventType,
            "timestamp": timestamp,
            "source": source
        ]
        
        if let sessionId = sessionId { dict["session_id"] = sessionId }
        if let userId = userId { dict["user_id"] = userId }
        if let deviceId = deviceId { dict["device_id"] = deviceId }
        if let vehicleId = vehicleId { dict["vehicle_id"] = vehicleId }
        if let keyId = keyId { dict["key_id"] = keyId }
        if let traceId = traceId { dict["trace_id"] = traceId }
        if !properties.isEmpty { dict["properties"] = properties.mapValues { $0.value } }
        if !context.isEmpty { dict["context"] = context.mapValues { $0.value } }
        
        return dict
    }
    
    public func toJsonString() -> String? {
        guard let data = try? JSONSerialization.data(withJSONObject: toDictionary()) else { return nil }
        return String(data: data, encoding: .utf8)
    }
}

/// Any类型Codable包装
public struct AnyCodable: Codable {
    public let value: Any
    
    public init(_ value: Any) {
        self.value = value
    }
    
    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        
        if let intValue = try? container.decode(Int.self) {
            value = intValue
        } else if let doubleValue = try? container.decode(Double.self) {
            value = doubleValue
        } else if let boolValue = try? container.decode(Bool.self) {
            value = boolValue
        } else if let stringValue = try? container.decode(String.self) {
            value = stringValue
        } else if let arrayValue = try? container.decode([AnyCodable].self) {
            value = arrayValue.map { $0.value }
        } else if let dictValue = try? container.decode([String: AnyCodable].self) {
            value = dictValue.mapValues { $0.value }
        } else {
            value = NSNull()
        }
    }
    
    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        
        switch value {
        case let intValue as Int:
            try container.encode(intValue)
        case let intValue as Int64:
            try container.encode(intValue)
        case let doubleValue as Double:
            try container.encode(doubleValue)
        case let boolValue as Bool:
            try container.encode(boolValue)
        case let stringValue as String:
            try container.encode(stringValue)
        case let arrayValue as [Any]:
            try container.encode(arrayValue.map { AnyCodable($0) })
        case let dictValue as [String: Any]:
            try container.encode(dictValue.mapValues { AnyCodable($0) })
        default:
            try container.encodeNil()
        }
    }
}

// MARK: - Telemetry Config

/// 埋点配置
public struct DkTelemetryConfig {
    public var endpoint: String = ""
    public var batchSize: Int = 10
    public var flushIntervalSeconds: TimeInterval = 5.0
    public var enableDebug: Bool = false
    public var sampleRate: Float = 1.0
    
    public init() {}
}

// MARK: - Telemetry

/// 统一埋点接口
public protocol DkTelemetryProtocol {
    func track(_ eventType: String, properties: [String: Any])
    func trackError(_ code: Int, errorMessage: String, context: [String: Any])
    func setUser(_ userId: String?)
    func setDevice(_ deviceId: String?)
    func setVehicle(_ vehicleId: String?)
    func setTraceId(_ traceId: String?)
    func flush()
}

// MARK: - DkTelemetry

/// 统一埋点实现
public final class DkTelemetry: DkTelemetryProtocol {
    
    // MARK: - Singleton
    
    public static let shared = DkTelemetry()
    
    // MARK: - Properties
    
    private var config: DkTelemetryConfig
    private var userId: String?
    private var deviceId: String?
    private var vehicleId: String?
    private var traceId: String?
    private var sessionId: String
    
    private let eventQueue = DispatchQueue(label: "com.digitalkey.telemetry.queue", attributes: .concurrent)
    private var events: [DkTelemetryEvent] = []
    private var flushTimer: Timer?
    private let lock = NSLock()
    private var eventHandlers: [DkEventHandler] = []
    
    // MARK: - Initialization
    
    private init() {
        self.config = DkTelemetryConfig()
        self.sessionId = UUID().uuidString
        self.deviceId = UIDevice.current.identifierForVendor?.uuidString
    }
    
    // MARK: - Configuration
    
    public func configure(_ config: DkTelemetryConfig) {
        lock.lock()
        defer { lock.unlock() }
        self.config = config
        
        // Restart timer with new config
        restartFlushTimer()
    }
    
    // MARK: - Tracking
    
    public func track(_ eventType: String, properties: [String: Any] = [:]) {
        // Sample check
        if Float.random(in: 0...1) > config.sampleRate { return }
        
        let event = DkTelemetryEvent(
            eventType: eventType,
            source: .sdk,
            sessionId: sessionId,
            userId: userId,
            deviceId: deviceId,
            vehicleId: vehicleId,
            keyId: nil,
            traceId: traceId,
            properties: properties,
            context: getCommonContext()
        )
        
        lock.lock()
        events.append(event)
        let count = events.count
        lock.unlock()
        
        if config.enableDebug {
            DkLogger.shared.debug("Event tracked: \(eventType)", tag: .telemetry, fields: ["event_id": event.eventId])
        }
        
        // Flush if batch size reached
        if count >= config.batchSize {
            flush()
        }
        
        // Notify handlers
        notifyHandlers(event)
    }
    
    public func trackError(_ code: Int, errorMessage: String, context: [String: Any] = [:]) {
        var props = context
        props["error_code"] = code
        props["error_code_hex"] = String(format: "0x%04X", code)
        props["error_message"] = errorMessage
        
        track(DkEventType.sdkError, properties: props)
    }
    
    // MARK: - Setters
    
    public func setUser(_ userId: String?) {
        lock.lock()
        defer { lock.unlock() }
        self.userId = userId
    }
    
    public func setDevice(_ deviceId: String?) {
        lock.lock()
        defer { lock.unlock() }
        self.deviceId = deviceId
    }
    
    public func setVehicle(_ vehicleId: String?) {
        lock.lock()
        defer { lock.unlock() }
        self.vehicleId = vehicleId
    }
    
    public func setTraceId(_ traceId: String?) {
        lock.lock()
        defer { lock.unlock() }
        self.traceId = traceId
    }
    
    // MARK: - Flush
    
    public func flush() {
        lock.lock()
        guard !events.isEmpty else {
            lock.unlock()
            return
        }
        let eventsToSend = events
        events.removeAll()
        lock.unlock()
        
        sendEvents(eventsToSend)
    }
    
    private func sendEvents(_ events: [DkTelemetryEvent]) {
        if config.endpoint.isEmpty {
            // No endpoint configured, just print
            if config.enableDebug {
                for event in events {
                    if let json = event.toJsonString() {
                        print("[TELEMETRY] \(json)")
                    }
                }
            }
            return
        }
        
        // TODO: Implement actual network sending
        // You can use URLSession or any networking library here
        
        if config.enableDebug {
            DkLogger.shared.info("Sent \(events.count) events to \(config.endpoint)", tag: .telemetry)
        }
    }
    
    private func restartFlushTimer() {
        flushTimer?.invalidate()
        flushTimer = Timer.scheduledTimer(withTimeInterval: config.flushIntervalSeconds, repeats: true) { [weak self] _ in
            self?.flush()
        }
    }
    
    // MARK: - Common Context
    
    private func getCommonContext() -> [String: Any] {
        return [
            "app_version": Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "unknown",
            "os_version": UIDevice.current.systemVersion,
            "sdk_version": "1.0.0",
            "device_model": UIDevice.current.model,
            "manufacturer": "Apple"
        ]
    }
    
    // MARK: - Convenience Methods
    
    public func trackKeyUse(keyId: String, vehicleId: String, channel: String, durationMs: Int) {
        track(DkEventType.keyUse, properties: [
            "key_id": keyId,
            "vehicle_id": vehicleId,
            "channel_type": channel,
            "duration_ms": durationMs
        ])
    }
    
    public func trackVehicleCommand(action: String, vehicleId: String, result: String, errorCode: Int?) {
        var props: [String: Any] = [
            "action": action,
            "vehicle_id": vehicleId,
            "result": result
        ]
        if let code = errorCode {
            props["error_code"] = code
        }
        track(DkEventType.protocolMsg, properties: props)
    }
    
    public func trackSecurityEvent(eventType: String, threatLevel: Int, details: [String: Any]) {
        var props = details
        props["security_event_type"] = eventType
        props["threat_level"] = threatLevel
        track(DkEventType.securityAlert, properties: props)
    }
    
    public func trackBleConnect(deviceId: String, macAddress: String, mtu: Int, success: Bool) {
        track(DkEventType.bleConnect, properties: [
            "device_id": deviceId,
            "mac_address": macAddress,
            "mtu": mtu,
            "success": success
        ])
    }
    
    public func trackNfcTap(tagId: String, vehicleId: String, success: Bool) {
        track(DkEventType.nfcTap, properties: [
            "nfc_tag_id": tagId,
            "vehicle_id": vehicleId,
            "success": success
        ])
    }
    
    public func trackUwbRanging(vehicleId: String, distanceMm: Int, accuracyMm: Int) {
        track(DkEventType.uwbRanging, properties: [
            "vehicle_id": vehicleId,
            "distance_mm": distanceMm,
            "accuracy_mm": accuracyMm
        ])
    }
    
    // MARK: - Event Handlers
    
    public func addHandler(_ handler: DkEventHandler) {
        lock.lock()
        defer { lock.unlock() }
        eventHandlers.append(handler)
    }
    
    public func removeHandler(_ handler: DkEventHandler) {
        lock.lock()
        defer { lock.unlock() }
        eventHandlers.removeAll { $0 === handler }
    }
    
    private func notifyHandlers(_ event: DkTelemetryEvent) {
        lock.lock()
        let handlers = eventHandlers
        lock.unlock()
        
        for handler in handlers {
            handler.onEvent(event)
        }
    }
    
    // MARK: - Cleanup
    
    public func release() {
        flushTimer?.invalidate()
        flushTimer = nil
        flush()
    }
}

// MARK: - Event Handler

/// 事件处理器协议
public protocol DkEventHandler: AnyObject {
    func onEvent(_ event: DkTelemetryEvent)
}
