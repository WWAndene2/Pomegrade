// Copyright Pomegrade Project
// Licensed under GPLv2 or any later version

package org.citra.citra_emu.overlay

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Typeface
import androidx.core.content.res.ResourcesCompat
import androidx.preference.PreferenceManager
import io.github.wwandene2.pomegrade.emulatorui.ClassicControl3D
import io.github.wwandene2.pomegrade.emulatorui.ClassicControl3D.Press
import io.github.wwandene2.pomegrade.emulatorui.ClassicControlPainter
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import org.citra.citra_emu.NativeLibrary.ButtonType
import kotlin.math.max

/**
 * Pomegrade: the Classic layout's controls, drawn as the Punica 3DS skin template draws them
 * (assets/consoles/skins/gabarit_skin_3ds_1220x2712.svg), so each fills its template rectangle (ClassicOverlayLayout),
 * in the style chosen in Punica's settings ("punica_classic_button_style", shared with the DS Classic layout).
 * Every size is a share of the control's template size: L / R 271 x 92 with a 46 corner, ZL / ZR 165.6 x 84.9 pills,
 * A / B / X / Y 136.6, START / SELECT / Punica 83, the D-pad 329.3 in the DS Lite's shape with its grooves.
 *
 * Buttons and the D-pad are drawn by the overlay itself each frame (their painters, ClassicControl3D): filled, solid
 * key caps that sink while pressed, the D-pad rocking toward its pressed directions, with no in-between (owner's
 * choice); outlined, flat outlines 4 template pixels wide. Their bitmaps are blank and only give their size. The
 * circle pad and the C stick keep images (ClassicControlPainter): their range, and a knob in relief.
 */
object ClassicOverlayControls {
    // the circle pad's range, filled style: a faint disc, the knob solid
    private const val RANGE_FILL = 0x59DCDEE2

    private class Style(val outlined: Boolean, val typeface: Typeface)

    private fun style(context: Context) = Style(
        PreferenceManager.getDefaultSharedPreferences(context).getString("punica_classic_button_style", "filled") == "outline",
        ResourcesCompat.getFont(context, EmulatorMenuR.font.punica_bricolage_grotesque_bold) ?: Typeface.DEFAULT_BOLD
    )

    /** A drawn control: its blank bitmap (its size) and its painter. */
    class Drawn<P>(val bitmap: Bitmap, val painter: P)

