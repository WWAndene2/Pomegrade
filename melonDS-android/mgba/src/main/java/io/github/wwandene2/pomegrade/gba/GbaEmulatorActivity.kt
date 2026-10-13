package io.github.wwandene2.pomegrade.gba

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.GravityCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.drawerlayout.widget.DrawerLayout
import com.google.android.material.navigation.NavigationView
import io.github.wwandene2.pomegrade.emulatorui.EmulatorMenu
import io.github.wwandene2.pomegrade.emulatorui.ScreenshotGallery
import io.github.wwandene2.pomegrade.emulatorui.ShortcutPill
import io.github.wwandene2.pomegrade.emulatorui.R as MenuR
import io.github.wwandene2.pomegrade.emulatorui.SaveStateSlotUi
import io.github.wwandene2.pomegrade.emulatorui.SaveStatesDialog
import java.io.File
import java.util.Date

/**
 * Plays a GBA game. The game's battery save and save states are in the saves folder the intent gives
 * (Punica's GBA, as each console's saves are in its own folder), or next to its file without one:
 * `<game>.sav`, `<game>.ss0` (quick slot) to `.ss9`, each state with a `.png` of its screen.
 * Back opens the shared in-game menu, which pauses the game while it is open.
 */
class GbaEmulatorActivity : AppCompatActivity() {

    companion object {
        private const val EXTRA_ROM_PATH = "rom_path"
        private const val EXTRA_TITLE = "title"
        private const val EXTRA_BIOS_PATH = "bios_path"
        private const val EXTRA_SMOOTH_SCREEN = "smooth_screen"
        private const val EXTRA_SAVE_DIR = "save_dir"
        private const val PREFERENCES = "gba"
        private const val PREF_PERFORMANCE_OVERLAY = "performance_overlay"
        private const val STATE_SLOTS = 10

        /** Plays the game at [romPath] (a file path); [biosPath] a GBA BIOS, or null for mGBA's own. */
        fun intent(context: Context, romPath: String, title: String, biosPath: String?, smoothScreen: Boolean, saveDir: String?): Intent =
            Intent(context, GbaEmulatorActivity::class.java)
                .putExtra(EXTRA_ROM_PATH, romPath)
                .putExtra(EXTRA_TITLE, title)
                .putExtra(EXTRA_BIOS_PATH, biosPath)
                .putExtra(EXTRA_SMOOTH_SCREEN, smoothScreen)
                .putExtra(EXTRA_SAVE_DIR, saveDir)
    }

    private lateinit var screen: GbaScreenView
    private lateinit var controls: GbaControlsView
    private lateinit var drawer: DrawerLayout
    private lateinit var rom: File
    private var saveDir: File? = null
    private var thread: GbaEmulationThread? = null
    // keys held on a controller or keyboard, merged with the on-screen ones
    @Volatile
    private var padKeys = 0
    // directions pressed by a controller's hat axes or left stick
    @Volatile
    private var axisKeys = 0
    private var pausedByUser = false
    private lateinit var performanceText: TextView
    private val performanceHandler = Handler(Looper.getMainLooper())
    private var performanceFrames = 0L
    private var performanceAt = 0L
    // the frame rate and speed, once a second, while the overlay is on
    private val performanceUpdater = object : Runnable {
        override fun run() {
            val frames = thread?.frames ?: 0L
            val now = SystemClock.elapsedRealtime()
            if (performanceAt != 0L && now > performanceAt && frames >= performanceFrames) {
                val fps = (frames - performanceFrames) * 1000.0 / (now - performanceAt)
                performanceText.text = getString(R.string.gba_performance, fps, fps * 100 / GbaEmulationThread.FRAME_RATE)
            }
            performanceFrames = frames
            performanceAt = now
            performanceHandler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.gba_emulator_activity)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        screen = findViewById(R.id.gba_screen)
        screen.smooth = intent.getBooleanExtra(EXTRA_SMOOTH_SCREEN, false)
        controls = findViewById(R.id.gba_controls)
        performanceText = findViewById(R.id.gba_performance)
        drawer = findViewById(R.id.gba_drawer)
        rom = File(intent.getStringExtra(EXTRA_ROM_PATH) ?: return finish())
        saveDir = intent.getStringExtra(EXTRA_SAVE_DIR)?.let { File(it) }?.takeIf { it.isDirectory || it.mkdirs() }
        val menu = findViewById<NavigationView>(R.id.gba_menu)
        setUpMenu(menu, intent.getStringExtra(EXTRA_TITLE) ?: rom.nameWithoutExtension)
        // the shortcut pill: the menu's entries, run as the menu runs them
        ShortcutPill.attach(this, { EmulatorMenu.visibleItems(menu.menu) }, { id -> menu.menu.performIdentifierAction(id, 0) })
        if (!ShortcutPill.edgeSwipeAllowed(this)) drawer.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED, GravityCompat.START)

