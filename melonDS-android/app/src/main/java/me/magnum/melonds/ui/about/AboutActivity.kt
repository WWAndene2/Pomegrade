package me.magnum.melonds.ui.about

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import me.magnum.melonds.ui.about.ui.AboutScreen
import me.magnum.melonds.ui.theme.MelonTheme

/**
 * Pomegrade's about screen: version, source code and the projects Pomegrade is built on, with
 * their licences (the GPL requires both cores to stay credited and their licences to be given).
 */
class AboutActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val versionName = packageManager.getPackageInfo(packageName, 0).versionName.orEmpty()

        setContent {
            MelonTheme {
                AboutScreen(
                    versionName = versionName,
                    onNavigateBack = { onNavigateUp() },
                )
            }
        }
    }
}
