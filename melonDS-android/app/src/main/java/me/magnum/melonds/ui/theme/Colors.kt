package me.magnum.melonds.ui.theme

import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.colorResource
import me.magnum.melonds.R

val uncheckedThumbColor: Color @Composable get() = colorResource(id = R.color.switchThumbUnselected)
val gameMasteryColor: Color get() = Color(0xFFFFD700)

// Pomegrade palette, from the icon's coral red (#DC4249); kept in step with res/values*/colors.xml
val LightMelonColors @Composable get() = lightColors(
    primary = Color(0xFFDC4249),          // R.color.colorPrimary
    primaryVariant = Color(0xFFB42430),   // R.color.colorPrimaryDark
    secondary = Color(0xFFDC4249),        // R.color.colorAccent
    secondaryVariant = Color(0xFFDC4249), // R.color.colorAccent
    background = Color(0xFFFFFBFF),       // R.color.colorBackground
    surface = Color(0xFFFFFBFF),          // R.color.colorSurface
    onPrimary = Color(0xFFFFFFFF),        // R.color.colorOnSecondary
    onSecondary = Color(0xFFFFFFFF),      // R.color.colorOnSecondary
    onSurface = Color(0xFF222222),        // R.color.textColorPrimary
    onBackground = Color(0xFF767676),     // R.color.textColorSecondary
)

val DarkMelonColors @Composable get() = darkColors(
    primary = Color(0xFF261E1D),          // R.color.colorPrimary,
    primaryVariant = Color(0xFF191211),   // R.color.colorPrimaryDark,
    secondary = Color(0xFFFF7071),        // R.color.colorAccent,
    secondaryVariant = Color(0xFFFF7071), // R.color.colorAccent,
    background = Color(0xFF1D1615),       // R.color.colorBackground,
    surface = Color(0xFF1D1615),          // R.color.colorSurface,
    onPrimary = Color(0xFFFFFFFF),        // R.color.colorOnSecondary,
    onSecondary = Color(0xFFFFFFFF),      // R.color.colorOnSecondary,
    onSurface = Color(0xFFFFFFFF),        // R.color.textColorPrimary,
    onBackground = Color(0xFFC1C1C1),     // R.color.textColorSecondary,
)
