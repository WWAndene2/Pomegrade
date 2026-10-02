package me.magnum.melonds.ui.emulator.input

import android.view.MotionEvent
import android.view.View
import me.magnum.melonds.common.vibration.TouchVibrator
import me.magnum.melonds.domain.model.Input
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sqrt

/**
 * Pomegrade: the touch D-pad used as a joystick (Analogue movement setting). The finger's offset from the centre is the stick's
 * position, sent as is for games with analogue movement, and as the nearest of the 8 D-pad directions as keys for every game.
 */
class JoystickInputHandler(
    inputListener: IInputListener,
    private val drawable: JoystickDrawable,
    enableHapticFeedback: Boolean,
    touchVibrator: TouchVibrator,
) : FeedbackInputHandler(inputListener, enableHapticFeedback, touchVibrator) {

    private val pressedInputs = mutableSetOf<Input>()

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        var x = 0f
        var y = 0f
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                val radius = min(v.width, v.height) / 2f
                if (radius > 0f) {
                    x = (event.x - v.width / 2f) / radius
                    y = -(event.y - v.height / 2f) / radius
                    val length = sqrt(x * x + y * y)
                    if (length > 1f) {
                        x /= length
                        y /= length
                    }
                }
            }
        }

        inputListener.onAnalogueStick(x, y)
        drawable.setKnobPosition(x, y)

        val newInputs = directionInputs(x, y)
        val released = pressedInputs - newInputs
        val pressed = newInputs - pressedInputs
        released.forEach { inputListener.onKeyReleased(it) }
        pressed.forEach { inputListener.onKeyPress(it) }
        // once when the stick leaves the centre, not on every change of direction
        if (pressedInputs.isEmpty() && newInputs.isNotEmpty()) {
            performHapticFeedback(v, HapticFeedbackType.KEY_PRESS)
        }
        pressedInputs.clear()
        pressedInputs.addAll(newInputs)
        return true
    }

    companion object {
        /** Below this tilt no direction is held */
        const val DEAD_ZONE = 0.3f

        /**
         * The D-pad keys for a stick position (x to the right, y up): the nearest of the 8 directions, each one 45 degrees wide.
         */
        fun directionInputs(x: Float, y: Float): Set<Input> {
            if (sqrt(x * x + y * y) < DEAD_ZONE) {
                return emptySet()
            }
            // 0 = right, counter-clockwise, in eighths of a turn
            val sector = ((atan2(y, x) / (PI / 4)).roundToInt() + 8) % 8
            return when (sector) {
                0 -> setOf(Input.RIGHT)
                1 -> setOf(Input.UP, Input.RIGHT)
                2 -> setOf(Input.UP)
                3 -> setOf(Input.UP, Input.LEFT)
                4 -> setOf(Input.LEFT)
                5 -> setOf(Input.DOWN, Input.LEFT)
                6 -> setOf(Input.DOWN)
                else -> setOf(Input.DOWN, Input.RIGHT)
            }
        }
    }
}
