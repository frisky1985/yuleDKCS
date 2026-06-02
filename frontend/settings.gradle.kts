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

rootProject.name = "digitalkey-android"

include(":sdk")
include(":app")

project(":sdk").projectDir = File("android")
project(":app").projectDir = File("android-app")
