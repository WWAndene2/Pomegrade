package me.magnum.melonds.impl.layout

import me.magnum.melonds.domain.model.Rect
import me.magnum.melonds.domain.model.layout.LayoutComponent
import me.magnum.melonds.domain.model.layout.PositionedLayoutComponent
import me.magnum.melonds.domain.model.layout.ScreenLayout
import me.magnum.melonds.domain.model.layout.UILayout
import me.magnum.melonds.domain.model.layout.UILayoutVariant
import me.magnum.melonds.domain.model.ui.Orientation
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Builds Pomegrade's "Classic" layout: the screens and buttons of the Punica DS skin template
 * (assets/consoles/skins/gabarit_skin_ds_1220x2712.svg), whose buttons are measured on the DS Lite plan at one
 * common scale. The template is scaled to fit the safe area, centred horizontally and anchored to its top.
 *
 * Only a plain portrait window has a template; landscape, folding and dual-display variants fall back to the default
 * layout.
 */
class ClassicLayoutProvider(private val defaultLayoutProvider: DefaultLayoutProvider) {

    private companion object {
        const val TEMPLATE_WIDTH = 1220f
        const val TEMPLATE_HEIGHT = 2712f

        // Template rectangles (x, y, width, height), in template pixels
        val TOP_SCREEN = Rect(56, 56, 1108, 831)
        val BOTTOM_SCREEN = Rect(56, 943, 1108, 831)
        val BUTTON_L = Rect(56, 1830, 294, 100)
        val BUTTON_R = Rect(870, 1830, 294, 100)
        val DPAD = Rect(56, 2099, 326, 326)
        val BUTTONS = Rect(758, 2059, 406, 406)
        val BUTTON_START = Rect(699, 2593, 63, 63)
        val BUTTON_SELECT = Rect(459, 2593, 63, 63)

        // Not on the DS: a row of four between L and R, so every input stays reachable
        const val UTILITY_SIZE = 96
        const val UTILITY_SPACING = 24
        const val UTILITY_TOP = 1830 + (100 - UTILITY_SIZE) / 2
        val UTILITY_COMPONENTS = listOf(
            LayoutComponent.BUTTON_HINGE,
            LayoutComponent.BUTTON_TOGGLE_SOFT_INPUT,
            LayoutComponent.BUTTON_MICROPHONE_TOGGLE,
            LayoutComponent.BUTTON_FAST_FORWARD_TOGGLE,
        )
    }

    fun buildClassicLayout(variant: UILayoutVariant): UILayout {
        val isPlainPortrait = variant.orientation == Orientation.PORTRAIT && variant.folds.isEmpty() && variant.displays.secondaryScreenDisplay == null
        if (!isPlainPortrait) {
            return defaultLayoutProvider.buildDefaultLayout(variant)
        }

        val insets = variant.uiInsets
        val safeWidth = variant.uiSize.x - insets.left - insets.right
        val safeHeight = variant.uiSize.y - insets.top - insets.bottom
        val scale = min(safeWidth / TEMPLATE_WIDTH, safeHeight / TEMPLATE_HEIGHT)
        val offsetX = insets.left + (safeWidth - TEMPLATE_WIDTH * scale) / 2f
        val offsetY = insets.top.toFloat()

        fun place(rect: Rect, component: LayoutComponent): PositionedLayoutComponent {
            val left = (offsetX + rect.x * scale).roundToInt()
            val top = (offsetY + rect.y * scale).roundToInt()
            val right = (offsetX + rect.right * scale).roundToInt()
            val bottom = (offsetY + rect.bottom * scale).roundToInt()
            return PositionedLayoutComponent(Rect(left, top, right - left, bottom - top), component)
        }

        val utilityRowWidth = UTILITY_COMPONENTS.size * UTILITY_SIZE + (UTILITY_COMPONENTS.size - 1) * UTILITY_SPACING
        val utilityLeft = (TEMPLATE_WIDTH.toInt() - utilityRowWidth) / 2
        val utilityButtons = UTILITY_COMPONENTS.mapIndexed { index, component ->
            place(Rect(utilityLeft + index * (UTILITY_SIZE + UTILITY_SPACING), UTILITY_TOP, UTILITY_SIZE, UTILITY_SIZE), component)
        }

        val mainScreenLayout = ScreenLayout(
            listOf(
                place(TOP_SCREEN, LayoutComponent.TOP_SCREEN),
                place(BOTTOM_SCREEN, LayoutComponent.BOTTOM_SCREEN),
                place(DPAD, LayoutComponent.DPAD),
                place(BUTTONS, LayoutComponent.BUTTONS),
                place(BUTTON_L, LayoutComponent.BUTTON_L),
                place(BUTTON_R, LayoutComponent.BUTTON_R),
                place(BUTTON_START, LayoutComponent.BUTTON_START),
                place(BUTTON_SELECT, LayoutComponent.BUTTON_SELECT),
                *utilityButtons.toTypedArray(),
            )
        )
        return UILayout(mainScreenLayout, ScreenLayout())
    }
}
