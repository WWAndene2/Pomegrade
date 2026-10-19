package io.github.wwandene2.pomegrade.emulatorui

import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import kotlin.math.max
import kotlin.math.sqrt

/**
 * Pomegrade: the Classic layouts' (DS, 3DS) controls drawn as solid key caps, the filled style of Punica's
 * "Classic layout buttons" setting, after the New 3DS (owner's choice, from the interactive mock-up he approved
 * on 18 October): low matte caps nearly flush with the shell, lit from the top left.
 *
 * A cap sits behind a fine seam cut in the shell ([seam], its lower lip catching the light) and casts a soft
 * shadow into it. It is its footprint (the template's shape) seen from slightly in front, the view tilted forward
 * a few degrees ([VIEW]): its side shows below its face, drawn as [SLICES] copies of the outline from the footprint
 * up to the face, shaded across and darker low. The face has a top-left-to-bottom-right gradient, a soft rounded
 * bevel (light on its upper edge, shade on its lower one) and a faint sheen. Pressing moves the face toward the
 * footprint ([Press.sink], 0 up to 1 down, [SINK_TRAVEL] of the height) and a rocker tilts the whole cap
 * ([Press.tilt] along ([Press.dx], [Press.dy]) about its centre: the pressed side down, the other up, sideways
 * at [SIDEWAYS_TILT] of that, a shear reading as a tilt only when small). There is no in-between: a control is
 * up or down, as a real key. Caps, their seams and shadows draw slightly past their rectangle: the screens leave
 * room for it.
 *
 * Heights, shares of the control's smaller side: buttons 0.06, the D-pad 0.045, times [VIEW]; seams 0.03 of
 * buttons, 0.012 of the D-pad.
 *
 * The outlined style stays flat: the outline, filled faintly while pressed.
 */
