package com.cat.systemmanager.presentation

import android.app.Presentation
import android.content.Context
import android.graphics.*
import android.os.Bundle
import android.os.SystemClock
import android.view.Display
import android.view.View
import android.view.WindowManager
import com.cat.systemmanager.data.BatteryMetrics
import com.cat.systemmanager.data.MemoryMetrics
import com.cat.systemmanager.data.PowerProfile
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class OuterPresentation(
    outerContext: Context,
    display: Display
) : Presentation(outerContext, display) {

    private lateinit var terminalView: CleanTerminalView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        window?.let { win ->
            val lp = win.attributes
            lp.screenBrightness = 0.15f // 15% hardware brightness override
            win.attributes = lp

            win.addFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN or
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                        WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
            )
        }

        terminalView = CleanTerminalView(context)
        setContentView(terminalView)
    }

    fun updateMetrics(battery: BatteryMetrics, memory: MemoryMetrics, profile: PowerProfile) {
        if (::terminalView.isInitialized) {
            terminalView.setData(battery, memory, profile)
        }
    }

    class CleanTerminalView(context: Context) : View(context) {

        private var batteryMetrics = BatteryMetrics()
        private var memoryMetrics = MemoryMetrics()
        private var currentProfile = PowerProfile.BALANCED
        private var tickCounter = 0

        // Balanced, crisp phosphor palette (Optimal contrast, legible without blinding glare)
        private val colBg = Color.parseColor("#000000")
        private val colGreen = Color.parseColor("#00D95A")      // Vivid, readable phosphor green
        private val colDimGreen = Color.parseColor("#008F3B")   // Medium-contrast legible green
        private val colDarkGreen = Color.parseColor("#002B11")  // Clean dark bar track
        private val colCyan = Color.parseColor("#00C8E0")       // Crisp, legible cyber cyan
        private val colAmber = Color.parseColor("#D4A500")      // Crisp warm amber
        private val colRed = Color.parseColor("#D42B2B")        // Crisp alert red
        private val colBorder = Color.parseColor("#00421A")

        private val bgPaint = Paint().apply {
            color = colBg
            style = Paint.Style.FILL
        }

        private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colBorder
            strokeWidth = 1f
            style = Paint.Style.STROKE
        }

        // Crisp flat typography (setShadowLayer completely removed)
        private val fontTitle = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colDimGreen
            typeface = Typeface.MONOSPACE
            textSize = 9f
        }

        private val fontClock = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colGreen
            typeface = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
            textSize = 21f
        }

        private val fontDataBold = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colGreen
            typeface = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
            textSize = 10f
        }

        private val fontDataNormal = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colGreen
            typeface = Typeface.MONOSPACE
            textSize = 9.5f
        }

        private val fontDataDim = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = colDimGreen
            typeface = Typeface.MONOSPACE
            textSize = 9f
        }

        private val segActivePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL
        }

        private val segInactivePaint = Paint().apply {
            color = colDarkGreen
            style = Paint.Style.FILL
        }

        private val timeFmt = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
        private val dateFmt = SimpleDateFormat("EEE dd MMM", Locale.getDefault())

        fun setData(bat: BatteryMetrics, mem: MemoryMetrics, profile: PowerProfile) {
            this.batteryMetrics = bat
            this.memoryMetrics = mem
            this.currentProfile = profile
            tickCounter++
            invalidate()
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)

            // 1. Pure OLED Black Canvas
            canvas.drawRect(0f, 0f, 128f, 128f, bgPaint)

            // 2. Crisp Tech Outer Frame
            canvas.drawRect(1f, 1f, 127f, 127f, borderPaint)
            canvas.drawLine(1f, 14f, 127f, 14f, borderPaint)

            // Top Header: Network / OS & Profile Tag
            val blinkCursor = if (tickCounter % 2 == 0) "_" else " "
            canvas.drawText(">CAT S22$blinkCursor", 4f, 11f, fontTitle)

            val modeStr = when (currentProfile) {
                PowerProfile.ULTRA -> "[ECO]"
                PowerProfile.ENDURANCE -> "[END]"
                PowerProfile.BALANCED -> "[BAL]"
                PowerProfile.PERFORMANCE -> "[MAX]"
            }
            fontTitle.color = if (currentProfile == PowerProfile.ULTRA) colAmber else colCyan
            canvas.drawText(modeStr, 96f, 11f, fontTitle)

            // 3. Crisp Flat Digital Time & Date
            val now = Date()
            val timeText = timeFmt.format(now) // "15:37:42"
            fontClock.color = colGreen
            canvas.drawText(timeText, 4f, 32f, fontClock)

            val dateText = dateFmt.format(now).uppercase() // "SUN 25 OCT"
            fontDataDim.color = colDimGreen
            canvas.drawText(dateText, 4f, 43f, fontDataDim)

            canvas.drawLine(1f, 47f, 127f, 47f, borderPaint)

            // 4. BATTERY ROW + 14-Segment Micro-Bar
            val chargeSign = if (batteryMetrics.isCharging) "+" else "-"
            val currentMa = Math.abs(batteryMetrics.currentNowMa)
            val batPercent = "${batteryMetrics.level}%"
            val batRate = "${chargeSign}${currentMa}mA"

            fontDataBold.color = if (batteryMetrics.level <= 20) colRed else colGreen
            canvas.drawText("BAT $batPercent", 4f, 57f, fontDataBold)

            fontDataDim.color = if (batteryMetrics.isCharging) colCyan else colDimGreen
            canvas.drawText(batRate, 76f, 57f, fontDataDim)

            drawSegmentedBar(
                canvas = canvas,
                x = 4f,
                y = 61f,
                width = 120f,
                height = 4f,
                fraction = batteryMetrics.level / 100f,
                activeColor = if (batteryMetrics.level <= 20) colRed else colGreen
            )

            // 5. RAM ROW + 14-Segment Micro-Bar
            val ramUsedMb = memoryMetrics.usedRamMb
            val ramTotalMb = memoryMetrics.totalRamMb
            val ramPct = "${memoryMetrics.usedPercent}%"
            val ramMbText = "${ramUsedMb}M/${ramTotalMb}M"

            fontDataBold.color = colCyan
            canvas.drawText("RAM $ramPct", 4f, 74f, fontDataBold)

            fontDataDim.color = colDimGreen
            canvas.drawText(ramMbText, 66f, 74f, fontDataDim)

            drawSegmentedBar(
                canvas = canvas,
                x = 4f,
                y = 78f,
                width = 120f,
                height = 4f,
                fraction = memoryMetrics.usedPercent / 100f,
                activeColor = colCyan
            )

            canvas.drawLine(1f, 86f, 127f, 86f, borderPaint)

            // 6. DEEP HARDWARE TELEMETRY GRID (4 Data Points)
            // Voltage & Battery Temp
            val voltText = String.format(Locale.US, "V:%.2fV", batteryMetrics.voltageMv / 1000f)
            val tempText = String.format(Locale.US, "T:%.0f°C", batteryMetrics.temperatureC)
            fontDataNormal.color = colGreen
            canvas.drawText(voltText, 4f, 96f, fontDataNormal)
            canvas.drawText(tempText, 70f, 96f, fontDataNormal)

            // CPU Load & System Uptime
            val uptimeHours = SystemClock.elapsedRealtime() / (1000 * 3600)
            val uptimeMins = (SystemClock.elapsedRealtime() / (1000 * 60)) % 60
            val upText = String.format(Locale.US, "UP:%dh%02dm", uptimeHours, uptimeMins)

            val cpuLoad = readCpuLoad()
            val loadText = "CPU:$cpuLoad"

            fontDataDim.color = colDimGreen
            canvas.drawText(loadText, 4f, 107f, fontDataDim)
            canvas.drawText(upText, 70f, 107f, fontDataDim)

            // 7. FOOTER STATUS
            canvas.drawLine(1f, 112f, 127f, 112f, borderPaint)
            val footerText = if (batteryMetrics.isCharging) {
                "> STATUS: CHARGING [AC/USB]"
            } else {
                "> STATUS: RUNNING NORMAL"
            }
            fontTitle.color = if (batteryMetrics.isCharging) colCyan else colDimGreen
            canvas.drawText(footerText, 4f, 122f, fontTitle)
        }

        private fun drawSegmentedBar(
            canvas: Canvas,
            x: Float,
            y: Float,
            width: Float,
            height: Float,
            fraction: Float,
            activeColor: Int
        ) {
            val totalSegments = 15
            val gap = 1.5f
            val segWidth = (width - (gap * (totalSegments - 1))) / totalSegments
            val activeSegments = (fraction * totalSegments).toInt().coerceIn(0, totalSegments)

            segActivePaint.color = activeColor

            for (i in 0 until totalSegments) {
                val segX = x + i * (segWidth + gap)
                val paint = if (i < activeSegments) segActivePaint else segInactivePaint
                canvas.drawRect(segX, y, segX + segWidth, y + height, paint)
            }
        }

        private fun readCpuLoad(): String {
            return try {
                val line = File("/proc/loadavg").readText()
                val parts = line.split(" ")
                if (parts.isNotEmpty()) parts[0] else "0.5"
            } catch (_: Exception) {
                "0.5"
            }
        }
    }
}
