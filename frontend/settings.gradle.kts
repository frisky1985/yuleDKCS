pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
    plugins {
        id("com.android.application") version "8.2.2"
        id("com.android.library") version "8.2.2"
        id("org.jetbrains.kotlin.android") version "1.9.22"
        id("org.jetbrains.kotlin.kapt") version "1.9.22"
        id("com.google.dagger.hilt.android") version "2.50"
        id("org.jetbrains.kotlin.plugin.serialization") version "1.9.22"
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "digitalkey-android"

include(":sdk")
include(":app")

project(":sdk").projectDir = File("android")
project(":app").projectDir = File("android-app")
