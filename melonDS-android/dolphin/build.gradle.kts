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
    // Dolphin's own dependencies (Source/Android/gradle/libs.versions.toml), without the baseline profile
    implementation("androidx.core:core-ktx:1.18.0")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.cardview:cardview:1.0.0")
    implementation("androidx.recyclerview:recyclerview:1.4.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.1")
    implementation("androidx.fragment:fragment-ktx:1.8.9")
    implementation("androidx.slidingpanelayout:slidingpanelayout:1.2.0")
    implementation("com.google.android.material:material:1.13.0")
    implementation("androidx.core:core-splashscreen:1.2.0")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.10.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.10.0")
    implementation("androidx.leanback:leanback:1.2.0")
    implementation("androidx.tvprovider:tvprovider:1.1.0")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.2.0")
    implementation("io.coil-kt:coil:2.7.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.10.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    implementation("com.nononsenseapps:filepicker:4.2.1")
}
