package com.cat.systemmanager

import android.content.Context
import android.content.Intent
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Bundle
import android.view.KeyEvent
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.*
import com.cat.systemmanager.data.BatteryMetrics
import com.cat.systemmanager.data.MemoryMetrics
import com.cat.systemmanager.data.PowerProfile
import com.cat.systemmanager.data.SystemController
import com.cat.systemmanager.ui.CatTheme
import com.cat.systemmanager.ui.MainScreen
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity(), SensorEventListener {

    private lateinit var systemController: SystemController
    private lateinit var sensorManager: SensorManager

    private var currentPressure by mutableFloatStateOf(0.0f)
    private var selectedProfile by mutableStateOf(PowerProfile.BALANCED)
    private var isKeypadLightOn by mutableStateOf(false)
    private var outerBrightness by mutableIntStateOf(255)
    private var batteryState by mutableStateOf(BatteryMetrics())
    private var memoryState by mutableStateOf(MemoryMetrics())
    private var cpuTempState by mutableFloatStateOf(40.0f)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        systemController = SystemController(this)
        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager

        isKeypadLightOn = systemController.isKeypadLightOn()
        setupPressureSensor()
        handleIntentProfile(intent)

        setContent {
            CatTheme {
                LaunchedEffect(Unit) {
                    while (true) {
                        batteryState = systemController.getBatteryMetrics()
                        memoryState = systemController.getMemoryMetrics()
                        cpuTempState = systemController.getCpuThermal()
                        isKeypadLightOn = systemController.isKeypadLightOn()
                        delay(2000)
                    }
                }

                MainScreen(
                    battery = batteryState,
                    memory = memoryState,
                    cpuTemp = cpuTempState,
                    pressureHpa = currentPressure,
                    currentProfile = selectedProfile,
                    isKeypadLightOn = isKeypadLightOn,
                    outerBrightness = outerBrightness,
                    onProfileSelected = { profile ->
                        selectedProfile = profile
                        systemController.applyPowerProfile(profile)
                        try {
                            val intent = Intent(this@MainActivity, com.cat.systemmanager.service.OuterScreenService::class.java).apply {
                                putExtra("PROFILE", profile.name)
                            }
                            startService(intent)
                        } catch (_: Exception) {}
                        Toast.makeText(this@MainActivity, "Kernel Profile: ${profile.title}", Toast.LENGTH_SHORT).show()
                    },
                    onCleanRam = {
                        val freedMb = systemController.cleanBackgroundMemory()
                        memoryState = systemController.getMemoryMetrics()
                        Toast.makeText(this@MainActivity, "Kernel Dropped Caches! (+$freedMb MB)", Toast.LENGTH_SHORT).show()
                    },
                    onToggleKeypadLight = { enabled ->
                        isKeypadLightOn = enabled
                        systemController.setKeypadLight(enabled)
                    },
                    onOuterBrightnessChanged = { b ->
                        outerBrightness = b
                        systemController.setOuterScreenBrightness(b)
                    },
                    onTurnOnOuterScreen = {
                        systemController.forceOuterScreenOn()
                        Toast.makeText(this@MainActivity, "Outer Screen Triggered ON", Toast.LENGTH_SHORT).show()
                    }
                )
            }
        }
    }

    override fun onResume() {
        super.onResume()
        isKeypadLightOn = systemController.isKeypadLightOn()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntentProfile(intent)
    }

    private fun handleIntentProfile(intent: Intent?) {
        val profStr = intent?.getStringExtra("SET_PROFILE") ?: return
        try {
            val prof = PowerProfile.valueOf(profStr)
            selectedProfile = prof
            systemController.applyPowerProfile(prof)
            Toast.makeText(this, "Switched Profile -> ${prof.title}", Toast.LENGTH_SHORT).show()
        } catch (_: Exception) {}
    }

    private fun setupPressureSensor() {
        val baro = sensorManager.getDefaultSensor(Sensor.TYPE_PRESSURE)
        baro?.let {
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_UI)
        }
    }

    override fun onSensorChanged(event: SensorEvent?) {
        if (event?.sensor?.type == Sensor.TYPE_PRESSURE) {
            currentPressure = event.values[0]
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onDestroy() {
        sensorManager.unregisterListener(this)
        super.onDestroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER || keyCode == KeyEvent.KEYCODE_ENTER) {
            return super.onKeyDown(keyCode, event)
        }
        return super.onKeyDown(keyCode, event)
    }
}
