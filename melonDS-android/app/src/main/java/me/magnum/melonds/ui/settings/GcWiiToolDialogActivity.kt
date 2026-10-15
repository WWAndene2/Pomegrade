package me.magnum.melonds.ui.settings

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.R as DolphinR
import org.dolphinemu.dolphinemu.dialogs.GamePropertiesDialog
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.main.MainPresenter
import org.dolphinemu.dolphinemu.ui.main.MainView
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner

/**
 * The tools of Dolphin's home screen (Wii system menu, WAD install, Wii save and NAND backup import, online system
 * update, About) and a game's properties (its long-press menu there), shown over Punica's screens. Dolphin's
 * dialogs need its Material 3 theme, so they get this see-through screen with Dolphin's theme; the tools are
 * Dolphin's own (MainPresenter, the home screen's menu), not copies. The screen closes once it has its window's
 * focus back: every dialog closed, the file picker or the game it started gone.
 */
class GcWiiToolDialogActivity : AppCompatActivity(), MainView {

    enum class Tool { WII_SYSTEM_MENU, INSTALL_WAD, IMPORT_WII_SAVE, IMPORT_NAND_BACKUP, ONLINE_SYSTEM_UPDATE, ABOUT, GAME_PROPERTIES }

    private lateinit var presenter: MainPresenter
    // set once a dialog, the picker or a game has taken the focus: getting it back then means the tool is done
    private var focusLeft = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        DolphinApplication.start()
        // registers the presenter's file pickers: before the activity is started
        presenter = MainPresenter(this, this)
        if (savedInstanceState != null) return
        val tool = Tool.valueOf(intent.getStringExtra(EXTRA_TOOL) ?: return finish())
        when (tool) {
            Tool.GAME_PROPERTIES -> showGameProperties(intent.getStringExtra(EXTRA_GAME_PATH) ?: return finish())
            else -> presenter.handleOptionSelection(menuItemId(tool), this)
        }
    }

    // the home screen menu's item for each tool, as MainPresenter.handleOptionSelection takes them
    private fun menuItemId(tool: Tool): Int = when (tool) {
        Tool.WII_SYSTEM_MENU -> DolphinR.id.menu_load_wii_system_menu
        Tool.INSTALL_WAD -> DolphinR.id.menu_install_wad
        Tool.IMPORT_WII_SAVE -> DolphinR.id.menu_import_wii_save
        Tool.IMPORT_NAND_BACKUP -> DolphinR.id.menu_import_nand_backup
        Tool.ONLINE_SYSTEM_UPDATE -> DolphinR.id.menu_online_system_update
        Tool.ABOUT -> DolphinR.id.menu_about
        Tool.GAME_PROPERTIES -> error("not a menu item")
    }

    // as Dolphin's game list does on a long press (GameAdapter): the game's file read off the main thread
    private fun showGameProperties(path: String) {
        AfterDirectoryInitializationRunner().runWithLifecycle(this) {
            lifecycleScope.launch {
                val game = withContext(Dispatchers.IO) { GameFileCacheManager.addOrGet(path) }
                GamePropertiesDialog.newInstance(game).show(supportFragmentManager, GamePropertiesDialog.TAG)
            }
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (!hasFocus) {
            focusLeft = true
            return
        }
        // a tool's next dialog can follow the last one (progress, then its result): closes only if the focus stays
        if (focusLeft) window.decorView.postDelayed({ if (hasWindowFocus() && !isFinishing) finish() }, FOCUS_SETTLE_MS)
    }

    override fun onResume() {
        super.onResume()
        presenter.onResume()
    }

    // MainView: the home screen's game list, which this screen does not have
    override fun setVersionString(version: String) = Unit
    override fun launchSettingsActivity(menuTag: MenuTag?) = Unit
    override fun setRefreshing(refreshing: Boolean) = Unit
    override fun showGames() = Unit
    override fun reloadGrid() = Unit
    override fun showGridOptions() = Unit

    companion object {
        private const val EXTRA_TOOL = "tool"
        private const val EXTRA_GAME_PATH = "game_path"
        private const val FOCUS_SETTLE_MS = 300L

        fun intent(context: Context, tool: Tool): Intent =
            Intent(context, GcWiiToolDialogActivity::class.java).putExtra(EXTRA_TOOL, tool.name)

        /** A game's properties (settings, cheats, conversion...), [path] as Punica's list holds it (its URI). */
        fun gamePropertiesIntent(context: Context, path: String): Intent =
            intent(context, Tool.GAME_PROPERTIES).putExtra(EXTRA_GAME_PATH, path)
    }
}
