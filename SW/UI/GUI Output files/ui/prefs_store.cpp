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
    // Opening read-only FAILS (NOT_FOUND) if the "psuui" namespace has
    // never been written -- e.g. a truly first-ever boot, or after a full
    // chip erase / partition table change. That's not an error: the
    // saved_* globals already carry sane compile-time defaults (see
    // app_state.cpp), so just skip the reads and fall through to derive
    // the runtime timeout/timer state from those. Calling get*()/end() on
    // a Preferences object after a failed begin() corrupts NVS state and
    // crashes the boot, so this check isn't optional.
    if (preferences.begin("psuui", true)) {
        saved_online_v       = preferences.getFloat("on_v", saved_online_v);
        saved_offline_v      = preferences.getFloat("off_v", saved_offline_v);
        saved_online_i       = preferences.getFloat("on_i", saved_online_i);
        saved_offline_i      = preferences.getFloat("off_i", saved_offline_i);

        saved_brightness_pct = preferences.getInt("bright", saved_brightness_pct);
        saved_sleep_sec      = preferences.getInt("sleep", saved_sleep_sec);
        saved_dim_sec        = preferences.getInt("dim", max(3, saved_sleep_sec / 2));

        saved_output_enable  = preferences.getBool("out_en", saved_output_enable);
        saved_fan_manual     = preferences.getBool("fan_man", saved_fan_manual);

        saved_timer_seconds  = preferences.getUInt("timer_sec", saved_timer_seconds);
        saved_use_timer      = preferences.getBool("use_timer", saved_use_timer);

        energy_kwh            = preferences.getDouble("energy", energy_kwh);

        preferences.end();
    }

    saved_brightness_pct = constrain(saved_brightness_pct, 1, 100);
    saved_sleep_sec      = constrain(saved_sleep_sec, 5, 3600);
    saved_dim_sec        = constrain(saved_dim_sec, 3, saved_sleep_sec);

    dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;
    off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;

    timer_remaining_seconds = saved_timer_seconds;
    timer_running = (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable);
    last_timer_tick_ms = millis();
}
