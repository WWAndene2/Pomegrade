package io.github.wwandene2.pomegrade.emulatorui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView

/**
 * Draws Punica's cards behind a settings list (owner's choice, 13 October), as the home screen's: the rows between two
 * section titles share one card, 16 dp from the screen's sides, 16 dp corners, the card colour with a 1 dp outline, a
 * 1 dp line between rows. Shared by every console's settings (the app's and the cores'); [isCardRow] tells a row from
 * what stays outside the cards (section titles, full-width rows).
 */
open class PunicaSettingsCards(
    context: Context,
    private val isCardRow: (RecyclerView, Int) -> Boolean,
) : RecyclerView.ItemDecoration() {

    private val margin = context.resources.getDimensionPixelSize(R.dimen.emulator_settings_margin)
    private val radius = context.resources.getDimension(R.dimen.emulator_settings_card_radius)
    private val line = context.resources.displayMetrics.density
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = ContextCompat.getColor(context, R.color.emulator_menu_surface) }
    private val outline = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.emulator_settings_line)
        style = Paint.Style.STROKE
        strokeWidth = line
    }
    private val divider = Paint().apply { color = ContextCompat.getColor(context, R.color.emulator_settings_line) }

    private fun cardRow(parent: RecyclerView, position: Int): Boolean {
        val count = parent.adapter?.itemCount ?: return false
        return position in 0 until count && isCardRow(parent, position)
    }

    override fun getItemOffsets(outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State) {
        if (cardRow(parent, parent.getChildAdapterPosition(view))) {
            outRect.left = margin
            outRect.right = margin
        }
    }

    override fun onDraw(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
        // runs of card rows among the children on screen; a run whose real start or end is off screen keeps that
        // side square. The cards are drawn first, the lines between rows on top of them
        val dividers = ArrayList<RectF>()
        var runTop = Float.NaN
        var runStartsGroup = false
        var runLeft = 0f
        var runRight = 0f
        for (i in 0 until parent.childCount) {
            val child = parent.getChildAt(i)
            val position = parent.getChildAdapterPosition(child)
            if (!cardRow(parent, position)) continue
            val top = child.top + child.translationY
            val bottom = child.bottom + child.translationY
            val startsGroup = !cardRow(parent, position - 1)
            val endsGroup = !cardRow(parent, position + 1)
            if (runTop.isNaN()) {
                runTop = top
                runStartsGroup = startsGroup
                runLeft = child.left.toFloat()
                runRight = child.right.toFloat()
            }
            if (!startsGroup && top > runTop) {
                dividers += RectF(child.left.toFloat(), top, child.right.toFloat(), top + line)
            }
            if (endsGroup || i == parent.childCount - 1) {
                drawCard(canvas, RectF(runLeft, runTop, runRight, bottom), runStartsGroup, endsGroup)
                runTop = Float.NaN
            }
        }
        dividers.forEach { canvas.drawRect(it, divider) }
    }

    private fun drawCard(canvas: Canvas, bounds: RectF, roundTop: Boolean, roundBottom: Boolean) {
        val t = if (roundTop) radius else 0f
        val b = if (roundBottom) radius else 0f
        val radii = floatArrayOf(t, t, t, t, b, b, b, b)
        canvas.drawPath(Path().apply { addRoundRect(bounds, radii, Path.Direction.CW) }, fill)
        // the outline's stroke centred half a line inside the card's edge
        val inset = RectF(bounds).apply { inset(line / 2, line / 2) }
        canvas.drawPath(Path().apply { addRoundRect(inset, radii, Path.Direction.CW) }, outline)
    }
}
