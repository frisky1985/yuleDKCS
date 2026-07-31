import Foundation

/// yuleDKCS SDK 配置
public struct SDKConfig {
    /// Hub REST Gateway 主机地址（如 "hub.yuletech.com"）
    let hubEndpoint: String
    /// Hub REST Gateway 端口（默认 8080）
    let hubPort: Int
    /// 手机平台（SDK 自动检测）
    let platform: Platform
    /// 是否启用日志
    let enableLogging: Bool

    public init(
        hubEndpoint: String,
        hubPort: Int = 8080,
        platform: Platform = .iOS,
        enableLogging: Bool = false
    ) {
        self.hubEndpoint = hubEndpoint
        self.hubPort = hubPort
        self.platform = platform
        self.enableLogging = enableLogging
    }
}

/// 手机平台
public enum Platform: Int, Codable {
    case unspecified = 0
    case iOS = 1
    case android = 2
    case harmony = 3
}
