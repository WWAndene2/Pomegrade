package io.github.wwandene2.pomegrade.gba

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.view.KeyEvent
import android.view.WindowManager
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
import io.github.wwandene2.pomegrade.emulatorui.R as MenuR
import io.github.wwandene2.pomegrade.emulatorui.SaveStateSlotUi
import io.github.wwandene2.pomegrade.emulatorui.SaveStatesDialog
import java.io.File
import java.util.Date

/**
 * Plays a GBA game. The game's battery save and save states sit next to its file, as the DS games'
 * do: `<game>.sav`, `<game>.ss0` (quick slot) to `.ss9`, each state with a `.png` of its screen.
 * Back opens the shared in-game menu, which pauses the game while it is open.
 */
class GbaEmulatorActivity : AppCompatActivity() {

    companion object {
        private const val EXTRA_ROM_PATH = "rom_path"
        private const val EXTRA_TITLE = "title"
        private const val EXTRA_BIOS_PATH = "bios_path"
        private const val STATE_SLOTS = 10

        /** Plays the game at [romPath] (a file path); [biosPath] a GBA BIOS, or null for mGBA's own. */
        fun intent(context: Context, romPath: String, title: String, biosPath: String?): Intent =
            Intent(context, GbaEmulatorActivity::class.java)
                .putExtra(EXTRA_ROM_PATH, romPath)
                .putExtra(EXTRA_TITLE, title)
                .putExtra(EXTRA_BIOS_PATH, biosPath)
    }

    private lateinit var screen: GbaScreenView
    private lateinit var controls: GbaControlsView
    private lateinit var drawer: DrawerLayout
    private lateinit var rom: File
    private var thread: GbaEmulationThread? = null
    // keys held on a controller or keyboard, merged with the on-screen ones
    @Volatile
    private var padKeys = 0
    private var pausedByUser = false

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
        controls = findViewById(R.id.gba_controls)
        drawer = findViewById(R.id.gba_drawer)
        rom = File(intent.getStringExtra(EXTRA_ROM_PATH) ?: return finish())
        setUpMenu(findViewById(R.id.gba_menu), intent.getStringExtra(EXTRA_TITLE) ?: rom.nameWithoutExtension)

        val loaded = GbaNative.load(rom.path, sibling(".sav").path, intent.getStringExtra(EXTRA_BIOS_PATH), screen.frameBuffer, GbaEmulationThread.SAMPLE_RATE)
        if (!loaded) {
            Toast.makeText(this, R.string.gba_load_failed, Toast.LENGTH_LONG).show()
            finish()
            return
        }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (drawer.isDrawerOpen(GravityCompat.START)) drawer.closeDrawer(GravityCompat.START) else drawer.openDrawer(GravityCompat.START)
            }
        })
    }

    private fun setUpMenu(menu: NavigationView, title: String) {
        EmulatorMenu.showOnly(
            menu.menu,
            setOf(MenuR.id.emulator_menu_pause, MenuR.id.emulator_menu_save_state, MenuR.id.emulator_menu_load_state, MenuR.id.emulator_menu_reset, MenuR.id.emulator_menu_exit),
        )
        EmulatorMenu.setHeader(menu, title, getString(R.string.gba_console))
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
                MenuR.id.emulator_menu_reset -> thread?.runBetweenFrames { GbaNative.reset() }
                MenuR.id.emulator_menu_exit -> finish()
            }
            if (item.itemId != MenuR.id.emulator_menu_save_state && item.itemId != MenuR.id.emulator_menu_load_state) {
                drawer.closeDrawer(GravityCompat.START)
            }
            true
        }
    }

    private fun sibling(extension: String) = File(rom.parentFile, rom.nameWithoutExtension + extension)

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
                screen.frameBuffer.rewind()
                val shot = Bitmap.createBitmap(GbaNative.WIDTH, GbaNative.HEIGHT, Bitmap.Config.ARGB_8888)
                shot.copyPixelsFromBuffer(screen.frameBuffer)
                previewFile(slot).outputStream().use { shot.compress(Bitmap.CompressFormat.PNG, 100, it) }
            }
            runOnUiThread {
                Toast.makeText(this, if (saved) R.string.gba_state_saved else R.string.gba_state_save_failed, Toast.LENGTH_SHORT).show()
            }
        }
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
            thread = GbaEmulationThread(screen) { controls.keys or padKeys }.also { it.start() }
        }
        updatePause()
    }

    override fun onPause() {
        // the core's battery save is written as the game saves; stopping the thread ends the frame in progress
        thread?.finish()
        thread = null
        super.onPause()
    }

    override fun onDestroy() {
        if (isFinishing) GbaNative.unload()
        super.onDestroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val key = GbaKeys.fromKeyCode(keyCode)
        if (key == 0) return super.onKeyDown(keyCode, event)
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
