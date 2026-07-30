#!/bin/bash
# yuleDKCS Proto 代码生成脚本
# 用法: ./scripts/gen-proto.sh [ios|android|all]
# 依赖: protoc, protoc-gen-swift, protoc-gen-grpc-swift, protoc-gen-grpc-kotlin

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROTO_DIR="$ROOT/api"

ios_gen() {
    echo ">>> Generating iOS Swift proto stubs..."
    OUT="$ROOT/mobile/ios/Sources/YDKProto"
    protoc \
        --swift_opt=Visibility=Public \
        --swift_out="$OUT" \
        --grpc-swift_opt=Visibility=Public \
        --grpc-swift_out="$OUT" \
        -I="$PROTO_DIR" \
        "$PROTO_DIR/v1/hub.proto" \
        "$PROTO_DIR/relay/v1/relay.proto" \
        "$PROTO_DIR/sdk/v1/sdk.proto"
    echo "    ✓ $OUT"
}

android_gen() {
    echo ">>> Generating Android Kotlin proto stubs..."
    OUT="$ROOT/mobile/android/sdk/src/main/kotlin"
    protoc \
        --kotlin_out="$OUT" \
        --grpc-kotlin_out="$OUT" \
        -I="$PROTO_DIR" \
        "$PROTO_DIR/v1/hub.proto" \
        "$PROTO_DIR/relay/v1/relay.proto" \
        "$PROTO_DIR/sdk/v1/sdk.proto"
    echo "    ✓ $OUT"
}

case "${1:-all}" in
    ios)     ios_gen ;;
    android) android_gen ;;
    all)     ios_gen; android_gen ;;
    *)       echo "Usage: $0 [ios|android|all]"; exit 1 ;;
esac

echo "Done."
