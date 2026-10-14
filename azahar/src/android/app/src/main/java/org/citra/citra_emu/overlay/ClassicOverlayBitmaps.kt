// Copyright Pomegrade Project
// Licensed under GPLv2 or any later version

package org.citra.citra_emu.overlay

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import androidx.preference.PreferenceManager
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR

/**
 * Pomegrade: the Classic layout's control images, drawn as the Punica 3DS skin template draws them
 * (assets/consoles/skins/gabarit_skin_3ds_1220x2712.svg) in the style chosen in Punica's settings
 * ("punica_classic_button_style", shared with the DS Classic layout): filled (light grey shapes, dark letters) or
 * outlined (white outlines 4 template pixels wide, as on the template). A pressed image is the same, darkened.
 */
object ClassicOverlayBitmaps {
    private const val FILL = 0xFFDCDEE2.toInt()
    private const val LINE = 0xFFF3F4F6.toInt()
    private const val PRESSED_SHADE = 0x59000000

    private fun outlined(context: Context): Boolean =
        PreferenceManager.getDefaultSharedPreferences(context).getString("punica_classic_button_style", "filled") == "outline"

    /** The Punica button: the pill's logo in a circle the size of START / SELECT (83 template pixels). */
    fun punica(context: Context, side: Int, pressed: Boolean): Bitmap {
        val size = side.coerceAtLeast(1)
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        val half = size / 2f
        val stroke = size * 4f / 83f
        val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        if (outlined(context)) {
            paint.style = Paint.Style.STROKE
            paint.strokeWidth = stroke
            paint.color = LINE
            canvas.drawCircle(half, half, half - stroke / 2f, paint)
        } else {
            // the logo on white, as the pill's Punica blade
            paint.color = 0xFFFFFFFF.toInt()
            canvas.drawCircle(half, half, half, paint)
        }
        BitmapFactory.decodeResource(context.resources, EmulatorMenuR.drawable.emulator_menu_ic_punica)?.let {
            val logo = size * 0.64f
            val offset = (size - logo) / 2f
            canvas.drawBitmap(it, null, RectF(offset, offset, offset + logo, offset + logo), Paint(Paint.FILTER_BITMAP_FLAG))
        }
        if (pressed) {
            canvas.drawCircle(half, half, half, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = PRESSED_SHADE })
        }
        return bitmap
    }
}
