// Copyright Pomegrade Project
// Licensed under GPLv2 or any later version

package org.citra.citra_emu.overlay

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import androidx.core.content.res.ResourcesCompat
import androidx.preference.PreferenceManager
import io.github.wwandene2.pomegrade.emulatorui.ClassicControlPainter
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import org.citra.citra_emu.NativeLibrary.ButtonType
import kotlin.math.max

/**
 * Pomegrade: the Classic layout's control images, drawn as the Punica 3DS skin template draws them
 * (assets/consoles/skins/gabarit_skin_3ds_1220x2712.svg), so each control fills its template rectangle
 * (ClassicOverlayLayout), in the style chosen in Punica's settings ("punica_classic_button_style", shared with the
 * DS Classic layout), painted by ClassicControlPainter: filled in relief (engraved letters) or outlined (white
 * outlines 4 template pixels wide, as on the template). Every size is a share of the control's template size:
 * L / R 271 x 92 with a 46 corner, ZL / ZR 165.6 x 84.9 pills, A / B / X / Y 136.6, START / SELECT / Punica 83,
 * the D-pad 329.3 in the DS Lite's shape with its grooves, the circle pad's 380.2 range and 257 knob, the C stick
 * 128.6. A pressed image is sunk (no relief) and darkened; InputOverlay fades between the two (the press
 * animation).
 */
object ClassicOverlayBitmaps {
    private const val PRESSED_SHADE = 0x59000000
    // the circle pad's range, filled style: a faint disc, the knob solid
    private const val RANGE_FILL = 0x59DCDEE2

    private class Style(val outlined: Boolean, val typeface: Typeface)

    private fun style(context: Context) = Style(
        PreferenceManager.getDefaultSharedPreferences(context).getString("punica_classic_button_style", "filled") == "outline",
        ResourcesCompat.getFont(context, EmulatorMenuR.font.punica_bricolage_grotesque_bold) ?: Typeface.DEFAULT_BOLD
    )

    /**
     * A button's default and pressed images, [width] pixels wide (its height from its template rectangle), or null
     * for a control the template does not draw (swap, turbo, combo keep Azahar's images).
     */
    fun button(context: Context, id: Int, width: Int): Pair<Bitmap, Bitmap>? {
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
            ButtonType.BUTTON_PUNICA -> ""
            else -> return null
        }
        val style = style(context)
        return draw(id, label, width, style, false, context) to draw(id, label, width, style, true, context)
    }

    private fun draw(id: Int, label: String, width: Int, style: Style, pressed: Boolean, context: Context): Bitmap {
        val w = width.coerceAtLeast(1).toFloat()
        val (templateWidth, templateHeight) = when (id) {
            ButtonType.TRIGGER_L, ButtonType.TRIGGER_R -> 271f to 92f
            ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR -> 165.6f to 84.9f
            ButtonType.BUTTON_START, ButtonType.BUTTON_SELECT, ButtonType.BUTTON_PUNICA -> 83f to 83f
            else -> 136.6f to 136.6f
        }
        val scale = w / templateWidth
        val h = templateHeight * scale
        val bitmap = Bitmap.createBitmap(w.toInt(), max(1, h.toInt()), Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        val stroke = 4f * scale
        val inset = if (style.outlined) stroke / 2f else 0f
        val path = Path()
        when (id) {
            ButtonType.TRIGGER_L, ButtonType.TRIGGER_R -> {
                // flat on three sides, the outer top corner rounded over 46 of the 92
                val c = 46f * scale
                val l = inset
                val t = inset
                val r = w - inset
                val b = h - inset
                if (id == ButtonType.TRIGGER_L) {
                    path.moveTo(l + c, t)
                    path.lineTo(r, t)
                    path.lineTo(r, b)
                    path.lineTo(l, b)
                    path.lineTo(l, t + c)
                    path.arcTo(l, t, l + 2 * c, t + 2 * c, 180f, 90f, false)
                } else {
                    path.moveTo(l, t)
                    path.lineTo(r - c, t)
                    path.arcTo(r - 2 * c, t, r, t + 2 * c, 270f, 90f, false)
                    path.lineTo(r, b)
                    path.lineTo(l, b)
                }
                path.close()
            }
            ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR ->
                path.addRoundRect(RectF(inset, inset, w - inset, h - inset), h / 2f, h / 2f, Path.Direction.CW)
            else -> path.addCircle(w / 2f, h / 2f, w / 2f - inset, Path.Direction.CW)
        }
        // the Punica button's face white, as the pill's Punica blade
        val face = if (id == ButtonType.BUTTON_PUNICA) ClassicControlPainter.WHITE_FACE else null
        val raised = ClassicControlPainter.paint(canvas, path, style.outlined, stroke, if (pressed) 1f else 0f, face)
        if (id == ButtonType.BUTTON_PUNICA) {
            BitmapFactory.decodeResource(context.resources, EmulatorMenuR.drawable.emulator_menu_ic_punica)?.let {
                val logo = w * 0.64f
                val offset = (w - logo) / 2f
                val top = (h - logo) / 2f - raised
                canvas.drawBitmap(it, null, RectF(offset, top, offset + logo, top + logo), Paint(Paint.FILTER_BITMAP_FLAG))
            }
        } else {
            val text = textPaint(style)
            text.textSize = when (id) {
                ButtonType.TRIGGER_L, ButtonType.TRIGGER_R -> 30f
                ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR -> 28f
                ButtonType.BUTTON_START, ButtonType.BUTTON_SELECT -> 16f
                else -> 34f
            } * scale
            // START / SELECT: their names fitted inside the circle
            val room = w * 0.8f
            val measured = text.measureText(label)
            if (measured > room) text.textSize *= room / measured
            ClassicControlPainter.drawLabel(canvas, text, label, w / 2f, h / 2f - raised, style.outlined)
        }
        return bitmap
    }

    /**
     * The D-pad's default, one direction (up) and two directions (up and left) images, [side] pixels: the DS Lite's
     * shape with its four grooves (ClassicControlPainter, owner's choice), sunk and its pressed arms darkened when
     * pressed.
     */
    fun dpad(context: Context, side: Int): Triple<Bitmap, Bitmap, Bitmap> {
        val style = style(context)
        return Triple(dpadImage(style, side, false, false), dpadImage(style, side, true, false), dpadImage(style, side, true, true))
    }

    private fun dpadImage(style: Style, side: Int, up: Boolean, left: Boolean): Bitmap {
        val s = side.coerceAtLeast(1).toFloat()
        val bitmap = Bitmap.createBitmap(s.toInt(), s.toInt(), Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        val stroke = 4f * s / 329.3f
        val cross = ClassicControlPainter.dpadPath(s, if (style.outlined) stroke / 2f else 0f)
        val pressed = up || left
        val raised = ClassicControlPainter.paint(canvas, cross, style.outlined, stroke, if (pressed) 1f else 0f)
        ClassicControlPainter.drawDpadGrooves(canvas, s, raised, style.outlined)
        val from = (s - s * 128f / 375f) / 2f
        val to = s - from
        if (pressed) {
            canvas.save()
            canvas.clipPath(cross)
            val shade = Paint().apply { color = PRESSED_SHADE }
            if (up) canvas.drawRect(from, 0f, to, from, shade)
            if (left) canvas.drawRect(0f, from, from, to, shade)
            canvas.restore()
        }
        return bitmap
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

    private fun textPaint(style: Style) = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ClassicControlPainter.labelColor(style.outlined)
        textAlign = Paint.Align.CENTER
        typeface = style.typeface
    }
}
