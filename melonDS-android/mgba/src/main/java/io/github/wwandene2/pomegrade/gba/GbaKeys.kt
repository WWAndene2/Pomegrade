package io.github.wwandene2.pomegrade.gba

import android.view.KeyEvent

/** The GBA's buttons as the core's key bits, and the controller keys that press them. */
internal object GbaKeys {

    const val A = 1 shl 0
    const val B = 1 shl 1
    const val SELECT = 1 shl 2
    const val START = 1 shl 3
    const val RIGHT = 1 shl 4
    const val LEFT = 1 shl 5
    const val UP = 1 shl 6
    const val DOWN = 1 shl 7
    const val R = 1 shl 8
    const val L = 1 shl 9

    /** The key bit a controller or keyboard key presses, or 0 if none. */
    fun fromKeyCode(keyCode: Int): Int = when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_X -> A
        KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_Z -> B
        KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_DEL -> SELECT
        KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> START
        KeyEvent.KEYCODE_DPAD_RIGHT -> RIGHT
        KeyEvent.KEYCODE_DPAD_LEFT -> LEFT
        KeyEvent.KEYCODE_DPAD_UP -> UP
        KeyEvent.KEYCODE_DPAD_DOWN -> DOWN
        KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_S -> R
        KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_A -> L
        else -> 0
    }
}
