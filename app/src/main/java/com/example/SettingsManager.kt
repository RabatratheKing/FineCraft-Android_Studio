package com.example

import android.content.Context
import android.content.SharedPreferences

class SettingsManager(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("finecraft_settings", Context.MODE_PRIVATE)

    var fov: Float
        get() = prefs.getFloat("fov", 75f)
        set(value) { prefs.edit().putFloat("fov", value).apply(); updateNative() }

    var sensitivity: Float
        get() = prefs.getFloat("sensitivity", 1.0f)
        set(value) { prefs.edit().putFloat("sensitivity", value).apply(); updateNative() }

    var invertY: Boolean
        get() = prefs.getBoolean("invertY", false)
        set(value) { prefs.edit().putBoolean("invertY", value).apply(); updateNative() }

    var renderDist: Int
        get() = prefs.getInt("renderDist", 5)
        set(value) { prefs.edit().putInt("renderDist", value).apply(); updateNative() }

    var graphicsQuality: Int
        get() = prefs.getInt("graphicsQuality", 1)
        set(value) { prefs.edit().putInt("graphicsQuality", value).apply(); updateNative() }

    var shadows: Boolean
        get() = prefs.getBoolean("shadows", true)
        set(value) { prefs.edit().putBoolean("shadows", value).apply(); updateNative() }

    var clouds: Boolean
        get() = prefs.getBoolean("clouds", true)
        set(value) { prefs.edit().putBoolean("clouds", value).apply(); updateNative() }

    var fog: Boolean
        get() = prefs.getBoolean("fog", true)
        set(value) { prefs.edit().putBoolean("fog", value).apply(); updateNative() }

    var viewBobbing: Boolean
        get() = prefs.getBoolean("viewBobbing", true)
        set(value) { prefs.edit().putBoolean("viewBobbing", value).apply(); updateNative() }

    var brightness: Float
        get() = prefs.getFloat("brightness", 1.0f)
        set(value) { prefs.edit().putFloat("brightness", value).apply(); updateNative() }

    var uiScale: Float
        get() = prefs.getFloat("uiScale", 1.0f)
        set(value) { prefs.edit().putFloat("uiScale", value).apply() }

    var controlOpacity: Float
        get() = prefs.getFloat("controlOpacity", 0.65f)
        set(value) { prefs.edit().putFloat("controlOpacity", value).apply() }

    var crosshairSize: Float
        get() = prefs.getFloat("crosshairSize", 1.0f)
        set(value) { prefs.edit().putFloat("crosshairSize", value).apply() }

    fun updateNative() {
        MainActivity.nativeUpdateSettings(fov, sensitivity, invertY, renderDist, graphicsQuality, shadows, clouds, fog, brightness, viewBobbing)
    }
}
