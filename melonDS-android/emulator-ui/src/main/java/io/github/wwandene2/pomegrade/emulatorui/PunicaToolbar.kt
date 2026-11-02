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

    private var hostAppBar: AppBarLayout? = null
    private val pageLayoutListener = ViewTreeObserver.OnGlobalLayoutListener { addPageMargins() }

    /**
     * In the cores' screens the header sits in Material's AppBarLayout: it is made transparent (the page's colour shows
     * through) and fixed, as in the app's screens, and the page stays laid out below it as the core placed it.
     * Drawing the page under the header (1 November) is undone: forms and fragment pages ended up hidden under it and
     * some lists stopped scrolling (owner, 1 November).
     */
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        val appBar = parent as? AppBarLayout ?: return
        hostAppBar = appBar
        appBar.background = null
        appBar.stateListAnimator = null
        appBar.elevation = 0f
        (layoutParams as? AppBarLayout.LayoutParams)?.scrollFlags = 0
        appBar.viewTreeObserver.addOnGlobalLayoutListener(pageLayoutListener)
    }

    override fun onDetachedFromWindow() {
        hostAppBar?.viewTreeObserver?.removeOnGlobalLayoutListener(pageLayoutListener)
        hostAppBar = null
        super.onDetachedFromWindow()
    }

    private fun isList(view: View): Boolean = view is RecyclerView || view is NestedScrollView || view is ScrollView

    /**
     * The page's 16 dp margin above the first item and under the last item of its lists, on top of the paddings the
     * core sets (usually the navigation bar's inset at the bottom), as in the app's pages (owner, 1 November). Re-run on
     * every layout: fragments put their lists in the page later, and a padding other than the one set here last means
     * the core's insets code changed it, so the margin goes on top of its new value.
     */
    private fun addPageMargins() {
        val appBar = hostAppBar ?: return
        val host = appBar.parent as? ViewGroup ?: return
        val margin = resources.getDimensionPixelSize(R.dimen.emulator_settings_margin)
        for (i in 0 until host.childCount) {
            val page = host.getChildAt(i)
            if (page === appBar) continue
            forEachList(page) { list ->
                val appliedTop = list.getTag(R.id.punica_header_applied_top) as? Int
                val top = if (appliedTop != null && list.paddingTop == appliedTop) appliedTop else list.paddingTop + margin
                val appliedBottom = list.getTag(R.id.punica_header_applied_bottom) as? Int
                val bottom = if (appliedBottom != null && list.paddingBottom == appliedBottom) appliedBottom else list.paddingBottom + margin
                if (list.paddingTop != top || list.paddingBottom != bottom) {
                    (list as ViewGroup).clipToPadding = false
                    list.setPadding(list.paddingLeft, top, list.paddingRight, bottom)
                }
                list.setTag(R.id.punica_header_applied_top, top)
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
