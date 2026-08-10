pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "yuleDKCS-DemoApp"

include(":app")
include(":sdk")

// 本地路径引用 yuleDKCS SDK module（发布后替换为 Maven 坐标）
project(":sdk").projectDir = File(rootDir, "../../../mobile/android/sdk")
