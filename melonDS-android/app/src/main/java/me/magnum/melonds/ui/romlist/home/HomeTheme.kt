package me.magnum.melonds.ui.romlist.home

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import me.magnum.melonds.R

/**
 * The home screen's look (owner's choice, 13 October: the dark "virtual console" mock-up), always dark whatever
 * the app's theme. Sizes are in the composables, on the PerfectSuite scale; the few taken as-is from the
 * owner's reference (hero 167, quick-access card 88 x 96) are noted where they are used.
 */
internal object HomeTheme {
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
        Font(R.font.bricolage_grotesque_bold, FontWeight.Bold),
        Font(R.font.bricolage_grotesque_extrabold, FontWeight.ExtraBold),
    )
}
