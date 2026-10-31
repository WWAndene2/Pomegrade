package me.magnum.melonds.ui.romlist.home

import android.text.format.DateUtils
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.outlined.AccessTime
import androidx.compose.material.icons.outlined.StarOutline
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.romlist.RomIcon
import me.magnum.melonds.ui.theme.PunicaTheme
import kotlin.time.Duration

/** What a game card's ⋮ and the hero's ⋮ offer. */
internal class GameActions(
    val onPlay: (Rom) -> Unit,
    val onToggleFavorite: (Rom) -> Unit,
    val isFavorite: (Rom) -> Boolean,
    /** Null when the game has no per-game settings screen reachable from here (only DS games have one). */
    val onGameSettings: ((Rom) -> Unit)?,
    val onInstallTexturePack: (Rom) -> Unit,
    /** Opens the game's mods and saves page (GameDataActivity). */
    val onManageData: (Rom) -> Unit,
    val onAddShortcut: (Rom) -> Unit,
    /** Opens the game sheet ([HomeGameSheet]): the ⋮ and the long press. */
    val onOpenSheet: (Rom) -> Unit,
    val loadIcon: suspend (Rom) -> RomIcon,
)

@Composable
internal fun platformShortName(platform: RomPlatform): String = stringResource(
    when (platform) {
        RomPlatform.NDS -> R.string.platform_ds
        RomPlatform.N3DS -> R.string.platform_three_ds
        RomPlatform.GBA -> R.string.platform_gba
        RomPlatform.SWITCH -> R.string.platform_switch
        RomPlatform.GC_WII -> R.string.platform_gc_wii
    }
)

/** The red pill with the console's name (filled on recent cards, outlined in the library). */
@Composable
internal fun PlatformTag(platform: RomPlatform, outlined: Boolean, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(8.dp)
    Box(
        modifier
            .height(16.dp)
            .then(if (outlined) Modifier.border(1.dp, PunicaTheme.red, shape) else Modifier.background(PunicaTheme.red, shape))
            .padding(horizontal = 6.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(platformShortName(platform), fontSize = 12.sp, lineHeight = 14.sp, fontWeight = if (outlined) FontWeight.Medium else FontWeight.Bold, color = if (outlined) PunicaTheme.redTagText else PunicaTheme.text)
    }
}

@Composable
internal fun GameMenuButton(rom: Rom, actions: GameActions, modifier: Modifier = Modifier) {
    Box(modifier.size(24.dp).clip(RoundedCornerShape(12.dp)).clickable { actions.onOpenSheet(rom) }, contentAlignment = Alignment.Center) {
        Icon(Icons.Filled.MoreVert, contentDescription = stringResource(R.string.home_game_menu), tint = PunicaTheme.text, modifier = Modifier.size(16.dp))
    }
}

/** "Last played: 5 minutes ago", in the phone's language. */
@Composable
internal fun lastPlayedText(rom: Rom): String {
    val context = LocalContext.current
    val date = rom.lastPlayed ?: return stringResource(R.string.home_never_played)
    val ago = DateUtils.getRelativeTimeSpanString(date.time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS).toString()
    return context.getString(R.string.home_last_played, ago)
}

/** "42 h 05" or "17 min"; empty for a game never played. */
internal fun playTimeText(duration: Duration): String {
    val minutes = duration.inWholeMinutes
    return when {
        minutes <= 0 -> ""
        minutes < 60 -> "$minutes min"
        else -> "%d h %02d".format(minutes / 60, minutes % 60)
    }
}

/** A "Recently played" card: 144 wide, image 2:1 (128 x 64), console tag on it, name on as many lines as it needs. */
@Composable
internal fun RecentGameCard(rom: Rom, selected: Boolean, actions: GameActions) {
    // 16, as the library card; its cover, 8 dp inside, takes 16 - 8 = 8: the two outlines stay parallel
    val shape = RoundedCornerShape(16.dp)
    Column(
        Modifier
            .width(144.dp)
            .then(if (selected) Modifier.shadow(16.dp, shape, ambientColor = PunicaTheme.red, spotColor = PunicaTheme.red) else Modifier)
            .clip(shape)
            .background(PunicaTheme.card)
            .border(if (selected) BorderStroke(2.dp, PunicaTheme.red) else BorderStroke(1.dp, PunicaTheme.line), shape)
            .combinedClickable(onLongClick = { actions.onOpenSheet(rom) }) { actions.onPlay(rom) }
            .padding(8.dp),
    ) {
        Box {
            GameCover(rom, actions.loadIcon, Modifier.fillMaxWidth().height(64.dp).clip(RoundedCornerShape(16.dp)))
            PlatformTag(rom.platform, outlined = false, modifier = Modifier.padding(4.dp))
        }
        Row(Modifier.padding(top = 4.dp), verticalAlignment = Alignment.Top) {
            // 4 dp down: its first line centred on the 24 dp menu button, as the clock on the line below
            Text(rom.config.customName ?: rom.name, fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text, modifier = Modifier.weight(1f).padding(top = 4.dp))
            GameMenuButton(rom, actions)
        }
        Row(Modifier.padding(top = 4.dp), verticalAlignment = Alignment.Top) {
            Icon(Icons.Outlined.AccessTime, contentDescription = null, tint = PunicaTheme.textMuted, modifier = Modifier.padding(top = 2.dp).size(12.dp))
            Spacer(Modifier.width(4.dp))
            Text(lastPlayedText(rom), fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted)
        }
    }
}

/** A library card: icon 48, name in full, console tag, star. */
@Composable
internal fun LibraryGameCard(rom: Rom, actions: GameActions, modifier: Modifier = Modifier) {
    // 0.24 x its 64 dp height (CLAUDE.md 7.3); its cover, 8 dp inside, takes 16 - 8 = 8: the outlines stay parallel
    val shape = RoundedCornerShape(16.dp)
    Box(modifier) {
        Row(
            Modifier
                .fillMaxWidth()
                .heightIn(min = 64.dp)
                .clip(shape)
                .background(PunicaTheme.card)
                .border(1.dp, PunicaTheme.line, shape)
                .combinedClickable(onLongClick = { actions.onOpenSheet(rom) }) { actions.onPlay(rom) }
                .padding(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            GameCover(rom, actions.loadIcon, Modifier.size(48.dp).clip(RoundedCornerShape(12.dp)))
            Spacer(Modifier.width(8.dp))
            Column(Modifier.weight(1f)) {
                Text(rom.config.customName ?: rom.name, fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text)
                Spacer(Modifier.height(4.dp))
                PlatformTag(rom.platform, outlined = true)
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                val favorite = actions.isFavorite(rom)
                Box(Modifier.size(24.dp).clip(RoundedCornerShape(12.dp)).clickable { actions.onToggleFavorite(rom) }, contentAlignment = Alignment.Center) {
                    Icon(
                        if (favorite) Icons.Filled.Star else Icons.Outlined.StarOutline,
                        contentDescription = stringResource(if (favorite) R.string.home_favorite_remove else R.string.home_favorite_add),
                        tint = if (favorite) PunicaTheme.star else PunicaTheme.text,
                        modifier = Modifier.size(16.dp),
                    )
                }
                GameMenuButton(rom, actions)
            }
        }
    }
}
