#include "psu_control.h"
#include <ui.h>
#include "config.h"
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"
#include "energy_meter.h"

float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ---------------------------------------------------------------------
   Shared setters. These are the ONE place a PSU setting actually changes --
   touchscreen callbacks, the serial console, and the web API all call
   into these instead of each having their own copy of "clamp, persist,
   send over CAN, log". That used to be duplicated three ways (touchscreen
   + serial already existed as separate copies; the web API would have
   been a third). --------------------------------------------------------- */

float psu_set_online_voltage(float v, bool persist) {
    v = clampf(v, PSU_VMIN, PSU_VMAX);
    saved_online_v = v;
    if (persist) save_float_pref("on_v", v);
    psu.setVoltage(v);

    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", v);
    ui_set_text_safe(ui_VarOnlineVout, buf);

    // persist=false means a high-frequency programmatic tick (the profile
    // executor) -- logging every one of those would spam the Settings log
    // for an entire profile's duration. Only log real setting changes.
    if (persist) log_to_settings("Online Vout set to " + String(v, 2) + " V");
    last_activity_time = millis();
    return v;
}

float psu_set_offline_voltage(float v) {
    v = clampf(v, PSU_VOFFLINE_MIN, PSU_VMAX);
    saved_offline_v = v;
    save_float_pref("off_v", v);
    psu.setOfflineVoltage(v);

    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", v);
    ui_set_text_safe(ui_VarOfflineVout, buf);

    log_to_settings("Offline Vout set to " + String(v, 2) + " V");
    last_activity_time = millis();
    return v;
}

float psu_set_online_current(float i, bool persist) {
    i = clampf(i, PSU_IMIN, PSU_IMAX);
    saved_online_i = i;
    if (persist) save_float_pref("on_i", i);
    psu.setCurrent(i);

    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", i);
    ui_set_text_safe(ui_VarOnlineIout, buf);

    if (persist) log_to_settings("Online Iout set to " + String(i, 2) + " A");
    last_activity_time = millis();
    return i;
}

float psu_set_offline_current(float i) {
    i = clampf(i, PSU_IMIN, PSU_IMAX);
    saved_offline_i = i;
    save_float_pref("off_i", i);
    psu.setOfflineCurrent(i);

    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", i);
    ui_set_text_safe(ui_VarOfflineIout, buf);

    log_to_settings("Offline Iout set to " + String(i, 2) + " A");
    last_activity_time = millis();
    return i;
}

void psu_set_output(bool on) {
    psu.enableOutput(on);
    saved_output_enable = on;
    save_bool_pref("out_en", on);

    suppress_switch_event = true;
    if (ui_Home_SwitchEnableOutput) {
        if (on) lv_obj_add_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
    }
    suppress_switch_event = false;

    if (ui_VarOutputState) {
        ui_set_text_safe(ui_VarOutputState, on ? "ON" : "OFF");
    }

    if (on && saved_use_timer && saved_timer_seconds > 0) {
        timer_remaining_seconds = saved_timer_seconds;
        timer_running = true;
        last_timer_tick_ms = millis();
    }
    if (!on) {
        timer_running = false;
    }

    log_to_settings(String("Output ") + (on ? "ENABLED" : "DISABLED"));
    last_activity_time = millis();
}

void psu_set_fan_manual(bool manual) {
    saved_fan_manual = manual;
    save_bool_pref("fan_man", manual);
    psu.setFanMode(manual);

    suppress_fan_event = true;
    if (ui_Settings_DropdownFanControll) {
        lv_dropdown_set_selected(ui_Settings_DropdownFanControll, manual ? 1 : 0);
    }
    suppress_fan_event = false;

    log_to_settings(String("Fan mode: ") + (manual ? "MANUAL" : "AUTO"));
    last_activity_time = millis();
}

