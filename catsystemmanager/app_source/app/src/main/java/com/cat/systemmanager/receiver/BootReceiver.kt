package com.cat.systemmanager.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.cat.systemmanager.service.OuterScreenService

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED) {
            val sIntent = Intent(context, OuterScreenService::class.java)
            context.startForegroundService(sIntent)
        }
    }
}
