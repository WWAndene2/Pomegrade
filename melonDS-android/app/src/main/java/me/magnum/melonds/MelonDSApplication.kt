package me.magnum.melonds

import android.app.Application
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationManagerCompat
import androidx.hilt.work.HiltWorkerFactory
import androidx.work.Configuration
import dagger.hilt.android.HiltAndroidApp
import me.magnum.melonds.common.UriFileHandler
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.repositories.SettingsRepository
import me.magnum.melonds.impl.GcWiiFolder
import me.magnum.melonds.impl.SettingsBackupManager
import me.magnum.melonds.impl.SwitchFolder
import me.magnum.melonds.impl.PlayTimeTracker
import me.magnum.melonds.domain.repositories.RomsRepository
import me.magnum.melonds.migrations.Migrator
import javax.inject.Inject

@HiltAndroidApp
class MelonDSApplication : Application(), Configuration.Provider {
    companion object {
        const val NOTIFICATION_CHANNEL_ID_BACKGROUND_TASKS = "channel_cheat_importing"

        init {
            System.loadLibrary("melonDS-android-frontend")
        }
    }

    @Inject lateinit var workerFactory: HiltWorkerFactory
    @Inject lateinit var settingsRepository: SettingsRepository
    @Inject lateinit var migrator: Migrator
    @Inject lateinit var uriHandler: UriHandler
    @Inject lateinit var settingsBackupManager: SettingsBackupManager
    @Inject lateinit var romsRepository: RomsRepository

    override fun onCreate() {
        super.onCreate()
        // 3DS core: only stores the context, the core itself starts with its first screen
        org.citra.citra_emu.CitraApplication.attach(this)
        // Switch core: the same, its core starts with its first screen
        org.yuzu.yuzu_emu.YuzuApplication.attach(this)
        org.yuzu.yuzu_emu.utils.DirectoryInitialization.userDirectoryProvider = { SwitchFolder.userDirectory(it) }
        // the other cores' play time, counted as the DS core's
        PlayTimeTracker.register(this, romsRepository)
        // GameCube and Wii core: the same
        org.dolphinemu.dolphinemu.DolphinApplication.attach(this)
        org.dolphinemu.dolphinemu.utils.DirectoryInitialization.userDirectoryProvider = { GcWiiFolder.userDirectory(it) }
        createNotificationChannels()
        applyTheme()
        performMigrations()
        // Pomegrade: before the 3DS core reads its control positions (see the function)
        settingsBackupManager.repairN3dsOverlayPositions()
        MelonDSAndroidInterface.setup(UriFileHandler(this, uriHandler))
    }

    private fun createNotificationChannels() {
        val defaultChannel = NotificationChannelCompat.Builder(NOTIFICATION_CHANNEL_ID_BACKGROUND_TASKS, NotificationManagerCompat.IMPORTANCE_LOW)
            .setName(getString(R.string.notification_channel_background_tasks))
            .build()

        val notificationManager = NotificationManagerCompat.from(this)
        notificationManager.createNotificationChannel(defaultChannel)
    }

    // Punica is always dark (owner's decision, 13 October): the upstream theme setting is hidden and not applied
    private fun applyTheme() {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
    }

    private fun performMigrations() {
        migrator.performMigrations()
    }

    override fun onTerminate() {
        super.onTerminate()
        MelonDSAndroidInterface.cleanup()
    }

    override val workManagerConfiguration: Configuration get() {
        return Configuration.Builder()
            .setWorkerFactory(workerFactory)
            .build()
    }
}