        val loaded = GbaNative.load(rom.path, sibling(".sav").path, intent.getStringExtra(EXTRA_BIOS_PATH), screen.frameBuffer, GbaEmulationThread.SAMPLE_RATE)
        if (!loaded) {
            Toast.makeText(this, R.string.gba_load_failed, Toast.LENGTH_LONG).show()
            finish()
            return
        }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (stopRewind()) return
                if (drawer.isDrawerOpen(GravityCompat.START)) drawer.closeDrawer(GravityCompat.START) else drawer.openDrawer(GravityCompat.START)
            }
        })
    }

    private fun setUpMenu(menu: NavigationView, title: String) {
        EmulatorMenu.showOnly(
            menu.menu,
            setOf(MenuR.id.emulator_menu_pause, MenuR.id.emulator_menu_save_state, MenuR.id.emulator_menu_load_state, MenuR.id.emulator_menu_rewind, MenuR.id.emulator_menu_screenshot, MenuR.id.emulator_menu_performance_overlay, MenuR.id.emulator_menu_reset, MenuR.id.emulator_menu_exit),
        )
        EmulatorMenu.setHeader(menu, title, getString(R.string.gba_console))
        menu.menu.findItem(MenuR.id.emulator_menu_performance_overlay)?.isChecked = showPerformance
        drawer.addDrawerListener(object : DrawerLayout.SimpleDrawerListener() {
            override fun onDrawerOpened(drawerView: android.view.View) = updatePause()
            override fun onDrawerClosed(drawerView: android.view.View) = updatePause()
        })
        menu.setNavigationItemSelectedListener { item ->
            when (item.itemId) {
                MenuR.id.emulator_menu_pause -> {
                    pausedByUser = !pausedByUser
                    item.setTitle(if (pausedByUser) MenuR.string.emulator_menu_resume else MenuR.string.emulator_menu_pause)
                }
                MenuR.id.emulator_menu_save_state -> showStates(saving = true)
                MenuR.id.emulator_menu_load_state -> showStates(saving = false)
                MenuR.id.emulator_menu_rewind -> startRewind()
                MenuR.id.emulator_menu_screenshot -> thread?.runBetweenFrames { ScreenshotGallery.save(this, frame(), "GBA", title) }
                MenuR.id.emulator_menu_performance_overlay -> {
                    showPerformance = !showPerformance
                    item.isChecked = showPerformance
                    updatePerformanceOverlay()
                }
                MenuR.id.emulator_menu_reset -> thread?.runBetweenFrames { GbaNative.reset() }
                MenuR.id.emulator_menu_exit -> finish()
            }
            if (item.itemId != MenuR.id.emulator_menu_save_state && item.itemId != MenuR.id.emulator_menu_load_state) {
                drawer.closeDrawer(GravityCompat.START)
            }
            true
        }
    }

    // the game's save, states and their previews: in the saves folder given (Punica's GBA), else next to the game
    private fun sibling(extension: String) = File(saveDir ?: rom.parentFile, rom.nameWithoutExtension + extension)

    private fun stateFile(slot: Int) = sibling(".ss$slot")

    private fun previewFile(slot: Int) = sibling(".ss$slot.png")

    private fun slots() = (0 until STATE_SLOTS).map { slot ->
        val state = stateFile(slot)
        val preview = previewFile(slot)
        SaveStateSlotUi(
            slot = slot,
            isQuickSlot = slot == 0,
            savedAt = if (state.isFile) Date(state.lastModified()) else null,
            screenshot = if (state.isFile && preview.isFile) Uri.fromFile(preview) else null,
        )
    }

    private fun showStates(saving: Boolean) {
        SaveStatesDialog.show(
            context = this,
            saving = saving,
            slots = slots(),
            onSlotPicked = { slot -> if (saving) saveState(slot.slot) else loadState(slot.slot) },
            onSlotDeleted = { slot ->
                stateFile(slot.slot).delete()
                previewFile(slot.slot).delete()
                slots()
            },
            onCancel = {},
            onDismiss = { drawer.closeDrawer(GravityCompat.START) },
        )
    }

    private fun saveState(slot: Int) {
        thread?.runBetweenFrames {
            val saved = GbaNative.saveState(stateFile(slot).path)
            if (saved) {
                previewFile(slot).outputStream().use { frame().compress(Bitmap.CompressFormat.PNG, 100, it) }
            }
            runOnUiThread {
                Toast.makeText(this, if (saved) R.string.gba_state_saved else R.string.gba_state_save_failed, Toast.LENGTH_SHORT).show()
            }
        }
    }

    // the last frame the core drew; on the emulation thread, between two frames
    private fun frame(): Bitmap {
        screen.frameBuffer.rewind()
        return Bitmap.createBitmap(GbaNative.WIDTH, GbaNative.HEIGHT, Bitmap.Config.ARGB_8888).apply { copyPixelsFromBuffer(screen.frameBuffer) }
    }

    private fun loadState(slot: Int) {
        thread?.runBetweenFrames {
            if (!GbaNative.loadState(stateFile(slot).path)) {
                runOnUiThread { Toast.makeText(this, R.string.gba_state_load_failed, Toast.LENGTH_SHORT).show() }
            }
        }
    }

    private fun updatePause() {
        thread?.paused = pausedByUser || drawer.isDrawerOpen(GravityCompat.START)
    }

    override fun onResume() {
        super.onResume()
        if (thread == null) {
            thread = GbaEmulationThread(screen) { controls.keys or padKeys or axisKeys }.also { it.start() }
        }
        updatePause()
        updatePerformanceOverlay()
    }

    // the overlay's choice, kept for every GBA game
    private var showPerformance: Boolean
        get() = getSharedPreferences(PREFERENCES, MODE_PRIVATE).getBoolean(PREF_PERFORMANCE_OVERLAY, false)
        set(value) = getSharedPreferences(PREFERENCES, MODE_PRIVATE).edit().putBoolean(PREF_PERFORMANCE_OVERLAY, value).apply()

    private fun updatePerformanceOverlay() {
        performanceHandler.removeCallbacks(performanceUpdater)
        performanceAt = 0L
        performanceText.text = null
        performanceText.visibility = if (showPerformance) View.VISIBLE else View.GONE
        if (showPerformance) performanceHandler.post(performanceUpdater)
    }

    // the game plays backwards until the screen is touched (or a button pressed), as far as mGBA's buffer goes
    private fun startRewind() {
        val running = thread ?: return
        running.onRewindEnd = { runOnUiThread { Toast.makeText(this, R.string.gba_rewind_end, Toast.LENGTH_SHORT).show() } }
        running.rewinding = true
        Toast.makeText(this, R.string.gba_rewind_hint, Toast.LENGTH_SHORT).show()
    }

    // true when it stopped a rewind: the touch or key that did it goes no further
    private fun stopRewind(): Boolean {
        val running = thread ?: return false
        if (!running.rewinding) return false
        running.rewinding = false
        return true
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN && stopRewind()) return true
        return super.dispatchTouchEvent(event)
    }

    override fun onPause() {
        thread?.rewinding = false
        performanceHandler.removeCallbacks(performanceUpdater)
        // the core's battery save is written as the game saves; stopping the thread ends the frame in progress
        thread?.finish()
        thread = null
        super.onPause()
    }

    override fun onDestroy() {
        if (isFinishing) GbaNative.unload()
        super.onDestroy()
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.isFromSource(InputDevice.SOURCE_JOYSTICK) && event.actionMasked == MotionEvent.ACTION_MOVE) {
            axisKeys = GbaKeys.fromAxes(
                event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y),
                event.getAxisValue(MotionEvent.AXIS_X), event.getAxisValue(MotionEvent.AXIS_Y),
            )
            return true
        }
        return super.onGenericMotionEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val key = GbaKeys.fromKeyCode(keyCode)
        if (key == 0) return super.onKeyDown(keyCode, event)
        if (stopRewind()) return true
        padKeys = padKeys or key
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        val key = GbaKeys.fromKeyCode(keyCode)
        if (key == 0) return super.onKeyUp(keyCode, event)
        padKeys = padKeys and key.inv()
        return true
    }
}
