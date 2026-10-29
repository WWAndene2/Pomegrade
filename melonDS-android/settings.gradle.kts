pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        // only the ADB library of the wireless debugging diagnostics and its SPAKE2 dependency, built by JitPack
        // from their GitHub tags
        maven("https://jitpack.io") {
            content {
                includeGroup("com.github.MuntashirAkon")
                includeGroup("com.github.MuntashirAkon.spake2-java")
            }
        }
    }
}

enableFeaturePreview("TYPESAFE_PROJECT_ACCESSORS")

include(":app")
include(":common")
include(":masterswitch")
include(":rcheevos-api")
include(":azahar")
include(":mgba")
include(":eden")
include(":dolphin")
include(":emulator-ui")