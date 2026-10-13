package me.magnum.melonds.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Shapes
import androidx.compose.runtime.Composable
import androidx.compose.ui.unit.dp

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
        // Punica's corners (PunicaDimens): chips and tiles 12, cards, menus and dialogs 16, sheets 24
        shapes = Shapes(small = RoundedCornerShape(12.dp), medium = RoundedCornerShape(16.dp), large = RoundedCornerShape(24.dp)),
    ) {
        content()
    }
}