object AppConfig {
    // CMake arguments that route compilation through ccache when the POMEGRADE_CCACHE
    // environment variable is set (CI does, see .github/workflows/android-apk.yml)
    val ccacheCmakeArguments: List<String>
        get() = if (System.getenv("POMEGRADE_CCACHE") != null) {
            listOf("-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
        } else {
            emptyList()
        }

    const val compileSdkVersion = 36
    const val targetSdkVersion = compileSdkVersion
    const val minSdkVersion = 29 // Android 10, required by the 3DS core (Azahar)
    const val ndkVersion = "28.0.13004108"

    const val versionCode = 41
    const val versionName = "2.0.1"
}