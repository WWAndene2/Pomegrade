// The GBA core: mGBA (MPL 2.0) built as a library, with Pomegrade's own emulation screen around it
// (mGBA has no Android frontend). Sources live in /mgba at the repository root; see /README.md.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "io.github.wwandene2.pomegrade.gba"
    compileSdk = AppConfig.compileSdkVersion
    ndkVersion = AppConfig.ndkVersion

    defaultConfig {
        minSdk = AppConfig.minSdkVersion

        ndk {
            abiFilters += AppConfig.abis(listOf("armeabi-v7a", "arm64-v8a", "x86_64"))
        }

        externalNativeBuild {
            cmake {
                arguments += AppConfig.ccacheCmakeArguments
                arguments += AppConfig.nativeBuildTypeCmakeArguments
                targets("pomegrade-gba")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = JvmTarget.JVM_21
    }
}

dependencies {
    // the in-game menu and save state dialog shared with the DS and 3DS screens
    implementation(projects.emulatorUi)
    implementation(libs.androidx.appcompat)
    testImplementation(libs.junit)
}
