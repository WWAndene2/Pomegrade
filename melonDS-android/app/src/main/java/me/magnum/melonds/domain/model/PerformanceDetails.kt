package me.magnum.melonds.domain.model

/**
 * Where the time of a DS frame goes (Pomegrade), in milliseconds per frame averaged over the last second, as the DS
 * core measures it (PerformanceCounters.h). The GPU values are null when nothing timed the GPU (software renderer, or
 * no GL_EXT_disjoint_timer_query on the device).
 */
data class PerformanceDetails(
    val emulationCpuMs: Float,
    val polygonMultiplierCpuMs: Float,
    val texturesCpuMs: Float,
    val gpuSceneMs: Float?,
    val gpuCompositorMs: Float?,
    val gpuFrameGenerationMs: Float?,
) {
    val gpuTotalMs: Float? get() = listOf(gpuSceneMs, gpuCompositorMs, gpuFrameGenerationMs)
        .takeIf { values -> values.all { it != null } }
        ?.sumOf { it!!.toDouble() }
        ?.toFloat()

    companion object {
        private const val SECTION_COUNT = 6

        /**
         * From the native counters: frames measured, then one value per section in the order of
         * PerformanceCounters::Section. Null when nothing has been measured yet.
         */
        fun fromCounters(counters: FloatArray): PerformanceDetails? {
            if (counters.size < 1 + SECTION_COUNT || counters[0] <= 0f) {
                return null
            }
            fun gpu(index: Int) = counters[1 + index].takeIf { it >= 0f }
            return PerformanceDetails(
                emulationCpuMs = counters[1],
                polygonMultiplierCpuMs = counters[2],
                texturesCpuMs = counters[3],
                gpuSceneMs = gpu(3),
                gpuCompositorMs = gpu(4),
                gpuFrameGenerationMs = gpu(5),
            )
        }
    }
}
