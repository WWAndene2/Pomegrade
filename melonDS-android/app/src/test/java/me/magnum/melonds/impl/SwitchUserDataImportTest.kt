package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class SwitchUserDataImportTest {

    @get:Rule val temp = TemporaryFolder()

    private val edenIni = """
        [Data%20Storage]
        nand_directory\default = false
        nand_directory = /storage/emulated/0/Android/data/dev.eden.eden_emulator/files/nand
        sdmc_directory\default = false
        sdmc_directory = /storage/emulated/0/Android/data/dev.eden.eden_emulator/files/sdmc
        dump_directory = /storage/emulated/0/Android/data/dev.eden.eden_emulator/files/dump
    """.trimIndent()

    @Test
    fun anImportedConfigNamesTheLayoutsFolders() {
        val result = SwitchUserDataImport.withDataFolders(edenIni, mapOf("nand_directory" to "/P/Switch/System/nand", "sdmc_directory" to "/P/Switch/System/sdmc"))
        assertEquals(
            """
            [Data%20Storage]
            nand_directory\default=false
            nand_directory=/P/Switch/System/nand
            sdmc_directory\default=false
            sdmc_directory=/P/Switch/System/sdmc
            dump_directory = /storage/emulated/0/Android/data/dev.eden.eden_emulator/files/dump
            """.trimIndent(),
            result,
        )
    }

    @Test
    fun withoutTheLayoutTheFoldersAreEdensDefaults() {
        val result = SwitchUserDataImport.withDataFolders(edenIni, mapOf("nand_directory" to null))
        assertTrue(result.contains("nand_directory\\default=true\nnand_directory=\n"))
    }

    @Test
    fun edensLiveFolderIsNeverDeleted() {
        val live = temp.newFolder("files")
        assertFalse(SwitchUserDataImport.isDeletable(File(live, "nand"), live))
        assertFalse(SwitchUserDataImport.isDeletable(live, File(live, "nand")))
        assertTrue(SwitchUserDataImport.isDeletable(File(live, "nand"), temp.newFolder("Switch")))
        assertTrue(SwitchUserDataImport.isDeletable(File(live, "nand"), null))
    }
}
