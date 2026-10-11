package me.magnum.melonds.domain.model.rom

/**
 * Console a game file runs on, decided by its extension. DS and DSi games are run by melonDS,
 * 3DS games by the Azahar core, GBA games by the mGBA core, Switch games by the Eden core, GameCube and
 * Wii games by the Dolphin core (one platform: their disc images share extensions).
 */
enum class RomPlatform {
    NDS,
    N3DS,
    GBA,
    SWITCH,
    GC_WII;

    companion object {
        // Formats the 3DS core loads directly (see Game.extensions in Azahar). CIA files are
        // installers and go through Azahar's own "install" screen instead.
        val n3dsExtensions = setOf("3ds", "cci", "cxi", "3dsx", "z3dsx", "zcci", "zcxi")

        val gbaExtensions = setOf("gba")

        // what Eden loads (its Game.extensions): game dumps, homebrew
        val switchExtensions = setOf("nsp", "xci", "nca", "nro")

        // what Dolphin loads (its GameFileCache extensions), but .bin (too common) and .json (Riivolution)
        val gcWiiExtensions = setOf("gcm", "tgc", "iso", "ciso", "gcz", "wbfs", "wia", "rvz", "nfs", "wad", "dol", "elf")

        fun fromFileName(fileName: String): RomPlatform {
            val extension = fileName.substringAfterLast('.', "").lowercase()
            return when (extension) {
                in n3dsExtensions -> N3DS
                in gbaExtensions -> GBA
                in switchExtensions -> SWITCH
                in gcWiiExtensions -> GC_WII
                else -> NDS
            }
        }
    }
}
