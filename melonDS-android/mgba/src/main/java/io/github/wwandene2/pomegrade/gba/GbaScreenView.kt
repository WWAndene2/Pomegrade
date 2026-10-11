package io.github.wwandene2.pomegrade.gba

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * The GBA screen: the core's frame, scaled to the largest size that keeps its 3:2 shape, centred on
 * black, without smoothing (sharp pixels).
 */
class GbaScreenView @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) : View(context, attrs) {

    /** The core writes each frame here (see GbaNative.load). */
    internal val frameBuffer: ByteBuffer = ByteBuffer.allocateDirect(GbaNative.WIDTH * GbaNative.HEIGHT * 4).order(ByteOrder.nativeOrder())

    private val bitmap = Bitmap.createBitmap(GbaNative.WIDTH, GbaNative.HEIGHT, Bitmap.Config.ARGB_8888)
    private val paint = Paint().apply { isFilterBitmap = false }
    private val target = Rect()
    private val lock = Any()

    /** Takes the frame the core just wrote; called from the emulation thread. */
    internal fun publishFrame() {
        synchronized(lock) {
            frameBuffer.rewind()
            bitmap.copyPixelsFromBuffer(frameBuffer)
        }
        postInvalidateOnAnimation()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        val scale = minOf(w.toFloat() / GbaNative.WIDTH, h.toFloat() / GbaNative.HEIGHT)
        val width = (GbaNative.WIDTH * scale).toInt()
        val height = (GbaNative.HEIGHT * scale).toInt()
        // in portrait the screen sits at the top, leaving the lower half to the controls
        val top = if (h > w) 0 else (h - height) / 2
        target.set((w - width) / 2, top, (w + width) / 2, top + height)
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(Color.BLACK)
        synchronized(lock) {
            canvas.drawBitmap(bitmap, null, target, paint)
        }
    }
}
