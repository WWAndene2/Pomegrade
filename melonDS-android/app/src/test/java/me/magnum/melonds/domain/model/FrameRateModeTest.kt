package me.magnum.melonds.domain.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class FrameRateModeTest {

    @Test
    fun everySettingValueGivesItsMode() {
        assertEquals(FrameRateMode.FPS_30, FrameRateMode.fromPreferenceValue("30"))
        assertEquals(FrameRateMode.FPS_60, FrameRateMode.fromPreferenceValue("60"))
        assertEquals(FrameRateMode.FPS_120, FrameRateMode.fromPreferenceValue("120"))
        assertEquals(FrameRateMode.FPS_240, FrameRateMode.fromPreferenceValue("240"))
        assertEquals(FrameRateMode.ADAPTIVE, FrameRateMode.fromPreferenceValue("adaptive"))
    }

    @Test
    fun unknownOrMissingValueGivesNoMode() {
        assertNull(FrameRateMode.fromPreferenceValue(null))
        assertNull(FrameRateMode.fromPreferenceValue("90"))
    }

    @Test
    fun nativeValuesAreImagesPerSecondAndZeroForAdaptive() {
        // FrameRatePacer::SetMode takes these (FrameRatePacer::Adaptive = 0)
        assertEquals(listOf(30, 60, 120, 240, 0), FrameRateMode.entries.map { it.nativeValue })
    }

    @Test
    fun defaultIsSixty() {
        assertEquals(FrameRateMode.FPS_60, FrameRateMode.DEFAULT)
    }
}
