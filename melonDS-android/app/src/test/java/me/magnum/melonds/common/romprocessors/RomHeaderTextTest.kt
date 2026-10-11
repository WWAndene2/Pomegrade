package me.magnum.melonds.common.romprocessors

import org.junit.Assert.assertEquals
import org.junit.Test

class RomHeaderTextTest {

    private fun header(text: String, size: Int = 16) = ByteArray(size).also { text.toByteArray(Charsets.US_ASCII).copyInto(it) }

    @Test
    fun readsTheFieldUpToItsFirstNul() {
        assertEquals("POKEMON EMER", RomHeaderText.ascii(header("POKEMON EMER\u0000XYZ"), 0, 16))
    }

    @Test
    fun readsAFieldAtItsOffset() {
        assertEquals("01", RomHeaderText.ascii(header("TITLE\u0000\u0000\u000001"), 8, 2))
    }

    @Test
    fun dropsNonPrintableBytesAndTrims() {
        val bytes = byteArrayOf(' '.code.toByte(), 'A'.code.toByte(), 0x07, 0x93.toByte(), 'B'.code.toByte(), ' '.code.toByte())
        assertEquals("AB", RomHeaderText.ascii(bytes, 0, bytes.size))
    }

    @Test
    fun anEmptyFieldGivesAnEmptyTitle() {
        assertEquals("", RomHeaderText.ascii(ByteArray(12), 0, 12))
    }
}
