#include "prefs_store.h"
#include <Preferences.h>
#include "app_state.h"
#include "config.h"

static Preferences preferences;

void save_float_pref(const char *key, float val) {
    preferences.begin("psuui", false);
    preferences.putFloat(key, val);
    preferences.end();
}

void save_int_pref(const char *key, int val) {
    preferences.begin("psuui", false);
    preferences.putInt(key, val);
    preferences.end();
}

void save_uint_pref(const char *key, uint32_t val) {
    preferences.begin("psuui", false);
    preferences.putUInt(key, val);
    preferences.end();
}

void save_bool_pref(const char *key, bool val) {
    preferences.begin("psuui", false);
    preferences.putBool(key, val);
    preferences.end();
}

void save_double_pref(const char *key, double val) {
    preferences.begin("psuui", false);
    preferences.putDouble(key, val);
    preferences.end();
}

void prefs_load_all() {
    preferences.begin("psuui", true);

    saved_online_v       = preferences.getFloat("on_v", 53.50f);
    saved_offline_v      = preferences.getFloat("off_v", 53.50f);
    saved_online_i       = preferences.getFloat("on_i", 10.00f);
    saved_offline_i      = preferences.getFloat("off_i", 10.00f);

    saved_brightness_pct = preferences.getInt("bright", 100);
    saved_sleep_sec      = preferences.getInt("sleep", 30);
    saved_dim_sec        = preferences.getInt("dim", saved_sleep_sec > 0 ? max(3, saved_sleep_sec / 2) : 0);

    saved_output_enable  = preferences.getBool("out_en", true);
    saved_fan_manual     = preferences.getBool("fan_man", false);

    saved_timer_seconds  = preferences.getUInt("timer_sec", 0);
    saved_use_timer      = preferences.getBool("use_timer", false);

    energy_kwh            = preferences.getDouble("energy", 0.0);

    preferences.end();

    saved_brightness_pct = constrain(saved_brightness_pct, 1, 100);
    saved_sleep_sec      = constrain(saved_sleep_sec, SLEEP_SEC_MIN, SLEEP_SEC_MAX);
    // 0 == "never sleep" (ui_SliderSleepTimer's minimum); disable dimming too
    // in that case rather than leaving the screen dim forever.
    saved_dim_sec        = (saved_sleep_sec > 0) ? constrain(saved_dim_sec, 3, saved_sleep_sec) : 0;

    dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;
    off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;

    timer_remaining_seconds = saved_timer_seconds;
    timer_running = (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable);
    last_timer_tick_ms = millis();
}
