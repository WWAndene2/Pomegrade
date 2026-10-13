package me.magnum.melonds.ui.romlist.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.GenericShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform

// the red art panel's slanted right edge, as in the reference
private val SlantedPanel = GenericShape { size, _ ->
    moveTo(0f, 0f)
    lineTo(size.width, 0f)
    lineTo(size.width * 0.88f, size.height)
    lineTo(0f, size.height)
    close()
}

/** The featured carousel: the last games played (or the first games found), one card each, dots below. */
@Composable
internal fun HomeFeatured(games: List<Rom>, actions: GameActions) {
    if (games.isEmpty()) return
    val pager = rememberPagerState { games.size }
    // the card's size is the reference's (360 x 167 on a 393 dp wide phone), kept as validated by the owner
    HorizontalPager(pager, pageSpacing = 16.dp, contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp)) { page ->
        FeaturedCard(games[page], actions)
    }
    Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.Center) {
        repeat(games.size) { i ->
            Box(Modifier.padding(horizontal = 4.dp).size(8.dp).clip(CircleShape).background(if (i == pager.currentPage) HomeTheme.red else HomeTheme.line))
        }
    }
}

@Composable
private fun FeaturedCard(rom: Rom, actions: GameActions) {
    val shape = RoundedCornerShape(16.dp)
    var menu by remember { mutableStateOf(false) }
    Row(
        Modifier
            .fillMaxWidth()
            .height(167.dp)
            .clip(shape)
            .background(HomeTheme.card)
            .border(1.dp, HomeTheme.line, shape),
    ) {
        Box(Modifier.width(176.dp).fillMaxHeight().clip(SlantedPanel).background(Brush.linearGradient(listOf(HomeTheme.heroRed, HomeTheme.heroRedDark)))) {
            GameCover(rom, actions.loadIcon, Modifier.padding(start = 12.dp, top = 12.dp).size(128.dp).clip(RoundedCornerShape(12.dp)))
        }
        Column(Modifier.weight(1f).fillMaxHeight().padding(start = 4.dp, top = 12.dp, end = 12.dp, bottom = 12.dp)) {
            Text(platformLongName(rom.platform).uppercase(), fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic, fontSize = 12.sp, lineHeight = 16.sp, color = HomeTheme.text)
            Text(rom.config.customName ?: rom.name, fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 16.sp, lineHeight = 20.sp, color = HomeTheme.text, maxLines = 3, modifier = Modifier.padding(top = 2.dp))
            val playTime = playTimeText(rom.totalPlayTime)
            Text(
                if (playTime.isEmpty()) lastPlayedText(rom) else stringResource(R.string.home_featured_detail, playTime, lastPlayedText(rom)),
                fontSize = 12.sp, lineHeight = 16.sp, color = HomeTheme.textMuted, modifier = Modifier.padding(top = 4.dp),
            )
            Spacer(Modifier.weight(1f))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Row(
                    Modifier
                        .weight(1f)
                        .height(32.dp)
                        .shadow(8.dp, RoundedCornerShape(16.dp), ambientColor = HomeTheme.red, spotColor = HomeTheme.red)
                        .clip(RoundedCornerShape(16.dp))
                        .background(Brush.verticalGradient(listOf(HomeTheme.redLight, HomeTheme.red)))
                        .clickable { actions.onPlay(rom) },
                    horizontalArrangement = Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(Icons.Filled.PlayArrow, contentDescription = null, tint = HomeTheme.text, modifier = Modifier.size(16.dp))
                    Spacer(Modifier.width(4.dp))
                    Text(stringResource(R.string.home_play), fontFamily = HomeTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 14.sp, color = HomeTheme.text)
                }
                Spacer(Modifier.width(8.dp))
                Box {
                    Box(
                        Modifier.size(32.dp).clip(CircleShape).background(HomeTheme.cardRaised).border(1.dp, HomeTheme.line, CircleShape).clickable { menu = true },
                        contentAlignment = Alignment.Center,
                    ) {
                        Icon(Icons.Filled.MoreVert, contentDescription = stringResource(R.string.home_game_menu), tint = HomeTheme.text, modifier = Modifier.size(16.dp))
                    }
                    GameMenu(rom, actions, menu) { menu = false }
                }
            }
        }
    }
}

@Composable
internal fun platformLongName(platform: RomPlatform): String = stringResource(
    when (platform) {
        RomPlatform.NDS -> R.string.home_console_ds
        RomPlatform.N3DS -> R.string.home_console_three_ds
        RomPlatform.GBA -> R.string.home_console_gba
        RomPlatform.SWITCH -> R.string.platform_switch
        RomPlatform.GC_WII -> R.string.platform_gc_wii
    }
)
