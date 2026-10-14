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
 * common scale, then enlarged 1.15 times (owner's choice) keeping their proportions and the clusters' mirror. L and
 * R are as tall as an A / B / X / Y button (151 px) at the DS Lite's 2.17:1. The template is scaled to fit the
 * safe area, centred horizontally and anchored to its top.
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
        val BUTTON_L = Rect(56, 1830, 328, 151)
        val BUTTON_R = Rect(836, 1830, 328, 151)
        val DPAD = Rect(102, 2082, 375, 375)
        val BUTTONS = Rect(697, 2036, 467, 467)
        val BUTTON_START = Rect(702, 2557, 72, 72)
        val BUTTON_SELECT = Rect(446, 2557, 72, 72)

        // Not on the DS: a row of four between L and R, so every input stays reachable. 80 px with 16 between them,
        // so the row (368 px) fits the 452 px between the bumpers
        const val UTILITY_SIZE = 80
        const val UTILITY_SPACING = 16
        const val UTILITY_TOP = 1830 + (151 - UTILITY_SIZE) / 2
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
