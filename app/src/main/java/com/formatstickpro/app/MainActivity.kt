package com.formatstickpro.app

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
 */

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.ContextCompat
import androidx.core.os.LocaleListCompat
import androidx.core.view.isGone

import android.net.Uri
import androidx.core.content.FileProvider
import java.io.File
import java.io.FileOutputStream
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    // Autor: "format Stick Pro creat de Șpac Dumitru"
    private val AUTHOR_ID = "Zm9ybWF0IFN0aWNrIFBybyBjcmVhdCBkZSDFn3BhYyBEdW1pdHJ1"

    private val actionUsbPermission = "com.formatstickpro.app.USB_PERMISSION"
    private lateinit var usbManager: UsbManager
    private var myUsbDevice: UsbDevice? = null

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                actionUsbPermission -> {
                    synchronized(this) {
                        val device: UsbDevice? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                        } else {
                            @Suppress("DEPRECATION")
                            intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                        }
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.let {
                                myUsbDevice = it
                                setupUsbReady()
                            }
                        } else {
                            Toast.makeText(context, getString(R.string.permission_denied), Toast.LENGTH_SHORT).show()
                        }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    scanForUsbDevices()
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val device: UsbDevice? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    }
                    if (device?.deviceId == myUsbDevice?.deviceId) {
                        resetUsbState()
                    }
                }
            }
        }
    }

    private fun setupUsbReady() {
        val textStatus = findViewById<TextView>(R.id.textStatus)
        findViewById<Button>(R.id.btnFormat).isEnabled = true
        textStatus.text = getString(R.string.status_ready_to_format, myUsbDevice?.productName ?: "USB")
        textStatus.setTextColor(-0x1f1f1f) // #E0E0E0
        findViewById<RelativeLayout>(R.id.layoutProgress).isGone = true
    }

    private fun resetUsbState() {
        myUsbDevice = null
        val textStatus = findViewById<TextView>(R.id.textStatus)
        findViewById<Button>(R.id.btnFormat).isEnabled = false
        textStatus.text = getString(R.string.status_disconnected)
        textStatus.setTextColor(-0x1f1f1f) // #E0E0E0
        findViewById<RelativeLayout>(R.id.layoutProgress).isGone = true
    }

    private fun scanForUsbDevices() {
        val deviceList = usbManager.deviceList
        val device = deviceList.values.firstOrNull { dev ->
            (0 until dev.interfaceCount).any { i ->
                dev.getInterface(i).interfaceClass == 8 // Mass Storage
            }
        }

        if (device != null) {
            myUsbDevice = device
            if (usbManager.hasPermission(device)) {
                setupUsbReady()
            } else {
                findViewById<TextView>(R.id.textStatus).text = getString(R.string.status_device_found, device.productName ?: "USB")
                val intent = Intent(actionUsbPermission).apply { setPackage(packageName) }
                val flags = if (Build.VERSION.SDK_INT >= 34) {
                    PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT
                } else {
                    PendingIntent.FLAG_MUTABLE
                }
                val permissionIntent = PendingIntent.getBroadcast(this, 0, intent, flags)
                usbManager.requestPermission(device, permissionIntent)
            }
        } else {
            resetUsbState()
        }
    }

    private val formatReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                "FORMAT_RESULT" -> {
                    val success = intent.getBooleanExtra("success", false)
                    val currentStatus = findViewById<TextView>(R.id.textStatus)
                    val btnFormat = findViewById<Button>(R.id.btnFormat)
                    val btnSendError = findViewById<Button>(R.id.btnSendErrorLogs)
                    
                    currentStatus.text = if (success) getString(R.string.status_format_success) else getString(R.string.status_format_error)
                    currentStatus.setTextColor(if (success) -0xff1a01 else -0xadad) // Cyan (#00E5FF) vs Red (#FF5252)

                    btnFormat.isEnabled = true
                    btnSendError.visibility = if (success) View.GONE else View.VISIBLE

                    // Refresh device info to be ready for another format without unplugging
                    scanForUsbDevices()
                    
                    val layoutProgress = findViewById<RelativeLayout>(R.id.layoutProgress)
                    val imgStick = findViewById<ImageView>(R.id.imgStick)
                    val progressBar = findViewById<ProgressBar>(R.id.progressBar)

                    // Reset to base state
                    layoutProgress.visibility = View.VISIBLE
                    layoutProgress.alpha = 1f
                    
                    // Stop animations and clear rotation
                    imgStick.animate().cancel()
                    imgStick.rotation = 0f
                    progressBar.visibility = View.INVISIBLE
                    
                    // Change image with overshoot animation
                    imgStick.scaleX = 0f
                    imgStick.scaleY = 0f
                    imgStick.setImageResource(if (success) R.drawable.ic_success else R.drawable.ic_error)
                    
                    imgStick.animate()
                        .scaleX(1.0f)
                        .scaleY(1.0f)
                        .setDuration(500)
                        .setInterpolator(android.view.animation.OvershootInterpolator())
                        .start()
                }
                "FORMAT_PROGRESS" -> {
                    val progress = intent.getIntExtra("progress", -1)
                    val stage = intent.getStringExtra("stage")
                    val error = intent.getStringExtra("error")
                    
                    val currentStatus = findViewById<TextView>(R.id.textStatus)
                    val textPercentage = findViewById<TextView>(R.id.textPercentage)
                    val layoutProgress = findViewById<RelativeLayout>(R.id.layoutProgress)
                    val progressBar = findViewById<ProgressBar>(R.id.progressBar)

                    if (layoutProgress.isGone && (error == null)) {
                        layoutProgress.visibility = View.VISIBLE
                        layoutProgress.alpha = 0f
                        layoutProgress.animate().alpha(1f).duration = 500
                    }

                    if (error != null) {
                        currentStatus.text = getString(R.string.status_error_generic, error)
                        currentStatus.setTextColor(-0x10000) // Color.RED
                        layoutProgress.isGone = true
                    } else {
                        if (progress != -1) {
                            textPercentage.text = getString(R.string.progress_percentage, progress)
                            progressBar.progress = progress
                            
                            val imgStick = findViewById<ImageView>(R.id.imgStick)
                            imgStick.animate().cancel()
                            imgStick.animate()
                                .scaleX(1.2f).scaleY(1.2f)
                                .rotation(if ((progress % 2) == 0) 5f else -5f)
                                .setDuration(300)
                                .withEndAction {
                                    imgStick.animate().scaleX(1.0f).scaleY(1.0f).rotation(0f).duration = 300
                                }
                        }
                        
                        if (stage != null) {
                            currentStatus.text = getString(R.string.status_stage, stage)
                            currentStatus.setTextColor(-0x1f1f1f) // #E0E0E0
                        }
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val textStatus = findViewById<TextView>(R.id.textStatus)
        textStatus.movementMethod = ScrollingMovementMethod.getInstance()

        val btnFormat = findViewById<Button>(R.id.btnFormat)
        val btnLang = findViewById<Button>(R.id.btnLang)
        val btnAbout = findViewById<ImageButton>(R.id.btnAbout)
        usbManager = getSystemService(USB_SERVICE) as UsbManager

        btnLang.setOnClickListener {
            toggleLanguage()
        }

        btnAbout.setOnClickListener {
            showAboutDialog()
        }

        findViewById<ImageButton>(R.id.btnShareLogs).setOnClickListener {
            shareLogcat()
        }

        findViewById<Button>(R.id.btnSendErrorLogs).setOnClickListener {
            shareLogcat()
        }

        // Register receivers
        val filter = IntentFilter().apply {
            addAction(actionUsbPermission)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        val resFilter = IntentFilter().apply {
            addAction("FORMAT_RESULT")
            addAction("FORMAT_PROGRESS")
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, RECEIVER_NOT_EXPORTED)
            registerReceiver(formatReceiver, resFilter, RECEIVER_NOT_EXPORTED)
        } else {
            ContextCompat.registerReceiver(this, usbReceiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
            ContextCompat.registerReceiver(this, formatReceiver, resFilter, ContextCompat.RECEIVER_NOT_EXPORTED)
        }

        // First scan at startup
        scanForUsbDevices()

        // Listen for selection changes to reset UI for next format
        val onSpinnerChange = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p0: AdapterView<*>?, p1: View?, p2: Int, p3: Long) {
                if (findViewById<Button>(R.id.btnFormat).isEnabled) {
                    resetUIForNextFormat()
                }
            }
            override fun onNothingSelected(p0: AdapterView<*>?) {}
        }
        findViewById<Spinner>(R.id.spinnerFs).onItemSelectedListener = onSpinnerChange
        findViewById<Spinner>(R.id.spinnerPartition).onItemSelectedListener = onSpinnerChange
        findViewById<Spinner>(R.id.spinnerCluster).onItemSelectedListener = onSpinnerChange

        btnFormat.setOnClickListener {
            // Explicitly request notification permission for Android 13+
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 1)
                    return@setOnClickListener
                }
            }

            btnFormat.isEnabled = false
            findViewById<Button>(R.id.btnSendErrorLogs).visibility = View.GONE
            textStatus.text = getString(R.string.status_starting)
            textStatus.setTextColor(-0x1f1f1f) // #E0E0E0
            
            val layoutProgress = findViewById<RelativeLayout>(R.id.layoutProgress)
            val imgStick = findViewById<ImageView>(R.id.imgStick)
            val progressBar = findViewById<ProgressBar>(R.id.progressBar)
            
            findViewById<TextView>(R.id.textPercentage).text = getString(R.string.progress_percentage, 0)
            
            // Reset visual state for new format
            imgStick.setImageResource(R.drawable.ic_usb_stick)
            imgStick.scaleX = 1f
            imgStick.scaleY = 1f
            progressBar.visibility = View.VISIBLE
            
            layoutProgress.visibility = View.VISIBLE
            layoutProgress.alpha = 0f
            layoutProgress.animate().alpha(1f).duration = 500
            
            // Start pulsing animation for the stick immediately
            imgStick.animate().scaleX(1.2f).scaleY(1.2f).duration = 1000
            imgStick.animate().scaleX(1.0f).scaleY(1.0f).duration = 1000

            val clusterSize = findViewById<Spinner>(R.id.spinnerCluster).selectedItem.toString().toInt()
            val partitionType = findViewById<Spinner>(R.id.spinnerPartition).selectedItemPosition
            val fsType = findViewById<Spinner>(R.id.spinnerFs).selectedItemPosition

            val serviceIntent = Intent(this, FormatService::class.java).apply {
                putExtra("usb_device", myUsbDevice)
                putExtra("clusterSize", clusterSize)
                putExtra("partitionType", partitionType)
                putExtra("fsType", fsType)
            }
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                startForegroundService(serviceIntent)
            } else {
                startService(serviceIntent)
            }
            
            textStatus.text = getString(R.string.status_check_notification)
        }
    }

    private fun toggleLanguage() {
        val currentLocales = AppCompatDelegate.getApplicationLocales()
        val newLocale = if (currentLocales.isEmpty || currentLocales.toLanguageTags().contains("ro")) {
            LocaleListCompat.forLanguageTags("en")
        } else {
            LocaleListCompat.forLanguageTags("ro")
        }
        AppCompatDelegate.setApplicationLocales(newLocale)
    }

    private fun resetUIForNextFormat() {
        val layoutProgress = findViewById<RelativeLayout>(R.id.layoutProgress)
        if (layoutProgress.visibility == View.VISIBLE && !findViewById<ProgressBar>(R.id.progressBar).isShown) {
            layoutProgress.animate().alpha(0f).setDuration(300).withEndAction {
                layoutProgress.visibility = View.GONE
                layoutProgress.alpha = 1f
            }
            findViewById<TextView>(R.id.textStatus).text = getString(R.string.status_ready_to_format, myUsbDevice?.productName ?: "USB")
        }
    }

    private fun showAboutDialog() {
        val aboutMessage = StringBuilder()
            .append(getString(R.string.about_description)).append("\n\n")
            .append(getString(R.string.about_legal_notice)).append("\n\n")
            .append("<b>").append(getString(R.string.about_libraries_title)).append("</b>\n")
            .append(getString(R.string.about_libraries_content))

        val dialog = androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle(R.string.about_title)
            .setMessage(android.text.Html.fromHtml(aboutMessage.toString(), android.text.Html.FROM_HTML_MODE_LEGACY))
            .setPositiveButton(R.string.btn_close, null)
            .show()

        // Această linie face ca link-ul să fie clickabil și să deschidă aplicația de Telegram
        dialog.findViewById<TextView>(android.R.id.message)?.movementMethod = android.text.method.LinkMovementMethod.getInstance()
    }

    private fun shareLogcat() {
        thread {
            try {
                val logFile = File(cacheDir, "formatstickpro_logs.txt")
                val process = Runtime.getRuntime().exec("logcat -d")
                val reader = process.inputStream.bufferedReader()
                val writer = FileOutputStream(logFile).bufferedWriter()
                
                reader.useLines { lines ->
                    lines.forEach { line ->
                        writer.write(line)
                        writer.newLine()
                    }
                }
                writer.close()

                val uri = FileProvider.getUriForFile(this, "${packageName}.provider", logFile)
                
                // Revenim la o metodă mai universală care funcționează pe toate telefoanele
                val intent = Intent(Intent.ACTION_SEND).apply {
                    // Tipul MIME pentru un mesaj de mail cu atașament
                    type = "message/rfc822"
                    
                    // Adăugăm detaliile necesare
                    putExtra(Intent.EXTRA_EMAIL, arrayOf("formatstickpro.pulse261@passinbox.com"))
                    putExtra(Intent.EXTRA_SUBJECT, "FormatStickPro Error Logs - Build 2026")
                    putExtra(Intent.EXTRA_TEXT, "Log-urile aplicatiei atasate pentru diagnosticare.\n\nSuport Telegram: https://t.me/NotzOs66")
                    putExtra(Intent.EXTRA_STREAM, uri)
                    
                    // Acordăm permisiuni de citire pentru fișier
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
                
                runOnUiThread {
                    try {
                        // Afișăm meniul de selecție (Gmail, Yahoo, Outlook, etc.)
                        startActivity(Intent.createChooser(intent, getString(R.string.logs_share_title)))
                    } catch (e: Exception) {
                        // Dacă chiar nu există nicio aplicație capabilă (foarte rar)
                        Toast.makeText(this, "Nu am găsit o aplicație de mail.", Toast.LENGTH_SHORT).show()
                    }
                }
            } catch (e: Exception) {
                runOnUiThread {
                    Toast.makeText(this, getString(R.string.logs_error_capture), Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(usbReceiver)
        unregisterReceiver(formatReceiver)
    }
}
