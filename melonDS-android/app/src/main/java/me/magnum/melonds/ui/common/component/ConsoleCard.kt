package me.magnum.melonds.ui.common.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.theme.PunicaTheme

/** A console card (96 x 96: the mock-up's 88 wide put on the scale, square; home quick access and the settings' console picker): the console's drawing and its full name; the chosen one ringed in red. */
@Composable
fun ConsoleCard(platform: RomPlatform, selected: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(12.dp)
    Column(
        Modifier
            .size(96.dp)
            .then(if (selected) Modifier.shadow(12.dp, shape, ambientColor = PunicaTheme.red, spotColor = PunicaTheme.red) else Modifier)
            .clip(shape)
            .background(PunicaTheme.card)
            .border(if (selected) BorderStroke(2.dp, PunicaTheme.red) else BorderStroke(1.dp, PunicaTheme.line), shape)
            .clickable(onClick = onClick)
            .padding(4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        ConsoleGlyph(platform)
        Spacer(Modifier.height(4.dp))
        Text(platformLongName(platform), fontSize = 12.sp, lineHeight = 16.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text, textAlign = TextAlign.Center)
    }
}

@Composable
fun platformLongName(platform: RomPlatform): String = stringResource(
    when (platform) {
        RomPlatform.NDS -> R.string.home_console_ds
        RomPlatform.N3DS -> R.string.home_console_three_ds
        RomPlatform.GBA -> R.string.home_console_gba
        RomPlatform.SWITCH -> R.string.platform_switch
        RomPlatform.GC_WII -> R.string.platform_gc_wii
    }
)
