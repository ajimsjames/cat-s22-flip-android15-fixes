package com.cat.systemmanager.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.cat.systemmanager.data.BatteryMetrics
import com.cat.systemmanager.data.MemoryMetrics
import com.cat.systemmanager.data.PowerProfile

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    battery: BatteryMetrics,
    memory: MemoryMetrics,
    cpuTemp: Float,
    pressureHpa: Float,
    currentProfile: PowerProfile,
    isKeypadLightOn: Boolean,
    onProfileSelected: (PowerProfile) -> Unit,
    onCleanRam: () -> Unit,
    onToggleKeypadLight: (Boolean) -> Unit,
    outerBrightness: Int = 255,
    onOuterBrightnessChanged: (Int) -> Unit = {},
    onTurnOnOuterScreen: () -> Unit = {}
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            modifier = Modifier
                                .size(12.dp)
                                .background(CatYellow, RoundedCornerShape(2.dp))
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "CAT KERNEL CONTROL",
                            fontWeight = FontWeight.Black,
                            fontSize = 17.sp,
                            letterSpacing = 1.sp,
                            color = CatYellow
                        )
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = DarkBackground,
                    titleContentColor = CatYellow
                )
            )
        },
        containerColor = DarkBackground
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 12.dp, vertical = 6.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // 1. Kernel Power Profile Selector
            SectionHeader(title = "KERNEL POWER PROFILES", icon = Icons.Default.Bolt)
            PowerProfileGrid(
                selectedProfile = currentProfile,
                onProfileSelected = onProfileSelected
            )

            // 2. Hardware Hardware Controls (Keypad Backlight & Outer Screen Brightness)
            SectionHeader(title = "HARDWARE LEDS & DISPLAY", icon = Icons.Default.Lightbulb)
            HardwareControlsCard(
                isKeypadLightOn = isKeypadLightOn,
                onToggleKeypadLight = onToggleKeypadLight,
                outerBrightness = outerBrightness,
                onOuterBrightnessChanged = onOuterBrightnessChanged,
                onTurnOnOuterScreen = onTurnOnOuterScreen
            )

            // 3. Battery Backup Hub
            SectionHeader(title = "BATTERY HEALTH & DRAW", icon = Icons.Default.BatteryChargingFull)
            BatteryCard(battery = battery)

            // 4. Memory Optimization Terminal
            SectionHeader(title = "SYSTEM RAM & CACHE", icon = Icons.Default.Memory)
            MemoryCard(memory = memory, onCleanRam = onCleanRam)

            // 5. Sensor Telemetry Grid
            SectionHeader(title = "HARDWARE SENSORS", icon = Icons.Default.Sensors)
            SensorTelemetryCard(cpuTemp = cpuTemp, pressureHpa = pressureHpa)

            Spacer(modifier = Modifier.height(16.dp))
        }
    }
}

@Composable
fun HardwareControlsCard(
    isKeypadLightOn: Boolean,
    onToggleKeypadLight: (Boolean) -> Unit,
    outerBrightness: Int,
    onOuterBrightnessChanged: (Int) -> Unit,
    onTurnOnOuterScreen: () -> Unit
) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = CardBackground),
        border = BorderStroke(1.dp, CardBorder)
    ) {
        Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            // Keypad Backlight Switch
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text(
                        text = "Keypad Backlight",
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                    Text(
                        text = "Physical keyboard LED illuminator",
                        fontSize = 10.sp,
                        color = TextSecondary
                    )
                }
                Switch(
                    checked = isKeypadLightOn,
                    onCheckedChange = onToggleKeypadLight,
                    colors = SwitchDefaults.colors(
                        checkedThumbColor = CatYellow,
                        checkedTrackColor = CardBorder,
                        uncheckedThumbColor = TextSecondary,
                        uncheckedTrackColor = DarkBackground
                    )
                )
            }

            Divider(color = CardBorder, thickness = 0.5.dp)

            // Outer Screen Brightness Slider
            Column {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "Outer Display Brightness",
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                    Text(
                        text = "${((outerBrightness / 255f) * 100).toInt()}%",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = CatYellow
                    )
                }
                Slider(
                    value = outerBrightness.toFloat(),
                    onValueChange = { onOuterBrightnessChanged(it.toInt()) },
                    valueRange = 25f..255f,
                    colors = SliderDefaults.colors(
                        thumbColor = CatYellow,
                        activeTrackColor = CatYellow,
                        inactiveTrackColor = DarkBackground
                    )
                )
            }

            // Button to Wake / Turn On Outer Screen
            Button(
                onClick = onTurnOnOuterScreen,
                colors = ButtonDefaults.buttonColors(containerColor = CatYellow),
                shape = RoundedCornerShape(4.dp),
                modifier = Modifier.fillMaxWidth().height(42.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Tv,
                    contentDescription = "Turn On Outer Screen",
                    tint = DarkBackground,
                    modifier = Modifier.size(16.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = "WAKE / TURN ON OUTER SCREEN",
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = DarkBackground
                )
            }
        }
    }
}

@Composable
fun SectionHeader(title: String, icon: ImageVector) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(top = 4.dp, bottom = 2.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = CatYellow,
            modifier = Modifier.size(16.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = title,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            color = TextSecondary,
            letterSpacing = 0.5.sp
        )
    }
}

