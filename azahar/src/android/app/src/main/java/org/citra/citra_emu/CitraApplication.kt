// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu

import android.annotation.SuppressLint
import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import org.citra.citra_emu.utils.DirectoryInitialization
import org.citra.citra_emu.utils.DocumentsTree
import org.citra.citra_emu.utils.GraphicsUtil
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.utils.MemoryUtil
import org.citra.citra_emu.utils.PermissionsHandler

// Pomegrade: Azahar is a module of a bigger app, which owns the Application class.
// attach() is called from the app's Application.onCreate(); start() does the rest of
// what used to be onCreate() and is called when a 3DS screen opens, so that DS-only use
// doesn't load the 3DS core.
class CitraApplication private constructor(private val app: Application) {
    private fun createNotificationChannel() {
        with(app.getSystemService(NotificationManager::class.java)) {
            // General notification
            val name: CharSequence = app.getString(R.string.app_notification_channel_name)
            val description = app.getString(R.string.app_notification_channel_description)
            val generalChannel = NotificationChannel(
                app.getString(R.string.app_notification_channel_id),
                name,
                NotificationManager.IMPORTANCE_LOW
            )
            generalChannel.description = description
            generalChannel.setSound(null, null)
            generalChannel.vibrationPattern = null
            createNotificationChannel(generalChannel)

            // CIA Install notifications
            val ciaChannel = NotificationChannel(
                app.getString(R.string.cia_install_notification_channel_id),
                app.getString(R.string.cia_install_notification_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT
            )
            ciaChannel.description =
                app.getString(R.string.cia_install_notification_channel_description)
            ciaChannel.setSound(null, null)
            ciaChannel.vibrationPattern = null
            createNotificationChannel(ciaChannel)
        }
    }

    private fun onStart() {
        documentsTree = DocumentsTree()
        if (PermissionsHandler.hasWriteAccess(app.applicationContext)) {
            DirectoryInitialization.start()
        }

        NativeLibrary.logDeviceInfo()
        logDeviceInfo()
        createNotificationChannel()
        NativeLibrary.playTimeManagerInit()
    }

    fun logDeviceInfo() {
        Log.info("Device Manufacturer - ${Build.MANUFACTURER}")
        Log.info("Device Model - ${Build.MODEL}")
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) {
            Log.info("SoC Manufacturer - ${Build.SOC_MANUFACTURER}")
            Log.info("SoC Model - ${Build.SOC_MODEL}")
        }
        Log.info("Total System Memory - ${MemoryUtil.getDeviceRAM()}")
        Log.info("OpenGL ES Renderer - ${GraphicsUtil.openGLRendererString}")
    }

    companion object {
        private var application: CitraApplication? = null
        private var started = false

        val appContext: Context get() = application!!.app.applicationContext

        @SuppressLint("StaticFieldLeak")
        lateinit var documentsTree: DocumentsTree

        // The 3DS core is only built for 64-bit ABIs
        val isSupported: Boolean get() = Build.SUPPORTED_64_BIT_ABIS.isNotEmpty()

        fun attach(app: Application) {
            if (application == null) {
                application = CitraApplication(app)
            }
        }

        @Synchronized
        fun start() {
            if (!started) {
                started = true
                application!!.onStart()
            }
        }
    }
}
