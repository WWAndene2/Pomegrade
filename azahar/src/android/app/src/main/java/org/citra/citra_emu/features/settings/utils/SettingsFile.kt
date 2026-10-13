// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.utils

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.BufferedReader
import java.io.FileNotFoundException
import java.io.IOException
import java.io.InputStreamReader
import java.util.TreeMap
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.BooleanSetting
import org.citra.citra_emu.features.settings.model.FloatSetting
import org.citra.citra_emu.features.settings.model.IntListSetting
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.model.ScaledFloatSetting
import org.citra.citra_emu.features.settings.model.SettingSection
import org.citra.citra_emu.features.settings.model.Settings.SettingsSectionMap
import org.citra.citra_emu.features.settings.model.StringSetting
import org.citra.citra_emu.features.settings.ui.SettingsActivityView
import org.citra.citra_emu.utils.BiMap
import org.citra.citra_emu.utils.DirectoryInitialization.userDirectory
import org.citra.citra_emu.utils.Log
import org.ini4j.Wini

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"

    private var sectionsMap = BiMap<String?, String?>()

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param ini          The ini file to load the settings from
     * @param isCustomGame
     * @param view         The current view.
     * @return An Observable that emits a HashMap of the file's contents, then completes.
     */
    fun readFile(
        ini: DocumentFile,
        isCustomGame: Boolean,
        view: SettingsActivityView?
    ): HashMap<String, SettingSection?> {
        val sections: HashMap<String, SettingSection?> = SettingsSectionMap()
        var reader: BufferedReader? = null
        try {
            val context: Context = CitraApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            reader = BufferedReader(InputStreamReader(inputStream))
            var current: SettingSection? = null
            var line: String?
            while (reader.readLine().also { line = it } != null) {
                if (line!!.startsWith("[") && line!!.endsWith("]")) {
                    current = sectionFromLine(line!!, isCustomGame)
                    sections[current.name] = current
                } else if (current != null) {
                    val setting = settingFromLine(line!!)
                    if (setting != null) {
                        current.putSetting(setting)
                    }
                }
            }
        } catch (e: FileNotFoundException) {
            Log.error("[SettingsFile] File not found: " + ini.uri + e.message)
            view?.onSettingsFileNotFound()
        } catch (e: IOException) {
            Log.error("[SettingsFile] Error reading from: " + ini.uri + e.message)
            view?.onSettingsFileNotFound()
        } finally {
            if (reader != null) {
                try {
                    reader.close()
                } catch (e: IOException) {
                    Log.error("[SettingsFile] Error closing: " + ini.uri + e.message)
                }
            }
        }
        return sections
    }

    fun readFile(fileName: String, view: SettingsActivityView?): HashMap<String, SettingSection?> =
        readFile(getSettingsFile(fileName), false, view)

    fun readFile(fileName: String): HashMap<String, SettingSection?> = readFile(fileName, null)

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param gameId the id of the game to load it's settings.
     * @param view   The current view.
     */
    fun readCustomGameSettings(
        gameId: String,
        view: SettingsActivityView?
    ): HashMap<String, SettingSection?> =
        // Pomegrade: a game without settings of its own has no file yet
        getCustomGameSettingsFile(gameId)?.let { readFile(it, true, view) } ?: SettingsSectionMap()

    /**
     * Pomegrade: saves a game's settings, read by the core when that game starts (jni/config.cpp): only the
     * values that differ from config.ini, so a later change of the global settings still applies to every
     * setting the game doesn't set. A game whose values are all the global ones has its file deleted.
     */
    fun saveCustomGameSettings(
        gameId: String,
        sections: Map<String, SettingSection?>,
        view: SettingsActivityView
    ) {
        try {
            val context: Context = CitraApplication.appContext
            val global = HashMap<String, String>()
            context.contentResolver.openInputStream(getSettingsFile(FILE_NAME_CONFIG).uri)?.bufferedReader()?.useLines { lines ->
                var section = ""
                lines.map { it.trim() }.forEach { line ->
                    if (line.startsWith("[") && line.endsWith("]")) {
                        section = line.substring(1, line.length - 1)
                    } else if ('=' in line && !line.startsWith("#") && !line.startsWith(";")) {
                        global["$section/${line.substringBefore('=').trim()}"] = line.substringAfter('=').trim()
                    }
                }
            }
            val overrides = Wini()
            var count = 0
            for ((name, section) in sections) {
                section ?: continue
                for (setting in section.settings.values) {
                    val value = setting.valueAsString
                    val globalValue = global["$name/${setting.key}"]
                    // the same number written another way ("1" and "1.0") is the same value
                    val same = globalValue == value ||
                        (value.toDoubleOrNull() != null && value.toDoubleOrNull() == globalValue?.toDoubleOrNull())
                    if (!same) {
                        overrides.put(mapSectionNameToIni(name), setting.key, value)
                        count++
                    }
                }
            }
            val root = DocumentFile.fromTreeUri(context, Uri.parse(userDirectory))!!
            val directory = root.findFile("GameSettings") ?: root.createDirectory("GameSettings")!!
            val existing = directory.findFile("$gameId.ini")
            if (count == 0) {
                existing?.delete()
                return
            }
            val file = existing ?: directory.createFile("application/octet-stream", "$gameId.ini")!!
            context.contentResolver.openOutputStream(file.uri, "wt")!!.use { overrides.store(it) }
        } catch (e: Exception) {
            Log.error("[SettingsFile] Game settings not saved: $gameId.ini: ${e.message}")
            view.showToastMessage(
                CitraApplication.appContext.getString(R.string.error_saving, gameId, e.message),
                false
            )
        }
    }

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param sections The HashMap containing the Settings we want to serialize.
     * @param view     The current view.
     */
    fun saveFile(
        fileName: String,
        sections: TreeMap<String, SettingSection?>,
        view: SettingsActivityView
    ) {
        val ini = getSettingsFile(fileName)
        try {
            val context: Context = CitraApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            val writer = Wini(inputStream)
            val keySet: Set<String> = sections.keys
            for (key in keySet) {
                val section = sections[key]
                writeSection(writer, section!!)
            }
            inputStream!!.close()
            val outputStream = context.contentResolver.openOutputStream(ini.uri, "wt")
            writer.store(outputStream)
            outputStream!!.flush()
            outputStream.close()
        } catch (e: Exception) {
            Log.error("[SettingsFile] File not found: $fileName.ini: ${e.message}")
            view.showToastMessage(
                CitraApplication.appContext
                    .getString(R.string.error_saving, fileName, e.message),
                false
            )
        }
    }

    fun saveFile(fileName: String, setting: AbstractSetting) {
        val ini = getSettingsFile(fileName)
        try {
            val context: Context = CitraApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            val writer = Wini(inputStream)
            writer.put(setting.section, setting.key, setting.valueAsString)
            inputStream!!.close()
            val outputStream = context.contentResolver.openOutputStream(ini.uri, "wt")
            writer.store(outputStream)
            outputStream!!.flush()
            outputStream.close()
        } catch (e: Exception) {
            Log.error("[SettingsFile] File not found: $fileName.ini: ${e.message}")
        }
    }

    private fun mapSectionNameFromIni(generalSectionName: String): String? =
        if (sectionsMap.getForward(generalSectionName) != null) {
            sectionsMap.getForward(generalSectionName)
        } else {
            generalSectionName
        }

    private fun mapSectionNameToIni(generalSectionName: String): String =
        if (sectionsMap.getBackward(generalSectionName) != null) {
            sectionsMap.getBackward(generalSectionName).toString()
        } else {
            generalSectionName
        }

    fun getSettingsFile(fileName: String): DocumentFile {
        val root = DocumentFile.fromTreeUri(CitraApplication.appContext, Uri.parse(userDirectory))
        val configDirectory = root!!.findFile("config")
        return configDirectory!!.findFile("$fileName.ini")!!
    }

    private fun getCustomGameSettingsFile(gameId: String): DocumentFile? {
        val root = DocumentFile.fromTreeUri(CitraApplication.appContext, Uri.parse(userDirectory))
        val configDirectory = root?.findFile("GameSettings")
        return configDirectory?.findFile("$gameId.ini")
    }

    private fun sectionFromLine(line: String, isCustomGame: Boolean): SettingSection {
        var sectionName: String = line.substring(1, line.length - 1)
        if (isCustomGame) {
            sectionName = mapSectionNameToIni(sectionName)
        }
        return SettingSection(sectionName)
    }

    /**
     * For a line of text, determines what type of data is being represented, and returns
     * a Setting object containing this data.
     *
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private fun settingFromLine(line: String): AbstractSetting? {
        val splitLine = line.split("=".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
        if (splitLine.size != 2) {
            return null
        }
        val key = splitLine[0].trim { it <= ' ' }
        val value = splitLine[1].trim { it <= ' ' }
        if (value.isEmpty()) {
            return null
        }

        val booleanSetting = BooleanSetting.from(key)
        if (booleanSetting != null) {
            booleanSetting.boolean = value.toBoolean()
            return booleanSetting
        }

        val intSetting = IntSetting.from(key)
        if (intSetting != null) {
            try {
                intSetting.int = value.toInt()
            } catch (e: NumberFormatException) {
                intSetting.int = if (value.toBoolean()) 1 else 0
            }
            return intSetting
        }

        val scaledFloatSetting = ScaledFloatSetting.from(key)
        if (scaledFloatSetting != null) {
            scaledFloatSetting.float = value.toFloat() * scaledFloatSetting.scale
            return scaledFloatSetting
        }

        val floatSetting = FloatSetting.from(key)
        if (floatSetting != null) {
            floatSetting.float = value.toFloat()
            return floatSetting
        }

        val stringSetting = StringSetting.from(key)
        if (stringSetting != null) {
            stringSetting.string = value
            return stringSetting
        }

        val intListSetting = IntListSetting.from(key)
        if (intListSetting != null) {
            intListSetting.list = value.split(", ").map { it.toInt() }
        }

        return null
    }

    /**
     * Writes the contents of a Section HashMap to disk.
     *
     * @param parser  A Wini pointed at a file on disk.
     * @param section A section containing settings to be written to the file.
     */
    private fun writeSection(parser: Wini, section: SettingSection) {
        // Write the section header.
        val header = section.name

        // Write this section's values.
        val settings = section.settings
        val keySet: Set<String> = settings.keys
        for (key in keySet) {
            val setting = settings[key]
            parser.put(header, setting!!.key, setting.valueAsString)
        }
    }
}
