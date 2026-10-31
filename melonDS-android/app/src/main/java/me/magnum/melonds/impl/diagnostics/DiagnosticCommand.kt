package me.magnum.melonds.impl.diagnostics

/**
 * The only commands Punica runs over wireless debugging (owner's decision, 29 October: diagnostics only). Each is a
 * fixed string; the only values put into one are Punica's package name and the output folder, both from the app
 * itself, never typed by the user. Their output goes to Punica/Switch/Core/log, beside Eden's log.
 */
enum class DiagnosticCommand {
    /**
     * A Perfetto system trace of [TRACE_SECONDS] seconds after a delay (the time to start a game and reach the
     * part to measure): every thread's scheduling and the cores' clocks, the GPU's clock, and Punica's own trace
     * sections (Eden's GPU waits, submissions, transform feedback runs, frame ends). It runs detached from the
     * connection, so it carries on while Punica is in a game, then copies the trace into the output folder.
     */
    SYSTEM_TRACE,

    /** The GPU's driver, its Vulkan version and its memory (dumpsys gpu), and Punica's frame statistics. */
    GPU_STATE,

    /** The GPU driver's own messages so far (the Mali driver's, Android's graphics loader's). */
    DRIVER_LOG;

    /** The shell command, for Punica's [packageName] and an [outputFolder] without quotes. */
    fun shellCommand(packageName: String, outputFolder: String, delaySeconds: Int, stamp: String): String {
        require(SHELL_SPECIALS.none { it in outputFolder || it in packageName }) { "shell character in a diagnostic argument" }
        return when (this) {
            SYSTEM_TRACE -> {
                val trace = "/data/misc/perfetto-traces/punica-$stamp.perfetto-trace"
                val config = perfettoConfig(packageName)
                // setsid: the trace outlives the connection's shell; the config is fed on its standard input
                "setsid sh -c 'sleep $delaySeconds; echo \"$config\" | perfetto --txt -c - -o $trace; " +
                    "cp $trace \"$outputFolder/punica-$stamp.perfetto-trace\"; rm $trace' " +
                    "> /dev/null 2>&1 < /dev/null &"
            }
            GPU_STATE -> "dumpsys gpu; dumpsys gfxinfo $packageName"
            DRIVER_LOG -> "logcat -d -v time -s mali:V Mali:V libmali:V GraphicsEnv:V vulkan:V libEGL:V AdrenoVK:V"
        }
    }

    /** The file the output of a command run in the foreground is written to (the trace names its own). */
    fun outputName(stamp: String): String? = when (this) {
        SYSTEM_TRACE -> null
        GPU_STATE -> "punica-$stamp-gpu.txt"
        DRIVER_LOG -> "punica-$stamp-driver-log.txt"
    }

    companion object {
        const val TRACE_SECONDS = 20

        // characters the shell would read in a quoted argument
        private val SHELL_SPECIALS = listOf('\'', '"', '$', '`', '\\')

        // Perfetto's text config on one line, its quotes escaped for the shell's double quotes
        private fun perfettoConfig(packageName: String): String = """
            buffers { size_kb: 131072 fill_policy: RING_BUFFER }
            buffers { size_kb: 4096 fill_policy: RING_BUFFER }
            data_sources { config { name: "linux.ftrace" target_buffer: 0 ftrace_config {
                ftrace_events: "sched/sched_switch" ftrace_events: "sched/sched_wakeup"
                ftrace_events: "sched/sched_waking" ftrace_events: "sched/sched_process_exit"
                ftrace_events: "power/cpu_frequency" ftrace_events: "power/cpu_idle"
                ftrace_events: "power/gpu_frequency" ftrace_events: "power/suspend_resume"
                ftrace_events: "task/task_newtask" ftrace_events: "task/task_rename"
                atrace_categories: "gfx" atrace_categories: "view" atrace_categories: "sched"
                atrace_categories: "freq" atrace_categories: "binder_driver"
                atrace_apps: "$packageName"
                buffer_size_kb: 8192 drain_period_ms: 1000 } } }
            data_sources { config { name: "linux.process_stats" target_buffer: 1
                process_stats_config { scan_all_processes_on_start: true proc_stats_poll_ms: 1000 } } }
            data_sources { config { name: "linux.sys_stats" target_buffer: 1 sys_stats_config {
                meminfo_period_ms: 1000 vmstat_period_ms: 1000 cpufreq_period_ms: 250 } } }
            duration_ms: ${TRACE_SECONDS * 1000}
        """.trimIndent().replace('\n', ' ').replace("\"", "\\\"")
    }
}
