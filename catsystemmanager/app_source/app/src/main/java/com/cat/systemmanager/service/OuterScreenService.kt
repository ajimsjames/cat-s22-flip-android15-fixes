package com.cat.systemmanager.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import android.util.Log
import androidx.core.app.NotificationCompat
import com.cat.systemmanager.data.PowerProfile
import com.cat.systemmanager.data.SystemController
import com.cat.systemmanager.presentation.OuterFramebuffer
import com.cat.systemmanager.presentation.OuterPresentation

class OuterScreenService : Service(), SensorEventListener {

    companion object {
        private const val TAG = "OuterScreenService"
    }

    private lateinit var sensorManager: SensorManager
    private lateinit var systemController: SystemController
    private lateinit var powerManager: PowerManager
    private var wakeLock: PowerManager.WakeLock? = null

    private lateinit var terminalView: OuterPresentation.CleanTerminalView
    private lateinit var renderBitmap: Bitmap
    private lateinit var renderCanvas: Canvas
    private val outerFb = OuterFramebuffer()

    private val handler = Handler(Looper.getMainLooper())
    private var currentProfile = PowerProfile.BALANCED

    private val tickerRunnable = object : Runnable {
        override fun run() {
            refreshOuterScreen()
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate() {
        super.onCreate()
        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        systemController = SystemController(this)

        renderBitmap = Bitmap.createBitmap(OuterFramebuffer.WIDTH, OuterFramebuffer.HEIGHT, Bitmap.Config.ARGB_8888)
        renderCanvas = Canvas(renderBitmap)
        terminalView = OuterPresentation.CleanTerminalView(this)
        terminalView.measure(
            android.view.View.MeasureSpec.makeMeasureSpec(OuterFramebuffer.WIDTH, android.view.View.MeasureSpec.EXACTLY),
            android.view.View.MeasureSpec.makeMeasureSpec(OuterFramebuffer.HEIGHT, android.view.View.MeasureSpec.EXACTLY)
        )
        terminalView.layout(0, 0, OuterFramebuffer.WIDTH, OuterFramebuffer.HEIGHT)

        try {
            wakeLock = powerManager.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK,
                "CatSystemManager:OuterScreenWakeLock"
            ).apply {
                acquire(24 * 60 * 60 * 1000L)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to acquire wakelock", e)
        }

        startForegroundServiceNotification()
        handler.post(tickerRunnable)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        intent?.getStringExtra("PROFILE")?.let {
            try {
                currentProfile = PowerProfile.valueOf(it)
                refreshOuterScreen()
            } catch (_: Exception) {}
        }
        return START_STICKY
    }

    private fun startForegroundServiceNotification() {
        val channelId = "cat_power_service"
        val channel = NotificationChannel(
            channelId,
            "CAT System Daemon",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Maintains outer display and battery guard"
        }
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(channel)

        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("CAT System Active")
            .setContentText("Sub-screen & Power Guard active")
            .setSmallIcon(android.R.drawable.ic_lock_idle_charging)
            .setOngoing(true)
            .build()

        startForeground(1001, notification)
    }

    override fun onSensorChanged(event: SensorEvent?) {}
    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    private fun refreshOuterScreen() {
        try {
            val bat = systemController.getBatteryMetrics()
            val mem = systemController.getMemoryMetrics()
            terminalView.setData(bat, mem, currentProfile)
            terminalView.draw(renderCanvas)
            outerFb.render(renderBitmap)
        } catch (e: Exception) {
            Log.e(TAG, "Error refreshing outer screen", e)
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(tickerRunnable)
        try {
            if (wakeLock?.isHeld == true) wakeLock?.release()
        } catch (_: Exception) {}
        outerFb.close()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