@Composable
fun PowerProfileGrid(
    selectedProfile: PowerProfile,
    onProfileSelected: (PowerProfile) -> Unit
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        PowerProfile.entries.chunked(2).forEach { rowProfiles ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                rowProfiles.forEach { profile ->
                    val isSelected = profile == selectedProfile
                    val interactionSource = remember { MutableInteractionSource() }
                    val isFocused by interactionSource.collectIsFocusedAsState()

                    val targetBg = when {
                        isSelected -> CatYellow.copy(alpha = 0.15f)
                        isFocused -> CardBackground.copy(alpha = 0.9f)
                        else -> CardBackground
                    }
                    val animatedBg by animateColorAsState(targetValue = targetBg, label = "card_bg")

                    val targetBorder = when {
                        isSelected -> CatYellow
                        isFocused -> CatYellow.copy(alpha = 0.5f)
                        else -> CardBorder
                    }
                    val animatedBorder by animateColorAsState(targetValue = targetBorder, label = "card_border")

                    Card(
                        modifier = Modifier
                            .weight(1f)
                            .focusable(interactionSource = interactionSource)
                            .clickable { onProfileSelected(profile) },
                        shape = RoundedCornerShape(8.dp),
                        colors = CardDefaults.cardColors(containerColor = animatedBg),
                        border = BorderStroke(1.dp, animatedBorder)
                    ) {
                        Column(
                            modifier = Modifier.padding(10.dp)
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.SpaceBetween,
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Text(
                                    text = profile.title,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 13.sp,
                                    color = if (isSelected) CatYellow else TextPrimary
                                )
                                if (isSelected) {
                                    Icon(
                                        imageVector = Icons.Default.CheckCircle,
                                        contentDescription = "Active",
                                        tint = CatYellow,
                                        modifier = Modifier.size(14.dp)
                                    )
                                }
                            }
                            Spacer(modifier = Modifier.height(2.dp))
                            Text(
                                text = profile.subtitle,
                                fontSize = 10.sp,
                                color = TextSecondary,
                                lineHeight = 12.sp
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun BatteryCard(battery: BatteryMetrics) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = CardBackground),
        border = BorderStroke(1.dp, CardBorder)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "${battery.level}%",
                        fontSize = 32.sp,
                        fontWeight = FontWeight.Black,
                        fontFamily = FontFamily.Monospace,
                        color = if (battery.level <= 15) AlertRed else TextPrimary
                    )
                    Text(
                        text = if (battery.isCharging) "STATUS: CHARGING" else "STATUS: DISCHARGING",
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold,
                        color = if (battery.isCharging) PowerGreen else TextSecondary
                    )
                }

                Column(horizontalAlignment = Alignment.End) {
                    Text(
                        text = "${if (battery.isCharging) "+" else "-"}${Math.abs(battery.currentNowMa)} mA",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        color = if (battery.isCharging) PowerGreen else CatYellow
                    )
                    Text(
                        text = "${battery.voltageMv / 1000f} V",
                        fontSize = 12.sp,
                        color = TextSecondary,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }

            LinearProgressIndicator(
                progress = { battery.level / 100f },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(6.dp),
                color = if (battery.level <= 15) AlertRed else CatYellow,
                trackColor = DarkBackground,
            )
        }
    }
}

@Composable
fun MemoryCard(memory: MemoryMetrics, onCleanRam: () -> Unit) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = CardBackground),
        border = BorderStroke(1.dp, CardBorder)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "${memory.usedPercent}% USED",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        color = TextPrimary
                    )
                    Text(
                        text = "${memory.usedRamMb} MB / ${memory.totalRamMb} MB",
                        fontSize = 11.sp,
                        color = TextSecondary,
                        fontFamily = FontFamily.Monospace
                    )
                }

                Button(
                    onClick = onCleanRam,
                    colors = ButtonDefaults.buttonColors(containerColor = CatYellow),
                    shape = RoundedCornerShape(4.dp),
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.CleaningServices,
                        contentDescription = "Drop Caches",
                        tint = DarkBackground,
                        modifier = Modifier.size(14.dp)
                    )
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(
                        text = "PURGE RAM",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = DarkBackground
                    )
                }
            }

            LinearProgressIndicator(
                progress = { memory.usedPercent / 100f },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(6.dp),
                color = CatYellow,
                trackColor = DarkBackground,
            )
        }
    }
}

@Composable
fun SensorTelemetryCard(cpuTemp: Float, pressureHpa: Float) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // CPU Temperature
        Card(
            modifier = Modifier.weight(1f),
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = CardBackground),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(modifier = Modifier.padding(10.dp)) {
                Text(
                    text = "CPU THERMAL",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextSecondary
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "${cpuTemp.toInt()} °C",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = if (cpuTemp >= 48f) AlertRed else CatYellow
                )
            }
        }

        // Barometric Pressure
        Card(
            modifier = Modifier.weight(1f),
            shape = RoundedCornerShape(8.dp),
            colors = CardDefaults.cardColors(containerColor = CardBackground),
            border = BorderStroke(1.dp, CardBorder)
        ) {
            Column(modifier = Modifier.padding(10.dp)) {
                Text(
                    text = "BAROMETER",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextSecondary
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "${pressureHpa.toInt()} hPa",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    color = PowerGreen
                )
            }
        }
    }
}
