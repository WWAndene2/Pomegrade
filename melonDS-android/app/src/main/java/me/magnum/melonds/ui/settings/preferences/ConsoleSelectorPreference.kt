package me.magnum.melonds.ui.settings.preferences

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import me.magnum.melonds.R

/**
 * The console logos at the top of the settings (preference_console_selector). Tapping one picks that
 * console, kept as this preference's value ([DS] or [THREE_DS]); the settings screen then shows only
 * that console's section. The 3DS logo is hidden when 3DS games can't run (32-bit devices).
 */
class ConsoleSelectorPreference @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) : Preference(context, attrs) {

    companion object {
        const val DS = "ds"
        const val THREE_DS = "three_ds"
    }

    /** Called with the console picked, after it is saved. */
    var onConsoleSelected: ((String) -> Unit)? = null

    var threeDsAvailable = true
        set(value) {
            field = value
            notifyChanged()
        }

    init {
        layoutResource = R.layout.preference_console_selector
        isSelectable = false
    }

    val selected: String
        get() = getPersistedString(DS).takeIf { it == THREE_DS && threeDsAvailable } ?: DS

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        val ds = holder.findViewById(R.id.settings_console_ds)
        val threeDs = holder.findViewById(R.id.settings_console_three_ds)
        threeDs.visibility = if (threeDsAvailable) View.VISIBLE else View.GONE
        ds.isSelected = selected == DS
        threeDs.isSelected = selected == THREE_DS
        ds.setOnClickListener { select(DS) }
        threeDs.setOnClickListener { select(THREE_DS) }
    }

    private fun select(console: String) {
        persistString(console)
        notifyChanged()
        onConsoleSelected?.invoke(console)
    }
}
