package me.magnum.melonds.ui.romlist.home

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import me.magnum.melonds.domain.model.RomIconFiltering
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.ui.romlist.RomIcon
import me.magnum.melonds.ui.theme.PunicaTheme

/** A game's icon filling [modifier]'s box (clip it there), read with [loadIcon]; the card colour until it is read, or without one. */
@Composable
internal fun GameCover(rom: Rom, loadIcon: suspend (Rom) -> RomIcon, modifier: Modifier = Modifier) {
    val icon by produceState<Pair<ImageBitmap, Boolean>?>(null, rom.uri) {
        val romIcon = loadIcon(rom)
        value = romIcon.bitmap?.let { it.asImageBitmap() to (romIcon.filtering == RomIconFiltering.LINEAR) }
    }
    Box(modifier.background(PunicaTheme.cardRaised), contentAlignment = Alignment.Center) {
        icon?.let { (bitmap, linear) ->
            Image(
                bitmap = bitmap,
                contentDescription = null,
                contentScale = ContentScale.Crop,
                // the games' icons are pixel art (32 x 32 for DS): sharp unless the user chose smoothing
                filterQuality = if (linear) FilterQuality.Low else FilterQuality.None,
                modifier = Modifier.matchParentSize(),
            )
        }
    }
}
