// Azahar (3DS emulator, GPLv2 or later) built as a library module of the app.
// Sources live in /azahar at the repository root; see /README.md.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.parcelize)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.navigation.safeargs)
}

val azaharRoot = file("../../azahar")
val azaharAndroid = file("../../azahar/src/android/app/src/main")

android {
    namespace = "org.citra.citra_emu"
    compileSdk = AppConfig.compileSdkVersion
    // the NDK version Azahar is developed against
    ndkVersion = "27.3.13750724"

    defaultConfig {
        minSdk = AppConfig.minSdkVersion

        ndk {
            // dynarmic has no 32-bit ARM backend
            abiFilters += AppConfig.abis(listOf("arm64-v8a", "x86_64"))
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0",
                    "-DENABLE_SDL2=0",
                    "-DANDROID_ARM_NEON=true",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DENABLE_GDBSTUB=OFF",
                    // the core's unit tests are a separate ~800 MB executable the app never uses
                    "-DENABLE_TESTS=OFF",
                )
                arguments += AppConfig.ccacheCmakeArguments
                arguments += AppConfig.nativeBuildTypeCmakeArguments
                // Gradle otherwise builds every executable and library CMake defines (libyuv tools...).
                // libadrenotools' hook libraries are their own targets: its Vulkan loader opens them
                // at runtime from the app's native library folder, and without them 3DS games abort
                // with "Failed to load Vulkan driver library"
                targets("citra-android", "hook_impl", "main_hook", "file_redirect_hook", "gsl_alloc_hook")
            }
        }

        buildConfigField("String", "GIT_VERSION", "\"pomegrade\"")
        buildConfigField("String", "GIT_HASH", "\"pomegrade\"")
        buildConfigField("String", "BRANCH", "\"pomegrade\"")
        buildConfigField("String", "VERSION_NAME", "\"pomegrade\"")
        // the vanilla flavour of Azahar, see azahar/src/android/app/build.gradle.kts
        buildConfigField("String", "FLAVOR", "\"vanilla\"")
    }

    buildFeatures {
        viewBinding = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    externalNativeBuild {
        cmake {
            version = "3.25.0+"
            path = file("$azaharRoot/CMakeLists.txt")
        }
    }

    sourceSets {
        named("main") {
            manifest.srcFile("$azaharAndroid/AndroidManifest.xml")
            java.directories += "$azaharAndroid/java"
            kotlin.directories += "$azaharAndroid/java"
            res.directories += "$azaharAndroid/res"
            assets.directories += "$azaharAndroid/assets"
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
    // Pomegrade: the emulation screen UI shared with the DS core
    implementation(projects.emulatorUi)
    // Azahar's own dependencies; those other modules also use come from the version catalogue
    // (gradle/libs.versions.toml): the APK holds one version of each, so each module compiles against
    // the one it runs with
    implementation(libs.androidx.activity)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.splashscreen)
    implementation(libs.androidx.documentfile)
    implementation(libs.androidx.fragment)
    implementation(libs.androidx.lifecycle.viewmodel)
    implementation(libs.androidx.navigation.fragment)
    implementation(libs.androidx.navigation.ui)
    implementation(libs.androidx.preference)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.slidingpanelayout)
    implementation(libs.androidx.swiperefreshlayout)
    implementation(libs.androidx.work)
    implementation(libs.android.material)
    implementation(libs.string.similarity)
    implementation(libs.coil.view)
    implementation("org.ini4j:ini4j:0.5.4")
    implementation(libs.kotlin.serialization)
}
