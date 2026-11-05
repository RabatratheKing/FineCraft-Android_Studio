package com.example.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import android.content.res.Configuration
import androidx.compose.ui.platform.LocalConfiguration

private val DarkColorScheme =
  darkColorScheme(
    primary = AccentBlue,
    secondary = AccentIndigo,
    tertiary = AccentBlue,
    background = BackgroundDark,
    surface = BackgroundDark,
    onPrimary = TextPrimary,
    onSecondary = TextPrimary,
    onTertiary = TextPrimary,
    onBackground = TextPrimary,
    onSurface = TextPrimary,
  )

@Composable
fun MyApplicationTheme(
  darkTheme: Boolean = true, // Force dark theme for Immersive UI
  // Dynamic color is available on Android 12+
  dynamicColor: Boolean = false,
  content: @Composable () -> Unit,
) {
  val colorScheme = DarkColorScheme

  MaterialTheme(colorScheme = colorScheme, typography = Typography, content = content)
}
