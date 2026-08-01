#include "psu_control.h"
#include <ui.h>
#include "config.h"
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"
#include "energy_meter.h"
#include "psu_timer.h"

float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
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

    snprintf(b, sizeof(b), "%d%%", saved_brightness_pct);
    ui_set_text_safe(ui_Settings_LabelBrightness, b);

    snprintf(b, sizeof(b), "%d", saved_sleep_sec);
    ui_set_text_safe(ui_Settings_TextAreaSleep, b);

    ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());

    if (ui_Settings_SliderBrightness && obj_is_slider(ui_Settings_SliderBrightness)) {
        lv_slider_set_value(ui_Settings_SliderBrightness, saved_brightness_pct, LV_ANIM_OFF);
    }

    update_energy_ui();

    if (ui_SetValues_CheckboxUseTimer) {
        if (saved_use_timer) lv_obj_add_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
    }

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

    String text = ui_get_text_safe(target);
    float val = text.toFloat();
    char buf[24];

    if (target == ui_VarOnlineVout) {
        float clamped = clampf(val, PSU_VMIN, PSU_VMAX);
        saved_online_v = clamped;
        save_float_pref("on_v", saved_online_v);
        psu.setVoltage(saved_online_v);
        snprintf(buf, sizeof(buf), "%.2f", saved_online_v);
        ui_set_text_safe(ui_VarOnlineVout, buf);
        log_to_settings("Online Vout set to " + String(saved_online_v, 2) + " V");
    }

    else if (target == ui_VarOfflineVout) {
        float clamped = clampf(val, PSU_VOFFLINE_MIN, PSU_VMAX);
        saved_offline_v = clamped;
        save_float_pref("off_v", saved_offline_v);
        psu.setOfflineVoltage(saved_offline_v);
        snprintf(buf, sizeof(buf), "%.2f", saved_offline_v);
        ui_set_text_safe(ui_VarOfflineVout, buf);
        log_to_settings("Offline Vout set to " + String(saved_offline_v, 2) + " V");
    }

    else if (target == ui_VarOnlineIout) {
        float clamped = clampf(val, PSU_IMIN, PSU_IMAX);
        saved_online_i = clamped;
        save_float_pref("on_i", saved_online_i);
        psu.setCurrent(saved_online_i);
        snprintf(buf, sizeof(buf), "%.2f", saved_online_i);
        ui_set_text_safe(ui_VarOnlineIout, buf);
        log_to_settings("Online Iout set to " + String(saved_online_i, 2) + " A");
    }

    else if (target == ui_VarOfflineIout) {
        float clamped = clampf(val, PSU_IMIN, PSU_IMAX);
        saved_offline_i = clamped;
        save_float_pref("off_i", saved_offline_i);
        psu.setOfflineCurrent(saved_offline_i);
        snprintf(buf, sizeof(buf), "%.2f", saved_offline_i);
        ui_set_text_safe(ui_VarOfflineIout, buf);
        log_to_settings("Offline Iout set to " + String(saved_offline_i, 2) + " A");
    }

    last_activity_time = millis();
}

static void output_switch_cb(lv_event_t *e) {
    if (suppress_switch_event) return;

    lv_obj_t *sw = lv_event_get_target(e);
    if (!sw) return;

    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    psu.enableOutput(checked);
    saved_output_enable = checked;
    save_bool_pref("out_en", saved_output_enable);

    if (ui_VarOutputState) {
        ui_set_text_safe(ui_VarOutputState, checked ? "ON" : "OFF");
    }

    if (checked && saved_use_timer && saved_timer_seconds > 0) {
        timer_remaining_seconds = saved_timer_seconds;
        timer_running = true;
        last_timer_tick_ms = millis();
    }

    if (!checked) {
        timer_running = false;
    }

    log_to_settings(String("Output ") + (checked ? "ENABLED" : "DISABLED"));
    last_activity_time = millis();
}

static void fan_dropdown_cb(lv_event_t *e) {
    if (suppress_fan_event) return;

    lv_obj_t *dd = lv_event_get_target(e);
    if (!dd) return;

    uint16_t sel = lv_dropdown_get_selected(dd);

    bool manual = (sel == 1); // 0=Auto, 1=Manual
    saved_fan_manual = manual;
    save_bool_pref("fan_man", saved_fan_manual);

    psu.setFanMode(manual);

    log_to_settings(String("Fan mode: ") + (manual ? "MANUAL" : "AUTO"));
    last_activity_time = millis();
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
