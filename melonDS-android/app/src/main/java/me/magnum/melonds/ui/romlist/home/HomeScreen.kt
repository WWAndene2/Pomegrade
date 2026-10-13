package me.magnum.melonds.ui.romlist.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.outlined.Folder
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.common.component.ConsoleCard
import me.magnum.melonds.ui.common.component.PunicaHeaderButton
import me.magnum.melonds.ui.common.component.PunicaLogo
import me.magnum.melonds.ui.common.component.PunicaPill
import me.magnum.melonds.ui.common.component.punicaBackground
import me.magnum.melonds.ui.theme.PunicaTheme

/** How the library is ordered; [FAVORITES] also keeps only the starred games. */
internal enum class LibrarySort { RECENT, FAVORITES, PLAY_TIME, ALPHABETICAL }

/** An entry of the header's ⋮ (the old toolbar's actions that have no place of their own on the home screen). */
internal class HomeMenuEntry(val title: String, val onClick: () -> Unit)

/**
 * Punica's home screen (owner's mock-up, 13 October): header, featured carousel, quick access by console (which
 * filters everything below), recently played, then the library with its sort pills. No bottom bar: settings are
 * the header's gear, the other actions its ⋮. The page scrolls; the carousel and the rows scroll sideways.
 */
@Composable
internal fun HomeScreen(
    roms: List<Rom>,
    platforms: List<RomPlatform>,
    platformFilter: RomPlatform?,
    favorites: Set<String>,
    searchQuery: String,
    actions: GameActions,
    menuEntries: List<HomeMenuEntry>,
    onSearchQueryChange: (String) -> Unit,
    onPlatformSelected: (RomPlatform?) -> Unit,
    onSettings: () -> Unit,
) {
    var sort by rememberSaveable { mutableStateOf(LibrarySort.RECENT) }
    val played = remember(roms) { roms.filter { it.lastPlayed != null }.sortedByDescending { it.lastPlayed } }
    val featured = remember(played, roms) { (played.ifEmpty { roms }).take(5) }
    val library = remember(roms, sort, favorites) {
        when (sort) {
            LibrarySort.RECENT -> played + roms.filter { it.lastPlayed == null }.sortedBy { it.name.lowercase() }
            LibrarySort.FAVORITES -> roms.filter { it.uri.toString() in favorites }.sortedBy { it.name.lowercase() }
            LibrarySort.PLAY_TIME -> roms.sortedByDescending { it.totalPlayTime }
            LibrarySort.ALPHABETICAL -> roms.sortedBy { (it.config.customName ?: it.name).lowercase() }
        }
    }
    val insets = WindowInsets.safeDrawing.asPaddingValues()
    var sheetRom by remember { mutableStateOf<Rom?>(null) }
    val sheetActions = remember(actions) {
        GameActions(
            onPlay = actions.onPlay,
            onToggleFavorite = actions.onToggleFavorite,
            isFavorite = actions.isFavorite,
            onGameSettings = actions.onGameSettings,
            onInstallTexturePack = actions.onInstallTexturePack,
            onAddShortcut = actions.onAddShortcut,
            onOpenSheet = { sheetRom = it },
            loadIcon = actions.loadIcon,
        )
    }

    Box(Modifier.fillMaxSize().punicaBackground()) {
        LazyColumn(contentPadding = PaddingValues(top = insets.calculateTopPadding(), bottom = insets.calculateBottomPadding() + 32.dp)) {
            item { HomeHeader(searchQuery, menuEntries, onSearchQueryChange, onSettings) }
            item { HomeFeatured(featured, sheetActions) }
            item { SectionTitle(stringResource(R.string.home_quick_access), action = stringResource(R.string.home_all_consoles)) { onPlatformSelected(null) } }
            item {
                LazyRow(contentPadding = PaddingValues(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(platforms) { platform ->
                        ConsoleCard(platform, selected = platform == platformFilter) { onPlatformSelected(if (platform == platformFilter) null else platform) }
                    }
                }
            }
            if (played.isNotEmpty()) {
                item { SectionTitle(stringResource(R.string.home_recently_played)) }
                item {
                    LazyRow(contentPadding = PaddingValues(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        items(played.take(10), key = { it.uri.toString() }) { rom -> RecentGameCard(rom, selected = rom == played.first(), actions = sheetActions) }
                    }
                }
            }
            item { SectionTitle(stringResource(R.string.home_library), icon = true) }
            item {
                LazyRow(contentPadding = PaddingValues(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(LibrarySort.entries) { option -> SortPill(option, option == sort) { sort = option } }
                }
                Spacer(Modifier.height(8.dp))
            }
            if (library.isEmpty()) {
                item {
                    Text(
                        stringResource(if (sort == LibrarySort.FAVORITES) R.string.home_no_favorites else R.string.no_roms_found),
                        fontSize = 14.sp, color = PunicaTheme.textMuted, textAlign = TextAlign.Center,
                        modifier = Modifier.fillMaxWidth().padding(32.dp),
                    )
                }
            }
            // two columns, each name in full
            items(library.chunked(2)) { pair ->
                Row(Modifier.padding(horizontal = 16.dp, vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    pair.forEach { rom -> LibraryGameCard(rom, sheetActions, Modifier.weight(1f)) }
                    if (pair.size == 1) Spacer(Modifier.weight(1f))
                }
            }
        }
        HomeGameSheet(sheetRom, sheetActions) { sheetRom = null }
    }
}

@Composable
private fun HomeHeader(searchQuery: String, menuEntries: List<HomeMenuEntry>, onSearchQueryChange: (String) -> Unit, onSettings: () -> Unit) {
    var searching by rememberSaveable { mutableStateOf(searchQuery.isNotEmpty()) }
    var menu by remember { mutableStateOf(false) }
    Row(Modifier.fillMaxWidth().height(64.dp).padding(start = 16.dp, end = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        if (searching) {
            BasicTextField(
                value = searchQuery,
                onValueChange = onSearchQueryChange,
                singleLine = true,
                textStyle = TextStyle(color = PunicaTheme.text, fontSize = 16.sp),
                cursorBrush = SolidColor(PunicaTheme.red),
                modifier = Modifier.weight(1f).height(48.dp).clip(RoundedCornerShape(24.dp)).background(PunicaTheme.card).border(1.dp, PunicaTheme.line, RoundedCornerShape(24.dp)).padding(horizontal = 16.dp, vertical = 14.dp),
                decorationBox = { field ->
                    if (searchQuery.isEmpty()) Text(stringResource(R.string.hint_search_roms), fontSize = 16.sp, color = PunicaTheme.textMuted)
                    field()
                },
            )
            PunicaHeaderButton(Icons.Filled.Close, stringResource(R.string.home_search_close)) { onSearchQueryChange(""); searching = false }
        } else {
            PunicaLogo(stringResource(R.string.home_subtitle), Modifier.weight(1f))
            PunicaHeaderButton(Icons.Filled.Search, stringResource(R.string.action_search_roms)) { searching = true }
            PunicaHeaderButton(Icons.Filled.Settings, stringResource(R.string.settings), onSettings)
        }
        Box {
            PunicaHeaderButton(Icons.Filled.MoreVert, stringResource(R.string.home_more)) { menu = true }
            DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                menuEntries.forEach { entry ->
                    DropdownMenuItem(onClick = { menu = false; entry.onClick() }) { Text(entry.title) }
                }
            }
        }
    }
}

@Composable
private fun SectionTitle(title: String, icon: Boolean = false, action: String? = null, onAction: (() -> Unit)? = null) {
    Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        if (icon) {
            Icon(Icons.Outlined.Folder, contentDescription = null, tint = PunicaTheme.text, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(8.dp))
        }
        Text(title, fontFamily = PunicaTheme.display, fontWeight = FontWeight.Bold, fontSize = 16.sp, lineHeight = 24.sp, color = PunicaTheme.text, modifier = Modifier.weight(1f))
        if (action != null && onAction != null) {
            Text(action, fontSize = 12.sp, color = PunicaTheme.textMuted, modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onAction).padding(8.dp))
        }
    }
}

@Composable
private fun SortPill(option: LibrarySort, selected: Boolean, onClick: () -> Unit) {
    PunicaPill(
        stringResource(
            when (option) {
                LibrarySort.RECENT -> R.string.home_sort_recent
                LibrarySort.FAVORITES -> R.string.home_sort_favorites
                LibrarySort.PLAY_TIME -> R.string.home_sort_play_time
                LibrarySort.ALPHABETICAL -> R.string.home_sort_alphabetical
            }
        ),
        selected, onClick,
    )
}
