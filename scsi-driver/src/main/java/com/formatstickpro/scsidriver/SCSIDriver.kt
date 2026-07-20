package com.formatstickpro.scsidriver

/**
 * High-performance SCSI-over-USB Driver for Android.
 * Developed by Șpac Dumitru.
 */
class SCSIDriver {

    companion object {
        init {
            System.loadLibrary("scsidriver")
        }
    }

    /**
     * Initializes the driver using a file descriptor from UsbDeviceConnection.
     */
    external fun nativeInit(fd: Int)

    /**
     * Returns the encoded author signature.
     */
    external fun getAuthorSignature(): String

    /**
     * Forces a cache flush and SCSI SYNCHRONIZE CACHE command.
     */
    external fun nativeSync(): Int

    /**
     * Decode the signature to verify the author.
     */
    fun getDecodedAuthor(): String {
        return try {
            val data = android.util.Base64.decode(getAuthorSignature(), android.util.Base64.DEFAULT)
            String(data, Charsets.UTF_8)
        } catch (e: Exception) {
            "Unknown Author"
        }
    }
}
