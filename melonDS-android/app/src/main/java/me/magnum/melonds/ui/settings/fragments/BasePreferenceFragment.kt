package me.magnum.melonds.ui.settings.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.preference.PreferenceFragmentCompat
import androidx.recyclerview.widget.RecyclerView
import me.magnum.melonds.R
import me.magnum.melonds.ui.common.preference.PunicaPreferenceCards
import me.magnum.melonds.ui.settings.preferences.ConsoleSelectorPreference

abstract class BasePreferenceFragment : PreferenceFragmentCompat() {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        // Punica's settings: rows in cards (PunicaPreferenceCards) instead of the list's dividers; the console picker
        // spans the screen as the home screen's quick access
        setDivider(null)
        listView.addItemDecoration(PunicaPreferenceCards(requireContext()) { it is ConsoleSelectorPreference })
    }

    override fun onCreateRecyclerView(inflater: LayoutInflater, parent: ViewGroup, savedInstanceState: Bundle?): RecyclerView {
        return super.onCreateRecyclerView(inflater, parent, savedInstanceState).apply {
            clipToPadding = false

            ViewCompat.setOnApplyWindowInsetsListener(this) { view, windowInsets ->
                val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
                // top: the status bar and the transparent header (SettingsActivity), which the list scrolls under,
                // then the page's 16 dp margin above the first card (owner, 1 November: the sub-pages' first card
                // touched the header); bottom: the navigation bar, then the same margin under the last card
                val margin = view.resources.getDimensionPixelSize(R.dimen.punica_margin)
                view.updatePadding(top = insets.top + margin, bottom = insets.bottom + margin)

                WindowInsetsCompat.CONSUMED
            }
        }
    }
}