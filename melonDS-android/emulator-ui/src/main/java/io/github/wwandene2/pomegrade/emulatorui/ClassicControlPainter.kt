package io.github.wwandene2.pomegrade.emulatorui

import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.max
import kotlin.math.min

/**
 * Pomegrade: how the Classic layouts (DS, 3DS) paint a control, in the two styles of Punica's "Classic layout
 * buttons" setting. Outlined: a white outline. Filled: a key cap in relief (owner's request), lit from above, all
 * inside the shape's own outline so it never leaves the control's rectangle: a darker lip along its bottom (9 % of
 * its smaller side, shaded top to bottom) raises the face, a light-to-darker gradient with a bright rim along its
 * top edge; labels are engraved (dark, with a light edge below). Pressing sinks the face onto the lip ([depth]
 * 0 raised to 1 down) and darkens it.
 *
 * The D-pad has the DS Lite's shape on both consoles (owner's choice): arms 128 / 375 of its side, corners 19.7 /
 * 375, and on each arm the groove the DS Lite has, along its axis from 0.12 to 0.31 of the side from its tip,
 * 0.021 of the side wide (measured on the owner's photo).
 */
object ClassicControlPainter {
    const val INK = 0xFF2B2E35.toInt()
    const val LINE = 0xFFF3F4F6.toInt()
    private const val FACE_TOP = 0xFFF7F8FA.toInt()
    private const val FACE_BOTTOM = 0xFFC9CCD2.toInt()
    private const val LIP_TOP = 0xFF9A9EA6.toInt()
    private const val LIP_BOTTOM = 0xFF6E727A.toInt()
    private const val RIM = 0x8CFFFFFF.toInt()
    private const val PRESSED_SHADE = 0x40000000
    private const val ENGRAVED_EDGE = 0x99FFFFFF.toInt()
    private const val LIP_SHARE = 0.09f

    /** The white face pair of the Punica button, as the pill's Punica blade. */
    val WHITE_FACE = 0xFFFFFFFF.toInt() to 0xFFE2E4E8.toInt()

    /**
     * Paints [path] and returns how far its face is raised now, in pixels (0 when outlined): its label is drawn that
     * much higher. [stroke]: the outline's width. [face]: the face's colours, top to bottom, when not the grey.
     */
    fun paint(canvas: Canvas, path: Path, outlined: Boolean, stroke: Float, depth: Float = 0f, face: Pair<Int, Int>? = null): Float {
        if (outlined) {
            canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE
                strokeWidth = stroke
                color = LINE
            })
            if (depth > 0f) canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x33FFFFFF; alpha = (0x33 * depth).toInt() })
            return 0f
        }
        val bounds = RectF().also { path.computeBounds(it, true) }
        val side = min(bounds.width(), bounds.height())
        val lip = max(1f, side * LIP_SHARE) * (1f - depth.coerceIn(0f, 1f))
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(0f, bounds.top, 0f, bounds.bottom, LIP_TOP, LIP_BOTTOM, Shader.TileMode.CLAMP)
        })
        canvas.save()
        canvas.clipPath(path)
        canvas.translate(0f, -lip)
        val (top, bottom) = face ?: (FACE_TOP to FACE_BOTTOM)
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(0f, bounds.top, 0f, bounds.bottom, top, bottom, Shader.TileMode.CLAMP)
        })
        // the bright rim along the face's top edge: the outline's inner half, moved down, clipped to the face
        canvas.save()
        canvas.clipPath(path)
        canvas.translate(0f, side * 0.025f)
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = side * 0.05f
            shader = LinearGradient(0f, bounds.top, 0f, bounds.centerY(), RIM, 0x00FFFFFF, Shader.TileMode.CLAMP)
        })
        canvas.restore()
        if (depth > 0f) canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = PRESSED_SHADE; alpha = (0x40 * depth).toInt() })
        canvas.restore()
        return lip
    }

    /** A label centred on ([cx], [cy]): engraved on the filled face, white with the outline. */
    fun drawLabel(canvas: Canvas, paint: Paint, text: String, cx: Float, cy: Float, outlined: Boolean) {
        val baseline = cy - (paint.fontMetrics.ascent + paint.fontMetrics.descent) / 2f
        if (!outlined) {
            val color = paint.color
            paint.color = ENGRAVED_EDGE
            canvas.drawText(text, cx, baseline + max(1f, paint.textSize * 0.05f), paint)
            paint.color = color
        }
        paint.color = labelColor(outlined)
        canvas.drawText(text, cx, baseline, paint)
    }

    fun labelColor(outlined: Boolean) = if (outlined) LINE else INK

    /** The D-pad's outline in a square of [side] pixels, both arms as one shape, [inset] in from the square. */
    fun dpadPath(side: Float, inset: Float): Path {
        val arm = side * 128f / 375f
        val corner = side * 19.7f / 375f
        val from = (side - arm) / 2f
        val to = from + arm
        val path = Path().apply { addRoundRect(RectF(from, inset, to, side - inset), corner, corner, Path.Direction.CW) }
        path.op(Path().apply { addRoundRect(RectF(inset, from, side - inset, to), corner, corner, Path.Direction.CW) }, Path.Op.UNION)
        return path
    }

    /** The four grooves of the D-pad's arms, on its face raised by [raised] pixels. */
    fun drawDpadGrooves(canvas: Canvas, side: Float, raised: Float, outlined: Boolean) {
        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeCap = Paint.Cap.ROUND
            strokeWidth = side * 0.021f
            color = if (outlined) LINE else 0x8C2B2E35.toInt()
        }
        val near = side * 0.12f
        val far = side * 0.31f
        val mid = side / 2f
        canvas.save()
        canvas.translate(0f, -raised)
        canvas.drawLine(mid, near, mid, far, paint)
        canvas.drawLine(mid, side - near, mid, side - far, paint)
        canvas.drawLine(near, mid, far, mid, paint)
        canvas.drawLine(side - near, mid, side - far, mid, paint)
        canvas.restore()
    }
}
