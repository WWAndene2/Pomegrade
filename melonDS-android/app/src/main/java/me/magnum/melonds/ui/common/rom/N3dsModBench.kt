package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Toast
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.PomegradeFolder
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.activities.EmulationActivity
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Test bench for the Pokemon remake mods (tools/remake/ORAS_LITTLEROOT.md section 10, item 5): runs the game once per mod
 * found in Pomegrade/3DS/bench/<name>/ (each a mod folder: romfs_ext/, exefs/, as load/mods/<title>/ holds them) and writes
 * whether the field came up to Pomegrade/3DS/bench/results.txt. Temporary diagnostic, approved by the owner, to find the code
 * that bounds a map piece to about 1 MiB; it runs only when the bench folder exists (off by default) and is to be removed
 * once that is found.
 *
 * A run: the mod is copied over load/mods/<title>/ (the game's mod folder, emptied first), the game started, A pressed every
 * [PRESS_EVERY_MS] (title screen, "Continue"), and Azahar's log read: the field is up when the module DllFieldEventPlayer loads
 * (seen on the phone: it loads a few seconds after DllField when the map shows, and never when the field hangs). After
 * [TIMEOUT_MS] without it, the run fails. The log is read from where it ended before the run, or from its start when it is
 * shorter (Azahar starts a new log when the game starts: the first bench run read nothing and reported p1 as no field). Azahar
 * flushes it on each error-level line, which the field writes every few seconds. A bench started again (the app restarted)
 * skips the mods results.txt already has a result for: delete results.txt to run them all again.
 */
object N3dsModBench {
    private const val TAG = "N3dsModBench"
    private const val TITLE = "000400000011C400" // Omega Ruby
    private const val SUCCESS = "CRO \"DllFieldEventPlayer\" loaded"
    private const val TIMEOUT_MS = 90_000L
    private const val PRESS_EVERY_MS = 1_500L
    private const val POLL_MS = 500L

    private val handler = Handler(Looper.getMainLooper())
    private var emulation: Activity? = null
    private var running = false

    private class Run(val launcher: Activity, val rom: Rom, val n3ds: File, val mods: List<File>) {
        var index = 0
        var started = 0L
        var logFrom = 0L
        var lastPress = 0L
        var presses = 0
        var consumed = 0
    }

    private fun benchFolder(activity: Activity): File? =
        PomegradeFolder.subFolder(activity, PomegradeFolder.N3DS)?.let { File(it, "bench") }?.takeIf { it.isDirectory }

    /** True when a bench folder exists: the launch then runs the bench instead of the game. */
    fun isRequested(activity: Activity): Boolean = !running && benchFolder(activity)?.listFiles { f -> f.isDirectory }?.isNotEmpty() == true

    fun start(activity: Activity, rom: Rom) {
        val bench = benchFolder(activity) ?: return
        val n3ds = bench.parentFile ?: return
        val done = File(bench, "results.txt").takeIf { it.exists() }?.readLines().orEmpty()
            .mapNotNull { line -> line.substringAfter(' ', "").substringBefore(": ", "").takeIf { it.isNotEmpty() && ": " in line } }.toSet()
        val mods = bench.listFiles { f -> f.isDirectory }?.sortedBy { it.name }.orEmpty().filter { it.name !in done }
        if (mods.isEmpty()) {
            Toast.makeText(activity, "Test bench: every mod has a result (delete 3DS/bench/results.txt to run them again)", Toast.LENGTH_LONG).show()
            return
        }
        running = true
        watchEmulation(activity.application)
        result(bench, "bench started, ${mods.size} mods: ${mods.joinToString { it.name }}")
        Toast.makeText(activity, "Test bench: ${mods.size} mods, results in 3DS/bench/results.txt", Toast.LENGTH_LONG).show()
        next(Run(activity, rom, n3ds, mods))
    }

    private fun next(run: Run) {
        val bench = File(run.n3ds, "bench")
        if (run.index >= run.mods.size) {
            result(bench, "bench finished")
            File(run.n3ds, "load/mods/$TITLE").deleteRecursively()
            running = false
            return
        }
        val mod = run.mods[run.index]
        val target = File(run.n3ds, "load/mods/$TITLE")
        target.deleteRecursively()
        if (!mod.copyRecursively(target, overwrite = true)) {
            result(bench, "${mod.name}: NOT RUN (copy failed)")
            run.index++
            handler.post { next(run) }
            return
        }
        run.logFrom = log(run).let { if (it.exists()) it.length() else 0L }
        run.started = System.currentTimeMillis()
        run.lastPress = run.started
        run.presses = 0
        run.consumed = 0
        Toast.makeText(run.launcher, "Test bench: ${run.index + 1}/${run.mods.size} ${mod.name}", Toast.LENGTH_SHORT).show()
        N3dsLauncher.launch(run.launcher, run.rom)
        handler.postDelayed({ watch(run) }, POLL_MS)
    }

    private fun watch(run: Run) {
        val mod = run.mods[run.index]
        val now = System.currentTimeMillis()
        val elapsed = now - run.started
        val text = newLog(run)
        val fieldUp = text.contains(SUCCESS)
        if (fieldUp || elapsed > TIMEOUT_MS) {
            // where the game got to: the last code module it loaded (DllTitle: title screen, DllStartMenu: the Continue menu,
            // DllField: the field loading), the log's size and whether the presses reached the game
            val lastModule = Regex("CRO \"([^\"]+)\" loaded").findAll(text).lastOrNull()?.groupValues?.get(1) ?: "none"
            result(File(run.n3ds, "bench"), "${mod.name}: ${if (fieldUp) "FIELD" else "NO FIELD"} after ${elapsed / 1000} s " +
                "(last module $lastModule, log ${text.length} new bytes of ${log(run).length()}, A pressed ${run.presses}x, taken ${run.consumed}x)")
            emulation?.finish()
            run.index++
            waitStopped(run)
            return
        }
        if (now - run.lastPress >= PRESS_EVERY_MS && NativeLibrary.isRunning()) {
            run.lastPress = now
            run.presses++
            if (NativeLibrary.onGamePadEvent(NativeLibrary.TOUCHSCREEN_DEVICE, NativeLibrary.ButtonType.BUTTON_A, NativeLibrary.ButtonState.PRESSED)) run.consumed++
            handler.postDelayed({
                NativeLibrary.onGamePadEvent(NativeLibrary.TOUCHSCREEN_DEVICE, NativeLibrary.ButtonType.BUTTON_A, NativeLibrary.ButtonState.RELEASED)
            }, 150)
        }
        handler.postDelayed({ watch(run) }, POLL_MS)
    }

    // the next run starts once the emulation of this one has closed (its activity gone, the core stopped)
    private fun waitStopped(run: Run, waited: Long = 0) {
        if ((emulation == null && !NativeLibrary.isRunning()) || waited > 20_000) {
            handler.postDelayed({ next(run) }, 2_000)
            return
        }
        handler.postDelayed({ waitStopped(run, waited + POLL_MS) }, POLL_MS)
    }

    private fun log(run: Run) = File(run.n3ds, "log/azahar_log.txt")

    // what Azahar wrote to its log since this run started (the log is per app start, so it holds the earlier runs too)
    private fun newLog(run: Run): String {
        val file = log(run)
        if (!file.exists()) return ""
        val from = if (file.length() < run.logFrom) 0L else run.logFrom // a new log, started with this run's game
        return file.inputStream().use { input ->
            input.skip(from)
            input.readBytes().toString(Charsets.UTF_8)
        }
    }

    private fun result(bench: File, line: String) {
        val stamp = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
        Log.i(TAG, line)
        File(bench, "results.txt").appendText("$stamp $line\n")
    }

    private var watching = false

    // the emulation activity, so a run can close it
    private fun watchEmulation(application: Application) {
        if (watching) return
        watching = true
        application.registerActivityLifecycleCallbacks(object : Application.ActivityLifecycleCallbacks {
            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
                if (activity is EmulationActivity) emulation = activity
            }
            override fun onActivityDestroyed(activity: Activity) {
                if (activity === emulation) emulation = null
            }
            override fun onActivityStarted(activity: Activity) {}
            override fun onActivityResumed(activity: Activity) {}
            override fun onActivityPaused(activity: Activity) {}
            override fun onActivityStopped(activity: Activity) {}
            override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}
        })
    }
}
