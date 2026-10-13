package me.magnum.melonds.ui.shortcutsetup

import android.graphics.Color
import android.os.Bundle
import android.view.ViewGroup
import androidx.activity.SystemBarStyle
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.commit
import androidx.lifecycle.lifecycleScope
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.launch
import me.magnum.melonds.databinding.ActivityShortcutSetupBinding
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.ui.common.rom.GameShortcut
import me.magnum.melonds.ui.common.rom.N3dsLauncher
import me.magnum.melonds.ui.romlist.RomListFragment
import me.magnum.melonds.ui.romlist.RomListViewModel

@AndroidEntryPoint
class ShortcutSetupActivity : AppCompatActivity() {
    companion object {
        private const val FRAGMENT_ROM_LIST = "rom_list"
    }

    private val viewModel: RomListViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge(statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT))
        super.onCreate(savedInstanceState)
        val binding = ActivityShortcutSetupBinding.inflate(layoutInflater)
        setContentView(binding.root)
        setSupportActionBar(binding.toolbar)

        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, windowInsets ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            // Punica's header: its padding holds the side insets, its content insets stay as styled
            binding.toolbar.applySideInsets(insets.left, insets.right)
            binding.viewStatusBarBackground.updateLayoutParams {
                height = insets.top
            }
            binding.layoutRoot.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                leftMargin = insets.left
                rightMargin = insets.right
            }

            windowInsets.inset(insets.left, insets.top, insets.right, 0)
        }

        val fragment = if (savedInstanceState == null) {
            // 3DS games too where the 3DS core runs (64-bit devices)
            val criteria = if (N3dsLauncher.isSupported()) RomListFragment.RomEnableCriteria.ENABLE_ALL else RomListFragment.RomEnableCriteria.ENABLE_DS_ONLY
            RomListFragment.newInstance(false, criteria).also {
                supportFragmentManager.commit {
                    replace(binding.layoutRoot.id, it, FRAGMENT_ROM_LIST)
                }
            }
        } else {
            supportFragmentManager.findFragmentByTag(FRAGMENT_ROM_LIST) as RomListFragment
        }

        fragment.setRomSelectedListener { onRomSelected(it) }
    }

    private fun onRomSelected(rom: Rom) {
        // every console's shortcut opens the game list, which starts the game as a tap in it does: the same
        // checks (the folder, its access) and preparation (the Switch core's keys, firmware and folder) for all
        lifecycleScope.launch {
            val shortcutInfo = GameShortcut.build(this@ShortcutSetupActivity, rom, viewModel.getRomIcon(rom))
            val shortcutIntent = ShortcutManagerCompat.createShortcutResultIntent(this@ShortcutSetupActivity, shortcutInfo)

            setResult(RESULT_OK, shortcutIntent)
            finish()
        }
    }
}