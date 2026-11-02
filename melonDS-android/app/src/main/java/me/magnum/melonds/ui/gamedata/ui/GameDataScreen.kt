package me.magnum.melonds.ui.gamedata.ui

import android.text.format.DateUtils
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.Scaffold
import androidx.compose.material.Switch
import androidx.compose.material.SwitchDefaults
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material.icons.outlined.FileUpload
import androidx.compose.material.icons.outlined.Restore
import androidx.compose.material.icons.outlined.Save
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.GameData
import me.magnum.melonds.ui.common.component.PunicaHeader
import me.magnum.melonds.ui.common.component.platformLongName
import me.magnum.melonds.ui.common.component.punicaBackground
import me.magnum.melonds.ui.theme.PunicaDimens
import me.magnum.melonds.ui.theme.PunicaTheme

/** What the page shows of a game's data ([GameData]), read off the main thread by its activity. */
data class GameDataState(
    val loaded: Boolean = false,
    val title: String = "",
    val hasSave: Boolean = false,
    val lastSaved: Long? = null,
    val backups: List<GameData.Backup> = emptyList(),
    val mods: List<GameData.Mod> = emptyList(),
    val takesMods: Boolean = false,
    val takesTextures: Boolean = false,
)

/**
 * A game's mods and saves: the save (its last write; copy, export, import, delete) and its copies (restore, delete),
 * then its mods and texture packs (on / off, delete; install from a zip). Every row 64 dp, cards as the game sheet's.
 */
@Composable
fun GameDataScreen(
    rom: Rom,
    state: GameDataState,
    onBack: () -> Unit,
    onBackup: () -> Unit,
    onRestore: (GameData.Backup) -> Unit,
    onDeleteBackup: (GameData.Backup) -> Unit,
    onExport: () -> Unit,
    onImport: () -> Unit,
    onDeleteSave: () -> Unit,
    onInstall: (GameData.Kind) -> Unit,
    onToggleMod: (GameData.Mod) -> Unit,
    onDeleteMod: (GameData.Mod) -> Unit,
) {
    Scaffold(
        modifier = Modifier.punicaBackground(),
        topBar = { PunicaHeader(subtitle = stringResource(R.string.game_data_title), onBack = onBack) },
        backgroundColor = Color.Transparent,
        // the system bars and the cutout reach the content as Scaffold's padding (bottom: navigation bar + margin)
        contentWindowInsets = WindowInsets.safeDrawing,
    ) { padding ->
        val direction = LocalLayoutDirection.current
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(
                start = padding.calculateStartPadding(direction) + PunicaDimens.MARGIN.dp,
                top = padding.calculateTopPadding() + PunicaDimens.MARGIN.dp,
                end = padding.calculateEndPadding(direction) + PunicaDimens.MARGIN.dp,
                bottom = padding.calculateBottomPadding() + PunicaDimens.MARGIN.dp,
            ),
            verticalArrangement = Arrangement.spacedBy(PunicaDimens.GAP.dp),
        ) {
            item {
                Column(Modifier.padding(bottom = PunicaDimens.GAP.dp)) {
                    Text(platformLongName(rom.platform).uppercase(), fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted)
                    Text(state.title, fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 24.sp, lineHeight = 32.sp, color = PunicaTheme.text)
                }
            }
            if (!state.loaded) return@LazyColumn

            // the save
            item { SectionTitle(stringResource(R.string.game_data_save)) }
            item {
                Card {
                    Row(Modifier.fillMaxWidth().heightIn(min = PunicaDimens.ROW_MIN.dp).padding(horizontal = PunicaDimens.MARGIN.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            if (state.hasSave) stringResource(R.string.game_data_last_saved, relative(state.lastSaved)) else stringResource(R.string.game_data_no_save),
                            fontSize = 14.sp, lineHeight = 20.sp, color = if (state.hasSave) PunicaTheme.text else PunicaTheme.textMuted,
                        )
                    }
                    Divider(color = PunicaTheme.line)
                    if (state.hasSave) ActionRow(Icons.Outlined.Save, stringResource(R.string.game_data_backup_now), onBackup)
                    if (state.hasSave) Divider(color = PunicaTheme.line)
                    if (state.hasSave) ActionRow(Icons.Outlined.FileUpload, stringResource(R.string.game_data_export), onExport)
                    if (state.hasSave) Divider(color = PunicaTheme.line)
                    ActionRow(Icons.Outlined.FileDownload, stringResource(R.string.game_data_import), onImport)
                    if (state.hasSave) {
                        Divider(color = PunicaTheme.line)
                        ActionRow(Icons.Outlined.Delete, stringResource(R.string.game_data_delete_save), onDeleteSave, danger = true)
                    }
                }
            }

            // its copies
            item { SectionTitle(stringResource(R.string.game_data_backups)) }
            if (state.backups.isEmpty()) {
                item { Muted(stringResource(R.string.game_data_no_backups)) }
            } else {
                items(state.backups, key = { it.folder.path }) { backup ->
                    Card {
                        Row(Modifier.fillMaxWidth().heightIn(min = PunicaDimens.ROW_MIN.dp).padding(start = PunicaDimens.MARGIN.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(relative(backup.time), fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text)
                                if (backup.reason.isNotEmpty()) Text(backup.reason, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted)
                            }
                            IconAction(Icons.Outlined.Restore, stringResource(R.string.game_data_restore)) { onRestore(backup) }
                            IconAction(Icons.Outlined.Delete, stringResource(R.string.game_data_delete_backup)) { onDeleteBackup(backup) }
                        }
                    }
                }
            }

            // the mods
            if (state.takesMods || state.takesTextures) {
                item { SectionTitle(stringResource(R.string.game_data_mods)) }
                if (state.mods.isEmpty()) item { Muted(stringResource(R.string.game_data_no_mods)) }
                items(state.mods, key = { it.folder.path }) { mod ->
                    Card {
                        Row(Modifier.fillMaxWidth().heightIn(min = PunicaDimens.ROW_MIN.dp).padding(start = PunicaDimens.MARGIN.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(mod.name, fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Medium, color = PunicaTheme.text)
                                Text(
                                    stringResource(if (mod.kind == GameData.Kind.TEXTURES) R.string.game_data_kind_textures else R.string.game_data_kind_mod),
                                    fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted,
                                )
                            }
                            Switch(
                                checked = mod.active,
                                onCheckedChange = { onToggleMod(mod) },
                                colors = SwitchDefaults.colors(checkedThumbColor = PunicaTheme.red, checkedTrackColor = PunicaTheme.red),
                            )
                            IconAction(Icons.Outlined.Delete, stringResource(R.string.game_data_delete_mod)) { onDeleteMod(mod) }
                        }
                    }
                }
                item {
                    Card {
                        if (state.takesMods) ActionRow(Icons.Outlined.Add, stringResource(R.string.game_data_install_mod), onClick = { onInstall(GameData.Kind.MOD) })
                        if (state.takesMods && state.takesTextures) Divider(color = PunicaTheme.line)
                        if (state.takesTextures) ActionRow(Icons.Outlined.Add, stringResource(R.string.texture_pack_install), onClick = { onInstall(GameData.Kind.TEXTURES) })
                    }
                }
                item { Muted(stringResource(R.string.game_data_mods_note)) }
            }
        }
    }
}

