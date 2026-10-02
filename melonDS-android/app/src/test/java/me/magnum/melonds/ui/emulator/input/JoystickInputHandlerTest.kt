package me.magnum.melonds.ui.emulator.input

import me.magnum.melonds.domain.model.Input
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.math.cos
import kotlin.math.sin

class JoystickInputHandlerTest {

    private fun at(degrees: Double, tilt: Double = 1.0): Set<Input> {
        val radians = Math.toRadians(degrees)
        return JoystickInputHandler.directionInputs((cos(radians) * tilt).toFloat(), (sin(radians) * tilt).toFloat())
    }

    @Test
    fun centreAndDeadZoneHoldNoDirection() {
        assertEquals(emptySet<Input>(), JoystickInputHandler.directionInputs(0f, 0f))
        assertEquals(emptySet<Input>(), at(30.0, 0.29))
    }

    @Test
    fun eachDirectionCoversFortyFiveDegrees() {
        // 0 = right, counter-clockwise
        val expected = listOf(
            setOf(Input.RIGHT), setOf(Input.UP, Input.RIGHT), setOf(Input.UP), setOf(Input.UP, Input.LEFT),
            setOf(Input.LEFT), setOf(Input.DOWN, Input.LEFT), setOf(Input.DOWN), setOf(Input.DOWN, Input.RIGHT),
        )
        expected.forEachIndexed { i, inputs ->
            val centre = i * 45.0
            assertEquals("at $centre", inputs, at(centre))
            assertEquals("at ${centre - 22}", inputs, at(centre - 22))
            assertEquals("at ${centre + 22}", inputs, at(centre + 22))
        }
    }

    @Test
    fun aSmallTiltPastTheDeadZoneHoldsTheDirection() {
        assertEquals(setOf(Input.UP), at(90.0, 0.31))
    }
}
