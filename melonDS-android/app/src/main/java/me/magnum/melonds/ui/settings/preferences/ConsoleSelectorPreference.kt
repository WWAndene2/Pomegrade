package me.magnum.melonds.ui.settings.preferences

import android.content.Context
import android.util.AttributeSet
import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import me.magnum.melonds.R
import me.magnum.melonds.ui.theme.MelonTheme
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme

/**
 * The consoles' logo cards at the top of the settings (preference_console_selector). Tapping one picks that
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
            add(GBA to R.drawable.settings_console_logo_gba)
            add(DS to R.drawable.settings_console_logo_ds)
            if (threeDsAvailable) add(THREE_DS to R.drawable.settings_console_logo_three_ds)
            if (switchAvailable) add(SWITCH to R.drawable.settings_console_logo_switch)
            if (gcWiiAvailable) add(GC_WII to R.drawable.settings_console_logo_gc_wii)
        }
        val chosen = selected
        // The layout's root itself: Preference.onBindViewHolder resets the item view's id (setId(mViewId)), so a
        // lookup by id finds nothing once super has run
        (holder.itemView as ComposeView).setContent {
            MelonTheme {
                // the consoles' logos (owner's choice, 13 October), two by two so the wide ones stay readable; an odd
                // last one takes the whole row
                Column(
                    Modifier.padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP_WIDE.dp),
                    verticalArrangement = Arrangement.spacedBy(PunicaDimens.MARGIN.dp),
                ) {
                    consoles.chunked(2).forEach { row ->
                        Row(horizontalArrangement = Arrangement.spacedBy(PunicaDimens.MARGIN.dp)) {
                            row.forEach { (key, logo) ->
                                ConsoleLogoCard(logo, key, selected = key == chosen, Modifier.weight(1f)) { select(key) }
                            }
                        }
                    }
                }
            }
        }
    }

    /** A console's logo on a light card (the logos are dark), outlined in red when it is the one shown. */
    @Composable
    private fun ConsoleLogoCard(@DrawableRes logo: Int, key: String, selected: Boolean, modifier: Modifier, onClick: () -> Unit) {
        val shape = RoundedCornerShape(PunicaDimens.TILE_RADIUS.dp)
        Image(
            painter = painterResource(logo),
            contentDescription = context.getString(sectionTitle(key)),
            contentScale = ContentScale.Fit,
            modifier = modifier
                .height(PunicaDimens.TOUCH.dp)
                .clip(shape)
                .background(if (selected) Color.White else Color.White.copy(alpha = 0.7f))
                .then(if (selected) Modifier.border(3.dp, PunicaTheme.red, shape) else Modifier)
                .clickable(onClick = onClick)
                .padding(if (key == SWITCH) 4.dp else PunicaDimens.GAP_WIDE.dp),
        )
    }

    private fun sectionTitle(key: String): Int = when (key) {
        GBA -> R.string.settings_section_gba
        THREE_DS -> R.string.settings_section_three_ds
        SWITCH -> R.string.settings_section_switch
        GC_WII -> R.string.settings_section_gc_wii
        else -> R.string.settings_section_ds
    }

    private fun select(console: String) {
        persistString(console)
        notifyChanged()
        onConsoleSelected?.invoke(console)
    }
}
