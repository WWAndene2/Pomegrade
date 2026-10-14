package me.magnum.melonds.ui.emulator.input

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
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import me.magnum.melonds.domain.model.input.ClassicButtonStyle
import kotlin.math.min

/**
 * Pomegrade: a button of the Classic layout drawn as the Punica DS skin template draws it
 * (assets/consoles/skins/gabarit_skin_ds_1220x2712.svg), so the real button fills its template rectangle: every
 * size is a share of that rectangle, taken from the template (L / R 328 x 151 with a 64 corner, D-pad 375 with
 * 128 arms and 19.7 corners, ABXY 467 with buttons of 151.2, SELECT / START / Punica 72). Outlines are 4 template
 * pixels wide, as on the template.
 */
class ClassicButtonDrawable(context: Context, private val kind: Kind, private val style: ClassicButtonStyle) : Drawable() {

    enum class Kind { L, R, DPAD, BUTTONS, START, SELECT, PUNICA }

    private companion object {
        const val FILL = 0xFFDCDEE2.toInt()
        const val INK = 0xFF2B2E35.toInt()
        const val LINE = 0xFFF3F4F6.toInt()

        // ABXY: a button's diameter and centres (X, Y, A, B), shares of the cluster's side
        const val FACE_DIAMETER = 151.2f / 467f
        val FACE_CENTRES = listOf(
            "X" to (0.5f to 0.1606f),
            "Y" to (0.1630f to 0.4980f),
            "A" to (0.8370f to 0.4980f),
            "B" to (0.5f to 0.8357f),
        )
    }

