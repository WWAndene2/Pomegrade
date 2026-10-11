package me.magnum.melonds.ui.settings.preferences

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import me.magnum.melonds.R

/**
 * The console logos at the top of the settings (preference_console_selector). Tapping one picks that
 * console, kept as this preference's value ([GBA], [DS], [THREE_DS] or [SWITCH]); the settings screen then shows
 * only that console's section. The 3DS and Switch logos are hidden on devices that can't run them.
 */
class ConsoleSelectorPreference @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) : Preference(context, attrs) {

    companion object {
        const val GBA = "gba"
        const val DS = "ds"
        const val THREE_DS = "three_ds"
        const val SWITCH = "switch"
    }

    /** Called with the console picked, after it is saved. */
    var onConsoleSelected: ((String) -> Unit)? = null

    var threeDsAvailable = true
        set(value) {
            field = value
            notifyChanged()
        }

    var switchAvailable = true
        set(value) {
            field = value
            notifyChanged()
        }

    init {
        layoutResource = R.layout.preference_console_selector
        isSelectable = false
    }

    val selected: String
        get() = getPersistedString(DS).takeIf { it == GBA || (it == THREE_DS && threeDsAvailable) || (it == SWITCH && switchAvailable) } ?: DS

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        val gba = holder.findViewById(R.id.settings_console_gba)
        val ds = holder.findViewById(R.id.settings_console_ds)
        val threeDs = holder.findViewById(R.id.settings_console_three_ds)
        val switch = holder.findViewById(R.id.settings_console_switch)
        threeDs.visibility = if (threeDsAvailable) View.VISIBLE else View.GONE
        switch.visibility = if (switchAvailable) View.VISIBLE else View.GONE
        gba.isSelected = selected == GBA
        ds.isSelected = selected == DS
        threeDs.isSelected = selected == THREE_DS
        switch.isSelected = selected == SWITCH
        gba.setOnClickListener { select(GBA) }
        ds.setOnClickListener { select(DS) }
        threeDs.setOnClickListener { select(THREE_DS) }
        switch.setOnClickListener { select(SWITCH) }
    }

    private fun select(console: String) {
        persistString(console)
        notifyChanged()
        onConsoleSelected?.invoke(console)
    }
}
