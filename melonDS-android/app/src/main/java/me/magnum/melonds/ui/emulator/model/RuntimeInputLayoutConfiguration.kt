package me.magnum.melonds.ui.emulator.model

import me.magnum.melonds.domain.model.input.ClassicButtonStyle
import me.magnum.melonds.domain.model.input.SoftInputBehaviour
import me.magnum.melonds.domain.model.layout.LayoutConfiguration
import me.magnum.melonds.domain.model.layout.UILayout

data class RuntimeInputLayoutConfiguration(
    val softInputBehaviour: SoftInputBehaviour,
    val softInputOpacity: Int,
    val isHapticFeedbackEnabled: Boolean,
    // Pomegrade: the D-pad is shown and used as a joystick
    val isAnalogueMovementEnabled: Boolean,
    // Pomegrade: the Classic layout's own button images, null for every other layout (upstream's images)
    val classicButtonStyle: ClassicButtonStyle?,
    val layoutOrientation: LayoutConfiguration.LayoutOrientation,
    val layout: UILayout,
)