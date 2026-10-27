package com.cat.systemmanager.data

enum class PowerProfile(
    val title: String,
    val subtitle: String,
    val lowPowerValue: Int,
    val animationScale: Float,
    val aggressiveDoze: Boolean
) {
    ULTRA(
        title = "Ultra Saver",
        subtitle = "Zero animations, instant sleep on flip, max battery",
        lowPowerValue = 1,
        animationScale = 0.0f,
        aggressiveDoze = true
    ),
    ENDURANCE(
        title = "Endurance",
        subtitle = "50% animations, quick Doze, throttled background",
        lowPowerValue = 0,
        animationScale = 0.5f,
        aggressiveDoze = true
    ),
    BALANCED(
        title = "Balanced",
        subtitle = "Standard Android behavior, normal Doze",
        lowPowerValue = 0,
        animationScale = 1.0f,
        aggressiveDoze = false
    ),
    PERFORMANCE(
        title = "Performance",
        subtitle = "Full clock speeds, unrestricted background tasks",
        lowPowerValue = 0,
        animationScale = 1.0f,
        aggressiveDoze = false
    )
}

data class BatteryMetrics(
    val level: Int = 0,
    val isCharging: Boolean = false,
    val currentNowMa: Int = 0,
    val voltageMv: Int = 0,
    val temperatureC: Float = 0.0f,
    val health: String = "Good",
    val chargeCounterUah: Int = 0
)

data class MemoryMetrics(
    val totalRamMb: Long = 0,
    val availRamMb: Long = 0,
    val usedRamMb: Long = 0,
    val usedPercent: Int = 0,
    val swapTotalMb: Long = 0,
    val swapFreeMb: Long = 0
)

data class HardwareMetrics(
    val cpuFrequencies: List<String> = emptyList(),
    val cpuTempC: Float = 0.0f,
    val pressureHpa: Float = 0.0f,
    val lightLux: Float = 0.0f,
    val stepCount: Int = 0
)
