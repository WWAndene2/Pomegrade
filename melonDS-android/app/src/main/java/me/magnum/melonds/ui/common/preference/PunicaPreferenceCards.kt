package me.magnum.melonds.ui.common.preference

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.view.View
import androidx.core.content.ContextCompat
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceGroupAdapter
import androidx.recyclerview.widget.RecyclerView
import me.magnum.melonds.R

/**
 * Draws Punica's cards behind the settings (owner's choice, 13 October), as the home screen's: the rows between two
 * section titles share one card, 16 dp from the screen's sides, 16 dp corners, the card colour with a 1 dp
 * outline, a 1 dp line between rows. Section titles and full-width rows ([isFullWidth]) stay outside the cards.
 */
class PunicaPreferenceCards(context: Context, private val isFullWidth: (Preference) -> Boolean = { false }) : RecyclerView.ItemDecoration() {

    private val margin = context.resources.getDimensionPixelSize(R.dimen.punica_margin)
    private val radius = context.resources.getDimension(R.dimen.punica_card_radius)
    private val line = context.resources.displayMetrics.density
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = ContextCompat.getColor(context, R.color.punicaCard) }
    private val outline = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = ContextCompat.getColor(context, R.color.punicaLine)
        style = Paint.Style.STROKE
        strokeWidth = line
    }
    private val divider = Paint().apply { color = ContextCompat.getColor(context, R.color.punicaLine) }

    private fun preferenceAt(parent: RecyclerView, position: Int): Preference? {
        val adapter = parent.adapter as? PreferenceGroupAdapter ?: return null
        if (position < 0 || position >= adapter.itemCount) return null
        return adapter.getItem(position)
    }

    private fun isCardRow(preference: Preference?) = preference != null && preference !is PreferenceCategory && !isFullWidth(preference)

    override fun getItemOffsets(outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State) {
        val position = parent.getChildAdapterPosition(view)
        if (isCardRow(preferenceAt(parent, position))) {
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
            if (!isCardRow(preferenceAt(parent, position))) continue
            val top = child.top + child.translationY
            val bottom = child.bottom + child.translationY
            val startsGroup = !isCardRow(preferenceAt(parent, position - 1))
            val endsGroup = !isCardRow(preferenceAt(parent, position + 1))
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
