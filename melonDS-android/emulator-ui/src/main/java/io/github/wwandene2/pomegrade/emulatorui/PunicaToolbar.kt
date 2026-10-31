package io.github.wwandene2.pomegrade.emulatorui

import android.content.Context
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import android.util.AttributeSet
import android.view.Gravity
import android.view.LayoutInflater
import android.widget.TextView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.text.inSpans

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
