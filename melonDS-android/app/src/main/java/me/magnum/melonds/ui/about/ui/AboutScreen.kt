package me.magnum.melonds.ui.about.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.ui.about.model.Credit

private const val POMEGRADE_SOURCE_URL = "https://github.com/WWAndene2/Pomegrade"
private const val GPL_LICENCES_URL = "https://www.gnu.org/licenses/"

// The cores first: their authors and licences must stay visible in every Pomegrade build
private val credits = listOf(
    Credit("melonDS", R.string.pomegrade_about_role_melonds, "Arisotura and the melonDS team", "GPL 3.0", "https://melonds.kuribo64.net"),
    Credit("melonDS Android", R.string.pomegrade_about_role_melonds_android, "Rafael Caetano and contributors", "GPL 3.0", "https://github.com/rafaelvcaetano/melonDS-android"),
    Credit("Azahar", R.string.pomegrade_about_role_azahar, "The Azahar project, based on Citra by the Citra team", "GPL 2.0 or later", "https://azahar-emu.org"),
    Credit("stb", R.string.pomegrade_about_role_stb, "Sean Barrett", "Public domain", "https://github.com/nothings/stb"),
    Credit("Oboe", R.string.pomegrade_about_role_oboe, "Google", "Apache 2.0", "https://github.com/google/oboe"),
    Credit("FAAD2", R.string.pomegrade_about_role_faad2, "M. Bakker, Nero AG and contributors", "GPL 2.0 or later", "https://github.com/knik0/faad2"),
    Credit("ENet", R.string.pomegrade_about_role_enet, "Lee Salzman", "MIT", "https://github.com/lsalzman/enet"),
)

@Composable
fun AboutScreen(
    versionName: String,
    onNavigateBack: () -> Unit,
) {
    val uriHandler = LocalUriHandler.current

    Scaffold(
        topBar = {
            Box(Modifier.background(MaterialTheme.colors.primaryVariant).statusBarsPadding()) {
                TopAppBar(
                    navigationIcon = {
                        IconButton(onClick = onNavigateBack) {
                            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.navigate_back))
                        }
                    },
                    title = { Text(stringResource(R.string.pomegrade_about)) },
                    backgroundColor = MaterialTheme.colors.primary,
                    contentColor = MaterialTheme.colors.onPrimary,
                    windowInsets = WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal + WindowInsetsSides.Top),
                )
            }
        },
        backgroundColor = MaterialTheme.colors.background,
    ) { padding ->
        LazyColumn(
            modifier = Modifier.fillMaxSize().padding(padding),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            item {
                Header(versionName = versionName, onSourceClick = { uriHandler.openUri(POMEGRADE_SOURCE_URL) })
            }
            item {
                SectionTitle(stringResource(R.string.pomegrade_about_built_on))
            }
            items(credits) { credit ->
                CreditCard(credit = credit, onClick = { uriHandler.openUri(credit.url) })
            }
            item {
                Text(
                    text = stringResource(R.string.pomegrade_about_licences_notice),
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { uriHandler.openUri(GPL_LICENCES_URL) }
                        .padding(vertical = 16.dp)
                        .navigationBarsPadding(),
                    style = MaterialTheme.typography.caption,
                    color = MaterialTheme.colors.onBackground,
                    textAlign = TextAlign.Center,
                )
            }
        }
    }
}

@Composable
private fun Header(versionName: String, onSourceClick: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxWidth().padding(vertical = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Image(
            painter = painterResource(R.mipmap.ic_launcher_foreground),
            contentDescription = null,
            modifier = Modifier.size(128.dp),
        )
        Text(
            text = stringResource(R.string.app_name),
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colors.onSurface,
        )
        Text(
            text = stringResource(R.string.pomegrade_about_version, versionName),
            style = MaterialTheme.typography.body2,
            color = MaterialTheme.colors.onBackground,
        )
        Text(
            text = stringResource(R.string.pomegrade_about_description),
            modifier = Modifier.padding(top = 16.dp),
            style = MaterialTheme.typography.body1,
            color = MaterialTheme.colors.onSurface,
            textAlign = TextAlign.Center,
        )
        Text(
            text = stringResource(R.string.pomegrade_about_source_code),
            modifier = Modifier
                .padding(top = 8.dp)
                .clickable(onClick = onSourceClick)
                .padding(8.dp),
            style = MaterialTheme.typography.button,
            color = MaterialTheme.colors.secondary,
        )
    }
}

@Composable
private fun SectionTitle(text: String) {
    Text(
        text = text,
        modifier = Modifier.padding(top = 8.dp, bottom = 4.dp),
        style = MaterialTheme.typography.subtitle2,
        color = MaterialTheme.colors.secondary,
    )
}

@Composable
private fun CreditCard(credit: Credit, onClick: () -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth().clickable(onClick = onClick),
        backgroundColor = MaterialTheme.colors.surface,
        elevation = 2.dp,
    ) {
        Column(Modifier.padding(16.dp)) {
            Text(
                text = credit.name,
                style = MaterialTheme.typography.subtitle1,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colors.onSurface,
            )
            Text(
                text = stringResource(credit.role),
                style = MaterialTheme.typography.body2,
                color = MaterialTheme.colors.onSurface,
            )
            Text(
                text = stringResource(R.string.pomegrade_about_credit_authors_licence, credit.authors, credit.licence),
                modifier = Modifier.padding(top = 4.dp),
                style = MaterialTheme.typography.caption,
                color = MaterialTheme.colors.onBackground,
            )
        }
    }
}
