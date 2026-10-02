package me.magnum.melonds.ui.emulator.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import me.magnum.melonds.domain.model.ControllerConfiguration
import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.InputConfig
import kotlin.math.absoluteValue
import kotlin.math.sqrt

/**
 * @param analogueMovement Pomegrade: the left stick is a joystick (see [JoystickInputHandler]): its exact position for games with
 * analogue movement, the nearest of the 8 directions as keys, in place of its own key mapping
 */
class InputProcessor(
    private val controllerConfiguration: ControllerConfiguration,
    private val systemInputListener: IInputListener,
    private val frontendInputListener: IInputListener,
    private val analogueMovement: Boolean = false,
) : INativeInputListener {

    private var stickX = 0f
    private var stickY = 0f
    private val stickInputs = mutableSetOf<Input>()

    private val axisStates: Map<Axis, AxisState>

    init {
        val axis = controllerConfiguration.inputMapper.flatMap { inputConfig ->
            listOf(inputConfig.assignment, inputConfig.altAssignment)
        }.mapNotNull { assignment ->
            (assignment as? InputConfig.Assignment.Axis)?.let {
                Axis(it.deviceId, it.axisCode, it.direction)
            }
        }

        axisStates = axis.associateWith { AxisState(0f, false) }
    }

    override fun onKeyEvent(keyEvent: KeyEvent): Boolean {
        val input = controllerConfiguration.keyToInput(keyEvent.keyCode) ?: return false
        if (input.isSystemInput) {
            when (keyEvent.action) {
                KeyEvent.ACTION_DOWN -> {
                    systemInputListener.onKeyPress(input)
                    return true
                }
                KeyEvent.ACTION_UP -> {
                    systemInputListener.onKeyReleased(input)
                    return true
                }
            }
        } else {
            when (keyEvent.action) {
                KeyEvent.ACTION_DOWN -> {
                    frontendInputListener.onKeyPress(input)
                    return true
                }
                KeyEvent.ACTION_UP -> {
                    frontendInputListener.onKeyReleased(input)
                    return true
                }
            }
        }
        return false
    }

    override fun onMotionEvent(motionEvent: MotionEvent): Boolean {
        if (motionEvent.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) {
            if (analogueMovement) {
                updateAnalogueStick(motionEvent)
            }
            val deviceAxis = axisStates.filterKeys {
                (it.deviceId == null || it.deviceId == motionEvent.deviceId) && !(analogueMovement && it.axisCode in LEFT_STICK_AXES)
            }
            deviceAxis.forEach {
                val axis = it.key
                val axisState = it.value

                val newValue = motionEvent.getAxisValue(axis.axisCode)
                val clampedValue = when (axis.direction) {
                    InputConfig.Assignment.Axis.Direction.POSITIVE -> newValue.coerceAtLeast(0f)
                    InputConfig.Assignment.Axis.Direction.NEGATIVE -> newValue.coerceAtMost(0f)
                }

                if (axisState.shouldToggleFor(newValue = clampedValue)) {
                    controllerConfiguration.axisToInput(axis.axisCode, axis.direction)?.let { input ->
                        if (axisState.active) {
                            axisState.active = false
                            if (input.isSystemInput) {
                                systemInputListener.onKeyReleased(input)
                            } else {
                                frontendInputListener.onKeyReleased(input)
                            }
                        } else {
                            axisState.active = true
                            if (input.isSystemInput) {
                                systemInputListener.onKeyPress(input)
                            } else {
                                frontendInputListener.onKeyPress(input)
                            }
                        }
                    }
                }
                axisState.value = clampedValue
            }
            return deviceAxis.isNotEmpty() || analogueMovement
        } else {
            return false
        }
    }

    private fun updateAnalogueStick(motionEvent: MotionEvent) {
        var x = motionEvent.getAxisValue(MotionEvent.AXIS_X)
        var y = -motionEvent.getAxisValue(MotionEvent.AXIS_Y)
        val length = sqrt(x * x + y * y)
        if (length < JoystickInputHandler.DEAD_ZONE) {
            // a resting or drifting stick: the D-pad decides
            x = 0f
            y = 0f
        } else if (length > 1f) {
            x /= length
            y /= length
        }
        if (x != stickX || y != stickY) {
            stickX = x
            stickY = y
            systemInputListener.onAnalogueStick(x, y)
        }

        val newInputs = JoystickInputHandler.directionInputs(x, y)
        (stickInputs - newInputs).forEach { systemInputListener.onKeyReleased(it) }
        (newInputs - stickInputs).forEach { systemInputListener.onKeyPress(it) }
        stickInputs.clear()
        stickInputs.addAll(newInputs)
    }

    private companion object {
        val LEFT_STICK_AXES = setOf(MotionEvent.AXIS_X, MotionEvent.AXIS_Y)
    }

    private data class Axis(
        val deviceId: Int?,
        val axisCode: Int,
        val direction: InputConfig.Assignment.Axis.Direction,
    )

    private data class AxisState(
        var value: Float,
        var active: Boolean,
    ) {
        fun shouldToggleFor(newValue: Float): Boolean {
            return if (active) {
                newValue.absoluteValue < 0.5f
            } else {
                newValue.absoluteValue >= 0.5f
            }
        }
    }
}