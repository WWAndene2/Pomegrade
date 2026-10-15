// SPDX-FileCopyrightText: Copyright Pomegrade Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import java.io.File

/**
 * Pomegrade: TEMPORARY diagnostic (no ADB, owner's decision): when the video core fails to start, what a custom
 * Vulkan driver (Mesa PanVK) wrote to the system log and what the phone gives it to show images, copied into Eden's
 * log, which the owner shares. The PanVK swapchain needs the phone maker's gralloc mapper library: whether it found
 * one is only in the system log (tag MESA). An app may read its own process's lines. Removed once read.
 */
object PomegradeDriverDiagnostic {

    private const val MAX_LINES = 400
    private val INTERESTING = Regex("MESA|panvk|PanVK|vulkan|Vulkan|adrenotools|gralloc|mapper|linker|AHardwareBuffer", RegexOption.IGNORE_CASE)
    private val PROPERTIES = listOf(
        "ro.build.version.sdk", "ro.product.first_api_level", "ro.board.first_api_level", "ro.vendor.api_level",
        "ro.hardware", "ro.hardware.gralloc", "ro.hardware.vulkan", "ro.board.platform",
    )

    fun log() {
        runCatching {
            Log.error("[Pomegrade diag] video core failed: driver and display details follow")
            PROPERTIES.forEach { Log.error("[Pomegrade diag] prop $it=${getprop(it)}") }
            listOf("/vendor/lib64/hw", "/odm/lib64/hw", "/vendor/lib64", "/system/lib64").forEach { dir ->
                val names = File(dir).list()?.filter { it.contains("mapper") || it.contains("gralloc") }?.sorted()
                Log.error("[Pomegrade diag] $dir: ${names?.joinToString(", ") ?: "not listable"}")
            }
            val process = ProcessBuilder("logcat", "-d", "-v", "threadtime", "--pid=${android.os.Process.myPid()}")
                .redirectErrorStream(true).start()
            val lines = process.inputStream.bufferedReader().readLines().filter { INTERESTING.containsMatchIn(it) }
            process.waitFor()
            Log.error("[Pomegrade diag] system log: ${lines.size} lines about the driver (last $MAX_LINES)")
            lines.takeLast(MAX_LINES).forEach { Log.error("[Pomegrade diag] logcat $it") }
        }.onFailure { Log.error("[Pomegrade diag] driver diagnostic failed: $it") }
    }

    private fun getprop(name: String): String = runCatching {
        ProcessBuilder("getprop", name).start().inputStream.bufferedReader().readText().trim()
    }.getOrDefault("?")
}
