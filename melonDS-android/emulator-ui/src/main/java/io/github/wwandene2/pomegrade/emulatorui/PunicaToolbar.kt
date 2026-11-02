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
    private val overlayLayoutListener = ViewTreeObserver.OnGlobalLayoutListener { padPagesBottom() }

    /**
     * In the cores' screens the header sits in Material's AppBarLayout. It is made transparent and fixed (no lift, no
     * elevation, no scrolling away), and stays where Material lays it out: the page below it, never under it.
     *
     * Owner (2 November): the earlier version re-anchored the pages to the top of the screen, brought the bar to the
     * front and padded the lists by the bar's height on every layout, so they would scroll under the header; on the
     * phone the settings could then be neither scrolled nor tapped and content sat under the header. It is removed:
     * no page is moved, no padding at the top, nothing drawn over the page.
     */
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        val appBar = parent as? AppBarLayout ?: return
        overlaidAppBar = appBar
        appBar.background = null
        appBar.stateListAnimator = null
        appBar.elevation = 0f
        (layoutParams as? AppBarLayout.LayoutParams)?.scrollFlags = 0
        appBar.viewTreeObserver.addOnGlobalLayoutListener(overlayLayoutListener)
    }

    override fun onDetachedFromWindow() {
        overlaidAppBar?.viewTreeObserver?.removeOnGlobalLayoutListener(overlayLayoutListener)
        overlaidAppBar = null
        super.onDetachedFromWindow()
    }

    /** A list, or a host of one (a fragment's frame, a sliding pane): the pages beside the header. */
    private fun isPage(view: View): Boolean =
        isList(view) || view is FrameLayout && view !is ScrollView || view.javaClass.name == "androidx.slidingpanelayout.widget.SlidingPaneLayout"

    private fun isList(view: View): Boolean = view is RecyclerView || view is NestedScrollView || view is ScrollView

    /**
     * The page's 16 dp margin under the last item, as in the app's settings (owner, 1 November), on top of whatever
     * bottom padding the core set (usually the navigation bar's inset). Re-run on every layout: fragments put their
     * lists in the page later, and their own insets code may change the padding; a bottom other than the one set here
     * last means the core changed it, and the margin goes on top of its new value.
     */
    private fun padPagesBottom() {
        val appBar = overlaidAppBar ?: return
        var bar: View = appBar
        var host = appBar.parent as? ViewGroup ?: return
        // a bar alone in its own CoordinatorLayout (Dolphin's cheats): the pages are that layout's siblings
        if (host.childCount == 1) {
            bar = host
            host = host.parent as? ViewGroup ?: return
        }
        val margin = resources.getDimensionPixelSize(R.dimen.emulator_settings_margin)
        for (i in 0 until host.childCount) {
            val page = host.getChildAt(i)
            if (page === bar || !isPage(page)) continue
            forEachList(page) { list ->
                val applied = list.getTag(R.id.punica_header_applied_bottom) as? Int
                val bottom = if (applied != null && list.paddingBottom == applied) applied else list.paddingBottom + margin
                if (list.paddingBottom != bottom) {
                    (list as ViewGroup).clipToPadding = false
                    list.setPadding(list.paddingLeft, list.paddingTop, list.paddingRight, bottom)
                }
                list.setTag(R.id.punica_header_applied_bottom, bottom)
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
