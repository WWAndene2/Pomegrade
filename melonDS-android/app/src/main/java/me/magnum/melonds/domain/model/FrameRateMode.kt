package me.magnum.melonds.domain.model

/**
 * Frame rate modes (Pomegrade): images shown per second for a DS game (which runs at 60). See FrameRatePacer.h in the
 * native code for what each does.
 *
 * @property nativeValue the value the native code takes (images per second, 0 = adaptive)
 * @property preferenceValue the value stored in the settings
 */
enum class FrameRateMode(val nativeValue: Int, val preferenceValue: String) {
    FPS_30(30, "30"),
    FPS_60(60, "60"),
    FPS_120(120, "120"),
    FPS_240(240, "240"),
    ADAPTIVE(0, "adaptive");

    companion object {
        val DEFAULT = FPS_60

        fun fromPreferenceValue(value: String?): FrameRateMode? = entries.firstOrNull { it.preferenceValue == value }
    }
}
