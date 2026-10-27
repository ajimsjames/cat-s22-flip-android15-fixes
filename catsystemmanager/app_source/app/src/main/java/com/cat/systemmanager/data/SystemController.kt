package com.cat.systemmanager.data

import android.app.ActivityManager
import android.content.Context
import android.os.BatteryManager
import android.provider.Settings
import android.util.Log
import java.io.File

class SystemController(private val context: Context) {

    companion object {
        private const val TAG = "SystemController"
        private const val KEYBOARD_LIGHT_PATH = "/sys/class/leds/keyboard_light/brightness"
        private const val OUTER_BRIGHTNESS_CMD = "/dev/shm_outerscreen_cmd"
    }

    private fun writeSysfs(path: String, value: String): Boolean {
        try {
            val f = File(path)
            if (f.exists()) {
                val fos = java.io.FileOutputStream(f)
                fos.write("$value\n".toByteArray())
                fos.flush()
                fos.close()
                return true
            }
        } catch (e: Exception) {
            android.util.Log.e("SystemController", "Failed direct write to $path: ${e.message}")
        }

        try {
            val p = Runtime.getRuntime().exec(arrayOf("sh", "-c", "echo '$value' > '$path'"))
            p.waitFor()
            return p.exitValue() == 0
        } catch (e: Exception) {
            android.util.Log.e("SystemController", "Failed shell write to $path: ${e.message}")
        }

        return false
    }

    fun setKeypadLight(enabled: Boolean) {
        val valStr = if (enabled) "255" else "0"
        writeSysfs(KEYBOARD_LIGHT_PATH, valStr)
    }

    fun isKeypadLightOn(): Boolean {
        return try {
            val f = File(KEYBOARD_LIGHT_PATH)
            if (f.exists()) {
                val txt = f.readText().trim()
                txt.toIntOrNull()?.let { it > 0 } ?: false
            } else false
        } catch (_: Exception) {
            false
        }
    }

    fun setOuterScreenBrightness(brightness: Int) {
        val b = brightness.coerceIn(10, 255)
        writeSysfs(OUTER_BRIGHTNESS_CMD, "BRIGHTNESS=$b")
    }

    fun toggleOuterScreen() {
        writeSysfs(OUTER_BRIGHTNESS_CMD, "TOGGLE")
    }

    fun forceOuterScreenOn() {
        writeSysfs(OUTER_BRIGHTNESS_CMD, "FORCE_ON")
        writeSysfs("/sys/class/leds/lcd-backlight2/brightness", "255")
    }

    fun forceOuterScreenOff() {
        writeSysfs(OUTER_BRIGHTNESS_CMD, "FORCE_OFF")
        writeSysfs("/sys/class/leds/lcd-backlight2/brightness", "0")
    }

    fun setFlashlight(enabled: Boolean) {
        writeSysfs("/sys/class/leds/flashlight/brightness", if (enabled) "255" else "0")
    }

    fun applyPowerProfile(profile: PowerProfile) {
        val cr = context.contentResolver
        try {
            Settings.Global.putInt(cr, "low_power", profile.lowPowerValue)
            Settings.Global.putFloat(cr, Settings.Global.WINDOW_ANIMATION_SCALE, profile.animationScale)
            Settings.Global.putFloat(cr, Settings.Global.TRANSITION_ANIMATION_SCALE, profile.animationScale)
            Settings.Global.putFloat(cr, Settings.Global.ANIMATOR_DURATION_SCALE, profile.animationScale)

            val timeoutSec = when (profile) {
                PowerProfile.ULTRA -> 15000
                PowerProfile.ENDURANCE -> 30000
                PowerProfile.BALANCED -> 60000
                PowerProfile.PERFORMANCE -> 120000
            }
            Settings.System.putInt(cr, Settings.System.SCREEN_OFF_TIMEOUT, timeoutSec)

            when (profile) {
                PowerProfile.ULTRA -> {
                    for (c in 0..3) {
                        writeSysfs("/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor", "powersave")
                    }
                    writeSysfs("/sys/devices/system/cpu/cpu2/online", "0")
                    writeSysfs("/sys/devices/system/cpu/cpu3/online", "0")
                    setKeypadLight(false)
                }
                PowerProfile.ENDURANCE -> {
                    for (c in 0..3) {
                        writeSysfs("/sys/devices/system/cpu/cpu$c/online", "1")
                        writeSysfs("/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor", "conservative")
                    }
                }
                PowerProfile.BALANCED -> {
                    for (c in 0..3) {
                        writeSysfs("/sys/devices/system/cpu/cpu$c/online", "1")
                        writeSysfs("/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor", "schedutil")
                    }
                }
                PowerProfile.PERFORMANCE -> {
                    for (c in 0..3) {
                        writeSysfs("/sys/devices/system/cpu/cpu$c/online", "1")
                        writeSysfs("/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor", "performance")
                    }
                    setKeypadLight(true)
                }
            }

        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun cleanBackgroundMemory(): Long {
        val before = getMemoryMetrics().availRamMb
        try {
            writeSysfs("/proc/sys/vm/drop_caches", "3")
            val pm = context.packageManager
            val installedApps = pm.getInstalledApplications(0)
            val am = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            
            for (appInfo in installedApps) {
                val pkg = appInfo.packageName
                if (pkg == context.packageName ||
                    pkg == "io.github.sspanak.tt9" ||
                    pkg == "com.android.launcher3" ||
                    pkg.startsWith("com.android.inputmethod")
                ) {
                    continue
                }
                am.killBackgroundProcesses(pkg)
            }
        } catch (_: Exception) {}

        val after = getMemoryMetrics().availRamMb
        return (after - before).coerceAtLeast(0)
    }

    fun forceDeepSleep() {
        try {
            writeSysfs("/sys/power/state", "mem")
        } catch (_: Exception) {}
    }

    fun getBatteryMetrics(): BatteryMetrics {
        val bm = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager
        val level = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
        val currentMa = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW) / 1000
        val isCharging = bm.isCharging

        var voltageMv = 3800
        var tempC = 28.0f

        try {
            val voltStr = File("/sys/class/power_supply/battery/voltage_now").readText().trim()
            voltageMv = voltStr.toInt() / 1000
        } catch (_: Exception) {}

        try {
            val tempStr = File("/sys/class/power_supply/battery/temp").readText().trim()
            tempC = tempStr.toFloat() / 10.0f
        } catch (_: Exception) {}

        return BatteryMetrics(
            level = level.coerceIn(0, 100),
            voltageMv = voltageMv,
            currentNowMa = currentMa,
            temperatureC = tempC,
            isCharging = isCharging
        )
    }

    fun getMemoryMetrics(): MemoryMetrics {
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am.getMemoryInfo(memInfo)

        val totalMb = memInfo.totalMem / (1024 * 1024)
        val availMb = memInfo.availMem / (1024 * 1024)
        val usedMb = totalMb - availMb
        val usedPct = if (totalMb > 0) ((usedMb * 100) / totalMb).toInt() else 0

        return MemoryMetrics(
            totalRamMb = totalMb,
            availRamMb = availMb,
            usedRamMb = usedMb,
            usedPercent = usedPct
        )
    }

    fun getCpuThermal(): Float {
        val thermalFiles = listOf(
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/thermal/thermal_zone1/temp"
        )
        for (p in thermalFiles) {
            try {
                val f = File(p)
                if (f.exists()) {
                    val raw = f.readText().trim().toFloat()
                    return if (raw > 1000) raw / 1000f else raw
                }
            } catch (_: Exception) {}
        }
        return 38.0f
    }
}
