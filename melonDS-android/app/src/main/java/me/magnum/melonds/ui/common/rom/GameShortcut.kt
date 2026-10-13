package me.magnum.melonds.ui.common.rom

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.graphics.applyCanvas
import androidx.core.graphics.createBitmap
import androidx.core.graphics.drawable.IconCompat
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.RomIconFiltering
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.ui.romlist.RomIcon

/**
 * A game's home-screen shortcut: its icon on white in an adaptive icon, opening the game list, which starts the game
 * as a tap in it does (ConsoleLauncher.gameListShortcutIntent). Made by the shortcut picker and the game sheet.
 */
object GameShortcut {

    fun build(context: Context, rom: Rom, romIcon: RomIcon): ShortcutInfoCompat {
        return ShortcutInfoCompat.Builder(context, rom.uri.toString())
            .setShortLabel(rom.name)
            .setIcon(IconCompat.createWithAdaptiveBitmap(buildBitmap(context, romIcon)))
            .setIntent(ConsoleLauncher.gameListShortcutIntent(context, rom))
            .build()
    }

    private fun buildBitmap(context: Context, romIcon: RomIcon): Bitmap {
        val iconBitmap = romIcon.bitmap ?: BitmapFactory.decodeResource(context.resources, R.drawable.logo_splash)
        val shortcutBitmap = createBitmap(256, 256)

        return shortcutBitmap.applyCanvas {
            drawRect(Rect(0, 0, width, height), Paint().apply { color = Color.WHITE })
            val iconRect = Rect(77, 77, shortcutBitmap.width - 77, shortcutBitmap.height - 77)
            drawBitmap(iconBitmap, null, iconRect, Paint().apply { isFilterBitmap = romIcon.filtering == RomIconFiltering.LINEAR })
        }
    }
}
