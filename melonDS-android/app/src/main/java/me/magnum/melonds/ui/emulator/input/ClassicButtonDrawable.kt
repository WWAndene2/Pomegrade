package me.magnum.melonds.ui.emulator.input

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PixelFormat
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import androidx.core.content.res.ResourcesCompat
import io.github.wwandene2.pomegrade.emulatorui.ClassicControlPainter
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.input.ClassicButtonStyle
import kotlin.math.min

/**
 * Pomegrade: a button of the Classic layout drawn as the Punica DS skin template draws it
 * (assets/consoles/skins/gabarit_skin_ds_1220x2712.svg), so the real button fills its template rectangle: every
 * size is a share of that rectangle, taken from the template (L / R 328 x 151 with a 64 corner, the DS Lite's D-pad
 * with its four grooves, ABXY 467 with buttons of 151.2, SELECT / START / Punica 72). Shapes are painted by
 * ClassicControlPainter: filled in relief, or outlined 4 template pixels wide as on the template.
 *
 * Pressing sinks a button ([setPressed], from the inputs the layout's handlers send): its face goes down in 40 ms
 * and comes back up in 80 ms; on the D-pad the whole cross sinks.
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
        const val PRESS_MS = 40L
        const val RELEASE_MS = 80L
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

    // how far each part is pressed, 0 to 1: one part per ABXY button, else the whole button
    private val depths = HashMap<Input?, Float>()
    private val animators = HashMap<Input?, ValueAnimator>()
    private val pressedDirections = HashSet<Input>()

    /** Sinks or raises the part [input] presses (an ABXY button, a D-pad direction, else the whole button). */
    fun setPressed(input: Input, pressed: Boolean) {
        val part = when (kind) {
            Kind.BUTTONS -> input
            Kind.DPAD -> {
                if (pressed) pressedDirections += input else pressedDirections -= input
                null
            }
            else -> null
        }
        val down = if (kind == Kind.DPAD) pressedDirections.isNotEmpty() else pressed
        animators[part]?.cancel()
        animators[part] = ValueAnimator.ofFloat(depths[part] ?: 0f, if (down) 1f else 0f).apply {
            duration = if (down) PRESS_MS else RELEASE_MS
            addUpdateListener {
                depths[part] = it.animatedValue as Float
                invalidateSelf()
            }
            start()
        }
    }

    override fun draw(canvas: Canvas) {
        val b = bounds
        val w = b.width().toFloat()
        val h = b.height().toFloat()
        if (w <= 0f || h <= 0f) return
        // alpha on the whole button, relief included
        val layer = canvas.saveLayerAlpha(b.left.toFloat(), b.top.toFloat(), b.right.toFloat(), b.bottom.toFloat(), alphaValue)
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

    // 328 x 151: the outer top corner rounded over 64 / 151 of the height, the others very slightly
    private fun drawBumper(canvas: Canvas, w: Float, h: Float) {
        // fitted at the template's 328:151, centred, whatever the rectangle the layout gives
        val scale = min(w / 328f, h / 151f)
        val bw = 328f * scale
        val bh = 151f * scale
        val left = (w - bw) / 2f
        val top = (h - bh) / 2f
        val stroke = 4f * scale
        val inset = if (outlined) stroke / 2f else 0f
        val path = ClassicControlPainter.bumperPath(left + inset, top + inset, left + bw - inset, top + bh - inset, 64f * scale, kind == Kind.L)
        val raised = ClassicControlPainter.paint(canvas, path, outlined, stroke, depths[null] ?: 0f)
        textPaint.textSize = 34f * scale
        ClassicControlPainter.drawLabel(canvas, textPaint, if (kind == Kind.L) "L" else "R", left + bw / 2f, top + bh / 2f - raised, outlined)
    }

    private fun drawDpad(canvas: Canvas, side: Float) {
        val stroke = 4f * side / 375f
        val path = ClassicControlPainter.dpadPath(side, if (outlined) stroke / 2f else 0f)
        val raised = ClassicControlPainter.paint(canvas, path, outlined, stroke, depths[null] ?: 0f)
        ClassicControlPainter.drawDpadGrooves(canvas, side, raised, outlined)
    }

    private fun drawFaceButtons(canvas: Canvas, side: Float) {
        val scale = side / 467f
        val stroke = 4f * scale
        val radius = FACE_DIAMETER * side / 2f - if (outlined) stroke / 2f else 0f
        textPaint.textSize = 39f * scale
        FACE_CENTRES.forEach { (input, centre) ->
            val cx = centre.first * side
            val cy = centre.second * side
            val circle = Path().apply { addCircle(cx, cy, radius, Path.Direction.CW) }
            val raised = ClassicControlPainter.paint(canvas, circle, outlined, stroke, depths[input] ?: 0f)
            ClassicControlPainter.drawLabel(canvas, textPaint, input.name, cx, cy - raised, outlined)
        }
    }

    // SELECT, START (their names inside, fitted to the circle) and the Punica button (the pill's logo)
    private fun drawRound(canvas: Canvas, side: Float, label: String?) {
        val scale = side / 72f
        val stroke = 4f * scale
        val radius = side / 2f - if (outlined) stroke / 2f else 0f
        val circle = Path().apply { addCircle(side / 2f, side / 2f, radius, Path.Direction.CW) }
        val face = if (label == null) ClassicControlPainter.WHITE_FACE else null
        val raised = ClassicControlPainter.paint(canvas, circle, outlined, stroke, depths[null] ?: 0f, face)
        val cy = side / 2f - raised
        if (label != null) {
            textPaint.textSize = 14f * scale
            val width = textPaint.measureText(label)
            val room = side * 0.8f
            if (width > room) textPaint.textSize *= room / width
            ClassicControlPainter.drawLabel(canvas, textPaint, label, side / 2f, cy, outlined)
        } else {
            logo?.let {
                val logoSide = side * 0.64f
                val offset = (side - logoSide) / 2f
                canvas.drawBitmap(it, null, RectF(offset, cy - logoSide / 2f, offset + logoSide, cy + logoSide / 2f), logoPaint)
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
