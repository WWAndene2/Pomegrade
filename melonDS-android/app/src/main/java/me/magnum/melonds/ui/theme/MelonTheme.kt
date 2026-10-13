package me.magnum.melonds.ui.theme

import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable

@Composable
fun MelonTheme(
    // always dark, as the rest of Punica (MelonDSApplication.applyTheme)
    isDarkTheme: Boolean = true,
    content: @Composable () -> Unit
) {
    val colors = if (isDarkTheme) DarkMelonColors else LightMelonColors

    MaterialTheme(
        colors = colors,
        typography = MelonTypography,
    ) {
        content()
    }
}