object ClassicControl3D {
    private const val SLICES = 10
    private const val VIEW = 1.7f
    private const val SIDE_LOW = 0xFF6E727A.toInt()
    private const val SIDE_HIGH = 0xFF9A9EA6.toInt()
    private const val SIDE_LIT = 0xFFB6B9BF.toInt()
    private const val FACE_TOP = 0xFFF1F2F4.toInt()
    private const val FACE_BOTTOM = 0xFFD2D5DA.toInt()
    private const val SEAM = 0xFF1B1C20.toInt()
    private const val BEVEL_SHADE = 0x282C34
    private const val SINK_TRAVEL = 0.75f
    private const val SIDEWAYS_TILT = 0.45f
    private const val TILT_DROP = 1.6f

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
                // sunk to 0.6 of its height, the tilt doing the rest
                return Press(sink = 0.4f / SINK_TRAVEL, tilt = 1f, dx = x / length, dy = y / length)
            }
        }
    }

    /** A round button of diameter [d] centred on ([cx], [cy]); [decorate] draws its label on the face; [stroke]: the outline's width. */
    fun round(canvas: Canvas, cx: Float, cy: Float, d: Float, outlined: Boolean, stroke: Float, press: Press, face: Pair<Int, Int>? = null, decorate: (Canvas) -> Unit) {
        val shape = Path().apply { addCircle(cx, cy, d / 2f - if (outlined) stroke / 2f else 0f, Path.Direction.CW) }
        cap(canvas, shape, RectF(cx - d / 2f, cy - d / 2f, cx + d / 2f, cy + d / 2f), d * 0.06f, d * 0.03f, outlined, stroke, press, face, decorate)
    }

    /** L or R ([isLeft] for L) in [rect], its outer top corner rounded over [corner], the others over [small] (null: bumperPath's default). */
    fun bumper(canvas: Canvas, rect: RectF, corner: Float, isLeft: Boolean, outlined: Boolean, stroke: Float, press: Press, small: Float? = null, decorate: (Canvas) -> Unit) {
        val inset = if (outlined) stroke / 2f else 0f
        val shape = if (small == null) ClassicControlPainter.bumperPath(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset, corner, isLeft)
                    else ClassicControlPainter.bumperPath(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset, corner, isLeft, small)
        cap(canvas, shape, rect, rect.height() * 0.06f, rect.height() * 0.03f, outlined, stroke, press, null, decorate)
    }

    /** ZL or ZR, a rectangle in [rect] with its corners rounded over [corner]. */
    fun zButton(canvas: Canvas, rect: RectF, corner: Float, outlined: Boolean, stroke: Float, press: Press, decorate: (Canvas) -> Unit) {
        val inset = if (outlined) stroke / 2f else 0f
        val r = corner
        val shape = Path().apply { addRoundRect(RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset), r, r, Path.Direction.CW) }
        cap(canvas, shape, rect, rect.height() * 0.06f, rect.height() * 0.03f, outlined, stroke, press, null, decorate)
    }

    /** The D-pad in the square ([left], [top], [side]), the DS Lite's shape with its grooves, rocking as [press]. */
    fun dpad(canvas: Canvas, left: Float, top: Float, side: Float, outlined: Boolean, press: Press) {
        val stroke = side * 4f / 375f
        val shape = ClassicControlPainter.dpadPath(side, if (outlined) stroke / 2f else 0f).apply { offset(left, top) }
        cap(canvas, shape, RectF(left, top, left + side, top + side), side * 0.045f, side * 0.012f, outlined, stroke, press, null) {
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
        height: Float,
        seamWidth: Float,
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
        val small = minOf(bounds.width(), bounds.height())
        val full = height * VIEW
        val h = full * (1f - press.sink * SINK_TRAVEL)
        val down = press.sink > 0f
        seam(canvas, shape, bounds, seamWidth)
        // the contact shadow inside the seam
        canvas.save()
        canvas.translate(0f, small * 0.015f * VIEW)
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = alpha(0xFF000000.toInt(), if (down) 0.35f else 0.45f)
            maskFilter = blur(small * if (down) 0.015f else 0.035f)
        })
        canvas.restore()
        val matrix = Matrix()
        val sidePaint = Paint(Paint.ANTI_ALIAS_FLAG)
        for (i in 0..SLICES) {
            val t = i.toFloat() / SLICES
            faceMatrix(matrix, bounds, h, press, t)
            canvas.save()
            canvas.concat(matrix)
            sidePaint.shader = LinearGradient(
                bounds.left, 0f, bounds.right, 0f,
                intArrayOf(mix(SIDE_LOW, SIDE_HIGH, t * 0.7f), mix(SIDE_HIGH, SIDE_LIT, t), mix(SIDE_LOW, SIDE_HIGH, t * 0.5f)),
                floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP
            )
            canvas.drawPath(shape, sidePaint)
            canvas.restore()
        }
        faceMatrix(matrix, bounds, h, press, 1f)
        canvas.save()
        canvas.concat(matrix)
        val (top, bottom) = face ?: (FACE_TOP to FACE_BOTTOM)
        // the face: matte, light falling from the top left
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(bounds.left, bounds.top, bounds.left + bounds.width() * 0.4f, bounds.bottom, top, bottom, Shader.TileMode.CLAMP)
        })
        canvas.save()
        canvas.clipPath(shape)
        // the rounded bevel: a light roll on the upper edge, a shade on the lower one, both soft
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = small * 0.09f
            maskFilter = blur(small * 0.035f)
            shader = LinearGradient(
                0f, bounds.top, 0f, bounds.bottom,
                intArrayOf(0xE6FFFFFF.toInt(), 0x00FFFFFF, BEVEL_SHADE, alpha(0xFF000000.toInt() or BEVEL_SHADE, 0.4f)),
                floatArrayOf(0f, 0.35f, 0.7f, 1f), Shader.TileMode.CLAMP
            )
        })
        // a broad, faint sheen toward the light
        canvas.save()
        val sheenX = bounds.left + bounds.width() * 0.38f
        val sheenY = bounds.top + bounds.height() * 0.3f
        canvas.rotate(-20f, sheenX, sheenY)
        canvas.drawOval(
            RectF(sheenX - bounds.width() * 0.28f, sheenY - bounds.height() * 0.16f, sheenX + bounds.width() * 0.28f, sheenY + bounds.height() * 0.16f),
            Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x38FFFFFF; maskFilter = blur(small * 0.12f) }
        )
        canvas.restore()
        // a rocker: its lowered side shaded, its raised one lit
        if (press.tilt > 0f) {
            val cx = bounds.centerX()
            val cy = bounds.centerY()
            val r = small / 2f
            canvas.drawPaint(Paint().apply {
                shader = LinearGradient(
                    cx - press.dx * r, cy - press.dy * r, cx + press.dx * r, cy + press.dy * r,
                    alpha(0xFFFFFFFF.toInt(), 0.18f * press.tilt), alpha(0xFF000000.toInt(), 0.22f * press.tilt), Shader.TileMode.CLAMP
                )
            })
        }
        if (down) canvas.drawColor(alpha(0xFF14161A.toInt(), 0.1f))
        canvas.restore()
        decorate(canvas)
        canvas.restore()
    }

    // the seam round a control: a fine dark gap [width] wide, its upper side shaded, a faint lit lip below
    private fun seam(canvas: Canvas, shape: Path, bounds: RectF, width: Float) {
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = width * 3f
            maskFilter = blur(width * 0.3f)
            shader = LinearGradient(
                0f, bounds.top - width, 0f, bounds.bottom + width,
                intArrayOf(0x4D000000, 0x00FFFFFF, 0x1FFFFFFF), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP
            )
        })
        canvas.drawPath(shape, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL_AND_STROKE
            strokeWidth = width * 2f
            strokeJoin = Paint.Join.ROUND
            color = SEAM
        })
    }

    // a point's place at height t (0 the footprint, 1 the face): lifted by h, and tilted about the centre at that
    // height by up to TILT_DROP h at the tips (the pressed side down on screen, the other up)
    private fun faceMatrix(matrix: Matrix, bounds: RectF, h: Float, press: Press, t: Float) {
        val lift = -h * t
        val k = t * press.tilt * TILT_DROP * h / (minOf(bounds.width(), bounds.height()) / 2f)
        val cx = bounds.centerX()
        val cy = bounds.centerY() + lift
        // y' = y + lift + k (s dx (x - cx) + dy (y - cy)), with s the sideways share
        val kx = k * SIDEWAYS_TILT * press.dx
        val ky = k * press.dy
        matrix.setValues(floatArrayOf(
            1f, 0f, 0f,
            kx, 1f + ky, lift - kx * cx - ky * cy,
            0f, 0f, 1f,
        ))
    }

    // a blur of [radius] pixels; Android 10+ (Pomegrade's minimum) draws mask filters with hardware acceleration
    private fun blur(radius: Float) = BlurMaskFilter(max(0.5f, radius), BlurMaskFilter.Blur.NORMAL)

    private fun mix(from: Int, to: Int, t: Float): Int {
        fun channel(shift: Int) = (((from shr shift) and 0xFF) + (((to shr shift) and 0xFF) - ((from shr shift) and 0xFF)) * t).toInt()
        return (0xFF shl 24) or (channel(16) shl 16) or (channel(8) shl 8) or channel(0)
    }

    private fun alpha(color: Int, share: Float) = (((share.coerceIn(0f, 1f) * 255).toInt()) shl 24) or (color and 0xFFFFFF)
}
