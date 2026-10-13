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
    // Eden's own dependencies (src/android/app/build.gradle.kts). Those other modules also use come from
    // the version catalogue (gradle/libs.versions.toml): the APK holds one version of each, and a module
    // compiled against an older one crashed at run time (material 1.12.0: NoSuchFieldError colorPrimary)
    implementation(libs.androidx.core)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.fragment)
    implementation(libs.androidx.documentfile)
    implementation(libs.android.material)
    implementation(libs.androidx.preference)
    implementation(libs.androidx.lifecycle.viewmodel)
    implementation(libs.okhttp)
    implementation(libs.coil.view)
    implementation(libs.androidx.splashscreen)
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin:2.17.2")
    implementation(libs.androidx.window)
    implementation(libs.androidx.swiperefreshlayout)
    // the app's Markwon brings commonmark 0.13 (com.atlassian.commonmark): one copy of org.commonmark only;
    // Eden uses only Parser and HtmlRenderer, unchanged since
    implementation("com.atlassian.commonmark:commonmark:0.13.0")
    implementation(libs.androidx.navigation.fragment)
    implementation(libs.androidx.navigation.ui)
    implementation(libs.string.similarity)
    implementation(libs.kotlin.serialization)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui.graphics)
    implementation(libs.compose.ui.text)
    implementation("net.swiftzer.semver:semver:2.0.0")
}
