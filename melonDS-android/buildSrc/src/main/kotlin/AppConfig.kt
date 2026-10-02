object AppConfig {
    // CMake arguments that route compilation through ccache when the POMEGRADE_CCACHE
    // environment variable is set (CI does, see .github/workflows/android-apk.yml)
    val ccacheCmakeArguments: List<String>
        get() = if (System.getenv("POMEGRADE_CCACHE") != null) {
            listOf("-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
        } else {
            emptyList()
        }

    // CMake build type override from the POMEGRADE_NATIVE_BUILD_TYPE environment variable. CI sets
    // it to Release: the default Debug native build is unoptimized (-O0) with full debug info, so
    // the cores run games far too slowly and their intermediates fill the runner's disk.
    val nativeBuildTypeCmakeArguments: List<String>
        get() = System.getenv("POMEGRADE_NATIVE_BUILD_TYPE")?.let { listOf("-DCMAKE_BUILD_TYPE=$it") } ?: emptyList()

    const val compileSdkVersion = 36
    const val targetSdkVersion = compileSdkVersion
    const val minSdkVersion = 29 // Android 10, required by the 3DS core (Azahar)
    const val ndkVersion = "28.0.13004108"

    const val versionCode = 41
    const val versionName = "2.0.1"
}