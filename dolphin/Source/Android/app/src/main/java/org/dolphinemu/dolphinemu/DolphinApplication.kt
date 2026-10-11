// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu

import android.app.Activity
import android.app.Application
import android.app.Application.ActivityLifecycleCallbacks
import android.content.Context
import android.content.Context.USB_SERVICE
import android.hardware.usb.UsbManager
import android.os.Bundle
import org.dolphinemu.dolphinemu.utils.ActivityTracker
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.GCAdapter
import org.dolphinemu.dolphinemu.utils.WiimoteAdapter

/**
 * Pomegrade: Dolphin runs inside Pomegrade's app, whose Application attaches it ([attach]); what Dolphin's
 * own Application did at launch is done by [start], when the first Dolphin screen is created (or earlier,
 * by Pomegrade before opening one). Only Dolphin's own screens are tracked.
 */
class DolphinApplication private constructor(private val app: Application) {
    private val activityTracker = ActivityTracker()

    private fun onStart() {
        System.loadLibrary("main")

        val usbManager = app.getSystemService(USB_SERVICE) as UsbManager
        GCAdapter.manager = usbManager
        WiimoteAdapter.manager = usbManager

        if (DirectoryInitialization.shouldStart(app.applicationContext)) {
            DirectoryInitialization.start(app.applicationContext)
        }
    }

    /** Forwards Dolphin's screens only to the tracker (it calls the native library), starting Dolphin first. */
    private val dolphinScreens = object : ActivityLifecycleCallbacks {
        private fun Activity.isDolphin() = javaClass.name.startsWith("org.dolphinemu.")

        override fun onActivityPreCreated(activity: Activity, bundle: Bundle?) {
            if (activity.isDolphin()) start()
        }
        override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
            if (activity.isDolphin()) activityTracker.onActivityCreated(activity, bundle)
        }
        override fun onActivityStarted(activity: Activity) {
            if (activity.isDolphin()) activityTracker.onActivityStarted(activity)
        }
        override fun onActivityResumed(activity: Activity) {
            if (activity.isDolphin()) activityTracker.onActivityResumed(activity)
        }
        override fun onActivityPaused(activity: Activity) {
            if (activity.isDolphin()) activityTracker.onActivityPaused(activity)
        }
        override fun onActivityStopped(activity: Activity) {
            if (activity.isDolphin()) activityTracker.onActivityStopped(activity)
        }
        override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {
            if (activity.isDolphin()) activityTracker.onActivitySaveInstanceState(activity, bundle)
        }
        override fun onActivityPostSaveInstanceState(activity: Activity, bundle: Bundle) {
            if (activity.isDolphin()) activityTracker.onActivityPostSaveInstanceState(activity, bundle)
        }
        override fun onActivityDestroyed(activity: Activity) {
            if (activity.isDolphin()) activityTracker.onActivityDestroyed(activity)
        }
    }

    companion object {
        private var app: DolphinApplication? = null
        private var started = false

        // Dolphin's native library is built for arm64 only here
        @JvmStatic
        val isSupported: Boolean
            get() = android.os.Build.SUPPORTED_ABIS.contains("arm64-v8a")

        @JvmStatic
        fun attach(application: Application) {
            if (app == null) {
                app = DolphinApplication(application).also { application.registerActivityLifecycleCallbacks(it.dolphinScreens) }
            }
        }

        @JvmStatic
        @Synchronized
        fun start() {
            if (!started) {
                started = true
                app!!.onStart()
            }
        }

        @JvmStatic
        fun getAppContext(): Context = app!!.app.applicationContext

        @JvmStatic
        fun getAppActivity(): Activity? = app!!.activityTracker.currentActivity
    }
}
