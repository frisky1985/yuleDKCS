// Android Digital Key App - Gradle构建配置
// 版本: 1.0.0

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.kapt")
    id("com.google.dagger.hilt.android")
    id("org.jetbrains.kotlin.plugin.serialization")
}

android {
    namespace = "com.digitalkey.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.digitalkey.app"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // 支持中文资源
        resourceConfigurations += listOf("zh")
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isMinifyEnabled = false
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        buildConfig = true
        viewBinding = true
    }

    testOptions {
        unitTests {
            isIncludeAndroidResources = true
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

// Kotlin编译器选项
kapt {
    correctErrorTypes = true
    useBuildCache = true
}

// 依赖配置
dependencies {
    // ==================== AndroidX Core ====================
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.activity:activity-ktx:1.8.2")
    implementation("androidx.fragment:fragment-ktx:1.6.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-livedata-ktx:2.7.0")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")
    implementation("androidx.coordinatorlayout:coordinatorlayout:1.2.0")

    // ==================== Material Design 3 ====================
    implementation("com.google.android.material:material:1.11.0")

    // ==================== Kotlin ====================
    implementation("org.jetbrains.kotlin:kotlin-stdlib:1.9.22")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.2")

    // ==================== Hilt依赖注入 ====================
    implementation("com.google.dagger:hilt-android:2.50")
    kapt("com.google.dagger:hilt-android-compiler:2.50")

    // ==================== 网络层 ====================
    implementation("com.squareup.retrofit2:retrofit:2.9.0")
    implementation("com.squareup.retrofit2:converter-gson:2.9.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("com.squareup.okhttp3:logging-interceptor:4.12.0")
    implementation("com.jakewharton.retrofit:retrofit2-kotlinx-serialization-converter:1.0.0")

    // ==================== 安全 ====================
    implementation("androidx.security:security-crypto:1.1.0-alpha06")
    implementation("androidx.biometric:biometric:1.1.0")

    // ==================== 图片加载 ====================
    implementation("io.coil-kt:coil:2.5.0")

    // ==================== RecyclerView ====================
    implementation("androidx.recyclerview:recyclerview:1.3.2")

    // ==================== 本地SDK依赖 (项目内部) ====================
    implementation(project(":sdk"))

    // ==================== 测试 ====================
    // ---- 单元测试 (JVM) ----
    testImplementation("junit:junit:4.13.2")
    // JUnit 5 (Jupiter)
    testImplementation("org.junit.jupiter:junit-jupiter:5.10.2")
    testImplementation("org.junit.vintage:junit-vintage-engine:5.10.2")
    // MockK - Kotlin Mock 框架
    testImplementation("io.mockk:mockk:1.13.9")
    // Coroutines 测试
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.7.3")
    // Robolectric - 无需真机的 Android 测试
    testImplementation("org.robolectric:robolectric:4.12")
    // OkHttp MockWebServer - HTTP 接口 Mock
    testImplementation("com.squareup.okhttp3:mockwebserver:4.12.0")
    // Google Truth - 可读性更强的断言
    testImplementation("com.google.truth:truth:1.4.2")

    // Hilt 测试支持
    testImplementation("com.google.dagger:hilt-android-testing:2.50")
    kaptTest("com.google.dagger:hilt-android-compiler:2.50")
    testAnnotationProcessor("com.google.dagger:hilt-android-compiler:2.50")

    // ---- 插桩测试 (Android设备/模拟器) ----
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
    androidTestImplementation("androidx.test.espresso:espresso-contrib:3.5.1")
    androidTestImplementation("androidx.test.espresso:espresso-intents:3.5.1")
    // Fragment 测试
    androidTestImplementation("androidx.fragment:fragment-testing:1.6.2")
    // Navigation 测试
    androidTestImplementation("androidx.navigation:navigation-testing:2.7.7")
}
