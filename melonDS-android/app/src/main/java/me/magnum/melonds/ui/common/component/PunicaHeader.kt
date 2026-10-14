package me.magnum.melonds.ui.common.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme

/**
 * Every Punica screen's background (owner's choice, 13 October): the dark ground and the red sweep in its top-left
 * corner, as on the home screen. Put it on the screen's root, under a transparent Scaffold.
 */
fun Modifier.punicaBackground(): Modifier = this
    .background(PunicaTheme.background)
    .drawBehind {
        val radius = size.width * 0.9f
        drawCircle(
            Brush.radialGradient(listOf(PunicaTheme.sweep, PunicaTheme.sweep.copy(alpha = 0.5f), Color.Transparent), center = Offset.Zero, radius = radius),
            radius = radius,
            center = Offset.Zero,
        )
    }

/** The logo: the pomegranate (the app's logo) and "Punica." in a 32 dp rounded outline, [subtitle] in italics under it. */
@Composable
fun PunicaLogo(subtitle: String, modifier: Modifier = Modifier) {
    Column(modifier) {
        Row(
            Modifier.height(32.dp).border(BorderStroke(3.dp, PunicaTheme.text), RoundedCornerShape(16.dp)).padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            // the app's logo, the pomegranate (owner's request, 13 October; was a red dot)
            Image(painterResource(R.drawable.punica_logo_fruit), contentDescription = null, modifier = Modifier.size(24.dp))
            Spacer(Modifier.width(6.dp))
            Text("Punica", fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 24.sp, color = PunicaTheme.text)
            Text(".", fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 24.sp, color = PunicaTheme.red)
        }
        Text(
            subtitle,
            fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic, fontSize = 16.sp, lineHeight = 20.sp,
            color = PunicaTheme.text, maxLines = 1, overflow = TextOverflow.Ellipsis,
            // lined up with the pill's content (its 12 dp inner padding), 4 dp under its outline
            modifier = Modifier.padding(start = PunicaDimens.GAP_WIDE.dp, top = 4.dp),
        )
    }
}

/**
 * The home screen's header for every other screen: back (when [onBack] is given), the logo with the screen's name
 * under it, then [actions] (48 dp round buttons, [PunicaHeaderButton]). 64 dp high, under the status bar.
 */
@Composable
fun PunicaHeader(subtitle: String, onBack: (() -> Unit)?, modifier: Modifier = Modifier, actions: @Composable RowScope.() -> Unit = {}) {
    Row(
        modifier
            .fillMaxWidth()
            .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal))
            .height(PunicaDimens.HEADER.dp)
            .padding(start = if (onBack != null) 4.dp else PunicaDimens.MARGIN.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (onBack != null) {
            PunicaHeaderButton(Icons.AutoMirrored.Filled.ArrowBack, stringResource(R.string.navigate_back), onBack)
        }
        PunicaLogo(subtitle, Modifier.weight(1f))
        actions()
    }
}

/** A header button: 48 dp round touch area, 24 dp icon. */
@Composable
fun PunicaHeaderButton(icon: ImageVector, description: String, onClick: () -> Unit) {
    Box(Modifier.size(PunicaDimens.TOUCH.dp).clip(CircleShape).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(icon, contentDescription = description, tint = PunicaTheme.text, modifier = Modifier.size(PunicaDimens.ICON.dp))
    }
}
