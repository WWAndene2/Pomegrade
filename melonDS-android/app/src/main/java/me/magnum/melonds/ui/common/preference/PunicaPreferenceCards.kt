package me.magnum.melonds.ui.common.preference

import android.content.Context
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceGroupAdapter
import io.github.wwandene2.pomegrade.emulatorui.PunicaSettingsCards

/**
 * Punica's cards behind the app's settings (PunicaSettingsCards, shared with the cores' settings): section titles and
 * full-width rows ([isFullWidth]) stay outside the cards.
 */
class PunicaPreferenceCards(context: Context, isFullWidth: (Preference) -> Boolean = { false }) :
    PunicaSettingsCards(context, { parent, position ->
        val preference = (parent.adapter as? PreferenceGroupAdapter)?.getItem(position)
        preference != null && preference !is PreferenceCategory && !isFullWidth(preference)
    })
