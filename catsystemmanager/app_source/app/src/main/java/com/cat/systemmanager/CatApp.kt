package com.cat.systemmanager

import android.app.Application
import android.content.Intent
import com.cat.systemmanager.service.OuterScreenService

class CatApp : Application() {
    override fun onCreate() {
        super.onCreate()
        // Start Outer screen & battery daemon service
        try {
            val serviceIntent = Intent(this, OuterScreenService::class.java)
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                startForegroundService(serviceIntent)
            } else {
                startService(serviceIntent)
            }
        } catch (_: Exception) {}
    }
}
