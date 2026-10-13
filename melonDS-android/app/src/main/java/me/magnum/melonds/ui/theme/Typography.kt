package me.magnum.melonds.ui.theme

import androidx.compose.material.MaterialTheme
import androidx.compose.material.Typography
import androidx.compose.runtime.Composable
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import me.magnum.melonds.R
import androidx.compose.ui.unit.sp

// titles in Punica's display face, as the home screen's (HomeTheme.display)
private val PunicaDisplay = FontFamily(
    Font(R.font.bricolage_grotesque_bold, FontWeight.Bold),
    Font(R.font.bricolage_grotesque_extrabold, FontWeight.ExtraBold),
)

val MelonTypography @Composable get() = Typography(
    h5 = MaterialTheme.typography.h5.copy(fontFamily = PunicaDisplay, fontWeight = FontWeight.ExtraBold),
    h6 = MaterialTheme.typography.h6.copy(fontFamily = PunicaDisplay, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 32.sp),
    subtitle1 = MaterialTheme.typography.subtitle1.copy(fontFamily = PunicaDisplay, fontWeight = FontWeight.Bold),
    body1 = MaterialTheme.typography.body1.copy(lineHeight = 20.sp),
    button = MaterialTheme.typography.button.copy(fontFamily = PunicaDisplay, fontWeight = FontWeight.Bold),
)