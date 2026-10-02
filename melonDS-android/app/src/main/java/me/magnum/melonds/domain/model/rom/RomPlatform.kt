package me.magnum.melonds.domain.model.rom

/**
 * Console a game file runs on, decided by its extension. DS and DSi games are run by melonDS,
 * 3DS games by the Azahar core.
 */
enum class RomPlatform {
    NDS,
    N3DS;

    companion object {
        // Formats the 3DS core loads directly (see Game.extensions in Azahar). CIA files are
        // installers and go through Azahar's own "install" screen instead.
        val n3dsExtensions = setOf("3ds", "cci", "cxi", "3dsx", "z3dsx", "zcci", "zcxi")

        fun fromFileName(fileName: String): RomPlatform {
            val extension = fileName.substringAfterLast('.', "").lowercase()
            return if (extension in n3dsExtensions) N3DS else NDS
        }
    }
}
