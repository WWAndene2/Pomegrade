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
 * Pomegrade: how the Classic layouts (DS, 3DS) paint a control's shape, in the two styles of Punica's "Classic
 * layout buttons" setting. Outlined: a white outline. Filled: a key cap in relief (owner's request), lit from
 * above: its face a light-to-darker gradient, raised by a darker lip along its bottom (7 % of its smaller side),
 * all inside the shape's own outline so it never leaves the control's rectangle; pressed, the face sinks onto the
 * lip.
 */
object ClassicControlPainter {
    const val INK = 0xFF2B2E35.toInt()
    const val LINE = 0xFFF3F4F6.toInt()
    private const val FACE_TOP = 0xFFF2F3F5.toInt()
    private const val FACE_BOTTOM = 0xFFCDD0D5.toInt()
    private const val LIP = 0xFF9DA1A9.toInt()
    private const val LIP_SHARE = 0.07f

    /**
     * Paints [path] and returns how far its face is raised, in pixels (0 when outlined or pressed): a label is
     * drawn that much higher, on the face. [stroke]: the outline's width. [face]: the face's colour pair, top to
     * bottom, when it is not the light grey (the Punica button's white).
     */
    fun paint(canvas: Canvas, path: Path, outlined: Boolean, stroke: Float, pressed: Boolean = false, face: Pair<Int, Int>? = null): Float {
        if (outlined) {
            canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE
                strokeWidth = stroke
                color = LINE
            })
            return 0f
        }
        val bounds = RectF().also { path.computeBounds(it, true) }
        val lip = if (pressed) 0f else max(1f, min(bounds.width(), bounds.height()) * LIP_SHARE)
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = LIP })
        canvas.save()
        canvas.clipPath(path)
        canvas.translate(0f, -lip)
        val (top, bottom) = face ?: (FACE_TOP to FACE_BOTTOM)
        canvas.drawPath(path, Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(0f, bounds.top, 0f, bounds.bottom, top, bottom, Shader.TileMode.CLAMP)
        })
        canvas.restore()
        return lip
    }

    /** A label's paint in the style: dark on the filled face, white with the outline. */
    fun labelColor(outlined: Boolean) = if (outlined) LINE else INK
}
