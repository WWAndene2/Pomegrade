package me.magnum.melonds.domain.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PerformanceDetailsTest {

    @Test
    fun nothingMeasuredYetGivesNoDetails() {
        assertNull(PerformanceDetails.fromCounters(FloatArray(9)))
    }

    @Test
    fun truncatedCountersGiveNoDetails() {
        assertNull(PerformanceDetails.fromCounters(floatArrayOf(60f, 1f, 2f)))
    }

    @Test
    fun countersAreReadInSectionOrder() {
        val details = PerformanceDetails.fromCounters(floatArrayOf(60f, 10f, 2f, 0.5f, 3f, 1f, 4f, 0.25f, 6f))!!

        assertEquals(10f, details.emulationCpuMs)
        assertEquals(2f, details.polygonMultiplierCpuMs)
        assertEquals(0.5f, details.texturesCpuMs)
        assertEquals(3f, details.gpuSceneMs)
        assertEquals(1f, details.gpuShadowsMs)
        assertEquals(4f, details.gpuLightingMs)
        assertEquals(0.25f, details.gpuCompositorMs)
        assertEquals(6f, details.gpuFrameGenerationMs)
        assertEquals(14.25f, details.gpuTotalMs!!, 0.0001f)
    }

    @Test
    fun gpuThatCantBeTimedGivesNoGpuValues() {
        val details = PerformanceDetails.fromCounters(floatArrayOf(60f, 10f, 2f, 0.5f, -1f, -1f, -1f, -1f, -1f))!!

        assertEquals(10f, details.emulationCpuMs)
        assertNull(details.gpuSceneMs)
        assertNull(details.gpuFrameGenerationMs)
        assertNull(details.gpuTotalMs)
    }
}
