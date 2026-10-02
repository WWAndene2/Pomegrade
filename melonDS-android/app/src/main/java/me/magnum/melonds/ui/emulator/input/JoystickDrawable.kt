package me.magnum.melonds.ui.emulator.input

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import kotlin.math.min

/**
 * Pomegrade: the on-screen joystick drawn in the D-pad's place: a ring and a knob that follows the finger. Sizes are shares of the
 * component's size, so it scales with the layout.
 */
class JoystickDrawable : Drawable() {
    private val ringPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        color = 0xFFFFFFFF.toInt()
    }
    private val basePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0x40FFFFFF
    }
    private val knobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = 0xC0FFFFFF.toInt()
    }
    private val paints = listOf(ringPaint to 0xFF, basePaint to 0x40, knobPaint to 0xC0) // each with its own alpha
    private var knobX = 0f
    private var knobY = 0f

    /** x to the right, y up, each -1 to 1 */
    fun setKnobPosition(x: Float, y: Float) {
        if (x != knobX || y != knobY) {
            knobX = x
            knobY = y
            invalidateSelf()
        }
    }

    override fun draw(canvas: Canvas) {
        val size = min(bounds.width(), bounds.height()).toFloat()
        val ringWidth = size / 32f
        val knobRadius = size / 4f
        val ringRadius = size / 2f - ringWidth
        ringPaint.strokeWidth = ringWidth
        val cx = bounds.exactCenterX()
        val cy = bounds.exactCenterY()
        canvas.drawCircle(cx, cy, ringRadius, basePaint)
        canvas.drawCircle(cx, cy, ringRadius, ringPaint)
        // the knob's centre travels up to the ring, less its own radius
        val travel = ringRadius - knobRadius
        canvas.drawCircle(cx + knobX * travel, cy - knobY * travel, knobRadius, knobPaint)
    }

    override fun setAlpha(alpha: Int) {
        paints.forEach { (paint, ownAlpha) -> paint.alpha = ownAlpha * alpha / 255 }
        invalidateSelf()
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        paints.forEach { (paint, _) -> paint.colorFilter = colorFilter }
        invalidateSelf()
    }

    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT
}
