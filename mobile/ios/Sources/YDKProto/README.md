# YDKProto — 由 proto 代码生成

## 生成命令

```bash
# 在 mobile/ios/ 目录下执行
cd mobile/ios

protoc \
  --swift_opt=Visibility=Public \
  --swift_out=./Sources/YDKProto \
  --grpc-swift_opt=Visibility=Public \
  --grpc-swift_out=./Sources/YDKProto \
  -I=../../api \
  ../../api/v1/hub.proto \
  ../../api/relay/v1/relay.proto \
  ../../api/sdk/v1/sdk.proto
```

## 输出

```
Sources/YDKProto/
├── hub.pb.swift         # Hub proto 消息类型
├── hub.grpc.swift       # Hub gRPC client stub
├── relay.pb.swift       # Relay proto 消息类型
├── relay.grpc.swift     # Relay gRPC client stub
├── sdk.pb.swift         # SDK proto 消息类型 (BLE/Mailbox/Callback/KeyManager)
└── sdk.grpc.swift       # 仅 HubService 生成 gRPC stub
```

## 注意

每次 `api/*.proto` 文件变更后，都需要重新生成。
CI 会自动检查 proto 变更并重新生成。
