package me.magnum.melonds.ui.theme

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import me.magnum.melonds.R

/**
 * Punica's look, the validated home screen's (owner's choice, 13 October), for every Punica screen: always dark.
 * The XML side has the same values (values-night/colors.xml, drawable/bg_punica_*). Sizes are on the
 * PerfectSuite scale (CLAUDE.md section 7) and live in [PunicaDimens].
 */
object PunicaTheme {
    val background = Color(0xFF111215)
    val card = Color(0xFF191B20)
    val cardRaised = Color(0xFF1F2228)
    val line = Color(0xFF2B2E35)
    val text = Color(0xFFF3F4F6)
    val textMuted = Color(0xFF9EA3AC)
    val red = Color(0xFFEC2B39)
    val redLight = Color(0xFFFF4553)
    val redTagText = Color(0xFFFF5C68)
    val heroRed = Color(0xFFD0101F)
    val heroRedDark = Color(0xFF8E0A16)
    val sweep = Color(0xFF6A0B15)
    val star = Color(0xFFFFC93C)

    // Bricolage Grotesque (SIL Open Font License, its notice is inside the font files): titles, buttons, the logo
    val display = FontFamily(
        Font(R.font.punica_bricolage_grotesque_bold, FontWeight.Bold),
        Font(R.font.punica_bricolage_grotesque_extrabold, FontWeight.ExtraBold),
    )
}

/**
 * The four opacity levels of Punica's screens; nothing else is used. The XML side has the same: the menu's selected
 * fill (:emulator-ui emulator_menu_item_fill, 0.14), the title mask (punica_bg_title_mask, #4D = 30 %), the switch
 * rail and the window's sweep (#80 = 50 %).
 */
object PunicaAlpha {
    /** a selected item's red tint over its card */
    const val SELECTED = 0.14f
    /** the see-through mask behind the header's title */
    const val MASK = 0.3f
    /** half: a fading gradient's middle, a secondary mark (sheet handle) */
    const val HALF = 0.5f
    /** a scrim over content, an item that is shown but not chosen */
    const val SOFT = 0.8f
}

/** The shared sizes, in dp (sp for text), all on the PerfectSuite scale. */
object PunicaDimens {
    const val HEADER = 64
    const val TOUCH = 48
    const val MARGIN = 16
    const val GAP = 8
    const val GAP_WIDE = 12
    const val CARD_RADIUS = 16
    const val TILE_RADIUS = 12
    const val SHEET_RADIUS = 24
    const val ROW_MIN = 64
    const val ICON = 24
    const val ROW_ICON = 32
}