    private val shapePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        if (style == ClassicButtonStyle.FILLED) {
            this.style = Paint.Style.FILL
            color = FILL
        } else {
            this.style = Paint.Style.STROKE
            color = LINE
        }
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = if (style == ClassicButtonStyle.FILLED) INK else LINE
        textAlign = Paint.Align.CENTER
        // Punica's title face, as the app's labels
        typeface = ResourcesCompat.getFont(context, EmulatorMenuR.font.punica_bricolage_grotesque_bold) ?: Typeface.DEFAULT_BOLD
    }
    private val logo: Bitmap? = if (kind == Kind.PUNICA) BitmapFactory.decodeResource(context.resources, EmulatorMenuR.drawable.emulator_menu_ic_punica) else null
    private val logoPaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)
    private val path = Path()
    private val rect = RectF()

    override fun draw(canvas: Canvas) {
        val b = bounds
        val w = b.width().toFloat()
        val h = b.height().toFloat()
        if (w <= 0f || h <= 0f) return
        canvas.save()
        canvas.translate(b.left.toFloat(), b.top.toFloat())
        when (kind) {
            Kind.L, Kind.R -> drawBumper(canvas, w, h)
            Kind.DPAD -> drawDpad(canvas, min(w, h))
            Kind.BUTTONS -> drawFaceButtons(canvas, min(w, h))
            Kind.START -> drawRound(canvas, min(w, h), "START")
            Kind.SELECT -> drawRound(canvas, min(w, h), "SELECT")
            Kind.PUNICA -> drawRound(canvas, min(w, h), null)
        }
        canvas.restore()
    }

    // 328 x 151: flat on three sides, the outer top corner rounded over 64 / 151 of the height
    private fun drawBumper(canvas: Canvas, w: Float, h: Float) {
        // fitted at the template's 328:151, centred, whatever the rectangle the layout gives
        val scale = min(w / 328f, h / 151f)
        val bw = 328f * scale
        val bh = 151f * scale
        val left = (w - bw) / 2f
        val top = (h - bh) / 2f
        val stroke = 4f * scale
        shapePaint.strokeWidth = stroke
        val inset = if (style == ClassicButtonStyle.OUTLINE) stroke / 2f else 0f
        val l = left + inset
        val t = top + inset
        val r = left + bw - inset
        val bt = top + bh - inset
        val c = 64f * scale
        path.reset()
        if (kind == Kind.L) {
            path.moveTo(l + c, t)
            path.lineTo(r, t)
            path.lineTo(r, bt)
            path.lineTo(l, bt)
            path.lineTo(l, t + c)
            path.arcTo(l, t, l + 2 * c, t + 2 * c, 180f, 90f, false)
        } else {
            path.moveTo(l, t)
            path.lineTo(r - c, t)
            path.arcTo(r - 2 * c, t, r, t + 2 * c, 270f, 90f, false)
            path.lineTo(r, bt)
            path.lineTo(l, bt)
        }
        path.close()
        canvas.drawPath(path, shapePaint)
        textPaint.textSize = 34f * scale
        drawCentredText(canvas, if (kind == Kind.L) "L" else "R", left + bw / 2f, top + bh / 2f)
    }

    // 375: arms of 128, corners of 19.7
    private fun drawDpad(canvas: Canvas, side: Float) {
        val scale = side / 375f
        val stroke = 4f * scale
        shapePaint.strokeWidth = stroke
        val inset = if (style == ClassicButtonStyle.OUTLINE) stroke / 2f else 0f
        val arm = 128f * scale
        val corner = 19.7f * scale
        val from = (side - arm) / 2f
        val to = from + arm
        // the two arms' union: one outline, no inner edges where they cross
        path.reset()
        path.addRoundRect(RectF(from, inset, to, side - inset), corner, corner, Path.Direction.CW)
        val horizontal = Path().apply { addRoundRect(RectF(inset, from, side - inset, to), corner, corner, Path.Direction.CW) }
        path.op(horizontal, Path.Op.UNION)
        canvas.drawPath(path, shapePaint)
    }

    private fun drawFaceButtons(canvas: Canvas, side: Float) {
        val scale = side / 467f
        shapePaint.strokeWidth = 4f * scale
        val radius = FACE_DIAMETER * side / 2f - if (style == ClassicButtonStyle.OUTLINE) 2f * scale else 0f
        textPaint.textSize = 39f * scale
        FACE_CENTRES.forEach { (label, centre) ->
            val cx = centre.first * side
            val cy = centre.second * side
            canvas.drawCircle(cx, cy, radius, shapePaint)
            drawCentredText(canvas, label, cx, cy)
        }
    }

    // SELECT, START (their names inside, fitted to the circle) and the Punica button (the pill's logo)
    private fun drawRound(canvas: Canvas, side: Float, label: String?) {
        val scale = side / 72f
        val stroke = 4f * scale
        shapePaint.strokeWidth = stroke
        val radius = side / 2f - if (style == ClassicButtonStyle.OUTLINE) stroke / 2f else 0f
        if (label == null && style == ClassicButtonStyle.FILLED) {
            // the logo on white, as the pill's Punica blade
            shapePaint.color = 0xFFFFFFFF.toInt()
        }
        canvas.drawCircle(side / 2f, side / 2f, radius, shapePaint)
        if (label != null) {
            textPaint.textSize = 14f * scale
            val width = textPaint.measureText(label)
            val room = side * 0.8f
            if (width > room) textPaint.textSize *= room / width
            drawCentredText(canvas, label, side / 2f, side / 2f)
        } else {
            logo?.let {
                val logoSide = side * 0.64f
                val offset = (side - logoSide) / 2f
                rect.set(offset, offset, offset + logoSide, offset + logoSide)
                canvas.drawBitmap(it, null, rect, logoPaint)
            }
        }
    }

    private fun drawCentredText(canvas: Canvas, text: String, cx: Float, cy: Float) {
        val metrics = textPaint.fontMetrics
        canvas.drawText(text, cx, cy - (metrics.ascent + metrics.descent) / 2f, textPaint)
    }

    override fun setAlpha(alpha: Int) {
        shapePaint.alpha = alpha
        textPaint.alpha = alpha
        logoPaint.alpha = alpha
        invalidateSelf()
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        shapePaint.colorFilter = colorFilter
        textPaint.colorFilter = colorFilter
        logoPaint.colorFilter = colorFilter
        invalidateSelf()
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT
}
