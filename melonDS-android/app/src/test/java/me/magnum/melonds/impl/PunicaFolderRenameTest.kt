package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Test

class PunicaFolderRenameTest {

    private val rewrite = PunicaFolderRename.rewriter(
        listOf(
            "primary%3APomegrade" to "primary%3APunica",
            "/storage/emulated/0/Pomegrade" to "/storage/emulated/0/Punica",
        )
    )

    @Test
    fun rewritesTreeAndDocumentUris() {
        val uri = "content://com.android.externalstorage.documents/tree/primary%3APomegrade/document/primary%3APomegrade%2FRoms%2FGame%2FGame.nds"
        assertEquals(
            "content://com.android.externalstorage.documents/tree/primary%3APunica/document/primary%3APunica%2FRoms%2FGame%2FGame.nds",
            rewrite(uri),
        )
    }

    @Test
    fun rewritesPathsInConfigFiles() {
        val ini = "nand_directory=/storage/emulated/0/Pomegrade/Switch/nand\nsdmc_directory=/storage/emulated/0/Pomegrade/Switch/sdmc\n"
        assertEquals("nand_directory=/storage/emulated/0/Punica/Switch/nand\nsdmc_directory=/storage/emulated/0/Punica/Switch/sdmc\n", rewrite(ini))
    }

    @Test
    fun rewritesTheFolderItselfAtTheEndOfAValue() {
        assertEquals("\"/storage/emulated/0/Punica\"", rewrite("\"/storage/emulated/0/Pomegrade\""))
        assertEquals("content://x/tree/primary%3APunica", rewrite("content://x/tree/primary%3APomegrade"))
    }

    @Test
    fun keepsOtherFoldersStartingWithTheSameName() {
        val other = "/storage/emulated/0/Pomegrade2/Roms primary%3APomegrade_old%2FRoms /storage/emulated/0/Pomegrade Backup"
        assertEquals(other, rewrite(other))
    }
}
