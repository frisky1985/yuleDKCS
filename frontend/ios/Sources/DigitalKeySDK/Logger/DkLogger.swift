// DkLogger.swift - 统一日志接口 - iOS SDK端
// 三端统一日志规范

import Foundation
import os.log

// MARK: - Log Level

/// 日志级别
public enum DkLogLevel: Int, Comparable {
    case trace = 0
    case debug = 1
    case info = 2
    case warn = 3
    case error = 4
    case fatal = 5
    
    public var osLogType: OSLogType {
        switch self {
        case .trace: return .debug
        case .debug: return .debug
        case .info: return .info
        case .warn: return .default
        case .error: return .error
        case .fatal: return .fault
        }
    }
    
    public var name: String {
        switch self {
        case .trace: return "TRACE"
        case .debug: return "DEBUG"
        case .info: return "INFO"
        case .warn: return "WARN"
        case .error: return "ERROR"
        case .fatal: return "FATAL"
        }
    }
    
    public static func < (lhs: DkLogLevel, rhs: DkLogLevel) -> Bool {
        return lhs.rawValue < rhs.rawValue
    }
}

// MARK: - Log Tag

/// 日志模块标签
public enum DkLogTag: String {
    case initTag = "INIT"
    case keyMgr = "KEYMGR"
    case auth = "AUTH"
    case ble = "BLE"
    case nfc = "NFC"
    case uwb = "UWB"
    case sec = "SEC"
    case transport = "TRANSPORT"
    case proto = "PROTO"
    case service = "SERVICE"
    case adapter = "ADAPTER"
    case telemetry = "TELEMETRY"
}

// MARK: - Log Entry

/// 日志条目
public struct LogEntry {
    public let timestamp: Date
    public let level: DkLogLevel
    public let tag: String
    public let message: String
    public let traceId: String?
    public let fields: [String: Any]
    public let file: String
    public let function: String
    public let line: Int
    
    public init(level: DkLogLevel, tag: String, message: String, traceId: String?, fields: [String: Any], file: String, function: String, line: Int) {
        self.timestamp = Date()
        self.level = level
        self.tag = tag
        self.message = message
        self.traceId = traceId
        self.fields = fields
        self.file = file
        self.function = function
        self.line = line
    }
}

// MARK: - Logger Config

/// 日志配置
public struct DkLoggerConfig {
    public var minLevel: DkLogLevel = .debug
    public var enableJson: Bool = true
    public var enableTimestamp: Bool = true
    public var enableTraceId: Bool = true
    public var subsystem: String = "com.digitalkey.sdk"
    
    public init() {}
}

// MARK: - DkLogger

/// 统一日志器
public final class DkLogger {
    
    // MARK: - Singleton
    
    public static let shared = DkLogger()
    
    // MARK: - Properties
    
    private var config: DkLoggerConfig
    private var traceId: String?
    private let dateFormatter: DateFormatter
    private let lock = NSLock()
    private var subscribers: [LogSubscriber] = []
    
    // OSLog instances for each tag
    private var osLogCache: [String: OSLog] = [:]
    
    // MARK: - Initialization
    
    private init() {
        self.config = DkLoggerConfig()
        self.dateFormatter = DateFormatter()
        self.dateFormatter.dateFormat = "yyyy-MM-dd'T'HH:mm:ss.SSS"
    }
    
    // MARK: - Configuration
    
    public func configure(_ config: DkLoggerConfig) {
        lock.lock()
        defer { lock.unlock() }
        self.config = config
    }
    
    public func setTraceId(_ traceId: String?) {
        lock.lock()
        defer { lock.unlock() }
        self.traceId = traceId
    }
    
    public func getTraceId() -> String? {
        lock.lock()
        defer { lock.unlock() }
        return traceId
    }
    
    public func generateTraceId() -> String {
        let tid = "SDK-\(Int(Date().timeIntervalSince1970))-\(Int.random(in: 1000...9999))"
        setTraceId(tid)
        return tid
    }
    
    // MARK: - Logging Methods
    
    public func trace(_ message: String, tag: DkLogTag = .initTag, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        log(level: .trace, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: fields)
    }
    
    public func debug(_ message: String, tag: DkLogTag = .initTag, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        log(level: .debug, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: fields)
    }
    
    public func info(_ message: String, tag: DkLogTag = .initTag, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        log(level: .info, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: fields)
    }
    
    public func warn(_ message: String, tag: DkLogTag = .initTag, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        log(level: .warn, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: fields)
    }
    
    public func error(_ message: String, tag: DkLogTag = .initTag, error: Error? = nil, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        var allFields = fields
        if let error = error {
            allFields["error"] = error.localizedDescription
            allFields["error_code"] = (error as? DigitalKeyError)?.hexCode
        }
        log(level: .error, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: allFields)
    }
    
    public func fatal(_ message: String, tag: DkLogTag = .initTag, file: String = #file, function: String = #function, line: Int = #line, fields: [String: Any] = [:]) {
        log(level: .fatal, tag: tag.rawValue, message: message, file: file, function: function, line: line, fields: fields)
    }
    
    // MARK: - Core Logging
    
    private func log(level: DkLogLevel, tag: String, message: String, file: String, function: String, line: Int, fields: [String: Any]) {
        guard level >= config.minLevel else { return }
        
        lock.lock()
        let currentTraceId = traceId
        lock.unlock()
        
        var allFields = fields
        allFields["file"] = (file as NSString).lastPathComponent
        allFields["function"] = function
        allFields["line"] = line
        
        let entry = LogEntry(
            level: level,
            tag: tag,
            message: message,
            traceId: currentTraceId,
            fields: allFields,
            file: file,
            function: function,
            line: line
        )
        
        if config.enableJson {
            outputJson(entry)
        } else {
            outputText(entry)
        }
        
        // Output to OSLog
        outputOsLog(entry)
        
        // Notify subscribers
        notifySubscribers(entry)
    }
    
