plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.yourcompany.demo"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.yourcompany.demo"
        minSdk = 26   // 与 SDK 一致 (Android 8.0)
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    // yuleDKCS SDK（本地 module）
    implementation(project(":sdk"))

    // 协程 + lifecycle（示例演示用）
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.0")
}
