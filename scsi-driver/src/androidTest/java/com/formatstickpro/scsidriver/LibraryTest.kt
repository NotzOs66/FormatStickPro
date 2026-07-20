package com.formatstickpro.scsidriver

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class LibraryTest {

    @Test
    fun testLibraryLoadAndSignature() {
        val driver = SCSIDriver()
        
        // 1. Verificăm dacă semnătura poate fi citită (asta demonstrează că JNI și codul nativ C++ funcționează)
        val signature = driver.getAuthorSignature()
        assertNotNull("Semnătura nu ar trebui să fie null", signature)
        
        // 2. Verificăm dacă decodarea funcționează și returnează numele tău
        val decodedAuthor = driver.getDecodedAuthor()
        assertEquals("Șpac Dumitru", decodedAuthor)
        
        println("Testul a reușit! Biblioteca funcționează. Autor: $decodedAuthor")
    }
}
