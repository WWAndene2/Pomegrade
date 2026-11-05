package me.magnum.melonds.ui.romlist.home

import android.text.format.DateUtils
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.outlined.AddToHomeScreen
import androidx.compose.material.icons.outlined.Image
import androidx.compose.material.icons.outlined.Save
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.StarOutline
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.common.component.PunicaPlayButton
import me.magnum.melonds.ui.common.component.platformLongName
import me.magnum.melonds.ui.theme.PunicaAlpha
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme

/**
 * The game sheet (owner's mock-up, 13 October), opened by a game's ⋮ or a long press: the game (icon 96, console, name,
 * console tag), its figures, Play and the star, then what can be done with it from the list. Slides up from the
 * bottom over a dimmed screen; a tap outside or Back closes it.
 */
// the consoles whose games have their own settings: DS (Punica's page), Switch and GameCube / Wii (their core's
// options page, which their own game lists opened)
private val GAME_SETTINGS_PLATFORMS = setOf(RomPlatform.NDS, RomPlatform.SWITCH, RomPlatform.GC_WII)

@Composable
internal fun HomeGameSheet(rom: Rom?, actions: GameActions, onDismiss: () -> Unit) {
    if (rom != null) BackHandler(onBack = onDismiss)
    AnimatedVisibility(rom != null, enter = fadeIn(), exit = fadeOut()) {
        Box(
            Modifier
                .fillMaxSize()
                .background(Color(0xA3050507))
                .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null, onClick = onDismiss),
        )
    }
    // the last game shown stays drawn while the sheet slides out
    var shown by remember { mutableStateOf(rom) }
    if (rom != null) shown = rom
    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.BottomCenter) {
        AnimatedVisibility(rom != null, enter = slideInVertically { it }, exit = slideOutVertically { it }) {
            shown?.let { SheetContent(it, actions, onDismiss) }
        }
    }
}

