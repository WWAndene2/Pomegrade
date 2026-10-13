// Dolphin (GameCube and Wii emulator, GPLv2 or later) built as a library module of the app, as Azahar and
// Eden are. Sources live in /dolphin at the repository root (Dolphin 2606a); its Externals are git
// submodules there. See /README.md.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.serialization)
}

val dolphinRoot = file("../../dolphin")
val dolphinAndroid = file("../../dolphin/Source/Android/app/src/main")

android {
    namespace = "org.dolphinemu.dolphinemu"
    compileSdk = AppConfig.compileSdkVersion
    // the NDK Dolphin 2606a is developed against: with 28.2, its sources don't compile (newer libc++ needs
    // complete types where Dolphin destroys unique_ptrs of forward-declared ones, e.g. DSPCaptureLogger.h)
    ndkVersion = "29.0.14206865"

    defaultConfig {
        minSdk = AppConfig.minSdkVersion

        ndk {
            // Dolphin supports arm64-v8a and x86_64 only; phones are arm64
            abiFilters += AppConfig.abis(listOf("arm64-v8a"))
        }

        externalNativeBuild {
            cmake {
                // Dolphin's own Android arguments (Source/Android/app/build.gradle.kts), without its unit
                // tests and without the GBA link cable (libmgba: Pomegrade has its own GBA core)
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DENABLE_TESTS=OFF",
                    "-DUSE_MGBA=OFF",
                )
                arguments += AppConfig.ccacheCmakeArguments
                arguments += AppConfig.nativeBuildTypeCmakeArguments
            }
        }

        // what Dolphin's own build defines for its sources
        resValue("string", "app_name_suffixed", "Dolphin")
        buildConfigField("String", "GIT_HASH", "\"2606a\"")
        buildConfigField("String", "BRANCH", "\"2606a\"")
        buildConfigField("String", "VERSION_NAME", "\"2606a\"")
    }

    buildFeatures {
        viewBinding = true
        buildConfig = true
        resValues = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1+"
            path = file("$dolphinRoot/CMakeLists.txt")
        }
    }

    sourceSets {
        named("main") {
            manifest.srcFile("$dolphinAndroid/AndroidManifest.xml")
            java.directories += "$dolphinAndroid/java"
            kotlin.directories += "$dolphinAndroid/java"
            res.directories += "$dolphinAndroid/res"
            // Data/Sys, copied there by Dolphin's CMake (Source/Android/jni/CMakeLists.txt)
            assets.directories += "$dolphinAndroid/assets"
        }
    }

    lint {
        abortOnError = false
    }
}

kotlin {
    compilerOptions {
        jvmTarget = JvmTarget.JVM_21
    }
}

dependencies {
    // Pomegrade: the in-game menu shared with the other consoles
    implementation(projects.emulatorUi)
    // Dolphin's own dependencies (Source/Android/gradle/libs.versions.toml), without the baseline profile.
    // Those other modules also use come from the version catalogue (gradle/libs.versions.toml): the APK
    // holds one version of each, so each module compiles against the one it runs with
    implementation(libs.androidx.core)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.cardview)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.fragment)
    implementation(libs.androidx.slidingpanelayout)
    implementation(libs.android.material)
    implementation(libs.androidx.splashscreen)
    implementation(libs.androidx.preference)
    implementation(libs.androidx.lifecycle.viewmodel)
    implementation(libs.androidx.lifecycle.runtime)
    implementation("androidx.leanback:leanback:1.2.0")
    implementation("androidx.tvprovider:tvprovider:1.1.0")
    implementation(libs.androidx.swiperefreshlayout)
    implementation(libs.coil.view)
    implementation(libs.kotlin.serialization)
    implementation(libs.kotlinx.coroutines.android)
    implementation("com.nononsenseapps:filepicker:4.2.1")
}
