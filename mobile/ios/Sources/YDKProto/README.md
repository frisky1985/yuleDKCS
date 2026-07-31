# YDKProto — 由 proto 代码生成

## 生成命令

```bash
# 从仓库根目录运行
./scripts/gen-proto.sh ios
```

或者手动：

```bash
# hub.proto
protoc \
  --swift_opt=Visibility=Public \
  --swift_out=./Sources/YDKProto \
  --grpc-swift_opt=Visibility=Public \
  --grpc-swift_out=./Sources/YDKProto \
  -I=../../../../backend/cloud/hub/api/v1 \
  ../../../../backend/cloud/hub/api/v1/hub.proto

# relay.proto
protoc \
  --swift_opt=Visibility=Public \
  --swift_out=./Sources/YDKProto \
  --grpc-swift_opt=Visibility=Public \
  --grpc-swift_out=./Sources/YDKProto \
  -I=../../../../backend/cloud/hub/api/relay/v1 \
  ../../../../backend/cloud/hub/api/relay/v1/relay.proto

# sdk.proto
protoc \
  --swift_opt=Visibility=Public \
  --swift_out=./Sources/YDKProto \
  --grpc-swift_opt=Visibility=Public \
  --grpc-swift_out=./Sources/YDKProto \
  -I=../../../../api/sdk/v1 \
  ../../../../api/sdk/v1/sdk.proto
```

## 输出

```
Sources/YDKProto/
├── hub.pb.swift         # Hub proto 消息类型 + gRPC client stub
├── hub.grpc.swift       # Hub gRPC service stub
├── relay.pb.swift       # Relay proto 消息类型
├── relay.grpc.swift     # Relay gRPC service stub
├── sdk.pb.swift         # SDK proto 消息类型
└── sdk.grpc.swift       # SDK HubService gRPC stub (仅用于参考)
```

## 注意

每次 proto 文件变更后都需要重新生成。
