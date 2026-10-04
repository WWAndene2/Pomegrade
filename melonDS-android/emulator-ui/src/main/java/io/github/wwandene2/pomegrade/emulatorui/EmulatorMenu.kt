package io.github.wwandene2.pomegrade.emulatorui

import android.view.Menu
import android.widget.ImageView
import android.widget.TextView
import com.google.android.material.navigation.NavigationView

/**
 * The in-game menu shared by both consoles: a [NavigationView] showing `R.menu.emulator_menu` with
 * `R.layout.emulator_menu_header` and the `Theme.Pomegrade.EmulatorMenu` theme. Each emulation
 * screen declares it in its layout, then calls these to fit it to its game and core.
 */
object EmulatorMenu {

    /** Menu entries every console has. Each screen adds its core's own (see the menu's comment). */
    val commonItems = setOf(
        R.id.emulator_menu_save_state,
        R.id.emulator_menu_load_state,
        R.id.emulator_menu_swap_screens,
        R.id.emulator_menu_cheats,
        R.id.emulator_menu_settings,
        R.id.emulator_menu_exit,
    )

    /** Shows the entries in [itemIds] and hides every other one. */
    fun showOnly(menu: Menu, itemIds: Set<Int>) {
        for (i in 0 until menu.size()) {
            val item = menu.getItem(i)
            item.isVisible = item.itemId in itemIds
        }
    }

    /**
     * Fills the header's title and console, and returns its icon view for the screen to load the
     * game's icon into (each core reads icons its own way).
     */
    fun setHeader(navigationView: NavigationView, title: String, console: String): ImageView? {
        val header = navigationView.getHeaderView(0) ?: return null
        header.findViewById<TextView>(R.id.emulator_menu_game_title).text = title
        header.findViewById<TextView>(R.id.emulator_menu_console).text = console
        return header.findViewById(R.id.emulator_menu_game_icon)
    }
}
