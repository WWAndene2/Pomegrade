// Copyright Pomegrade Project
// Licensed under GPLv2 or any later version

package org.citra.citra_emu.overlay

import android.content.Context
import android.util.DisplayMetrics
import android.view.WindowManager
import org.citra.citra_emu.NativeLibrary.ButtonType
import org.citra.citra_emu.display.PortraitScreenLayout
import org.citra.citra_emu.features.hotkeys.Hotkey
import org.citra.citra_emu.features.settings.model.IntSetting
import kotlin.math.min

/**
 * Pomegrade: the on-screen controls of the "Classic" portrait layout, placed and sized as the Punica 3DS skin
 * template draws them (assets/consoles/skins/gabarit_skin_3ds_1220x2712.svg). The template is scaled exactly as
 * the native PortraitClassicLayout scales the screens (to the window width, shrunk only for a window shorter than
 * its proportions, centred horizontally, anchored to the top), so screens and controls stay together.
 *
 * Each control's image is fitted inside its template rectangle and centred in it. Azahar's images are square
 * bitmaps; the shoulder ones (L, R, ZL, ZR) draw their button over the middle 48.8 % of their height, so they
 * fit by height where the template's button is flatter than the image. The user's control scale still applies
 * on top. Swap screens, turbo and combo are not on the template: they sit in the free column between the circle
 * pad and ABXY.
 */
object ClassicOverlayLayout {
    private const val TEMPLATE_WIDTH = 1220f
    private const val TEMPLATE_HEIGHT = 2712f
    private const val SHOULDER_CONTENT_HEIGHT = 250f / 512f

    private class Box(val x: Float, val y: Float, val width: Float, val height: Float)

    // Template rectangles, in template pixels. The D-pad is keyed by DPAD_UP, its preference key
    private val boxes = mapOf(
        ButtonType.TRIGGER_L to Box(55f, 1497f, 271f, 92f),
        ButtonType.TRIGGER_R to Box(894f, 1497f, 271f, 92f),
        ButtonType.BUTTON_ZL to Box(397f, 1497f, 165.6f, 84.9f),
        ButtonType.BUTTON_ZR to Box(657.5f, 1497f, 165.6f, 84.9f),
        ButtonType.STICK_LEFT to Box(55f, 1739.9f, 380.2f, 380.2f),
        ButtonType.STICK_C to Box(753.9f, 1718.4f, 128.6f, 128.6f),
        ButtonType.DPAD_UP to Box(80.5f, 2196.4f, 329.3f, 329.3f),
        ButtonType.BUTTON_X to Box(899.9f, 1864.4f, 136.6f, 136.6f),
        ButtonType.BUTTON_Y to Box(771.3f, 1998.2f, 136.6f, 136.6f),
        ButtonType.BUTTON_A to Box(1028.4f, 1998.2f, 136.6f, 136.6f),
        ButtonType.BUTTON_B to Box(899.9f, 2132.1f, 136.6f, 136.6f),
        ButtonType.BUTTON_START to Box(771.4f, 2434.7f, 83f, 83f),
        ButtonType.BUTTON_SELECT to Box(768.7f, 2574f, 83f, 83f),
        ButtonType.BUTTON_HOME to Box(521.6f, 2555.2f, 176.7f, 101.8f),
        ButtonType.BUTTON_SWAP to Box(568.5f, 1700f, 83f, 83f),
        ButtonType.BUTTON_TURBO to Box(568.5f, 1823f, 83f, 83f),
        Hotkey.COMBO_BUTTON.button to Box(568.5f, 1946f, 83f, 83f),
    )

    private val shoulders = setOf(ButtonType.TRIGGER_L, ButtonType.TRIGGER_R, ButtonType.BUTTON_ZL, ButtonType.BUTTON_ZR)

    fun isActive(orientation: String): Boolean =
        orientation == "-Portrait" && IntSetting.PORTRAIT_SCREEN_LAYOUT.int == PortraitScreenLayout.CLASSIC.int

    /**
     * The overlay's size, set by InputOverlay when it is laid out. The overlay covers the emulation surface, which the
     * native layout scales the screens to: on a phone whose status bar stays visible both are shorter than the
     * screen, and scaling to the full screen put the controls below the screens' scale (cut at the bottom).
     */
    var areaWidth = 0
    var areaHeight = 0

    /** Side of the control's square bitmap, as a fraction of the screen's smaller side (Azahar's scale unit). */
    fun bitmapScale(context: Context, id: Int): Float {
        // Azahar sizes bitmaps from the resources' display metrics (resizeBitmap), not the overlay
        val resourceMetrics = context.resources.displayMetrics
        var side = shownSide(id, area(context))
        if (id == ButtonType.STICK_C) {
            // The C stick's touch area is half its range image (InputOverlay's outerScale)
            side *= 2f
        }
        return side / min(resourceMetrics.widthPixels, resourceMetrics.heightPixels)
    }

    /** Top-left corner of the control as drawn, in the overlay's pixels, or null for a control the template does not place. */
    fun position(context: Context, id: Int): Pair<Float, Float>? {
        val box = boxes[id] ?: return null
        val area = area(context)
        val scale = templateScale(area)
        val offsetX = (area.first - TEMPLATE_WIDTH * scale) / 2f
        val side = shownSide(id, area)
        val centreX = offsetX + (box.x + box.width / 2f) * scale
        val centreY = (box.y + box.height / 2f) * scale
        return (centreX - side / 2f) to (centreY - side / 2f)
    }

    val placedControls: Set<Int> get() = boxes.keys

    private fun shownSide(id: Int, area: Pair<Float, Float>): Float {
        val box = boxes.getValue(id)
        val side = if (id in shoulders) {
            min(box.width, box.height / SHOULDER_CONTENT_HEIGHT)
        } else {
            min(box.width, box.height)
        }
        return side * templateScale(area)
    }

    private fun templateScale(area: Pair<Float, Float>): Float =
        min(area.first / TEMPLATE_WIDTH, area.second / TEMPLATE_HEIGHT)

    // Before the overlay is laid out: the full screen, the emulation surface's size when no system bar shows
    @Suppress("DEPRECATION")
    private fun area(context: Context): Pair<Float, Float> {
        if (areaWidth > 0 && areaHeight > 0) {
            return areaWidth.toFloat() to areaHeight.toFloat()
        }
        val metrics = DisplayMetrics()
        val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        windowManager.defaultDisplay.getRealMetrics(metrics)
        return metrics.widthPixels.toFloat() to metrics.heightPixels.toFloat()
    }
}
