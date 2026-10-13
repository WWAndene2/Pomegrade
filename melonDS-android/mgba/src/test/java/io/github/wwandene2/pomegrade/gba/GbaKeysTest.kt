package io.github.wwandene2.pomegrade.gba

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Test

class GbaKeysTest {

    @Test
    fun controllerAndKeyboardKeysPressTheirButton() {
        assertEquals(GbaKeys.A, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_BUTTON_A))
        assertEquals(GbaKeys.B, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_Z))
        assertEquals(GbaKeys.START, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_BUTTON_START))
        assertEquals(GbaKeys.L, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_BUTTON_L1))
        assertEquals(GbaKeys.UP, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_DPAD_UP))
        assertEquals(0, GbaKeys.fromKeyCode(KeyEvent.KEYCODE_VOLUME_UP))
    }

    @Test
    fun hatAxesPressTheDPad() {
        assertEquals(GbaKeys.RIGHT, GbaKeys.fromAxes(1f, 0f, 0f, 0f))
        assertEquals(GbaKeys.UP or GbaKeys.LEFT, GbaKeys.fromAxes(-1f, -1f, 0f, 0f))
    }

    @Test
    fun theLeftStickPressesTheDPadPastItsDeadZone() {
        assertEquals(GbaKeys.DOWN, GbaKeys.fromAxes(0f, 0f, 0.2f, 0.9f))
        assertEquals(0, GbaKeys.fromAxes(0f, 0f, 0.4f, -0.4f))
    }

    @Test
    fun centredAxesPressNothing() {
        assertEquals(0, GbaKeys.fromAxes(0f, 0f, 0f, 0f))
    }
}
