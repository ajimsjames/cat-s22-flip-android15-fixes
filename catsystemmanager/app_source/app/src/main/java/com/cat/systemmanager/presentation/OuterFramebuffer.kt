package com.cat.systemmanager.presentation

import android.graphics.Bitmap
import android.util.Log
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel

class OuterFramebuffer {
    companion object {
        private const val TAG = "OuterFramebuffer"
        const val WIDTH = 128
        const val HEIGHT = 128
        private const val SHM_PATH = "/dev/shm_outerscreen"
        private const val BL_PATH = "/sys/class/leds/lcd-backlight2/brightness"
        private const val MAGIC_HEADER = 0xCA705C22.toInt()
    }

    private var shmFile: RandomAccessFile? = null
    private var shmChannel: FileChannel? = null
    
    // Pixel buffer for 128x128 32-bit (65536 bytes)
    private val pixelBuffer: ByteBuffer = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4).order(ByteOrder.LITTLE_ENDIAN)
    // Target buffer with 4-byte header prepended (65536 + 4 = 65540 bytes)
    private val targetBuffer: ByteBuffer = ByteBuffer.allocateDirect(WIDTH * HEIGHT * 4 + 4).order(ByteOrder.LITTLE_ENDIAN)

    init {
        openSharedBuffer()
    }

    private fun openSharedBuffer(): Boolean {
        try {
            val file = File(SHM_PATH)
            if (!file.exists()) {
                Log.w(TAG, "$SHM_PATH does not exist yet")
                return false
            }
            shmFile = RandomAccessFile(file, "rw")
            shmChannel = shmFile?.channel
            Log.i(TAG, "Shared outer screen buffer opened successfully")
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open $SHM_PATH", e)
            return false
        }
    }

    @Synchronized
    fun render(bitmap: Bitmap) {
        try {
            if (shmChannel == null || shmChannel?.isOpen != true) {
                if (!openSharedBuffer()) return
            }

            pixelBuffer.rewind()
            bitmap.copyPixelsToBuffer(pixelBuffer)
            pixelBuffer.rewind()

            targetBuffer.rewind()
            targetBuffer.putInt(MAGIC_HEADER)
            targetBuffer.put(pixelBuffer)
            targetBuffer.rewind()

            shmChannel?.position(0)
            shmChannel?.write(targetBuffer)
        } catch (e: Exception) {
            Log.e(TAG, "Error writing to shared outer buffer", e)
        }
    }

    fun setBacklight(brightness: Int) {
        try {
            val blFile = File(BL_PATH)
            if (blFile.exists() && blFile.canWrite()) {
                blFile.writeText("$brightness\n")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error setting backlight: $brightness", e)
        }
    }

    fun close() {
        try {
            shmChannel?.close()
            shmFile?.close()
        } catch (_: Exception) {}
        shmChannel = null
        shmFile = null
    }
}
