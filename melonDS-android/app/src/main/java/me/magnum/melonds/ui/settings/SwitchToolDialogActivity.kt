package me.magnum.melonds.ui.settings

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R as EdenR
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.dialogs.NetPlayDialog
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.fragments.ProgressDialogFragment
import org.yuzu.yuzu_emu.fragments.SystemInfoDialogFragment

/**
 * The tools of Eden's home screen that are dialogs (multiplayer, checking the installed content, system
 * information), shown over Punica's settings. Eden's dialogs need its Material 3 theme, which Punica's
 * AppCompat theme is not, so they get this see-through screen with Eden's theme; it closes with its last dialog.
 * Started after SwitchLauncher.prepare, as the other Switch tools.
 */
class SwitchToolDialogActivity : AppCompatActivity() {

    enum class Tool { MULTIPLAYER, VERIFY_CONTENT, SYSTEM_INFO }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // closes once no dialog is left (the content check shows its result after its progress dialog)
        supportFragmentManager.registerFragmentLifecycleCallbacks(object : FragmentManager.FragmentLifecycleCallbacks() {
            override fun onFragmentDestroyed(fm: FragmentManager, f: Fragment) {
                window.decorView.post { if (supportFragmentManager.fragments.isEmpty() && !isChangingConfigurations) finish() }
            }
        }, false)
        if (savedInstanceState != null) return
        when (Tool.valueOf(intent.getStringExtra(EXTRA_TOOL) ?: return finish())) {
            Tool.MULTIPLAYER -> NetPlayDialog(this).apply { setOnDismissListener { finish() } }.show()
            Tool.SYSTEM_INFO -> SystemInfoDialogFragment.newInstance().show(supportFragmentManager, SystemInfoDialogFragment.TAG)
            Tool.VERIFY_CONTENT -> verifyContent()
        }
    }

    // as Eden's home screen does (HomeSettingsFragment, "Verify installed content")
    private fun verifyContent() {
        ProgressDialogFragment.newInstance(this, titleId = EdenR.string.verifying, cancellable = true) { progressCallback, _ ->
            val result = NativeLibrary.verifyInstalledContents(progressCallback)
            if (progressCallback.invoke(100, 100)) {
                MessageDialogFragment.newInstance(titleId = EdenR.string.verify_no_result, descriptionId = EdenR.string.verify_no_result_description)
            } else if (result.isEmpty()) {
                MessageDialogFragment.newInstance(titleId = EdenR.string.verify_success, descriptionId = EdenR.string.operation_completed_successfully)
            } else {
                val message = YuzuApplication.appContext.getString(EdenR.string.verification_failed_for, result.joinToString("\n"))
                MessageDialogFragment.newInstance(titleId = EdenR.string.verify_failure, descriptionString = message)
            }
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    companion object {
        private const val EXTRA_TOOL = "tool"

        fun intent(context: Context, tool: Tool): Intent =
            Intent(context, SwitchToolDialogActivity::class.java).putExtra(EXTRA_TOOL, tool.name)
    }
}
