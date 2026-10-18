package io.github.wwandene2.pomegrade.emulatorui

import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.sqrt

/**
 * Pomegrade: the Classic layouts' (DS, 3DS) controls drawn as solid key caps, the filled style of Punica's
 * "Classic layout buttons" setting (owner's choice, from the interactive prototype he approved).
 *
 * A cap is its footprint (the template's shape) seen from above and slightly in front, lifted by its thickness H:
 * its sides show below its face, drawn as [SLICES] copies of the outline from the footprint up to the face, darker
 * low. Pressing moves the face toward the footprint ([Press.sink], 0 up to 1 down, 80 % of H) and a rocker tilts it
 * ([Press.tilt] along ([Press.dx], [Press.dy]) about its centre: the pressed side down, the other up). There is no
 * in-between: a control is up or down, as a real key. The face carries a top-to-bottom gradient, a bright rim on
 * its top edge, a shade on its lowered side and, sunk, a slight darkening. Caps can draw past their rectangle by up
 * to their thickness (the D-pad tilted, twice): the screens leave room for it.
 *
 * Thicknesses, shares of the control: a round button 0.14 of its diameter, L / R / ZL / ZR 0.12 of their height,
 * the D-pad 0.075 of its side, tilting 1.2 times that at its tips.
 *
 * The outlined style stays flat: the outline, filled faintly while pressed.
 */
object ClassicControl3D {
    private const val SLICES = 12
    private const val SIDE_LOW = 0xFF5F636B.toInt()
    private const val SIDE_HIGH = 0xFFA3A7AF.toInt()
    private const val FACE_TOP = 0xFFF7F8FA.toInt()
    private const val FACE_BOTTOM = 0xFFC9CCD2.toInt()
    private const val SINK_TRAVEL = 0.8f

    /** How a control is pressed: [sink] 0 up to 1 down; [tilt] 0 or 1 toward the unit direction ([dx], [dy]). */
    data class Press(val sink: Float = 0f, val tilt: Float = 0f, val dx: Float = 0f, val dy: Float = 0f) {
        companion object {
            val UP = Press()
            val DOWN = Press(sink = 1f)

            /** The D-pad rocking toward the pressed directions (8 of them), or up when none is pressed. */
            fun rocker(up: Boolean, down: Boolean, left: Boolean, right: Boolean): Press {
                val x = (if (right) 1f else 0f) - (if (left) 1f else 0f)
                val y = (if (down) 1f else 0f) - (if (up) 1f else 0f)
                if (x == 0f && y == 0f) return UP
                val length = sqrt(x * x + y * y)
                return Press(sink = 0.25f, tilt = 1f, dx = x / length, dy = y / length)
            }
        }
    }

    /** A round button of diameter [d] centred on ([cx], [cy]); [decorate] draws its label on the face; [stroke]: the outline's width. */
    fun round(canvas: Canvas, cx: Float, cy: Float, d: Float, outlined: Boolean, stroke: Float, press: Press, face: Pair<Int, Int>? = null, decorate: (Canvas) -> Unit) {
        val shape = Path().apply { addCircle(cx, cy, d / 2f - if (outlined) stroke / 2f else 0f, Path.Direction.CW) }
        cap(canvas, shape, RectF(cx - d / 2f, cy - d / 2f, cx + d / 2f, cy + d / 2f), d * 0.14f, 0f, outlined, stroke, press, face, decorate)
    }

    /** L or R ([isLeft] for L) in [rect], its outer top corner rounded over [corner]. */
    fun bumper(canvas: Canvas, rect: RectF, corner: Float, isLeft: Boolean, outlined: Boolean, stroke: Float, press: Press, decorate: (Canvas) -> Unit) {
        val inset = if (outlined) stroke / 2f else 0f
        val shape = ClassicControlPainter.bumperPath(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset, corner, isLeft)
        cap(canvas, shape, rect, rect.height() * 0.12f, 0f, outlined, stroke, press, null, decorate)
    }

