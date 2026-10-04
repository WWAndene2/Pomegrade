// Pomegrade's emulation screen UI shared by both cores: the in-game menu (drawer), its header and
// theme, and the save state slots dialog. Used by the DS screen (:app) and the 3DS screen (:azahar),
// so it depends on neither.
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "io.github.wwandene2.pomegrade.emulatorui"
    compileSdk = AppConfig.compileSdkVersion

    defaultConfig {
        minSdk = AppConfig.minSdkVersion
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }
}

kotlin {
    compilerOptions {
        jvmTarget = JvmTarget.JVM_21
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.recyclerview)
    // NavigationView and the dialog builder are part of this module's API
    api(libs.android.material)
}
