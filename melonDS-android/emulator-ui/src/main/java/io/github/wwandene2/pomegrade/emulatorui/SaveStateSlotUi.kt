package io.github.wwandene2.pomegrade.emulatorui

import android.net.Uri
import java.util.Date

/**
 * A save state slot as the shared save state dialog shows it, whichever core it belongs to.
 * [savedAt] is null for an empty slot; [screenshot] is null when the core keeps none (3DS).
 */
data class SaveStateSlotUi(
    val slot: Int,
    val isQuickSlot: Boolean,
    val savedAt: Date?,
    val screenshot: Uri?,
) {
    val exists: Boolean get() = savedAt != null
}
