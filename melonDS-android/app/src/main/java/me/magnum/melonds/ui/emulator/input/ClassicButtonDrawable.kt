package me.magnum.melonds.ui.emulator.input

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import androidx.core.content.res.ResourcesCompat
import io.github.wwandene2.pomegrade.emulatorui.ClassicControl3D
import io.github.wwandene2.pomegrade.emulatorui.ClassicControl3D.Press
import io.github.wwandene2.pomegrade.emulatorui.ClassicControlPainter
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.input.ClassicButtonStyle
import kotlin.math.min

/**
 * Pomegrade: a button of the Classic layout drawn as the Punica DS skin template draws it
 * (assets/consoles/skins/gabarit_skin_ds_1220x2712.svg), so the real button fills its template rectangle: every
 * size is a share of that rectangle, taken from the template (L / R 328 x 151 with a 64 corner, the DS Lite's D-pad
 * with its four grooves, ABXY 467 with buttons of 151.2, SELECT / START / Punica 72).
 *
 * Filled, each is a solid key cap (ClassicControl3D) that sinks while pressed and, the D-pad, rocks toward the
 * pressed directions, with no in-between ([setPressed], from the inputs the layout's handlers send; owner's choice).
 * Outlined, they stay flat outlines, 4 template pixels wide as on the template. Caps draw a little past their
 * rectangle (their thickness): RuntimeLayoutView lets its children draw outside their bounds.
 */
class ClassicButtonDrawable(context: Context, private val kind: Kind, private val style: ClassicButtonStyle) : Drawable() {

    enum class Kind { L, R, DPAD, BUTTONS, START, SELECT, PUNICA }

    private companion object {
        // ABXY: a button's diameter and centres, shares of the cluster's side
        const val FACE_DIAMETER = 151.2f / 467f
        val FACE_CENTRES = listOf(
            Input.X to (0.5f to 0.1606f),
            Input.Y to (0.1630f to 0.4980f),
            Input.A to (0.8370f to 0.4980f),
            Input.B to (0.5f to 0.8357f),
        )
    }

    private val outlined = style == ClassicButtonStyle.OUTLINE
    private var alphaValue = 255
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        // Punica's title face, as the app's labels
        typeface = ResourcesCompat.getFont(context, EmulatorMenuR.font.punica_bricolage_grotesque_bold) ?: Typeface.DEFAULT_BOLD
    }
    private val logo: Bitmap? = if (kind == Kind.PUNICA) BitmapFactory.decodeResource(context.resources, EmulatorMenuR.drawable.emulator_menu_ic_punica) else null
    private val logoPaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)

    // the inputs held on this button: an ABXY button each, the D-pad's directions, else the button's own
    private val pressed = HashSet<Input>()

    /** Sinks or raises the part [input] presses: at once, as a real key. */
    fun setPressed(input: Input, isPressed: Boolean) {
        if (if (isPressed) pressed.add(input) else pressed.remove(input)) invalidateSelf()
    }

    override fun draw(canvas: Canvas) {
        val b = bounds
        val w = b.width().toFloat()
        val h = b.height().toFloat()
        if (w <= 0f || h <= 0f) return
        // alpha on the whole button, its sides included; the layer reaches past the bounds, where caps draw
        val layer = canvas.saveLayerAlpha(b.left - w, b.top - h, b.right + w, b.bottom + h, alphaValue)
        canvas.translate(b.left.toFloat(), b.top.toFloat())
        when (kind) {
            Kind.L, Kind.R -> drawBumper(canvas, w, h)
            Kind.DPAD -> drawDpad(canvas, min(w, h))
            Kind.BUTTONS -> drawFaceButtons(canvas, min(w, h))
            Kind.START -> drawRound(canvas, min(w, h), "START")
            Kind.SELECT -> drawRound(canvas, min(w, h), "SELECT")
            Kind.PUNICA -> drawRound(canvas, min(w, h), null)
        }
        canvas.restoreToCount(layer)
    }

    private fun held() = if (pressed.isEmpty()) Press.UP else Press.DOWN

    // 328 x 151, fitted at the template's 328:151 and centred whatever the rectangle the layout gives
    private fun drawBumper(canvas: Canvas, w: Float, h: Float) {
        val scale = min(w / 328f, h / 151f)
        val rect = RectF((w - 328f * scale) / 2f, (h - 151f * scale) / 2f, (w + 328f * scale) / 2f, (h + 151f * scale) / 2f)
        ClassicControl3D.bumper(canvas, rect, 64f * scale, kind == Kind.L, outlined, 4f * scale, held()) {
            textPaint.textSize = 34f * scale
            ClassicControlPainter.drawLabel(it, textPaint, if (kind == Kind.L) "L" else "R", rect.centerX(), rect.centerY(), outlined)
        }
    }

    private fun drawDpad(canvas: Canvas, side: Float) {
        val press = Press.rocker(Input.UP in pressed, Input.DOWN in pressed, Input.LEFT in pressed, Input.RIGHT in pressed)
        ClassicControl3D.dpad(canvas, 0f, 0f, side, outlined, press)
    }

    private fun drawFaceButtons(canvas: Canvas, side: Float) {
        val scale = side / 467f
        val diameter = FACE_DIAMETER * side
        textPaint.textSize = 39f * scale
        FACE_CENTRES.forEach { (input, centre) ->
            val cx = centre.first * side
            val cy = centre.second * side
            ClassicControl3D.round(canvas, cx, cy, diameter, outlined, 4f * scale, if (input in pressed) Press.DOWN else Press.UP) {
                ClassicControlPainter.drawLabel(it, textPaint, input.name, cx, cy, outlined)
            }
        }
    }

    // SELECT, START (their names inside, fitted to the circle) and the Punica button (the pill's logo, on white)
    private fun drawRound(canvas: Canvas, side: Float, label: String?) {
        val scale = side / 72f
        val face = if (label == null) ClassicControlPainter.WHITE_FACE else null
        ClassicControl3D.round(canvas, side / 2f, side / 2f, side, outlined, 4f * scale, held(), face) {
            if (label != null) {
                textPaint.textSize = 14f * scale
                val width = textPaint.measureText(label)
                val room = side * 0.8f
                if (width > room) textPaint.textSize *= room / width
                ClassicControlPainter.drawLabel(it, textPaint, label, side / 2f, side / 2f, outlined)
            } else {
                logo?.let { image ->
                    val logoSide = side * 0.64f
                    val offset = (side - logoSide) / 2f
                    it.drawBitmap(image, null, RectF(offset, offset, offset + logoSide, offset + logoSide), logoPaint)
                }
            }
        }
    }

    override fun setAlpha(alpha: Int) {
        alphaValue = alpha
        invalidateSelf()
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        textPaint.colorFilter = colorFilter
        logoPaint.colorFilter = colorFilter
        invalidateSelf()
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT
}
