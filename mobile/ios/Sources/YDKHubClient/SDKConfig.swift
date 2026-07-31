import Foundation

/// yuleDKCS SDK 配置
public struct SDKConfig {
    let hubEndpoint: String
    let hubPort: Int
    let platform: Platform
    let enableLogging: Bool

    public init(
        hubEndpoint: String,
        hubPort: Int = 9090,
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
public enum Platform: Int {
    case unspecified = 0
    case iOS = 1
    case android = 2
    case harmony = 3
}
