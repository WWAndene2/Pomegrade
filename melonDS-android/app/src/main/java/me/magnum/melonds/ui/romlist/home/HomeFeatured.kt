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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.ui.common.component.PunicaPlayButton
import me.magnum.melonds.ui.common.component.platformLongName
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme

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
    // the card's width is the reference's (360 on a 393 dp wide phone); its height is the cover's 128 plus 16 above and
    // below (was 167: 12 above the cover, 27 below; symmetry audit, 15 October)
    HorizontalPager(pager, pageSpacing = 16.dp, contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp)) { page ->
        FeaturedCard(games[page], actions)
    }
    Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.Center) {
        repeat(games.size) { i ->
            Box(Modifier.padding(horizontal = 4.dp).size(8.dp).clip(CircleShape).background(if (i == pager.currentPage) PunicaTheme.red else PunicaTheme.line))
        }
    }
}

@Composable
private fun FeaturedCard(rom: Rom, actions: GameActions) {
    // §7.3: 0.24 x 160 = 38 -> 32; the cover, 16 dp inside, takes 32 - 16 = 16 so the two outlines stay parallel
    val shape = RoundedCornerShape(32.dp)
    Row(
        Modifier
            .fillMaxWidth()
            .height(160.dp)
            .clip(shape)
            .background(PunicaTheme.card)
            .border(1.dp, PunicaTheme.line, shape),
    ) {
        Box(Modifier.width(192.dp).fillMaxHeight().clip(SlantedPanel).background(Brush.linearGradient(listOf(PunicaTheme.heroRed, PunicaTheme.heroRedDark)))) {
            GameCover(rom, actions.loadIcon, Modifier.padding(start = 16.dp, top = 16.dp).size(128.dp).clip(RoundedCornerShape(16.dp)) /* parallel to the card: 32 - 16 */)
        }
        Column(Modifier.weight(1f).fillMaxHeight().padding(start = 4.dp, top = 16.dp, end = 16.dp, bottom = 16.dp)) {
            Text(platformLongName(rom.platform).uppercase(), fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.text)
            Text(rom.config.customName ?: rom.name, fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 16.sp, lineHeight = 20.sp, color = PunicaTheme.text, maxLines = 3, modifier = Modifier.padding(top = 2.dp))
            val playTime = playTimeText(rom.totalPlayTime)
            Text(
                if (playTime.isEmpty()) lastPlayedText(rom) else stringResource(R.string.home_featured_detail, playTime, lastPlayedText(rom)),
                fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted, modifier = Modifier.padding(top = 4.dp),
            )
            Spacer(Modifier.weight(1f))
            Row(verticalAlignment = Alignment.CenterVertically) {
                PunicaPlayButton(stringResource(R.string.home_play), onClick = { actions.onPlay(rom) }, modifier = Modifier.weight(1f), height = 32.dp)
                Spacer(Modifier.width(8.dp))
                Box(
                    Modifier.size(32.dp).clip(CircleShape).background(PunicaTheme.cardRaised).border(1.dp, PunicaTheme.line, CircleShape).clickable { actions.onOpenSheet(rom) },
                    contentAlignment = Alignment.Center,
                ) {
                    Icon(Icons.Filled.MoreVert, contentDescription = stringResource(R.string.home_game_menu), tint = PunicaTheme.text, modifier = Modifier.size(16.dp))
                }
            }
        }
    }
}
