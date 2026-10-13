package me.magnum.melonds.impl

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.os.SystemClock
import io.github.wwandene2.pomegrade.gba.GbaEmulatorActivity
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.repositories.RomsRepository
import kotlin.time.Duration.Companion.milliseconds

/**
 * The play time of the games the other cores run (GBA, 3DS, Switch, GameCube / Wii), counted as Punica counts
 * the DS games' (EmulatorViewModel.startTrackingPlayTime): while the core's emulation screen is in the
 * foreground, into the game's entry (RomsRepository.addRomPlayTime), so the game details show one play time
 * for every console. The cores' own counters (Azahar's and Eden's play time managers, Dolphin's time
 * tracking) are left as they are: each counts its own games only, and reading them would need each core's
 * game ID. Registered by the Application; told the game by [gameLaunched] before its screen opens.
 */
class PlayTimeTracker(private val romsRepository: RomsRepository) : Application.ActivityLifecycleCallbacks {

    companion object {
        var instance: PlayTimeTracker? = null
            private set

        fun register(application: Application, romsRepository: RomsRepository) {
            instance = PlayTimeTracker(romsRepository).also { application.registerActivityLifecycleCallbacks(it) }
        }
    }

    private var rom: Rom? = null
    private var resumedAt = 0L

    /** [rom] is about to open in a core other than the DS one. */
    fun gameLaunched(rom: Rom) {
        this.rom = rom
        resumedAt = 0L
    }

    override fun onActivityResumed(activity: Activity) {
        if (isCoreScreen(activity) && rom != null) resumedAt = SystemClock.elapsedRealtime()
    }

    override fun onActivityPaused(activity: Activity) {
        if (isCoreScreen(activity)) addElapsed()
    }

    override fun onActivityDestroyed(activity: Activity) {
        if (isCoreScreen(activity) && activity.isFinishing) {
            addElapsed()
            rom = null
        }
    }

    private fun addElapsed() {
        val game = rom ?: return
        if (resumedAt == 0L) return
        val elapsed = SystemClock.elapsedRealtime() - resumedAt
        resumedAt = 0L
        if (elapsed > 0) romsRepository.addRomPlayTime(game, elapsed.milliseconds)
    }

    // the cores' emulation screens (the DS one counts its own)
    private fun isCoreScreen(activity: Activity) =
        activity is GbaEmulatorActivity ||
            activity is org.citra.citra_emu.activities.EmulationActivity ||
            activity is org.yuzu.yuzu_emu.activities.EmulationActivity ||
            activity is org.dolphinemu.dolphinemu.activities.EmulationActivity

    override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) = Unit
    override fun onActivityStarted(activity: Activity) = Unit
    override fun onActivityStopped(activity: Activity) = Unit
    override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) = Unit
}
