package io.github.wwandene2.pomegrade.gba

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View

/**
 * The on-screen GBA buttons: the D-pad and Select on the left, A, B and Start on the right, L and R
 * at the top corners. In portrait they fill the space under the screen; in landscape they sit over
 * the sides of the game. Several fingers at once; the D-pad's diagonals press two directions.
 */
class GbaControlsView @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) : View(context, attrs) {

    private class Button(val key: Int, val label: String, val round: Boolean) {
        val area = RectF()
    }

    /** The keys the fingers hold now, read by the emulation thread every frame. */
    @Volatile
    internal var keys = 0
        private set

    private val buttons = listOf(
        Button(GbaKeys.A, "A", true),
        Button(GbaKeys.B, "B", true),
        Button(GbaKeys.L, "L", false),
        Button(GbaKeys.R, "R", false),
        Button(GbaKeys.START, "START", false),
        Button(GbaKeys.SELECT, "SELECT", false),
    )
    private val dpad = RectF()

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(96, 255, 255, 255) }
    private val pressedFill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(176, 255, 255, 255) }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(224, 32, 32, 32)
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        val portrait = h > w
        // the controls' band: under the 3:2 screen in portrait, the whole view in landscape
        val bandTop = if (portrait) w * GbaNative.HEIGHT / GbaNative.WIDTH.toFloat() else 0f
        val bandHeight = h - bandTop
        val unit = minOf(w / 8f, bandHeight / 5f)
        val margin = unit * 0.4f
        val bottom = h - margin

        val dpadSize = unit * 3f
        dpad.set(margin, bottom - dpadSize - unit, margin + dpadSize, bottom - unit)
        val aSize = unit * 1.3f
        val right = w - margin
        area(GbaKeys.A).set(right - aSize, dpad.centerY() - aSize * 1.1f, right, dpad.centerY() - aSize * 0.1f)
        area(GbaKeys.B).set(right - aSize * 2.2f, dpad.centerY() - aSize * 0.1f, right - aSize * 1.2f, dpad.centerY() + aSize * 0.9f)

        val wide = unit * 1.6f
        val thin = unit * 0.6f
        area(GbaKeys.SELECT).set(w / 2f - wide - margin / 2, bottom - thin, w / 2f - margin / 2, bottom)
        area(GbaKeys.START).set(w / 2f + margin / 2, bottom - thin, w / 2f + wide + margin / 2, bottom)

        val shoulderTop = if (portrait) bandTop + margin else margin * 2
        area(GbaKeys.L).set(margin, shoulderTop, margin + wide * 1.4f, shoulderTop + thin * 1.2f)
        area(GbaKeys.R).set(w - margin - wide * 1.4f, shoulderTop, w - margin, shoulderTop + thin * 1.2f)
        text.textSize = unit * 0.38f
    }

    private fun area(key: Int) = buttons.first { it.key == key }.area

    override fun onDraw(canvas: Canvas) {
        val held = keys
        // the D-pad: a cross of its four arms
        val third = dpad.width() / 3
        val arms = listOf(
            GbaKeys.UP to RectF(dpad.left + third, dpad.top, dpad.right - third, dpad.top + third),
            GbaKeys.DOWN to RectF(dpad.left + third, dpad.bottom - third, dpad.right - third, dpad.bottom),
            GbaKeys.LEFT to RectF(dpad.left, dpad.top + third, dpad.left + third, dpad.bottom - third),
            GbaKeys.RIGHT to RectF(dpad.right - third, dpad.top + third, dpad.right, dpad.bottom - third),
        )
        canvas.drawRect(dpad.left + third, dpad.top + third, dpad.right - third, dpad.bottom - third, fill)
        for ((key, arm) in arms) canvas.drawRoundRect(arm, third / 5, third / 5, if (held and key != 0) pressedFill else fill)

        for (button in buttons) {
            val paint = if (held and button.key != 0) pressedFill else fill
            val r = button.area
            if (button.round) canvas.drawOval(r, paint) else canvas.drawRoundRect(r, r.height() / 2, r.height() / 2, paint)
            canvas.drawText(button.label, r.centerX(), r.centerY() - (text.descent() + text.ascent()) / 2, text)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        var pressed = 0
        val lifted = event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL
        for (i in 0 until event.pointerCount) {
            if (lifted) break
            if (event.actionMasked == MotionEvent.ACTION_POINTER_UP && i == event.actionIndex) continue
            pressed = pressed or keysAt(event.getX(i), event.getY(i))
        }
        if (pressed and keys.inv() != 0) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        if (pressed != keys) {
            keys = pressed
            invalidate()
        }
        return true
    }

    // the keys under a finger: a button, or one or two D-pad directions (the pad's diagonals)
    private fun keysAt(x: Float, y: Float): Int {
        buttons.firstOrNull { it.area.contains(x, y) }?.let { return it.key }
        val slack = dpad.width() * 0.2f
        if (x < dpad.left - slack || x > dpad.right + slack || y < dpad.top - slack || y > dpad.bottom + slack) return 0
        val dx = (x - dpad.centerX()) / (dpad.width() / 2)
        val dy = (y - dpad.centerY()) / (dpad.height() / 2)
        val dead = 0.25f
        var keys = 0
        if (dx > dead) keys = keys or GbaKeys.RIGHT
        if (dx < -dead) keys = keys or GbaKeys.LEFT
        if (dy > dead) keys = keys or GbaKeys.DOWN
        if (dy < -dead) keys = keys or GbaKeys.UP
        return keys
    }
}
