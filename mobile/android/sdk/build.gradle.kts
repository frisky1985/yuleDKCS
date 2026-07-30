plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.jvm")
    id("com.google.protobuf")
}

android {
    namespace = "com.yuledkcs.sdk"
    compileSdk = 35

    defaultConfig {
        minSdk = 26  // Android 8.0
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    // gRPC
    implementation("io.grpc:grpc-kotlin-stub:1.4.1")
    implementation("io.grpc:grpc-okhttp:1.66.0")
    implementation("io.grpc:grpc-protobuf-lite:1.66.0")
    implementation("com.google.protobuf:protobuf-kotlin-lite:4.29.3")

    // Test
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
    testImplementation("io.mockk:mockk:1.13.13")
    testImplementation("junit:junit:4.13.2")
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:4.29.3"
    }
    plugins {
        create("grpc") {
            artifact = "io.grpc:protoc-gen-grpc-java:1.66.0"
        }
        create("grpckt") {
            artifact = "io.grpc:protoc-gen-grpc-kotlin:1.4.1"
        }
    }
    generateProtoTasks {
        all().configureEach {
            plugins {
                create("grpc") {}
                create("grpckt") {}
            }
        }
    }
}
