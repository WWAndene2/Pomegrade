package me.magnum.melonds.ui.emulator.input

import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.Point
import me.magnum.melonds.domain.model.layout.LayoutComponent

/**
 * Pomegrade: passes the Classic layout's inputs on to [listener] and sinks the drawn button each one presses
 * (ClassicButtonDrawable.setPressed) while it is held.
 */
class ClassicPressListener(
    private val listener: IInputListener,
    private val drawables: Map<LayoutComponent, ClassicButtonDrawable>,
) : IInputListener {

    override fun onKeyPress(key: Input) {
        listener.onKeyPress(key)
        drawableOf(key)?.setPressed(key, true)
    }

    override fun onKeyReleased(key: Input) {
        listener.onKeyReleased(key)
        drawableOf(key)?.setPressed(key, false)
    }

    override fun onTouch(point: Point) = listener.onTouch(point)

    override fun onAnalogueStick(x: Float, y: Float) = listener.onAnalogueStick(x, y)

    private fun drawableOf(key: Input): ClassicButtonDrawable? {
        val component = when (key) {
            Input.A, Input.B, Input.X, Input.Y -> LayoutComponent.BUTTONS
            Input.UP, Input.DOWN, Input.LEFT, Input.RIGHT -> LayoutComponent.DPAD
            Input.L -> LayoutComponent.BUTTON_L
            Input.R -> LayoutComponent.BUTTON_R
            Input.START -> LayoutComponent.BUTTON_START
            Input.SELECT -> LayoutComponent.BUTTON_SELECT
            Input.PUNICA_MENU -> LayoutComponent.BUTTON_PUNICA_MENU
            else -> return null
        }
        return drawables[component]
    }
}
