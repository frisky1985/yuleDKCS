# yuleDKCS SDK Android 代码生成

proto 文件位于 `api/`，Gradle 构建时自动生成 Kotlin/Java stub。

## 生成方式

proto 源文件通过 Gradle 的 protobuf 插件在 `sdk/` 模块的 `src/main/proto/` 下引用：

```bash
# 创建软链接指向 api 目录（只需要做一次）
cd mobile/android/sdk/src/main/proto
ln -sf ../../../../../../api/v1/hub.proto .
ln -sf ../../../../../../api/relay/v1/relay.proto .
ln -sf ../../../../../../backend/cloud/protocol/sdk/v1/sdk.proto .
```

编译时自动生成到 `build/generated/source/proto/`。

## 手动生成（不运行 Gradle）

```bash
cd mobile/android
protoc \
  --kotlin_out=sdk/src/main/kotlin \
  --grpc-kotlin_out=sdk/src/main/kotlin \
  -I=../../api \
  ../../api/v1/hub.proto \
  ../../api/relay/v1/relay.proto \
  ../../backend/cloud/protocol/sdk/v1/sdk.proto
```

## 注意

需要安装 protoc + protoc-gen-grpc-kotlin。
建议直接用 Gradle 的 protobuf 插件管理。
