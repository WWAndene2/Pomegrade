package me.magnum.melonds.ui.emulator.input

import android.view.HapticFeedbackConstants
import android.view.View
import io.github.wwandene2.pomegrade.emulatorui.ClassicHaptics
import me.magnum.melonds.common.vibration.TouchVibrator

abstract class FeedbackInputHandler(inputListener: IInputListener, private val enableHapticFeedback: Boolean, private val touchVibrator: TouchVibrator) : BaseInputHandler(inputListener) {

    enum class HapticFeedbackType {
        KEY_PRESS,
        KEY_RELEASE
    }

    /** Pomegrade: the Classic layout's feedback, a hard short click on press and nothing on release */
    var clickHaptic = false

    protected fun performHapticFeedback(view: View, type: HapticFeedbackType) {
        if (enableHapticFeedback && clickHaptic) {
            if (type == HapticFeedbackType.KEY_PRESS) ClassicHaptics.click(view.context)
            return
        }
        if (enableHapticFeedback) {
            val feedbackType = when (type) {
                HapticFeedbackType.KEY_PRESS -> HapticFeedbackConstants.LONG_PRESS
                HapticFeedbackType.KEY_RELEASE -> HapticFeedbackConstants.CLOCK_TICK
            }
            view.performHapticFeedback(feedbackType)
        }
    }
}