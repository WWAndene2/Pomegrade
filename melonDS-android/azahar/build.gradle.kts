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
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0",
                    "-DENABLE_SDL2=0",
                    "-DANDROID_ARM_NEON=true",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DENABLE_GDBSTUB=OFF",
                )
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
    implementation("androidx.activity:activity-ktx:1.9.2")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("androidx.fragment:fragment-ktx:1.8.3")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.5")
    implementation("androidx.navigation:navigation-fragment-ktx:2.8.0")
    implementation("androidx.navigation:navigation-ui-ktx:2.8.0")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.slidingpanelayout:slidingpanelayout:1.2.0")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")
    implementation("androidx.work:work-runtime:2.9.1")
    implementation("com.google.android.material:material:1.9.0")
    implementation("info.debatty:java-string-similarity:2.0.0")
    implementation("io.coil-kt:coil:2.7.0")
    implementation("org.ini4j:ini4j:0.5.4")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.2")
}
