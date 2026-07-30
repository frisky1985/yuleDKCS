import Foundation
import GRPC
import NIO
import YDKProto

/// yuleDKCS Hub gRPC 客户端
///
/// 用法:
/// ```swift
/// let client = try YDKHubClient(config: SDKConfig(hubEndpoint: "hub.example.com"))
/// client.setToken("session-token-from-oem-server")
/// let key = try await client.bindKey(vehicleId: "LSV...")
/// ```
public final class YDKHubClient {

    // MARK: - 内部状态

    private let group: EventLoopGroup
    private let channel: GRPCChannel
    private let config: SDKConfig

    private lazy var keyManagement = KeyManagementServiceAsyncClient(
        channel: channel,
        interceptors: [AuthInterceptor(token: { [weak self] in self?.token })]
    )
    private lazy var keyShare = KeyShareServiceAsyncClient(
        channel: channel,
        interceptors: [AuthInterceptor(token: { [weak self] in self?.token })]
    )
    private lazy var vehicleControl = VehicleControlServiceAsyncClient(
        channel: channel,
        interceptors: [AuthInterceptor(token: { [weak self] in self?.token })]
    )

    private var token: String?

    // MARK: - 初始化

    public init(config: SDKConfig) throws {
        self.config = config
        self.group = PlatformSupport.makeEventLoopGroup(loopCount: 1)
        self.channel = try GRPCChannelPool.with(
            target: .host(config.hubEndpoint, port: config.hubPort),
            transportSecurity: .tls(GRPCTLSConfiguration.makeClientDefault()),
            eventLoopGroup: group
        )
    }

    deinit {
        try? channel.close().wait()
        try? group.syncShutdownGracefully()
    }

    // MARK: - Auth

    /// 设置用户 session token（从车厂 Server 获取后调用）
    public func setToken(_ token: String) {
        self.token = token
    }

    /// 清除 token（用户登出时调用）
    public func clearToken() {
        self.token = nil
    }
}
