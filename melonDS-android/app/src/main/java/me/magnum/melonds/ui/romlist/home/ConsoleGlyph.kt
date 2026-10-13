package me.magnum.melonds.ui.romlist.home

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.unit.dp
import me.magnum.melonds.domain.model.rom.RomPlatform

/** A small drawing of [platform]'s console, 40 x 28 dp, for the quick-access cards. No console maker's artwork. */
@Composable
internal fun ConsoleGlyph(platform: RomPlatform, modifier: Modifier = Modifier) {
    Canvas(modifier.size(40.dp, 28.dp)) {
        val u = size.width / 40f
        when (platform) {
            RomPlatform.NDS -> {
                screenHalf(8 * u, 1 * u, 24 * u, 12 * u, u, Color(0xFFC9CFDA), Color(0xFF2B3A5C))
                screenHalf(8 * u, 15 * u, 24 * u, 12 * u, u, Color(0xFFC9CFDA), Color(0xFF2B3A5C))
            }
            RomPlatform.N3DS -> {
                screenHalf(6 * u, 1 * u, 28 * u, 12 * u, u, Color(0xFFE2343F), Color(0xFF1C1C24))
                screenHalf(8 * u, 15 * u, 24 * u, 12 * u, u, Color(0xFFE2343F), Color(0xFF1C1C24))
            }
            RomPlatform.GBA -> {
                drawRoundRect(Color(0xFF6C55D8), Offset(1 * u, 6 * u), Size(38 * u, 17 * u), CornerRadius(9 * u))
                drawRoundRect(Color(0xFF1C1C24), Offset(13 * u, 9 * u), Size(14 * u, 11 * u), CornerRadius(1 * u))
            }
            RomPlatform.SWITCH -> {
                drawRoundRect(Color(0xFF19B5C8), Offset(0f, 7 * u), Size(10 * u, 15 * u), CornerRadius(4 * u))
                drawRoundRect(Color(0xFFFF5A4E), Offset(30 * u, 7 * u), Size(10 * u, 15 * u), CornerRadius(4 * u))
                drawRect(Color(0xFF2A2D33), Offset(6 * u, 7 * u), Size(28 * u, 15 * u))
            }
            RomPlatform.GC_WII -> {
                drawRoundRect(Color(0xFF6C55C8), Offset(9 * u, 2 * u), Size(22 * u, 22 * u), CornerRadius(4 * u))
                drawCircle(Color(0xFF3B2A73), 5 * u, Offset(20 * u, 13 * u))
            }
        }
    }
}

private fun DrawScope.screenHalf(x: Float, y: Float, w: Float, h: Float, u: Float, shell: Color, screen: Color) {
    drawRoundRect(shell, Offset(x, y), Size(w, h), CornerRadius(3 * u))
    drawRoundRect(screen, Offset(x + 5 * u, y + 2 * u), Size(w - 10 * u, h - 4 * u), CornerRadius(1 * u))
}
