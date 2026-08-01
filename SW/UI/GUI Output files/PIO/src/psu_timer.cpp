#include "psu_timer.h"
#include <ui.h>
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"

String format_hms(uint32_t totalSec) {
    uint32_t h = totalSec / 3600;
    uint32_t m = (totalSec % 3600) / 60;
    uint32_t s = totalSec % 60;

    char b[16];
    snprintf(b, sizeof(b), "%02lu.%02lu.%02lu",
             (unsigned long)h,
             (unsigned long)m,
             (unsigned long)s);
    return String(b);
}

bool parse_hms_string(const String &input, uint32_t &outSeconds) {
    String s = input;
    s.trim();

    if (s.length() == 0) return false;

    int p1 = s.indexOf('.');
    int p2 = s.lastIndexOf('.');

    if (p1 <= 0 || p2 <= p1 || p2 >= (int)s.length() - 1) return false;

    String hs = s.substring(0, p1);
    String ms = s.substring(p1 + 1, p2);
    String ss = s.substring(p2 + 1);

    if (hs.length() == 0 || ms.length() == 0 || ss.length() == 0) return false;

    // Only digits and dots allowed
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (!(isdigit(c) || c == '.')) return false;
    }

    long h   = hs.toInt();
    long m   = ms.toInt();
    long sec = ss.toInt();

    if (h < 0 || m < 0 || sec < 0) return false;

    // Convert everything into total seconds
    uint64_t total =
        (uint64_t)h   * 3600ULL +
        (uint64_t)m   * 60ULL +
        (uint64_t)sec;

    // Prevent overflow for uint32_t
    if (total > 0xFFFFFFFFULL) return false;

    outSeconds = (uint32_t)total;
    return true;
}

void handle_psu_timer() {
    if (!saved_use_timer) return;
    if (!timer_running) return;
    if (!saved_output_enable) {
        timer_running = false;
        return;
    }

    if (millis() - last_timer_tick_ms >= 1000) {
        last_timer_tick_ms += 1000;

        if (timer_remaining_seconds > 0) {
            timer_remaining_seconds--;
        }

        if (ui_SetValues_TextAreaVarTimer && obj_is_textarea(ui_SetValues_TextAreaVarTimer)) {
            ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(timer_remaining_seconds).c_str());
        }

        if (timer_remaining_seconds == 0) {
            psu.enableOutput(false);
            saved_output_enable = false;
            save_bool_pref("out_en", false);
            timer_running = false;

            suppress_switch_event = true;
            if (ui_Home_SwitchEnableOutput) {
                lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
            }
            suppress_switch_event = false;

            if (ui_VarOutputState) {
                ui_set_text_safe(ui_VarOutputState, "OFF");
            }

            log_to_settings("Timer expired -> PSU output OFF");
        }
    }
}

static void timer_setting_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!target) return;

    String text = ui_get_text_safe(target);
    uint32_t secs = 0;

    if (!parse_hms_string(text, secs)) {
        log_to_settings("Invalid timer format. Use HH.MM.SS");
        ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());
        return;
    }

    saved_timer_seconds = secs;
    timer_remaining_seconds = secs;
    timer_running = (saved_use_timer && secs > 0 && saved_output_enable);
    last_timer_tick_ms = millis();

    save_uint_pref("timer_sec", saved_timer_seconds);
    ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());

    if (saved_timer_seconds == 0) {
        log_to_settings("Timer disabled (00.00.00)");
    } else {
        log_to_settings("Timer set to " + format_hms(saved_timer_seconds));
    }

    last_activity_time = millis();
}

static void timer_checkbox_cb(lv_event_t *e) {
    if (suppress_timer_checkbox_event) return;

    lv_obj_t *cb = lv_event_get_target(e);
    if (!cb) return;

    bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    saved_use_timer = checked;
    save_bool_pref("use_timer", saved_use_timer);

    if (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable) {
        timer_remaining_seconds = saved_timer_seconds;
        timer_running = true;
        last_timer_tick_ms = millis();
        log_to_settings("Timer ENABLED");
    } else {
        timer_running = false;
        log_to_settings("Timer DISABLED");
    }

    last_activity_time = millis();
}

void psu_timer_register_callbacks() {
    if (ui_SetValues_TextAreaVarTimer) {
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, timer_setting_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_SetValues_CheckboxUseTimer) {
        lv_obj_add_event_cb(ui_SetValues_CheckboxUseTimer, timer_checkbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_SetValues_CheckboxUseTimer, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}
