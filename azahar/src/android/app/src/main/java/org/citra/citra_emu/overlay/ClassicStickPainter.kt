// Copyright Pomegrade Project
// Licensed under GPLv2 or any later version

package org.citra.citra_emu.overlay

import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Shader
import kotlin.math.max

/**
 * Pomegrade: the Classic layout's sticks in the filled style, after the New 3DS (owner's mock-up, 18 October), lit
 * from the top left as the caps (ClassicControl3D). Each draws centred on ([c], [c]) of its square image.
 */
object ClassicStickPainter {
    // the caps' forward tilt (ClassicControl3D.VIEW): a knob's side shows below its top
    private const val VIEW = 1.7f
    private const val SEAM = 0xFF1B1C20.toInt()

    /** The circle pad's recess, [r] its radius: shallow, shaded under its upper rim, a faint lit lip below. */
    fun recess(canvas: Canvas, c: Float, r: Float) {
        canvas.drawCircle(c, c, r, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = RadialGradient(c, c, r, intArrayOf(0xFF24262B.toInt(), 0xFF24262B.toInt(), 0xFF202226.toInt(), 0xFF2E3137.toInt()), floatArrayOf(0f, 0.6f, 0.92f, 1f), Shader.TileMode.CLAMP)
        })
        canvas.save()
        canvas.clipPath(Path().apply { addCircle(c, c, r, Path.Direction.CW) })
        canvas.drawCircle(c, c - r * 0.03f * VIEW, r * 1.05f, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = r * 0.14f * VIEW
            color = 0x99000000.toInt()
            maskFilter = blur(r * 0.05f)
        })
        canvas.restore()
        // the image is the recess's size: its lip drawn just inside the edge
        canvas.drawCircle(c, c, r * 0.99f, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = r * 0.02f
            maskFilter = blur(r * 0.012f)
            shader = LinearGradient(0f, c - r, 0f, c + r, intArrayOf(0x59000000, 0x00FFFFFF, 0x24FFFFFF), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
        })
    }

    /** The C stick's seam round its nub of radius [k]. */
    fun nubSeam(canvas: Canvas, c: Float, k: Float) {
        canvas.drawCircle(c, c, k * 1.12f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = SEAM })
    }

    /**
     * A knob of radius [k] centred on ([c], [c]), [height] its height as a share of [k] before the view's tilt: its
     * soft shadow, its side shaded across, its flat top slightly dished (shade under its upper lip, light on its
     * lower wall) with a soft lit rim.
     */
    fun knob(canvas: Canvas, c: Float, k: Float, height: Float) {
        val h = k * height * VIEW
        canvas.drawCircle(c, c + k * 0.06f, k, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x8C000000.toInt()
            maskFilter = blur(k * 0.1f)
        })
        val side = Paint(Paint.ANTI_ALIAS_FLAG)
        for (i in 0..8) {
            val t = i / 8f
            side.shader = LinearGradient(
                c - k, 0f, c + k, 0f,
                intArrayOf(mix(0xFF5E6168.toInt(), 0xFF7E828A.toInt(), t), mix(0xFF8A8E96.toInt(), 0xFFA6A9AF.toInt(), t), mix(0xFF55585F.toInt(), 0xFF73777E.toInt(), t)),
                floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP
            )
            canvas.drawCircle(c, c - h * t, k, side)
        }
        val top = c - h
        canvas.drawCircle(c, top, k, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(c - k * 0.4f, top - k, c + k * 0.3f, top + k, 0xFFC9CBD0.toInt(), 0xFFA3A6AC.toInt(), Shader.TileMode.CLAMP)
        })
        val dish = k * 0.8f
        canvas.save()
        canvas.clipPath(Path().apply { addCircle(c, top, dish, Path.Direction.CW) })
        canvas.drawCircle(c, top - k * 0.04f, dish + k * 0.05f, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = k * 0.14f
            maskFilter = blur(k * 0.07f)
            shader = LinearGradient(0f, top - dish, 0f, top + dish, intArrayOf(0x40000000, 0x00000000, 0x00FFFFFF, 0x8CFFFFFF.toInt()), floatArrayOf(0f, 0.5f, 0.75f, 1f), Shader.TileMode.CLAMP)
        })
        canvas.restore()
        canvas.drawCircle(c, top, k * 0.93f, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = k * 0.05f
            maskFilter = blur(k * 0.03f)
            shader = LinearGradient(0f, top - k, 0f, top + k, intArrayOf(0x8CFFFFFF.toInt(), 0x00FFFFFF, 0x26000000), floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
        })
    }

    private fun blur(radius: Float) = BlurMaskFilter(max(0.5f, radius), BlurMaskFilter.Blur.NORMAL)

    private fun mix(from: Int, to: Int, t: Float): Int {
        fun channel(shift: Int) = (((from shr shift) and 0xFF) + (((to shr shift) and 0xFF) - ((from shr shift) and 0xFF)) * t).toInt()
        return (0xFF shl 24) or (channel(16) shl 16) or (channel(8) shl 8) or channel(0)
    }
}
