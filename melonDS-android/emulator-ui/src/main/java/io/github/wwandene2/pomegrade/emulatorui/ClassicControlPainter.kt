package io.github.wwandene2.pomegrade.emulatorui

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import kotlin.math.max

/**
 * Pomegrade: how the Classic layouts (DS, 3DS) paint a control, in the two styles of Punica's "Classic layout
 * buttons" setting: the shapes, the outline, the labels and the D-pad's grooves. Filled caps are drawn by
 * ClassicControl3D; their labels and grooves are engraved (dark, with a light edge below).
 *
 * The D-pad has the DS Lite's shape on both consoles (owner's choice): arms 128 / 375 of its side, corners 19.7 /
 * 375, and on each arm the groove the DS Lite has, along its axis from 0.12 to 0.31 of the side from its tip,
 * 0.021 of the side wide (measured on the owner's photo).
 */
object ClassicControlPainter {
    const val LINE = 0xFFF3F4F6.toInt()
    private const val ENGRAVED_EDGE = 0xBFFFFFFF.toInt()
    // the engraved labels' and grooves' ink on the New 3DS caps (owner's mock-up, 18 October)
    private const val ENGRAVED_INK = 0xFF4A4E57.toInt()
    private const val GROOVE = 0xFF8A8E96.toInt()
    private const val GROOVE_EDGE = 0xB3FFFFFF.toInt()

    /** The white face pair of the Punica button, as the pill's Punica blade. */
    val WHITE_FACE = 0xFFFFFFFF.toInt() to 0xFFE2E4E8.toInt()

    /** Outlines [path], [stroke] wide, filled faintly while pressed ([depth] 0 up to 1 down). */
    fun outline(canvas: Canvas, path: Path, stroke: Float, depth: Float = 0f) {
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = stroke
            color = LINE
        })
        if (depth > 0f) canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x33FFFFFF; alpha = (0x33 * depth).toInt() })
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

    fun labelColor(outlined: Boolean) = if (outlined) LINE else ENGRAVED_INK

    /**
     * A shoulder button (L / R) between [left], [top], [right], [bottom]: its outer top corner rounded over [corner],
     * its three other corners over [small], by default very slightly, 4 % of its height (owner's choice); [isLeft] for
     * L, whose outer side is on the left.
     */
    fun bumperPath(left: Float, top: Float, right: Float, bottom: Float, corner: Float, isLeft: Boolean, small: Float = (bottom - top) * 0.04f): Path {
        // radii from the top left, clockwise
        val radii = if (isLeft) listOf(corner, small, small, small) else listOf(small, corner, small, small)
        return Path().apply {
            addRoundRect(RectF(left, top, right, bottom), radii.flatMap { listOf(it, it) }.toFloatArray(), Path.Direction.CW)
        }
    }

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
            color = if (outlined) LINE else GROOVE
        }
        // filled: engraved, a light edge below each groove
        val edge = if (outlined) null else Paint(paint).apply { color = GROOVE_EDGE }
        val near = side * 0.12f
        val far = side * 0.31f
        val mid = side / 2f
        canvas.save()
        canvas.translate(0f, -raised)
        if (edge != null) {
            val below = paint.strokeWidth * 0.5f
            canvas.drawLine(mid, near + below, mid, far + below, edge)
            canvas.drawLine(mid, side - near + below, mid, side - far + below, edge)
            canvas.drawLine(near, mid + below, far, mid + below, edge)
            canvas.drawLine(side - near, mid + below, side - far, mid + below, edge)
        }
        canvas.drawLine(mid, near, mid, far, paint)
        canvas.drawLine(mid, side - near, mid, side - far, paint)
        canvas.drawLine(near, mid, far, mid, paint)
        canvas.drawLine(side - near, mid, side - far, mid, paint)
        canvas.restore()
    }
}