    /** ZL or ZR, a rectangle in [rect] with its corners rounded over [corner]. */
    fun zButton(canvas: Canvas, rect: RectF, corner: Float, outlined: Boolean, stroke: Float, press: Press, decorate: (Canvas) -> Unit) {
        val inset = if (outlined) stroke / 2f else 0f
        val r = corner
        val shape = Path().apply { addRoundRect(RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset), r, r, Path.Direction.CW) }
        cap(canvas, shape, rect, rect.height() * 0.12f, 0f, outlined, stroke, press, null, decorate)
    }

    /** The D-pad in the square ([left], [top], [side]), the DS Lite's shape with its grooves, rocking as [press]. */
    fun dpad(canvas: Canvas, left: Float, top: Float, side: Float, outlined: Boolean, press: Press) {
        val stroke = side * 4f / 375f
        val shape = ClassicControlPainter.dpadPath(side, if (outlined) stroke / 2f else 0f).apply { offset(left, top) }
        cap(canvas, shape, RectF(left, top, left + side, top + side), side * 0.075f, 1.2f, outlined, stroke, press, null) {
            it.save()
            it.translate(left, top)
            ClassicControlPainter.drawDpadGrooves(it, side, 0f, outlined)
            it.restore()
        }
    }

    private fun cap(
        canvas: Canvas,
        shape: Path,
        bounds: RectF,
        thickness: Float,
        tiltDepth: Float,
        outlined: Boolean,
        stroke: Float,
        press: Press,
        face: Pair<Int, Int>?,
        decorate: (Canvas) -> Unit,
    ) {
        if (outlined) {
            canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE
                strokeWidth = stroke
                color = ClassicControlPainter.LINE
            })
            if (press.sink > 0f || press.tilt > 0f) canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x33FFFFFF })
            decorate(canvas)
            return
        }
        val radius = maxOf(bounds.width(), bounds.height()) / 2f
        val matrix = Matrix()
        val sidePaint = Paint(Paint.ANTI_ALIAS_FLAG)
        for (i in 0 until SLICES) {
            val t = i.toFloat() / SLICES
            faceMatrix(matrix, bounds, thickness, tiltDepth, radius, press, t)
            canvas.save()
            canvas.concat(matrix)
            sidePaint.color = mix(SIDE_LOW, SIDE_HIGH, t)
            canvas.drawPath(shape, sidePaint)
            canvas.restore()
        }
        faceMatrix(matrix, bounds, thickness, tiltDepth, radius, press, 1f)
        canvas.save()
        canvas.concat(matrix)
        val (top, bottom) = face ?: (FACE_TOP to FACE_BOTTOM)
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(0f, bounds.top, 0f, bounds.bottom, top, bottom, Shader.TileMode.CLAMP)
        })
        val smaller = minOf(bounds.width(), bounds.height())
        // the bright rim along the face's top edge
        canvas.save()
        canvas.clipPath(shape)
        canvas.translate(0f, smaller * 0.02f)
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = smaller * 0.05f
            shader = LinearGradient(0f, bounds.top, 0f, bounds.centerY(), 0x99FFFFFF.toInt(), 0x00FFFFFF, Shader.TileMode.CLAMP)
        })
        canvas.restore()
        // the lowered side shaded, the raised one lit; a sunk face slightly darker
        canvas.save()
        canvas.clipPath(shape)
        if (press.tilt > 0f) {
            val cx = bounds.centerX()
            val cy = bounds.centerY()
            canvas.drawPaint(Paint().apply {
                shader = LinearGradient(
                    cx - press.dx * radius, cy - press.dy * radius, cx + press.dx * radius, cy + press.dy * radius,
                    alpha(0xFFFFFFFF.toInt(), 0.25f * press.tilt), alpha(0xFF000000.toInt(), 0.3f * press.tilt), Shader.TileMode.CLAMP
                )
            })
        }
        if (press.sink > 0f) canvas.drawColor(alpha(0xFF000000.toInt(), 0.12f * press.sink))
        canvas.restore()
        decorate(canvas)
        canvas.restore()
    }

    // a point's place at height t (0 the footprint, 1 the face): lifted, sunk, and tilted about the centre
    private fun faceMatrix(matrix: Matrix, bounds: RectF, thickness: Float, tiltDepth: Float, radius: Float, press: Press, t: Float) {
        val lift = -thickness * (1f - press.sink * SINK_TRAVEL) * t
        val k = t * press.tilt * thickness * tiltDepth / radius
        val cx = bounds.centerX()
        val cy = bounds.centerY()
        // y' = y + lift + k dx (x - cx) + k dy (y - cy)
        matrix.setValues(floatArrayOf(
            1f, 0f, 0f,
            k * press.dx, 1f + k * press.dy, lift - k * press.dx * cx - k * press.dy * cy,
            0f, 0f, 1f,
        ))
    }

    private fun mix(from: Int, to: Int, t: Float): Int {
        fun channel(shift: Int) = (((from shr shift) and 0xFF) + (((to shr shift) and 0xFF) - ((from shr shift) and 0xFF)) * t).toInt()
        return (0xFF shl 24) or (channel(16) shl 16) or (channel(8) shl 8) or channel(0)
    }

    private fun alpha(color: Int, share: Float) = (((share.coerceIn(0f, 1f) * 255).toInt()) shl 24) or (color and 0xFFFFFF)
}
