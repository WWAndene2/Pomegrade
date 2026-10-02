package me.magnum.melonds.domain.model.rom

import android.net.Uri
import me.magnum.melonds.domain.model.rom.config.RomConfig
import java.util.*
import kotlin.time.Duration

data class Rom(
    val name: String,
    val developerName: String,
    val fileName: String,
    val uri: Uri,
    val parentTreeUri: Uri?,
    var config: RomConfig,
    var lastPlayed: Date? = null,
    val isDsiWareTitle: Boolean,
    val retroAchievementsHash: String,
    val totalPlayTime: Duration = Duration.ZERO,
) {

    // Derived from the file name rather than stored, so cached ROM lists stay compatible
    val platform: RomPlatform get() = RomPlatform.fromFileName(fileName)

    fun hasSameFileAsRom(other: Rom): Boolean {
        return uri == other.uri
    }
}