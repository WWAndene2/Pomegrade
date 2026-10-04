package me.magnum.melonds.ui.theme

import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.colorResource
import me.magnum.melonds.R

val uncheckedThumbColor: Color @Composable get() = colorResource(id = R.color.switchThumbUnselected)
val gameMasteryColor: Color get() = Color(0xFFFFD700)

// Pomegrade palette, from the icon's pomegranate red (#A20F27); kept in step with res/values*/colors.xml
val LightMelonColors @Composable get() = lightColors(
    primary = Color(0xFFA20F27),          // R.color.colorPrimary
    primaryVariant = Color(0xFF7A0B1D),   // R.color.colorPrimaryDark
    secondary = Color(0xFFC62839),        // R.color.colorAccent
    secondaryVariant = Color(0xFFC62839), // R.color.colorAccent
    background = Color(0xFFFBF7F5),       // R.color.colorBackground
    surface = Color(0xFFFBF7F5),          // R.color.colorSurface
    onPrimary = Color(0xFFFFFFFF),        // R.color.colorOnSecondary
    onSecondary = Color(0xFFFFFFFF),      // R.color.colorOnSecondary
    onSurface = Color(0xFF222222),        // R.color.textColorPrimary
    onBackground = Color(0xFF767676),     // R.color.textColorSecondary
)

val DarkMelonColors @Composable get() = darkColors(
    primary = Color(0xFF2A1A1D),          // R.color.colorPrimary,
    primaryVariant = Color(0xFF1C1113),   // R.color.colorPrimaryDark,
    secondary = Color(0xFFF0566B),        // R.color.colorAccent,
    secondaryVariant = Color(0xFFF0566B), // R.color.colorAccent,
    background = Color(0xFF1E1517),       // R.color.colorBackground,
    surface = Color(0xFF1E1517),          // R.color.colorSurface,
    onPrimary = Color(0xFFFFFFFF),        // R.color.colorOnSecondary,
    onSecondary = Color(0xFFFFFFFF),      // R.color.colorOnSecondary,
    onSurface = Color(0xFFFFFFFF),        // R.color.textColorPrimary,
    onBackground = Color(0xFFC1C1C1),     // R.color.textColorSecondary,
)
