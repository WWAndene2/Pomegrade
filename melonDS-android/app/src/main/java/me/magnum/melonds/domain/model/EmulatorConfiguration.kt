package me.magnum.melonds.domain.model

import android.net.Uri

data class EmulatorConfiguration(
        val useCustomBios: Boolean,
        val showBootScreen: Boolean,
        val dsBios7Uri: Uri?,
        val dsBios9Uri: Uri?,
        val dsFirmwareUri: Uri?,
        val dsiBios7Uri: Uri?,
        val dsiBios9Uri: Uri?,
        val dsiFirmwareUri: Uri?,
        val dsiNandUri: Uri?,
        val internalDirectory: String,
        val fastForwardSpeedMultiplier: Float,
        val rewindEnabled: Boolean,
        val rewindPeriodSeconds: Int,
        val rewindWindowSeconds: Int,
        val useJit: Boolean,
        val consoleType: ConsoleType,
        val soundEnabled: Boolean,
        val audioInterpolation: AudioInterpolation,
        val audioBitrate: AudioBitrate,
        val volume: Int,
        val audioLatency: AudioLatency,
        val micSource: MicSource,
        val firmwareConfiguration: FirmwareConfiguration,
        val rendererConfiguration: RendererConfiguration,
        // Pomegrade: HD texture replacement (compute renderer only)
        val texturesDirectory: String? = null,
        val hdTexturesEnabled: Boolean = false,
        val dumpTexturesEnabled: Boolean = false,
        // Pomegrade: polygon multiplier level, 1 = off (software and OpenGL renderers)
        val polygonMultiplier: Int = 1,
        // Pomegrade: OpenGL renderer splits polygons around a centre vertex (fewer seams at high resolution)
        val betterPolygons: Boolean = false,
        // Pomegrade: draw polygons past the DS limit (software and OpenGL renderers)
        val unlimitedPolygons: Boolean = false,
        // Pomegrade: sub-pixel vertex positions (OpenGL renderer)
        val highPrecisionGeometry: Boolean = false,
        // Pomegrade: 8-bit colour (OpenGL renderer)
        val highColor: Boolean = false,
        // Pomegrade: native texture upscaling factor, 1 = off (OpenGL renderer)
        val textureUpscale: Int = 1,
        // Pomegrade: scene-adaptive colour (OpenGL renderer)
        val oledBlacks: Boolean = false,
        val adaptiveColours: Boolean = false,
)