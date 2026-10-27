package com.cat.systemmanager.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val CatYellow = Color(0xFFFFCC00)
val CatYellowDark = Color(0xFFD4A600)
val CyanAccent = Color(0xFF00E5FF)
val DarkBackground = Color(0xFF0D1117)
val CardBackground = Color(0xFF161B22)
val CardBorder = Color(0xFF30363D)
val TextPrimary = Color(0xFFF0F6FC)
val TextSecondary = Color(0xFF8B949E)
val PowerGreen = Color(0xFF2EA043)
val AlertRed = Color(0xFFDA3633)

private val DarkColorScheme = darkColorScheme(
    primary = CatYellow,
    onPrimary = Color.Black,
    secondary = CyanAccent,
    background = DarkBackground,
    surface = CardBackground,
    onBackground = TextPrimary,
    onSurface = TextPrimary
)

@Composable
fun CatTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        content = content
    )
}
