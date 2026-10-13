package me.magnum.melonds.ui.common.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.ui.theme.PunicaTheme

/**
 * A choice pill (sorts, tabs, render modes): 32 dp high, outlined; the chosen one red-ringed with a faint red
 * fill and wider padding, as on the home screen.
 */
@Composable
fun PunicaPill(text: String, selected: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(16.dp)
    Box(
        modifier
            .height(32.dp)
            .clip(shape)
            .background(if (selected) PunicaTheme.red.copy(alpha = 0.14f) else PunicaTheme.card)
            .border(if (selected) BorderStroke(2.dp, PunicaTheme.red) else BorderStroke(1.dp, PunicaTheme.line), shape)
            .clickable(onClick = onClick)
            .padding(horizontal = if (selected) 16.dp else 12.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(text, fontSize = 12.sp, fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium, color = PunicaTheme.text)
    }
}

/**
 * The red "Play" button: a pill (corners half the height) with a light-to-red gradient and a red glow, the play
 * triangle before the text. [height] 32 (cards) or 48 (screens).
 */
@Composable
fun PunicaPlayButton(text: String, onClick: () -> Unit, modifier: Modifier = Modifier, height: Dp = 48.dp) {
    val shape = RoundedCornerShape(height / 2)
    val large = height >= 48.dp
    Row(
        modifier
            .height(height)
            .shadow(if (large) 16.dp else 8.dp, shape, ambientColor = PunicaTheme.red, spotColor = PunicaTheme.red)
            .clip(shape)
            .background(Brush.verticalGradient(listOf(PunicaTheme.redLight, PunicaTheme.red)))
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Filled.PlayArrow, contentDescription = null, tint = PunicaTheme.text, modifier = Modifier.size(if (large) 24.dp else 16.dp))
        Spacer(Modifier.width(if (large) 8.dp else 4.dp))
        Text(text, fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = if (large) 16.sp else 14.sp, color = PunicaTheme.text)
    }
}
