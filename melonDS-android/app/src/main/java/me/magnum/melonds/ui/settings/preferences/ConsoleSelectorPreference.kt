package me.magnum.melonds.ui.settings.preferences

import android.content.Context
import android.util.AttributeSet
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.common.component.ConsoleCard
import me.magnum.melonds.ui.theme.MelonTheme

/**
 * The console cards at the top of the settings (preference_console_selector, the home screen's ConsoleCard). Tapping one picks that
 * console, kept as this preference's value ([GBA], [DS], [THREE_DS], [SWITCH] or [GC_WII]); the settings screen then shows
 * only that console's section. The 3DS, Switch and GameCube / Wii cards are hidden on devices that can't run them.
 */
class ConsoleSelectorPreference @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null) : Preference(context, attrs) {

    companion object {
        const val GBA = "gba"
        const val DS = "ds"
        const val THREE_DS = "three_ds"
        const val SWITCH = "switch"
        const val GC_WII = "gc_wii"
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

    var gcWiiAvailable = true
        set(value) {
            field = value
            notifyChanged()
        }

    init {
        layoutResource = R.layout.preference_console_selector
        isSelectable = false
    }

    val selected: String
        get() = getPersistedString(DS).takeIf { it == GBA || (it == THREE_DS && threeDsAvailable) || (it == SWITCH && switchAvailable) || (it == GC_WII && gcWiiAvailable) } ?: DS

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        holder.itemView.isClickable = false
        val consoles = buildList {
            add(DS to RomPlatform.NDS)
            if (threeDsAvailable) add(THREE_DS to RomPlatform.N3DS)
            add(GBA to RomPlatform.GBA)
            if (switchAvailable) add(SWITCH to RomPlatform.SWITCH)
            if (gcWiiAvailable) add(GC_WII to RomPlatform.GC_WII)
        }
        val chosen = selected
        // The layout's root itself: Preference.onBindViewHolder resets the item view's id (setId(mViewId)), so a
        // lookup by id finds nothing once super has run
        (holder.itemView as ComposeView).setContent {
            MelonTheme {
                // the same cards, sizes and gaps as the home screen's quick access
                LazyRow(
                    contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    items(consoles) { (key, platform) ->
                        ConsoleCard(platform, selected = key == chosen) { select(key) }
                    }
                }
            }
        }
    }

    private fun select(console: String) {
        persistString(console)
        notifyChanged()
        onConsoleSelected?.invoke(console)
    }
}
