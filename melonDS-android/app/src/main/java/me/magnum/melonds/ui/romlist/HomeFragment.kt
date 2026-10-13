package me.magnum.melonds.ui.romlist

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import dagger.hilt.android.AndroidEntryPoint
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.impl.FavoriteGames
import me.magnum.melonds.parcelables.RomParcelable
import me.magnum.melonds.ui.common.rom.GcWiiLauncher
import me.magnum.melonds.ui.common.rom.N3dsLauncher
import me.magnum.melonds.ui.common.rom.SwitchLauncher
import me.magnum.melonds.ui.romdetails.RomDetailsActivity
import me.magnum.melonds.ui.romlist.home.GameActions
import me.magnum.melonds.ui.romlist.home.HomeMenuEntry
import me.magnum.melonds.ui.romlist.home.HomeScreen

/**
 * The game list's home screen ([HomeScreen]), in place of [RomListFragment] in [RomListActivity] (which other
 * screens still use to pick a game). The activity's toolbar is hidden: its actions are the header's buttons, and
 * [menuActions] in the header's ⋮, run by the activity as its menu items were.
 */
@AndroidEntryPoint
class HomeFragment : Fragment() {

    /** What the activity does for the home screen: start a game, and its old menu's actions. */
    interface Host {
        fun onHomeGameSelected(rom: Rom)
        fun onHomeMenuAction(itemId: Int)
    }

    private val viewModel: RomListViewModel by activityViewModels()

    // the old toolbar menu's actions with no button of their own on the home screen
    private val menuActions = listOf(
        R.id.action_rom_list_refresh to R.string.action_refresh_rom_list,
        R.id.action_boot_firmware_ds to R.string.home_boot_firmware_ds,
        R.id.action_boot_firmware_dsi to R.string.home_boot_firmware_dsi,
        R.id.action_three_ds_games to R.string.action_three_ds_games,
        R.id.action_dsiware_manager to R.string.dsiware_manager,
        R.id.action_about to R.string.pomegrade_about,
    )

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val host = requireActivity() as Host
        val favoriteGames = FavoriteGames(requireContext())
        val platforms = buildList {
            add(RomPlatform.NDS)
            if (N3dsLauncher.isSupported()) add(RomPlatform.N3DS)
            add(RomPlatform.GBA)
            if (SwitchLauncher.isSupported()) add(RomPlatform.SWITCH)
            if (GcWiiLauncher.isSupported()) add(RomPlatform.GC_WII)
        }
        return ComposeView(requireContext()).apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
            setContent {
                val roms by viewModel.roms.collectAsState()
                val platformFilter by viewModel.platformFilter.collectAsState()
                val favorites by favoriteGames.favorites.collectAsState()
                var query by rememberSaveable { mutableStateOf("") }
                val actions = remember(favorites) {
                    GameActions(
                        onPlay = { rom ->
                            viewModel.setRomLastPlayedNow(rom)
                            host.onHomeGameSelected(rom)
                        },
                        onToggleFavorite = { rom -> favoriteGames.toggle(rom.uri) },
                        isFavorite = { rom -> rom.uri.toString() in favorites },
                        onGameSettings = { rom ->
                            startActivity(Intent(requireContext(), RomDetailsActivity::class.java).putExtra(RomDetailsActivity.KEY_ROM, RomParcelable(rom)))
                        },
                        loadIcon = { rom -> viewModel.getRomIcon(rom) },
                    )
                }
                HomeScreen(
                    roms = roms.orEmpty(),
                    platforms = platforms,
                    platformFilter = platformFilter,
                    favorites = favorites,
                    searchQuery = query,
                    actions = actions,
                    menuEntries = menuActions.map { (id, title) -> HomeMenuEntry(getString(title)) { host.onHomeMenuAction(id) } },
                    onSearchQueryChange = { query = it; viewModel.setRomSearchQuery(it) },
                    onPlatformSelected = { viewModel.setPlatformFilter(it) },
                    onSettings = { host.onHomeMenuAction(R.id.action_settings) },
                )
            }
        }
    }
}
