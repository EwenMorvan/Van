package com.van.management.utils;

import android.content.Context;
import android.content.SharedPreferences;

public class PreferencesManager {
    private static final String PREF_NAME = "van_preferences";
    private static final String KEY_AUTO_CONNECT = "auto_connect";
    private static final String KEY_NOTIFICATION_INTERVAL = "notification_interval";
    private static final String KEY_KEEP_SCREEN_ON = "keep_screen_on";
    private static final String KEY_DARK_THEME = "dark_theme";
    
    private SharedPreferences preferences;
    
    public PreferencesManager(Context context) {
        preferences = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
    }
    
    public boolean getAutoConnect() {
        return preferences.getBoolean(KEY_AUTO_CONNECT, true);
    }
    
    public void setAutoConnect(boolean autoConnect) {
        preferences.edit().putBoolean(KEY_AUTO_CONNECT, autoConnect).apply();
    }
    
    public int getNotificationInterval() {
        return preferences.getInt(KEY_NOTIFICATION_INTERVAL, 2000);
    }
    
    public void setNotificationInterval(int interval) {
        preferences.edit().putInt(KEY_NOTIFICATION_INTERVAL, interval).apply();
    }
    
    public boolean getKeepScreenOn() {
        return preferences.getBoolean(KEY_KEEP_SCREEN_ON, true);
    }
    
    public void setKeepScreenOn(boolean keepOn) {
        preferences.edit().putBoolean(KEY_KEEP_SCREEN_ON, keepOn).apply();
    }
    
    public boolean getDarkTheme() {
        return preferences.getBoolean(KEY_DARK_THEME, false);
    }
    
    public void setDarkTheme(boolean darkTheme) {
        preferences.edit().putBoolean(KEY_DARK_THEME, darkTheme).apply();
    }
}
