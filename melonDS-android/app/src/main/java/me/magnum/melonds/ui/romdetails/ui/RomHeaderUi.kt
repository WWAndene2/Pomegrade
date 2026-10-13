package me.magnum.melonds.ui.romdetails.ui

import android.net.Uri
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.material3.adaptive.currentWindowAdaptiveInfo
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.platform.LocalInspectionMode
import androidx.compose.ui.platform.LocalResources
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.window.core.layout.WindowSizeClass
import coil.compose.AsyncImage
import kotlinx.coroutines.launch
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.ui.common.MelonPreviewSet
import me.magnum.melonds.ui.common.component.PunicaHeader
import me.magnum.melonds.ui.common.component.PunicaPill
import me.magnum.melonds.ui.common.component.PunicaPlayButton
import me.magnum.melonds.ui.romdetails.model.RomDetailsTab
import me.magnum.melonds.ui.theme.MelonTheme
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme
import java.util.Date
import kotlin.time.Duration

/**
 * The game's header, in Punica's style (owner's choice, 13 October): the shared header with the game's name, the
 * game (icon 96, console, name, play time) and the red Play button, then the tabs as pills.
 */
@Composable
fun RomHeaderUi(
    modifier: Modifier,
    rom: Rom,
    pagerState: PagerState,
    initialFocusRequester: FocusRequester,
    onLaunchRom: () -> Unit,
    onNavigateBack: () -> Unit,
    onTabClicked: (RomDetailsTab) -> Unit,
) {
    Column(modifier) {
        PunicaHeader(subtitle = stringResource(R.string.home_game_settings), onBack = onNavigateBack)
        val windowSizeClass = currentWindowAdaptiveInfo().windowSizeClass
        val isLandscape = windowSizeClass.isWidthAtLeastBreakpoint(WindowSizeClass.WIDTH_DP_MEDIUM_LOWER_BOUND)
        Row(
            Modifier
                .fillMaxWidth()
                .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal))
                .padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            RomIcon(rom, Modifier.size(96.dp).clip(RoundedCornerShape(PunicaDimens.TILE_RADIUS.dp)))
            Spacer(Modifier.width(PunicaDimens.GAP_WIDE.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    text = stringResource(R.string.home_console_ds).uppercase(),
                    fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic,
                    fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted,
                )
                Text(
                    text = rom.config.customName ?: rom.name,
                    fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 32.sp,
                    color = PunicaTheme.text, maxLines = 2, overflow = TextOverflow.Ellipsis,
                )
                PlayTimeText(rom.totalPlayTime)
            }
            if (isLandscape) {
                Spacer(Modifier.width(PunicaDimens.MARGIN.dp))
                PunicaPlayButton(stringResource(R.string.home_play), onClick = onLaunchRom, modifier = Modifier.width(192.dp).focusRequester(initialFocusRequester))
            }
        }
        if (!isLandscape) {
            PunicaPlayButton(
                stringResource(R.string.home_play),
                onClick = onLaunchRom,
                modifier = Modifier
                    .fillMaxWidth()
                    .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal))
                    .padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp)
                    .focusRequester(initialFocusRequester),
            )
        }
        Row(
            Modifier
                .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal))
                .padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp),
            horizontalArrangement = Arrangement.spacedBy(PunicaDimens.GAP.dp),
        ) {
            PunicaPill(
                text = stringResource(id = R.string.rom_details_configuration_tab),
                selected = pagerState.currentPage == RomDetailsTab.CONFIG.tabIndex,
                onClick = { onTabClicked(RomDetailsTab.CONFIG) },
            )
            PunicaPill(
                text = stringResource(id = R.string.retro_achievements_tab),
                selected = pagerState.currentPage == RomDetailsTab.RETRO_ACHIEVEMENTS.tabIndex,
                onClick = { onTabClicked(RomDetailsTab.RETRO_ACHIEVEMENTS) },
            )
        }
    }
}

// the game's icon, sharp (pixel art); a grey square in previews
@Composable
private fun RomIcon(rom: Rom, modifier: Modifier) {
    if (LocalInspectionMode.current) {
        Box(modifier.background(Color.Gray))
    } else {
        AsyncImage(
            modifier = modifier.background(PunicaTheme.cardRaised),
            model = rom,
            contentDescription = null,
            filterQuality = FilterQuality.None,
        )
    }
}

@Composable
private fun PlayTimeText(playTime: Duration) {
    val resources = LocalResources.current
    val romPlayTime = remember(playTime) {
        playTime.toComponents { hours, minutes, _, _ ->
            if (hours > 0) {
                resources.getString(R.string.info_play_time_hours_minutes, hours, minutes)
            } else {
                resources.getString(R.string.info_play_time_minutes, minutes)
            }
        }
    }
    Text(
        text = romPlayTime,
        fontSize = 12.sp,
        lineHeight = 16.sp,
        color = PunicaTheme.textMuted,
        maxLines = 1,
    )
}

@ExperimentalFoundationApi
@MelonPreviewSet
@Preview(name = "Landscape", device = "spec:width=411dp,height=600dp,orientation=landscape,dpi=420")
@Composable
private fun PreviewRomHeaderUi() {
    MelonTheme {
        val pagerState = rememberPagerState(
            initialPage = RomDetailsTab.CONFIG.tabIndex,
            pageCount = { RomDetailsTab.entries.size },
        )
        val coroutineScope = rememberCoroutineScope()

        RomHeaderUi(
            modifier = Modifier.fillMaxWidth(),
            rom = Rom(
                name = "Professor Layton and the Lost Future",
                developerName = "Nontendo",
                fileName = "layton.nds",
                uri = Uri.EMPTY,
                parentTreeUri = Uri.EMPTY,
                config = RomConfig(customName = "Layton Custom"),
                lastPlayed = Date(),
                isDsiWareTitle = false,
                retroAchievementsHash = "",
            ),
            pagerState = pagerState,
            initialFocusRequester = remember { FocusRequester() },
            onLaunchRom = { },
            onNavigateBack = { }
        ) {
            coroutineScope.launch {
                pagerState.scrollToPage(it.tabIndex)
            }
        }
    }
}