@Composable
private fun SheetContent(rom: Rom, actions: GameActions, onDismiss: () -> Unit) {
    val shape = RoundedCornerShape(topStart = PunicaDimens.SHEET_RADIUS.dp, topEnd = PunicaDimens.SHEET_RADIUS.dp)
    Column(
        Modifier
            .fillMaxWidth()
            .shadow(32.dp, shape, ambientColor = PunicaTheme.red, spotColor = PunicaTheme.red)
            .clip(shape)
            .background(PunicaTheme.background)
            .drawTopLine()
            // the sheet takes the taps, so they don't close it
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { }
            .windowInsetsPadding(WindowInsets.navigationBars)
            .padding(top = 8.dp, bottom = 24.dp),
    ) {
        Box(Modifier.padding(bottom = 8.dp).size(32.dp, 4.dp).clip(RoundedCornerShape(2.dp)).background(PunicaTheme.textMuted.copy(alpha = PunicaAlpha.HALF)).align(Alignment.CenterHorizontally))

        // the game
        Row(Modifier.padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp), verticalAlignment = Alignment.CenterVertically) {
            GameCover(rom, actions.loadIcon, Modifier.size(96.dp).clip(RoundedCornerShape(24.dp)) /* §7.3: 0.24 x 96 = 23 -> 24 */)
            Spacer(Modifier.width(PunicaDimens.GAP_WIDE.dp))
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(platformLongName(rom.platform).uppercase(), fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted)
                Text(rom.config.customName ?: rom.name, fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 32.sp, color = PunicaTheme.text)
                PlatformTag(rom.platform, outlined = false)
            }
        }

        // its figures
        Row(Modifier.padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp), horizontalArrangement = Arrangement.spacedBy(PunicaDimens.GAP.dp)) {
            val playTime = playTimeText(rom.totalPlayTime)
            Stat(playTime.ifEmpty { "0 min" }, stringResource(R.string.home_stat_play_time), Modifier.weight(1f))
            Stat(
                rom.lastPlayed?.let { DateUtils.getRelativeTimeSpanString(it.time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE).toString() }
                    ?: stringResource(R.string.home_never_played),
                stringResource(R.string.home_stat_last_played),
                Modifier.weight(1f),
            )
            Stat(rom.fileName.substringAfterLast('.', "").uppercase(), stringResource(R.string.home_stat_file), Modifier.weight(1f))
        }

        // Play and the star
        Row(Modifier.padding(horizontal = PunicaDimens.MARGIN.dp, vertical = PunicaDimens.GAP.dp), verticalAlignment = Alignment.CenterVertically) {
            PunicaPlayButton(stringResource(R.string.home_play), onClick = { onDismiss(); actions.onPlay(rom) }, modifier = Modifier.weight(1f))
            Spacer(Modifier.width(PunicaDimens.GAP.dp))
            val favorite = actions.isFavorite(rom)
            Box(
                Modifier
                    .size(PunicaDimens.TOUCH.dp)
                    .clip(CircleShape)
                    .background(PunicaTheme.cardRaised)
                    .border(1.dp, PunicaTheme.line, CircleShape)
                    .clickable { actions.onToggleFavorite(rom) },
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    if (favorite) Icons.Filled.Star else Icons.Outlined.StarOutline,
                    contentDescription = stringResource(if (favorite) R.string.home_favorite_remove else R.string.home_favorite_add),
                    tint = if (favorite) PunicaTheme.star else PunicaTheme.text,
                    modifier = Modifier.size(PunicaDimens.ICON.dp),
                )
            }
        }

        // what can be done with it from here
        val cardShape = RoundedCornerShape(PunicaDimens.CARD_RADIUS.dp)
        Column(
            Modifier
                .padding(start = PunicaDimens.MARGIN.dp, end = PunicaDimens.MARGIN.dp, top = PunicaDimens.GAP.dp)
                .fillMaxWidth()
                .clip(cardShape)
                .background(PunicaTheme.card)
                .border(1.dp, PunicaTheme.line, cardShape),
        ) {
            val rows = buildList<Triple<ImageVector, Int, () -> Unit>> {
                actions.onGameSettings?.takeIf { rom.platform in GAME_SETTINGS_PLATFORMS }?.let { open -> add(Triple(Icons.Outlined.Settings, R.string.home_game_settings) { onDismiss(); open(rom) }) }
                add(Triple(Icons.Outlined.Save, R.string.game_data_title) { onDismiss(); actions.onManageData(rom) })
                add(Triple(Icons.Outlined.Image, R.string.texture_pack_install) { onDismiss(); actions.onInstallTexturePack(rom) })
                add(Triple(Icons.Outlined.AddToHomeScreen, R.string.home_shortcut) { onDismiss(); actions.onAddShortcut(rom) })
            }
            rows.forEachIndexed { index, (icon, title, onClick) ->
                if (index > 0) Divider(color = PunicaTheme.line)
                SheetRow(icon, stringResource(title), onClick)
            }
        }
    }
}

@Composable
private fun Stat(value: String, label: String, modifier: Modifier) {
    val shape = RoundedCornerShape(PunicaDimens.TILE_RADIUS.dp)
    Column(modifier.clip(shape).background(PunicaTheme.card).border(1.dp, PunicaTheme.line, shape).padding(horizontal = 12.dp, vertical = 8.dp)) {
        Text(value, fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 16.sp, lineHeight = 24.sp, color = PunicaTheme.text, maxLines = 1)
        Text(label, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted)
    }
}

/** A row of a card: icon on a 32 dp tile, title, chevron; 64 dp high, as the settings' rows. */
@Composable
private fun SheetRow(icon: ImageVector, title: String, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().heightIn(min = PunicaDimens.ROW_MIN.dp).clickable(onClick = onClick).padding(horizontal = PunicaDimens.MARGIN.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(PunicaDimens.ROW_ICON.dp).clip(RoundedCornerShape(8.dp)).background(PunicaTheme.cardRaised), contentAlignment = Alignment.Center) {
            Icon(icon, contentDescription = null, tint = PunicaTheme.text, modifier = Modifier.size(16.dp))
        }
        Spacer(Modifier.width(PunicaDimens.GAP_WIDE.dp))
        Text(title, fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text, modifier = Modifier.weight(1f))
        Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, contentDescription = null, tint = PunicaTheme.textMuted, modifier = Modifier.size(PunicaDimens.ICON.dp))
    }
}

// the sheet's 2 dp red top edge, over its content
private fun Modifier.drawTopLine(): Modifier = drawWithContent {
    drawContent()
    drawRect(PunicaTheme.red, size = Size(size.width, 2.dp.toPx()))
}
