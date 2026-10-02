package me.magnum.melonds.ui.emulator.input

import me.magnum.melonds.domain.model.Input
import me.magnum.melonds.domain.model.Point

interface IInputListener {
    fun onKeyPress(key: Input)
    fun onKeyReleased(key: Input)
    fun onTouch(point: Point)
    /** Pomegrade: an analogue stick's position, x to the right and y up, each -1 to 1 (see MelonEmulator.onAnalogueStick) */
    fun onAnalogueStick(x: Float, y: Float) {}
}