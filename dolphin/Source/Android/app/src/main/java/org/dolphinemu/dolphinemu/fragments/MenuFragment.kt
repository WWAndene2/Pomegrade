// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import io.github.wwandene2.pomegrade.emulatorui.EmulatorMenu
import io.github.wwandene2.pomegrade.emulatorui.R as EmulatorMenuR
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentIngameMenuBinding
import org.dolphinemu.dolphinemu.features.settings.model.AchievementModel
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity

// Pomegrade: the in-game menu shared with the other consoles (:emulator-ui) in place of Dolphin's own buttons;
// each entry runs the EmulationActivity menu action Dolphin's button ran
class MenuFragment : Fragment() {
    private var cutInset = 0

    private var _binding: FragmentIngameMenuBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentIngameMenuBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        setInsets()

        val wii = requireArguments().getBoolean(KEY_WII, true)
        val shown = mutableSetOf(
            EmulatorMenuR.id.emulator_menu_pause,
            EmulatorMenuR.id.emulator_menu_screenshot,
            EmulatorMenuR.id.emulator_menu_quick_save,
            EmulatorMenuR.id.emulator_menu_quick_load,
            EmulatorMenuR.id.emulator_menu_save_state,
            EmulatorMenuR.id.emulator_menu_load_state,
            EmulatorMenuR.id.emulator_menu_change_disc,
            EmulatorMenuR.id.emulator_menu_settings,
            EmulatorMenuR.id.emulator_menu_exit,
        )
        // Pomegrade: this game's own settings, as Dolphin's game list opened them (GamePropertiesDialog)
        if (NativeLibrary.IsGameMetadataValid()) shown += EmulatorMenuR.id.emulator_menu_game_settings
        if (requireActivity().packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)) {
            shown += EmulatorMenuR.id.emulator_menu_overlay_options
        }
        if (wii) {
            shown += EmulatorMenuR.id.emulator_menu_refresh_wiimotes
            if (BooleanSetting.MAIN_EMULATE_SKYLANDER_PORTAL.boolean) shown += EmulatorMenuR.id.emulator_menu_skylanders
            if (BooleanSetting.MAIN_EMULATE_INFINITY_BASE.boolean) shown += EmulatorMenuR.id.emulator_menu_infinity_base
        }
        EmulatorMenu.showOnly(binding.inGameMenu.menu, shown)

        val title = requireArguments().getString(KEY_TITLE, null).orEmpty()
        EmulatorMenu.setHeader(binding.inGameMenu, title, getString(EmulatorMenuR.string.emulator_menu_console_gc_wii))
            ?.visibility = View.GONE // Dolphin's running game has no icon to show here

        updatePauseTitle()
        binding.inGameMenu.setNavigationItemSelectedListener { item -> onItemSelected(item.itemId); true }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            cutInset = cutInsets.left
            v.post { NativeLibrary.SetObscuredPixelsLeft(v.width) }
            windowInsets
        }

    override fun onResume() {
        super.onResume()
        val savestatesEnabled = BooleanSetting.MAIN_ENABLE_SAVESTATES.boolean
        val hardcoreEnabled = AchievementModel.isHardcoreModeActive()
        val menu = binding.inGameMenu.menu
        listOf(
            EmulatorMenuR.id.emulator_menu_quick_save,
            EmulatorMenuR.id.emulator_menu_quick_load,
            EmulatorMenuR.id.emulator_menu_save_state,
            EmulatorMenuR.id.emulator_menu_load_state,
        ).forEach { menu.findItem(it).isVisible = savestatesEnabled }
        // in hardcore mode, AchievementManager blocks loading and says so on screen
        menu.findItem(EmulatorMenuR.id.emulator_menu_quick_load).isEnabled = !hardcoreEnabled
        menu.findItem(EmulatorMenuR.id.emulator_menu_load_state).isEnabled = !hardcoreEnabled
    }

    override fun onDestroyView() {
        super.onDestroyView()
        NativeLibrary.SetObscuredPixelsLeft(cutInset)
        _binding = null
    }

    private fun updatePauseTitle() {
        binding.inGameMenu.menu.findItem(EmulatorMenuR.id.emulator_menu_pause).title = getString(
            if (EmulationActivity.hasUserPausedEmulation) EmulatorMenuR.string.emulator_menu_resume
            else EmulatorMenuR.string.emulator_menu_pause
        )
    }

    private fun onItemSelected(itemId: Int) {
        val activity = requireActivity() as EmulationActivity
        if (itemId == EmulatorMenuR.id.emulator_menu_overlay_options) {
            // anchored to the menu's header, as Dolphin anchored it to its title (a button gives a tiny menu)
            activity.showOverlayControlsMenu(binding.inGameMenu.getHeaderView(0))
            return
        }
        runItem(activity, itemId)
        if (itemId == EmulatorMenuR.id.emulator_menu_pause) updatePauseTitle()
    }

    companion object {
        private const val KEY_TITLE = "title"
        private const val KEY_WII = "wii"
        private val actions = mapOf(
            EmulatorMenuR.id.emulator_menu_screenshot to EmulationActivity.MENU_ACTION_TAKE_SCREENSHOT,
            EmulatorMenuR.id.emulator_menu_quick_save to EmulationActivity.MENU_ACTION_QUICK_SAVE,
            EmulatorMenuR.id.emulator_menu_quick_load to EmulationActivity.MENU_ACTION_QUICK_LOAD,
            EmulatorMenuR.id.emulator_menu_save_state to EmulationActivity.MENU_ACTION_SAVE_ROOT,
            EmulatorMenuR.id.emulator_menu_load_state to EmulationActivity.MENU_ACTION_LOAD_ROOT,
            EmulatorMenuR.id.emulator_menu_refresh_wiimotes to EmulationActivity.MENU_ACTION_REFRESH_WIIMOTES,
            EmulatorMenuR.id.emulator_menu_change_disc to EmulationActivity.MENU_ACTION_CHANGE_DISC,
            EmulatorMenuR.id.emulator_menu_exit to EmulationActivity.MENU_ACTION_EXIT,
            EmulatorMenuR.id.emulator_menu_settings to EmulationActivity.MENU_ACTION_SETTINGS,
            EmulatorMenuR.id.emulator_menu_skylanders to EmulationActivity.MENU_ACTION_SKYLANDERS,
            EmulatorMenuR.id.emulator_menu_infinity_base to EmulationActivity.MENU_ACTION_INFINITY_BASE,
        )

        /** Pomegrade: the entries the shortcut pill offers on this core (those that need no open menu). */
        fun shortcutItems(): Set<Int> {
            val items = mutableSetOf(
                EmulatorMenuR.id.emulator_menu_pause,
                EmulatorMenuR.id.emulator_menu_screenshot,
                EmulatorMenuR.id.emulator_menu_quick_save,
                EmulatorMenuR.id.emulator_menu_quick_load,
                EmulatorMenuR.id.emulator_menu_save_state,
                EmulatorMenuR.id.emulator_menu_load_state,
                EmulatorMenuR.id.emulator_menu_change_disc,
                EmulatorMenuR.id.emulator_menu_settings,
                EmulatorMenuR.id.emulator_menu_exit,
            )
            if (NativeLibrary.IsGameMetadataValid()) {
                items += EmulatorMenuR.id.emulator_menu_game_settings
                if (NativeLibrary.IsEmulatingWii()) items += EmulatorMenuR.id.emulator_menu_refresh_wiimotes
            }
            return items
        }

        /** Runs one of the menu's entries, from the menu or the shortcut pill. */
        fun runItem(activity: EmulationActivity, itemId: Int) {
            when (itemId) {
                // the disc's revision isn't exposed while running: 0, which only picks which of Dolphin's
                // default game settings are shown; the game's own file is named by its ID alone
                EmulatorMenuR.id.emulator_menu_game_settings ->
                    SettingsActivity.launch(activity, MenuTag.SETTINGS, NativeLibrary.GetCurrentGameID(), 0, NativeLibrary.IsEmulatingWii())
                EmulatorMenuR.id.emulator_menu_pause -> activity.handleMenuAction(
                    if (EmulationActivity.hasUserPausedEmulation) EmulationActivity.MENU_ACTION_UNPAUSE_EMULATION
                    else EmulationActivity.MENU_ACTION_PAUSE_EMULATION
                )
                else -> actions[itemId]?.let { activity.handleMenuAction(it) }
            }
        }

        fun newInstance(): MenuFragment {
            val fragment = MenuFragment()
            val arguments = Bundle()
            if (NativeLibrary.IsGameMetadataValid()) {
                arguments.putString(KEY_TITLE, NativeLibrary.GetCurrentTitleDescription())
                arguments.putBoolean(KEY_WII, NativeLibrary.IsEmulatingWii())
            }
            fragment.arguments = arguments
            return fragment
        }
    }
}
