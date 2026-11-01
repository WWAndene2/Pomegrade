package io.github.wwandene2.pomegrade.emulatorui

import android.content.Context
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import android.util.AttributeSet
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewTreeObserver
import android.widget.FrameLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.text.inSpans
import androidx.core.widget.NestedScrollView
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.appbar.AppBarLayout

/**
 * The XML screens' toolbar, every console's settings included (shared since 31 October) with the home screen's header (owner's choice, 13 October): 64 dp high, transparent over
 * the window's background, the logo "Punica." in its outline and the screen's title in italics under it. The title
 * set by the activity or the action bar goes under the logo; the toolbar's own title stays empty.
 */
class PunicaToolbar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = androidx.appcompat.R.attr.toolbarStyle,
) : Toolbar(context, attrs, defStyleAttr) {

    private val subtitleView: TextView

    init {
        val logo = LayoutInflater.from(context).inflate(R.layout.punica_toolbar_logo, this, false)
        // the title mask's edge, not the pill's, starts where the logo is placed, as in the Compose PunicaLogo
        addView(logo, LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER_VERTICAL))
        val red = ContextCompat.getColor(context, R.color.emulator_menu_primary)
        logo.findViewById<TextView>(R.id.text_punica_logo).text = SpannableStringBuilder("Punica").inSpans(ForegroundColorSpan(red)) { append(".") }
        subtitleView = logo.findViewById(R.id.text_punica_subtitle)
        subtitleView.text = super.getTitle()
        super.setTitle(null)
        minimumHeight = resources.getDimensionPixelSize(R.dimen.emulator_settings_header_height)
        setBackgroundColor(0)
    }

    private var overlaidAppBar: AppBarLayout? = null
    private val overlayLayoutListener = ViewTreeObserver.OnGlobalLayoutListener { padPagesUnderHeader() }

    /**
     * In the cores' screens the header sits in Material's AppBarLayout, which lays the page out below it, opaque, and
     * scrolls away with the list. Punica's header is transparent and the page scrolls under it, as in the DS's settings
     * (owner's request, 1 November): the bar is made transparent and fixed, the page re-anchored to the top of the
     * screen, and its list given the bar's height as top padding (clipToPadding off), so it starts below the header and
     * scrolls under it. Pages that are not lists (a form, a label) stay below the bar.
     */
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        val appBar = parent as? AppBarLayout ?: return
        overlaidAppBar = appBar
        appBar.background = null
        appBar.stateListAnimator = null
        appBar.elevation = 0f
        (layoutParams as? AppBarLayout.LayoutParams)?.scrollFlags = 0
        val host = appBar.parent as? ViewGroup ?: return
        for (i in 0 until host.childCount) {
            val page = host.getChildAt(i)
            if (page === appBar || !isPage(page)) continue
            when (val lp = page.layoutParams) {
                is CoordinatorLayout.LayoutParams -> if (lp.behavior is AppBarLayout.ScrollingViewBehavior) {
                    lp.behavior = null
                    page.layoutParams = lp
                }
                is ConstraintLayout.LayoutParams -> if (lp.topToBottom == appBar.id) {
                    lp.topToBottom = ConstraintLayout.LayoutParams.UNSET
                    lp.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    page.layoutParams = lp
                }
            }
        }
        // drawn over the page
        appBar.bringToFront()
        appBar.viewTreeObserver.addOnGlobalLayoutListener(overlayLayoutListener)
    }

    override fun onDetachedFromWindow() {
        overlaidAppBar?.viewTreeObserver?.removeOnGlobalLayoutListener(overlayLayoutListener)
        overlaidAppBar = null
        super.onDetachedFromWindow()
    }

    /** A list, or a host of one (a fragment's frame, a sliding pane): the views the header floats over. */
    private fun isPage(view: View): Boolean =
        isList(view) || view is FrameLayout && view !is ScrollView || view.javaClass.name == "androidx.slidingpanelayout.widget.SlidingPaneLayout"

    private fun isList(view: View): Boolean = view is RecyclerView || view is NestedScrollView || view is ScrollView

    /** Re-run on every layout: fragments put their lists in the page later, and their own insets code may change padding. */
    private fun padPagesUnderHeader() {
        val appBar = overlaidAppBar ?: return
        val host = appBar.parent as? ViewGroup ?: return
        val top = appBar.height
        for (i in 0 until host.childCount) {
            val page = host.getChildAt(i)
            if (page === appBar || page.top != 0 || !isPage(page)) continue
            forEachList(page) { list ->
                // the list's own top padding (its styled one) is kept on top of the header's height
                val base = list.getTag(R.id.punica_header_base_padding) as? Int ?: list.paddingTop.also { list.setTag(R.id.punica_header_base_padding, it) }
                if (list.paddingTop != base + top) {
                    (list as ViewGroup).clipToPadding = false
                    list.setPadding(list.paddingLeft, base + top, list.paddingRight, list.paddingBottom)
                }
            }
        }
    }

    /** The outermost lists inside [view] (a list's own nested lists scroll with it). */
    private fun forEachList(view: View, action: (View) -> Unit) {
        if (isList(view)) return action(view)
        if (view is ViewGroup) for (i in 0 until view.childCount) forEachList(view.getChildAt(i), action)
    }

    /** Keeps the header clear of the cutout and the system bars on its sides, its own 4 dp edge on top of them. */
    fun applySideInsets(left: Int, right: Int) {
        val edge = resources.getDimensionPixelSize(R.dimen.emulator_settings_header_edge)
        setPadding(edge + left, paddingTop, edge + right, paddingBottom)
    }

    override fun setTitle(title: CharSequence?) {
        // called by Toolbar's own constructor too, before the logo exists
        @Suppress("SENSELESS_COMPARISON")
        if (subtitleView == null) super.setTitle(title) else subtitleView.text = title
    }

    override fun getTitle(): CharSequence? = if (subtitleView == null) super.getTitle() else subtitleView.text
}
