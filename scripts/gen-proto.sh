#!/bin/bash
# yuleDKCS Proto 代码生成脚本
# 用法: ./scripts/gen-proto.sh [ios|android|all]
# 依赖: protoc, protoc-gen-swift, protoc-gen-grpc-swift
#
# Proto 文件位置:
#   backend/cloud/hub/api/v1/hub.proto        (package digitalkey.hub.v1)
#   backend/cloud/hub/api/relay/v1/relay.proto (package digitalkey.relay.v1)
#   api/sdk/v1/sdk.proto                       (package digitalkey.sdk.v1)
#
# 三个 proto 互相独立（无 import 关系），可分别编译。
# iOS: 生成 Swift gRPC stubs → mobile/ios/Sources/YDKProto/
# Android: 由 Gradle protobuf 插件自动处理，本脚本仅做参考

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HUB_PROTO_DIR="$ROOT/backend/cloud/hub/api"
SDK_PROTO_DIR="$ROOT/api/sdk/v1"

ios_gen() {
    echo ">>> Generating iOS Swift proto stubs..."
    OUT="$ROOT/mobile/ios/Sources/YDKProto"
    ARGS="--swift_opt=Visibility=Public --grpc-swift_opt=Visibility=Public"

    # hub.proto (KeyManagementService / KeyShareService / VehicleControlService)
    protoc $ARGS --swift_out="$OUT" --grpc-swift_out="$OUT" \
        -I="$HUB_PROTO_DIR/v1" \
        "$HUB_PROTO_DIR/v1/hub.proto"

    # relay.proto (Mailbox API)
    protoc $ARGS --swift_out="$OUT" --grpc-swift_out="$OUT" \
        -I="$HUB_PROTO_DIR/relay/v1" \
        "$HUB_PROTO_DIR/relay/v1/relay.proto"

    # sdk.proto (BLE / MailboxClient / Callback / KeyManager)
    protoc $ARGS --swift_out="$OUT" --grpc-swift_out="$OUT" \
        -I="$SDK_PROTO_DIR" \
        "$SDK_PROTO_DIR/sdk.proto"

    echo "    ✓ $OUT ($(ls "$OUT"/*.swift 2>/dev/null | wc -l) files)"
}

android_gen() {
    echo ">>> Android: handled by Gradle protobuf plugin"
    echo "    Run: cd mobile/android && ./gradlew :sdk:generateProto"
    echo "    Generated to: sdk/build/generated/source/proto/"
}

case "${1:-all}" in
    ios)     ios_gen ;;
    android) android_gen ;;
    all)     ios_gen; android_gen ;;
    *)       echo "Usage: $0 [ios|android|all]"; exit 1 ;;
esac

echo "Done."