    private func outputJson(_ entry: LogEntry) {
        var dict: [String: Any] = [
            "timestamp": dateFormatter.string(from: entry.timestamp),
            "level": entry.level.name,
            "tag": entry.tag,
            "message": entry.message
        ]
        
        if config.enableTraceId, let traceId = entry.traceId {
            dict["trace_id"] = traceId
        }
        
        if !entry.fields.isEmpty {
            dict["fields"] = entry.fields
        }
        
        if let jsonData = try? JSONSerialization.data(withJSONObject: dict),
           let jsonString = String(data: jsonData, encoding: .utf8) {
            print("[\(entry.tag)] \(jsonString)")
        }
    }
    
    private func outputText(_ entry: LogEntry) {
        var text = ""
        
        if config.enableTimestamp {
            text += "[\(dateFormatter.string(from: entry.timestamp))]"
        }
        
        text += "[\(entry.level.name)][\(entry.tag)]"
        
        if config.enableTraceId, let traceId = entry.traceId {
            text += "[T:\(traceId)]"
        }
        
        text += " \(entry.message)"
        
        for (key, value) in entry.fields {
            text += " \(key)=\(value)"
        }
        
        print(text)
    }
    
    private func outputOsLog(_ entry: LogEntry) {
        let osLog = getOsLog(for: entry.tag)
        
        if entry.fields.isEmpty {
            os_log("%{public}@", log: osLog, type: entry.level.osLogType, entry.message)
        } else {
            let fieldsString = entry.fields.map { "\($0.key)=\($0.value)" }.joined(separator: ", ")
            os_log("%{public}@ | %{public}@", log: osLog, type: entry.level.osLogType, entry.message, fieldsString)
        }
    }
    
    private func getOsLog(for tag: String) -> OSLog {
        if let cached = osLogCache[tag] {
            return cached
        }
        
        let osLog = OSLog(subsystem: config.subsystem, category: tag)
        osLogCache[tag] = osLog
        return osLog
    }
    
    private func notifySubscribers(_ entry: LogEntry) {
        for subscriber in subscribers {
            subscriber.onLog(entry)
        }
    }
    
    // MARK: - Convenience Methods
    
    public func keyMgr(_ message: String, fields: [String: Any] = [:]) {
        info(message, tag: .keyMgr, fields: fields)
    }
    
    public func auth(_ message: String, fields: [String: Any] = [:]) {
        info(message, tag: .auth, fields: fields)
    }
    
    public func ble(_ message: String, fields: [String: Any] = [:]) {
        info(message, tag: .ble, fields: fields)
    }
    
    public func nfc(_ message: String, fields: [String: Any] = [:]) {
        info(message, tag: .nfc, fields: fields)
    }
    
    public func sec(_ message: String, fields: [String: Any] = [:]) {
        info(message, tag: .sec, fields: fields)
    }
    
    public func proto(_ message: String, fields: [String: Any] = [:]) {
        debug(message, tag: .proto, fields: fields)
    }
    
    // MARK: - Error Logging
    
    public func keyMgrE(_ message: String, error: Error? = nil, fields: [String: Any] = [:]) {
        error(message, tag: .keyMgr, error: error, fields: fields)
    }
    
    public func authE(_ message: String, error: Error? = nil, fields: [String: Any] = [:]) {
        error(message, tag: .auth, error: error, fields: fields)
    }
    
    public func bleE(_ message: String, error: Error? = nil, fields: [String: Any] = [:]) {
        error(message, tag: .ble, error: error, fields: fields)
    }
    
    public func secE(_ message: String, error: Error? = nil, fields: [String: Any] = [:]) {
        error(message, tag: .sec, error: error, fields: fields)
    }
    
    // MARK: - Subscribers
    
    public func subscribe(_ subscriber: LogSubscriber) {
        subscribers.append(subscriber)
    }
    
    public func unsubscribe(_ subscriber: LogSubscriber) {
        subscribers.removeAll { $0 === subscriber }
    }
}

// MARK: - Log Subscriber

/// 日志订阅者协议
public protocol LogSubscriber: AnyObject {
    func onLog(_ entry: LogEntry)
}

// MARK: - Field Builders

/// 日志字段构建
public func DkField(_ key: String, _ value: Any?) -> [String: Any] {
    return [key: value ?? "null"]
}

public func DkField_traceId(_ value: String) -> [String: Any] {
    return ["trace_id": value]
}

public func DkField_errorCode(_ code: UInt16) -> [String: Any] {
    return ["error_code": String(format: "0x%04X", code)]
}

public func DkField_duration(_ ms: Int) -> [String: Any] {
    return ["duration_ms": ms]
}

public func DkField_keyId(_ id: String) -> [String: Any] {
    return ["key_id": id]
}

public func DkField_vehicleId(_ id: String) -> [String: Any] {
    return ["vehicle_id": id]
}

public func DkField_userId(_ id: String) -> [String: Any] {
    return ["user_id": id]
}

public func DkField_deviceId(_ id: String) -> [String: Any] {
    return ["device_id": id]
}

public func DkField_channel(_ type: String) -> [String: Any] {
    return ["channel_type": type]
}
