import GRPC
import NIOHPACK

/// 向每个 gRPC 请求注入 Bearer token
final class AuthInterceptor: ClientInterceptor<Any, Any> {

    private let tokenProvider: () -> String?

    init(token: @escaping @autoclosure () -> String?) {
        self.tokenProvider = token
    }

    override func intercept<Request: GRPCProtobufRequest, Response: GRPCProtobufResponse>(
        method: GRPCMethodDescriptor,
        request: GRPCRequest<Request>,
        context: StatusCallContext,
        next: (GRPCRequest<Request>, StatusCallContext)
            -> EventLoopFuture<GRPCResponse<Response>>
    ) {
        var headers = request.metadata
        if let token = tokenProvider() {
            headers.replaceOrAdd(name: "authorization", value: "Bearer \(token)")
        }
        headers.replaceOrAdd(name: "x-sdk-version", value: "1.0.0")
        headers.replaceOrAdd(name: "x-platform", value: "ios")

        let newRequest = GRPCRequest<Request>(metadata: headers, message: request.message)
        return next(newRequest, context)
    }
}
