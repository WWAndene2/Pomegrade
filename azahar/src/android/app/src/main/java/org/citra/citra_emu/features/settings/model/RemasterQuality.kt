// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.model

/**
 * Pomegrade: the Remaster's one quality slider, 0-100 %. It sets every option that costs speed or decides how noisy
 * the light is (the fast tracer's resolution, ray steps, shadow map, denoise, history, sun size; the effects' switches;
 * Ultra), and leaves the look's own options (exposure, contrast, colour, AO and bounce strength, reach, thickness,
 * indirect light, shadow) as they are. 0 turns the Remaster off; 50 gives each option its default (the tracer's measured
 * look, gl_pathtracer.cpp ReadTuning); 100 is everything at its best, Ultra included. Any other value makes the preset
 * Custom, the one that reads these options (remaster.cpp).
 */
object RemasterQuality {
    /** An option's value at 0 % and at 100 % of the slider, linear between (50 % is the option's default). */
    private class Range(val setting: IntSetting, val low: Int, val high: Int) {
        fun at(quality: Int) = low + (high - low) * quality / 100
    }

    private val ranges = listOf(
        Range(IntSetting.REMASTER_RT_RESOLUTION, 0, 66),     // 25-100 % of the screen; 33 by default
        Range(IntSetting.REMASTER_RT_STEPS, 0, 86),          // 4-32 steps a ray; 43
        Range(IntSetting.REMASTER_RT_SHADOW_SIZE, 0, 86),    // 512-4096 texels; 43
        Range(IntSetting.REMASTER_RT_SHADOW_RANGE, 34, 100), // the share of the scene in the shadow map; 67
        Range(IntSetting.REMASTER_RT_SOFTNESS, 34, 100),     // the sun's size; 67
        Range(IntSetting.REMASTER_RT_DENOISE, 50, 100),      // 2-4 passes; 75 (the noise is worst low, so never below 2)
        Range(IntSetting.REMASTER_RT_HISTORY, 90, 100),      // frames blended over time against noise; 95
    )

    /** The effects' switches: each on from that quality up (the cheap colour ones always). */
    private val switches = listOf(
        BooleanSetting.REMASTER_AO to 10,       // the traced light itself
        BooleanSetting.REMASTER_SKY to 10,
        BooleanSetting.REMASTER_OUTLINE to 20,
        BooleanSetting.REMASTER_GLOW to 30,
        BooleanSetting.REMASTER_AERIAL to 30,
        BooleanSetting.REMASTER_FAR_BLUR to 40,
        BooleanSetting.REMASTER_GRADING to 1,
        BooleanSetting.REMASTER_VIBRANCE to 1,
        BooleanSetting.REMASTER_CONTRAST to 1,
    )

    /** Every option the slider sets to [quality] (0-100); returns them, for the settings file. */
    fun apply(quality: Int): List<AbstractSetting> {
        val q = quality.coerceIn(0, 100)
        IntSetting.REMASTER_PRESET.int = if (q == 0) 0 else 2
        if (q == 0) return listOf(IntSetting.REMASTER_PRESET)
        ranges.forEach { it.setting.int = it.at(q) }
        switches.forEach { (setting, from) -> setting.boolean = q >= from }
        BooleanSetting.REMASTER_ULTRA.boolean = q == 100
        return listOf<AbstractSetting>(IntSetting.REMASTER_PRESET, BooleanSetting.REMASTER_ULTRA) +
            ranges.map { it.setting } + switches.map { it.first }
    }

    /** The slider's place for the options as they are: off is 0, else the trace resolution's place in its range. */
    fun current(): Int {
        if (IntSetting.REMASTER_PRESET.int == 0) return 0
        if (BooleanSetting.REMASTER_ULTRA.boolean) return 100
        val r = ranges.first()
        return ((r.setting.int - r.low) * 100 / (r.high - r.low)).coerceIn(1, 99)
    }
}