void apply_saved_psu_settings() {
    psu.setVoltage(saved_online_v);
    delay(50);
    psu.setOfflineVoltage(saved_offline_v);
    delay(50);
    psu.setCurrent(saved_online_i);
    delay(50);
    psu.setOfflineCurrent(saved_offline_i);
    delay(50);

    psu.setFanMode(saved_fan_manual);
    delay(50);

    psu.enableOutput(saved_output_enable);
    delay(50);
}

void save_current_settings_to_ui() {
    char b[24];

    snprintf(b, sizeof(b), "%.2f", saved_online_v);
    ui_set_text_safe(ui_VarOnlineVout, b);

    snprintf(b, sizeof(b), "%.2f", saved_offline_v);
    ui_set_text_safe(ui_VarOfflineVout, b);

    snprintf(b, sizeof(b), "%.2f", saved_online_i);
    ui_set_text_safe(ui_VarOnlineIout, b);

    snprintf(b, sizeof(b), "%.2f", saved_offline_i);
    ui_set_text_safe(ui_VarOfflineIout, b);

    // NOTE: the brightness label/slider, sleep textarea, run-timer field +
    // checkbox, and energy display no longer exist in the current
    // SquareLine UI (Settings/SetValues were redesigned). The underlying
    // state (saved_brightness_pct, saved_sleep_sec, timer_*, energy_kwh)
    // still persists and still works headlessly / via the serial console --
    // there's just nothing left on screen to push it to. See beep.cpp,
    // psu_timer.cpp, energy_meter.cpp for the matching removals.

    update_energy_ui();

    // NOTE: called before psu_control_register_callbacks(), so
    // output_switch_cb isn't attached yet -- no need to suppress it here.
    if (ui_Home_SwitchEnableOutput) {
        if (saved_output_enable) lv_obj_add_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
    }

    if (ui_VarOutputState) {
        ui_set_text_safe(ui_VarOutputState, saved_output_enable ? "ON" : "OFF");
    }
}

static void apply_psu_settings_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!target) return;

    float val = ui_get_text_safe(target).toFloat();

    if (target == ui_VarOnlineVout) psu_set_online_voltage(val);
    else if (target == ui_VarOfflineVout) psu_set_offline_voltage(val);
    else if (target == ui_VarOnlineIout) psu_set_online_current(val);
    else if (target == ui_VarOfflineIout) psu_set_offline_current(val);
}

static void output_switch_cb(lv_event_t *e) {
    if (suppress_switch_event) return;

    lv_obj_t *sw = lv_event_get_target(e);
    if (!sw) return;

    psu_set_output(lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void fan_dropdown_cb(lv_event_t *e) {
    if (suppress_fan_event) return;

    lv_obj_t *dd = lv_event_get_target(e);
    if (!dd) return;

    uint16_t sel = lv_dropdown_get_selected(dd);
    psu_set_fan_manual(sel == 1); // 0=Auto, 1=Manual
}

void psu_control_register_callbacks() {
    if (ui_VarOnlineVout) {
        lv_obj_add_event_cb(ui_VarOnlineVout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_VarOnlineVout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOnlineVout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineVout) {
        lv_obj_add_event_cb(ui_VarOfflineVout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_VarOfflineVout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOfflineVout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOnlineIout) {
        lv_obj_add_event_cb(ui_VarOnlineIout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_VarOnlineIout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOnlineIout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineIout) {
        lv_obj_add_event_cb(ui_VarOfflineIout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_VarOfflineIout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOfflineIout, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Home_SwitchEnableOutput) {
        lv_obj_add_event_cb(ui_Home_SwitchEnableOutput, output_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Home_SwitchEnableOutput, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Settings_DropdownFanControll) {
        lv_dropdown_set_options(ui_Settings_DropdownFanControll, "Auto\nManual");

        suppress_fan_event = true;
        lv_dropdown_set_selected(ui_Settings_DropdownFanControll, saved_fan_manual ? 1 : 0);
        suppress_fan_event = false;

        lv_obj_add_event_cb(ui_Settings_DropdownFanControll, fan_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Settings_DropdownFanControll, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}