    /**
     * A button [width] pixels wide (its height from its template rectangle), or null for a control the template does
     * not draw (swap, turbo, combo keep Azahar's images).
     */
    fun button(context: Context, id: Int, width: Int): Drawn<(Canvas, Rect, Boolean) -> Unit>? {
        val label = when (id) {
            ButtonType.BUTTON_A -> "A"
            ButtonType.BUTTON_B -> "B"
            ButtonType.BUTTON_X -> "X"
            ButtonType.BUTTON_Y -> "Y"
            ButtonType.TRIGGER_L -> "L"
            ButtonType.TRIGGER_R -> "R"
            ButtonType.BUTTON_ZL -> "ZL"
            ButtonType.BUTTON_ZR -> "ZR"
            ButtonType.BUTTON_START -> "START"
            ButtonType.BUTTON_SELECT -> "SELECT"
            ButtonType.BUTTON_PUNICA -> null
            else -> return null
        }
        val (templateWidth, templateHeight) = when (id) {
            ButtonType.TRIGGER_L, ButtonType.TRIGGER_R -> 271f to 92f
            ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR -> 165.6f to 84.9f
            ButtonType.BUTTON_START, ButtonType.BUTTON_SELECT, ButtonType.BUTTON_PUNICA -> 83f to 83f
            else -> 136.6f to 136.6f
        }
        val w = width.coerceAtLeast(1)
        val bitmap = Bitmap.createBitmap(w, max(1, (w * templateHeight / templateWidth).toInt()), Bitmap.Config.ALPHA_8)
        val style = style(context)
        val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            textAlign = Paint.Align.CENTER
            typeface = style.typeface
        }
        val logo = if (id == ButtonType.BUTTON_PUNICA) BitmapFactory.decodeResource(context.resources, EmulatorMenuR.drawable.emulator_menu_ic_punica) else null
        val logoPaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)
        val painter: (Canvas, Rect, Boolean) -> Unit = { canvas, bounds, pressed ->
            val rect = RectF(bounds)
            val scale = rect.width() / templateWidth
            val stroke = 4f * scale
            val press = if (pressed) Press.DOWN else Press.UP
            val decorate: (Canvas) -> Unit = { face ->
                if (label != null) {
                    text.textSize = when (id) {
                        ButtonType.TRIGGER_L, ButtonType.TRIGGER_R -> 30f
                        ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR -> 28f
                        ButtonType.BUTTON_START, ButtonType.BUTTON_SELECT -> 16f
                        else -> 34f
                    } * scale
                    // START / SELECT: their names fitted inside the circle
                    val room = rect.width() * 0.8f
                    val measured = text.measureText(label)
                    if (measured > room) text.textSize *= room / measured
                    ClassicControlPainter.drawLabel(face, text, label, rect.centerX(), rect.centerY(), style.outlined)
                } else if (logo != null) {
                    val side = rect.width() * 0.64f
                    face.drawBitmap(logo, null, RectF(rect.centerX() - side / 2f, rect.centerY() - side / 2f, rect.centerX() + side / 2f, rect.centerY() + side / 2f), logoPaint)
                }
            }
            when (id) {
                ButtonType.TRIGGER_L, ButtonType.TRIGGER_R ->
                    ClassicControl3D.bumper(canvas, rect, 46f * scale, id == ButtonType.TRIGGER_L, style.outlined, stroke, press, decorate)
                ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR ->
                    ClassicControl3D.pill(canvas, rect, style.outlined, stroke, press, decorate)
                else -> {
                    // the Punica button's face white, as the pill's Punica blade
                    val face = if (id == ButtonType.BUTTON_PUNICA) ClassicControlPainter.WHITE_FACE else null
                    ClassicControl3D.round(canvas, rect.centerX(), rect.centerY(), rect.width(), style.outlined, stroke, press, face, decorate)
                }
            }
        }
        return Drawn(bitmap, painter)
    }

    /** The D-pad, [side] pixels: the DS Lite's shape with its grooves, rocking toward its pressed directions. */
    fun dpad(context: Context, side: Int): Drawn<(Canvas, Rect, Boolean, Boolean, Boolean, Boolean) -> Unit> {
        val s = side.coerceAtLeast(1)
        val outlined = style(context).outlined
        val painter: (Canvas, Rect, Boolean, Boolean, Boolean, Boolean) -> Unit = { canvas, bounds, up, down, left, right ->
            ClassicControl3D.dpad(canvas, bounds.left.toFloat(), bounds.top.toFloat(), bounds.width().toFloat(), outlined, Press.rocker(up, down, left, right))
        }
        return Drawn(Bitmap.createBitmap(s, s, Bitmap.Config.ALPHA_8), painter)
    }

    /**
     * A stick's range, knob and pressed knob images, [side] pixels. The circle pad's knob is 257 of its 380.2
     * range; the C stick's image is drawn at twice its 128.6 (InputOverlay's outerScale), its knob 0.6 of it.
     */
    fun stick(context: Context, id: Int, side: Int): Triple<Bitmap, Bitmap, Bitmap> {
        val style = style(context)
        val s = side.coerceAtLeast(1).toFloat()
        val templateSide = if (id == ButtonType.STICK_C) 128.6f else 380.2f
        val stroke = 4f * s / (if (id == ButtonType.STICK_C) 2f * templateSide else templateSide)
        val knob = if (id == ButtonType.STICK_C) 0.6f else 257f / 380.2f
        fun image(draw: (Canvas, Float) -> Unit): Bitmap =
            Bitmap.createBitmap(s.toInt(), s.toInt(), Bitmap.Config.ARGB_8888).also { draw(Canvas(it), s / 2f) }
        val range = image { canvas, half ->
            if (style.outlined) {
                ClassicControlPainter.paint(canvas, Path().apply { addCircle(half, half, half - stroke / 2f, Path.Direction.CW) }, true, stroke)
            } else {
                // a faint disc, under the knob in relief
                canvas.drawCircle(half, half, half, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = RANGE_FILL })
            }
        }
        fun knobImage(pressed: Boolean) = image { canvas, half ->
            val radius = half * knob - if (style.outlined) stroke / 2f else 0f
            ClassicControlPainter.paint(canvas, Path().apply { addCircle(half, half, radius, Path.Direction.CW) }, style.outlined, stroke, if (pressed) 1f else 0f)
        }
        return Triple(range, knobImage(false), knobImage(true))
    }
}
