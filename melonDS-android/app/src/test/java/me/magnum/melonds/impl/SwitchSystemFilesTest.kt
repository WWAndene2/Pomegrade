package me.magnum.melonds.impl

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class SwitchSystemFilesTest {

    @get:Rule val temp = TemporaryFolder()

    @Test
    fun aNewNandDoesNotHoldTheFirmwareCopiedIntoTheFormerOne() {
        val firmware = temp.newFolder("Firmware")
        val ncas = listOf("a.nca", "b.nca").map { File(firmware, it).apply { writeText(it) } }
        val former = temp.newFolder("old", "nand", "registered")
        ncas.forEach { it.copyTo(File(former, it.name)) }
        val new = temp.newFolder("new", "nand", "registered")

        assertTrue(SwitchSystemFiles.holdsFirmware(former, ncas))
        assertFalse(SwitchSystemFiles.holdsFirmware(new, ncas))
    }

    @Test
    fun anNcaOfAnotherSizeIsNotHeld() {
        val firmware = temp.newFolder("Firmware")
        val nca = File(firmware, "a.nca").apply { writeText("new firmware") }
        val registered = temp.newFolder("registered")
        File(registered, "a.nca").writeText("old")
        assertFalse(SwitchSystemFiles.holdsFirmware(registered, listOf(nca)))
    }
}
