#!/bin/bash
# yuleDKCS Proto 代码生成脚本
#
# 移动端架构变化: 2026-07-31
#   手机 SDK 不再使用 gRPC stubs（grpc-swift 2.x 强制 iOS 18+）。
#   改用 HTTP/JSON 调用 Hub REST Gateway (:8080)。
#   Proto 仅作为 JSON 字段名和类型的合约参考。
#
# 因此本脚本已废弃（保留以备未来可能恢复 gRPC）。
# 后端 Go proto 由 Go build 系统自动处理（go generate）。
#
# 用法: 已废弃，仅作参考
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HUB_PROTO_DIR="$ROOT/backend/cloud/hub/api"
SDK_PROTO_DIR="$ROOT/backend/cloud/protocol/sdk/v1"

ios_gen() {
    echo "[DEPRECATED] iOS SDK 使用 HTTP/JSON，不生成 gRPC stubs"
    echo "  Proto 合约参考: backend/cloud/protocol/sdk/v1/sdk.proto"
    echo "  Proto 合约参考: $HUB_PROTO_DIR/v1/hub.proto"
    echo "  Proto 合约参考: $HUB_PROTO_DIR/relay/v1/relay.proto"
    echo "  如需手动生成消息类型（仅供文档参考）:"
    echo "    protoc --swift_opt=Visibility=Public --swift_out=./Sources/YDKProto \\"
    echo "      -I=$HUB_PROTO_DIR/v1 \$HUB_PROTO_DIR/v1/hub.proto"
}

android_gen() {
    echo "[DEPRECATED] Android SDK 使用 HTTP/JSON + Gson，不生成 gRPC stubs"
    echo "  JSON 字段名与 proto field names 手动对齐"
}

case "${1:-}" in
    ios)     ios_gen ;;
    android) android_gen ;;
    all|*)   ios_gen; android_gen ;;
esac
