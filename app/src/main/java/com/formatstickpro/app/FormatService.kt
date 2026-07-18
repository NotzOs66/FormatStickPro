/*
 * Copyright (C) 2026 FormatStickPro (NotzOs66)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this <https://www.gnu.org/licenses/>.
 */

package com.formatstickpro.app

import android.app.*
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import androidx.annotation.Keep
import androidx.core.app.NotificationCompat
import kotlin.concurrent.thread

class FormatService : Service() {

    // Autor: "format Stick Pro creat de Șpac Dumitru"
    private val AUTH_TAG = "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1"

    init {
        System.loadLibrary("formatstickpro")
    }

    private val channelId = "FormatChannel"
    private val notificationId = 101
    private lateinit var notificationManager: NotificationManager
    private lateinit var builder: NotificationCompat.Builder
    private lateinit var mainPendingIntent: PendingIntent
    private var wakeLock: PowerManager.WakeLock? = null
    private var lastNotificationTime = 0L

    override fun onCreate() {
        super.onCreate()
        notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        createNotificationChannel()

        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "FormatStickPro::WakeLock")
        wakeLock?.acquire(30 * 60 * 1000L)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val usbDevice: UsbDevice? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent?.getParcelableExtra("usb_device", UsbDevice::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent?.getParcelableExtra("usb_device")
        }
        
        val clusterSize = intent?.getIntExtra("clusterSize", 4096) ?: 4096
        val partitionType = intent?.getIntExtra("partitionType", 0) ?: 0
        val fsType = intent?.getIntExtra("fsType", 0) ?: 0

        mainPendingIntent = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        builder = NotificationCompat.Builder(this, channelId)
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentTitle(getString(R.string.notif_title))
            .setContentText(getString(R.string.notif_preparing))
            .setPriority(NotificationCompat.PRIORITY_MAX) 
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setContentIntent(mainPendingIntent)
            .setOngoing(true)
            .setOnlyAlertOnce(true) 
            .setProgress(100, 0, false)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(notificationId, builder.build(), ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(notificationId, builder.build())
        }

        thread {
            var success = false
            var connection: android.hardware.usb.UsbDeviceConnection? = null
            try {
                if (usbDevice != null) {
                    val usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
                    connection = usbManager.openDevice(usbDevice)
                    
                    if (connection != null) {
                        val fd = connection.fileDescriptor
                        val path = usbDevice.deviceName
                        Log.i("FormatService", "Starting stable format... FD=$fd")
                        success = startStableFormat(fd, path, clusterSize, partitionType, fsType)
                    } else {
                        Log.e("FormatService", "Error: Could not open USB connection.")
                        reportStage(getString(R.string.error_usb_access))
                    }
                } else {
                    Log.e("FormatService", "Error: USB device is null.")
                }
            } catch (e: Exception) {
                Log.e("FormatService", "Fatal error in thread: ${e.message}")
            } finally {
                connection?.close()

                val finalBuilder = NotificationCompat.Builder(this, channelId)
                    .setSmallIcon(if (success) android.R.drawable.stat_sys_download_done else android.R.drawable.stat_notify_error)
                    .setContentTitle(if (success) getString(R.string.notif_success_title) else getString(R.string.notif_error_title))
                    .setContentText(if (success) getString(R.string.notif_success_text) else getString(R.string.notif_error_text))
                    .setPriority(NotificationCompat.PRIORITY_HIGH)
                    .setOngoing(false)
                    .setAutoCancel(true)
                    .setContentIntent(mainPendingIntent)

                stopForeground(STOP_FOREGROUND_DETACH)
                notificationManager.notify(notificationId, finalBuilder.build())

                val resIntent = Intent("FORMAT_RESULT").apply {
                    setPackage(packageName)
                    putExtra("success", success)
                }
                sendBroadcast(resIntent)

                wakeLock?.let { if (it.isHeld) it.release() }
                stopSelf()
            }
        }

        return START_NOT_STICKY
    }

    override fun onDestroy() {
        wakeLock?.let { if (it.isHeld) it.release() }
        super.onDestroy()
    }

    @Keep
    fun reportProgress(progress: Int) {
        val currentTime = System.currentTimeMillis()
        if (currentTime - lastNotificationTime < 500 && progress < 100) {
            sendUpdateToUI(progress, null, null)
            return
        }
        
        lastNotificationTime = currentTime
        try {
            builder.setProgress(100, progress, false)
            builder.setContentTitle(getString(R.string.notif_progress, progress))
            notificationManager.notify(notificationId, builder.build())
        } catch (e: Exception) { }
        
        sendUpdateToUI(progress, null, null)
    }

    @Keep
    fun reportStage(message: String) {
        val stage = translateMessage(message)
        if (stage.isEmpty()) return

        val currentTime = System.currentTimeMillis()
        if (currentTime - lastNotificationTime >= 1000) {
            lastNotificationTime = currentTime
            try {
                builder.setContentText(stage)
                notificationManager.notify(notificationId, builder.build())
            } catch (e: Exception) { }
        }
        
        sendUpdateToUI(null, stage, null)
    }

    private fun sendUpdateToUI(progress: Int?, stage: String?, error: String?) {
        val intent = Intent("FORMAT_PROGRESS").apply {
            setPackage(packageName)
            progress?.let { putExtra("progress", it) }
            stage?.let { putExtra("stage", it) }
            error?.let { putExtra("error", it) }
        }
        sendBroadcast(intent)
    }

    private fun translateMessage(msg: String): String {
        val cleanMsg = msg.trim().replace("\b", "").replace("\r", "")
        if (cleanMsg.isEmpty()) return ""

        return when {
            cleanMsg.contains("Initializing device", true) -> getString(R.string.stage_initializing)
            cleanMsg.contains("Creating NTFS", true) -> getString(R.string.stage_ntfs)
            cleanMsg.contains("mft record", true) -> getString(R.string.stage_mft)
            cleanMsg.contains("Boot sector", true) -> getString(R.string.stage_boot)
            cleanMsg.contains("Syncing", true) -> getString(R.string.stage_sync)
            cleanMsg.contains("cannot use compression", true) -> getString(R.string.stage_compression_info)
            cleanMsg.contains("completed successfully", true) -> getString(R.string.stage_success)
            cleanMsg.contains("Error", true) -> getString(R.string.stage_error, cleanMsg)
            else -> if (cleanMsg.length > 5) cleanMsg else ""
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                channelId, getString(R.string.notif_channel_name),
                NotificationManager.IMPORTANCE_LOW
            )
            notificationManager.createNotificationChannel(serviceChannel)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private external fun startStableFormat(fd: Int, usbPath: String, clusterSize: Int, partitionType: Int, fsType: Int): Boolean
}