@Composable
private fun SectionTitle(text: String) {
    Text(
        text.uppercase(), fontFamily = PunicaTheme.display, fontWeight = FontWeight.ExtraBold, fontSize = 12.sp, lineHeight = 16.sp,
        color = PunicaTheme.textMuted, modifier = Modifier.padding(top = PunicaDimens.GAP_WIDE.dp, bottom = 4.dp),
    )
}

@Composable
private fun Muted(text: String) {
    Text(text, fontSize = 12.sp, lineHeight = 16.sp, color = PunicaTheme.textMuted, modifier = Modifier.padding(horizontal = 4.dp))
}

@Composable
private fun Card(content: @Composable () -> Unit) {
    val shape = RoundedCornerShape(PunicaDimens.CARD_RADIUS.dp)
    Column(Modifier.fillMaxWidth().clip(shape).background(PunicaTheme.card).border(1.dp, PunicaTheme.line, shape)) { content() }
}

/** A row of a card: icon on a 32 dp tile, title; 64 dp high, as the game sheet's rows. */
@Composable
private fun ActionRow(icon: ImageVector, title: String, onClick: () -> Unit, danger: Boolean = false) {
    Row(
        Modifier.fillMaxWidth().heightIn(min = PunicaDimens.ROW_MIN.dp).clickable(onClick = onClick).padding(horizontal = PunicaDimens.MARGIN.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(PunicaDimens.ROW_ICON.dp).clip(RoundedCornerShape(8.dp)).background(PunicaTheme.cardRaised), contentAlignment = Alignment.Center) {
            Icon(icon, contentDescription = null, tint = if (danger) PunicaTheme.redLight else PunicaTheme.text, modifier = Modifier.size(16.dp))
        }
        Spacer(Modifier.width(PunicaDimens.GAP_WIDE.dp))
        Text(title, fontSize = 14.sp, lineHeight = 20.sp, fontWeight = FontWeight.Medium, color = if (danger) PunicaTheme.redLight else PunicaTheme.text)
    }
}

@Composable
private fun IconAction(icon: ImageVector, description: String, onClick: () -> Unit) {
    Box(Modifier.size(PunicaDimens.TOUCH.dp).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(icon, contentDescription = description, tint = PunicaTheme.text, modifier = Modifier.size(PunicaDimens.ICON.dp))
    }
}

private fun relative(time: Long?): String =
    if (time == null) "" else DateUtils.getRelativeTimeSpanString(time, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE).toString()
