plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.jvm")
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
    // HTTP 客户端
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    // SSE 支持
    implementation("com.squareup.okhttp3:okhttp-sse:4.12.0")
    // JSON 序列化
    implementation("com.google.code.gson:gson:2.11.0")
    // 协程
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")

    // 测试
    testImplementation("com.squareup.okhttp3:mockwebserver:4.12.0")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
    testImplementation("junit:junit:4.13.2")
}
