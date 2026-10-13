package me.magnum.melonds.ui.romlist.home

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.foundation.shape.CircleShape
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform

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

    Box(
        Modifier
            .fillMaxSize()
            .background(HomeTheme.background)
            // the dark red sweep behind the header
            .drawBehind {
                drawCircle(
                    Brush.radialGradient(listOf(HomeTheme.sweep, HomeTheme.sweep.copy(alpha = 0.5f), Color.Transparent), center = Offset(0f, 0f), radius = size.width * 0.9f),
                    radius = size.width * 0.9f, center = Offset(0f, 0f),
                )
            },
    ) {
        LazyColumn(contentPadding = PaddingValues(top = insets.calculateTopPadding(), bottom = insets.calculateBottomPadding() + 32.dp)) {
            item { HomeHeader(searchQuery, menuEntries, onSearchQueryChange, onSettings) }
            item { HomeFeatured(featured, actions) }
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
                        items(played.take(10), key = { it.uri.toString() }) { rom -> RecentGameCard(rom, selected = rom == played.first(), actions = actions) }
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
                        fontSize = 14.sp, color = HomeTheme.textMuted, textAlign = TextAlign.Center,
                        modifier = Modifier.fillMaxWidth().padding(32.dp),
                    )
                }
            }
            // two columns, each name in full
            items(library.chunked(2)) { pair ->
                Row(Modifier.padding(horizontal = 16.dp, vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    pair.forEach { rom -> LibraryGameCard(rom, actions, Modifier.weight(1f)) }
                    if (pair.size == 1) Spacer(Modifier.weight(1f))
                }
            }
        }
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
                textStyle = TextStyle(color = HomeTheme.text, fontSize = 16.sp),
                cursorBrush = SolidColor(HomeTheme.red),
                modifier = Modifier.weight(1f).height(48.dp).clip(RoundedCornerShape(24.dp)).background(HomeTheme.card).border(1.dp, HomeTheme.line, RoundedCornerShape(24.dp)).padding(horizontal = 16.dp, vertical = 14.dp),
                decorationBox = { field ->
                    if (searchQuery.isEmpty()) Text(stringResource(R.string.hint_search_roms), fontSize = 16.sp, color = HomeTheme.textMuted)
                    field()
                },
            )
            HeaderButton(Icons.Filled.Close, stringResource(R.string.home_search_close)) { onSearchQueryChange(""); searching = false }
        } else {
            Column(Modifier.weight(1f)) {
                // the logo: "Punica." in a rounded outline, the pomegranate before it
                Row(
                    Modifier.height(32.dp).border(BorderStroke(3.dp, HomeTheme.text), RoundedCornerShape(16.dp)).padding(horizontal = 12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(Modifier.size(16.dp).clip(CircleShape).background(Brush.radialGradient(listOf(Color(0xFFFF8A8F), Color(0xFFDC4249), Color(0xFF8E1622)))))
                    Spacer(Modifier.width(6.dp))
                    Text("Punica", fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 24.sp, color = HomeTheme.text)
                    Text(".", fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 24.sp, color = HomeTheme.red)
                }
                Text(stringResource(R.string.home_subtitle), fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic, fontSize = 16.sp, lineHeight = 20.sp, color = HomeTheme.text)
            }
            HeaderButton(Icons.Filled.Search, stringResource(R.string.action_search_roms)) { searching = true }
            HeaderButton(Icons.Filled.Settings, stringResource(R.string.settings), onSettings)
        }
        Box {
            HeaderButton(Icons.Filled.MoreVert, stringResource(R.string.home_more)) { menu = true }
            DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                menuEntries.forEach { entry ->
                    DropdownMenuItem(onClick = { menu = false; entry.onClick() }) { Text(entry.title) }
                }
            }
        }
    }
}

@Composable
private fun HeaderButton(icon: androidx.compose.ui.graphics.vector.ImageVector, description: String, onClick: () -> Unit) {
    Box(Modifier.size(48.dp).clip(CircleShape).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(icon, contentDescription = description, tint = HomeTheme.text, modifier = Modifier.size(24.dp))
    }
}

@Composable
private fun SectionTitle(title: String, icon: Boolean = false, action: String? = null, onAction: (() -> Unit)? = null) {
    Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        if (icon) {
            Icon(Icons.Outlined.Folder, contentDescription = null, tint = HomeTheme.text, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(8.dp))
        }
        Text(title, fontFamily = HomeTheme.display, fontWeight = FontWeight.Bold, fontSize = 16.sp, lineHeight = 24.sp, color = HomeTheme.text, modifier = Modifier.weight(1f))
        if (action != null && onAction != null) {
            Text(action, fontSize = 12.sp, color = HomeTheme.textMuted, modifier = Modifier.clip(RoundedCornerShape(8.dp)).clickable(onClick = onAction).padding(8.dp))
        }
    }
}

/** A quick-access card (88 x 96, the validated mock-up's size): the console's drawing and its full name; the chosen one ringed in red. */
@Composable
private fun ConsoleCard(platform: RomPlatform, selected: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(12.dp)
    Column(
        Modifier
            .size(88.dp, 96.dp)
            .then(if (selected) Modifier.shadow(12.dp, shape, ambientColor = HomeTheme.red, spotColor = HomeTheme.red) else Modifier)
            .clip(shape)
            .background(HomeTheme.card)
            .border(if (selected) BorderStroke(2.dp, HomeTheme.red) else BorderStroke(1.dp, HomeTheme.line), shape)
            .clickable(onClick = onClick)
            .padding(4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        ConsoleGlyph(platform)
        Spacer(Modifier.height(4.dp))
        Text(platformLongName(platform), fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.Medium, color = HomeTheme.text, textAlign = TextAlign.Center)
    }
}

@Composable
private fun SortPill(option: LibrarySort, selected: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.dp)
    Box(
        Modifier
            .height(32.dp)
            .clip(shape)
            .background(if (selected) HomeTheme.red.copy(alpha = 0.14f) else HomeTheme.card)
            .border(if (selected) BorderStroke(2.dp, HomeTheme.red) else BorderStroke(1.dp, HomeTheme.line), shape)
            .clickable(onClick = onClick)
            .padding(horizontal = if (selected) 16.dp else 12.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            stringResource(
                when (option) {
                    LibrarySort.RECENT -> R.string.home_sort_recent
                    LibrarySort.FAVORITES -> R.string.home_sort_favorites
                    LibrarySort.PLAY_TIME -> R.string.home_sort_play_time
                    LibrarySort.ALPHABETICAL -> R.string.home_sort_alphabetical
                }
            ),
            fontSize = 12.sp, fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium, color = HomeTheme.text,
        )
    }
}
