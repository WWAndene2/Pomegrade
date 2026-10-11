// Eden (Switch emulator, GPLv3 or later) built as a library module of the app, as Azahar is.
// Sources live in /eden at the repository root; see /README.md. Its native dependencies are fetched by CMake
// (CPM) into eden/.cache/cpm at configure time: the first build needs the network.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.parcelize)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.navigation.safeargs)
}

val edenRoot = file("../../eden")
val edenAndroid = file("../../eden/src/android/app/src/main")

android {
    namespace = "org.yuzu.yuzu_emu"
    compileSdk = AppConfig.compileSdkVersion
    // the NDK version Eden is developed against
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = AppConfig.minSdkVersion

        ndk {
            // the Switch core is built for arm64 only (dynarmic's arm64 backend, NCE)
            abiFilters += AppConfig.abis(listOf("arm64-v8a"))
        }

        externalNativeBuild {
            cmake {
                // Eden's own Android arguments (src/android/app/build.gradle.kts), without the update checker
                arguments(
                    "-DENABLE_QT=0",
                    "-DENABLE_WEB_SERVICE=1",
                    "-DANDROID_ARM_NEON=true",
                    "-DYUZU_USE_CPM=ON",
                    "-DCPMUTIL_FORCE_BUNDLED=ON",
                    "-DYUZU_USE_BUNDLED_FFMPEG=ON",
                    "-DBUILD_TESTING=OFF",
                    "-DYUZU_TESTS=OFF",
                    "-DDYNARMIC_TESTS=OFF",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                )
                arguments += AppConfig.ccacheCmakeArguments
                arguments += AppConfig.nativeBuildTypeCmakeArguments
                targets("yuzu-android")
            }
        }

        // what Eden's own build defines for its sources (flavour "mainline")
        resValue("string", "app_name_suffixed", "Eden")
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
            path = file("$edenRoot/CMakeLists.txt")
        }
    }

    sourceSets {
        named("main") {
            manifest.srcFile("$edenAndroid/AndroidManifest.xml")
            java.directories += "$edenAndroid/java"
            kotlin.directories += "$edenAndroid/java"
            res.directories += "$edenAndroid/res"
            assets.directories += "$edenAndroid/assets"
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
    // Eden's own dependencies (src/android/app/build.gradle.kts)
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.recyclerview:recyclerview:1.4.0")
    implementation("androidx.constraintlayout:constraintlayout:2.2.1")
    implementation("androidx.fragment:fragment-ktx:1.8.6")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.7")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("io.coil-kt:coil:2.2.2")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin:2.17.2")
    implementation("androidx.window:window:1.3.0")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")
    implementation("org.commonmark:commonmark:0.22.0")
    implementation("androidx.navigation:navigation-fragment-ktx:2.8.9")
    implementation("androidx.navigation:navigation-ui-ktx:2.8.9")
    implementation("info.debatty:java-string-similarity:2.0.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.3")
    implementation("androidx.compose.ui:ui-graphics-android:1.7.8")
    implementation("androidx.compose.ui:ui-text-android:1.7.8")
    implementation("net.swiftzer.semver:semver:2.0.0")
}
