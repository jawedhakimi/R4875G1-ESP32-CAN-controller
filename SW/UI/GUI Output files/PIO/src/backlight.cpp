#include "backlight.h"
#include <ui.h>
#include "config.h"
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"

static int current_backlight_pwm = 255;

void backlight_init() {
    pinMode(BACKLIGHT_PIN, OUTPUT);
    analogWrite(BACKLIGHT_PIN, 255);
    current_backlight_pwm = 255;
}

void backlight_apply_saved_brightness() {
    int pwm = map(saved_brightness_pct, 0, 100, 0, 255);
    analogWrite(BACKLIGHT_PIN, pwm);
    current_backlight_pwm = pwm;
}

void handle_backlight() {
    unsigned long idle_time = millis() - last_activity_time;

    if (idle_time > off_timeout_ms && off_timeout_ms > 0) {
        if (current_backlight_pwm != 0) {
            analogWrite(BACKLIGHT_PIN, 0);
            current_backlight_pwm = 0;
        }
    }
    else if (idle_time > dim_timeout_ms && dim_timeout_ms > 0) {
        int full_brightness = map(saved_brightness_pct, 0, 100, 0, 255);
        int dim_pwm = full_brightness / 4;
        if (dim_pwm < 5) dim_pwm = 5;
        if (current_backlight_pwm != dim_pwm) {
            analogWrite(BACKLIGHT_PIN, dim_pwm);
            current_backlight_pwm = dim_pwm;
        }
    }
    else {
        int full_pwm = map(saved_brightness_pct, 0, 100, 0, 255);
        if (current_backlight_pwm != full_pwm) {
            analogWrite(BACKLIGHT_PIN, full_pwm);
            current_backlight_pwm = full_pwm;
        }
    }
}

bool backlight_is_asleep() {
    return current_backlight_pwm == 0;
}

/* BUGFIX: previously bound to LV_EVENT_VALUE_CHANGED only, which LVGL fires
   continuously while the slider is being dragged -- every single tick wrote
   to NVS flash. A few seconds of dragging could be dozens of flash writes.
   Now the PWM + on-screen label update live on every tick (cheap, RAM-only),
   but the value is only persisted to flash once, when the drag ends. */
static void brightness_slider_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!target || !obj_is_slider(target)) return;

    lv_event_code_t code = lv_event_get_code(e);

    int val = lv_slider_get_value(target);
    val = constrain(val, 1, 100);
    saved_brightness_pct = val;

    int pwm = map(saved_brightness_pct, 0, 100, 0, 255);
    analogWrite(BACKLIGHT_PIN, pwm);
    current_backlight_pwm = pwm;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", saved_brightness_pct);
    ui_set_text_safe(ui_Settings_LabelBrightness, buf);

    last_activity_time = millis();

    if (code == LV_EVENT_RELEASED) {
        save_int_pref("bright", saved_brightness_pct);
    }
}

static void settings_numeric_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!target) return;

    String text = ui_get_text_safe(target);
    int val = text.toInt();
    char buf[16];

    if (target == ui_Settings_TextAreaSleep) {
        val = constrain(val, 5, 3600);
        saved_sleep_sec = val;
        saved_dim_sec = max(3, saved_sleep_sec / 2);

        save_int_pref("sleep", saved_sleep_sec);
        save_int_pref("dim", saved_dim_sec);

        off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;
        dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;

        snprintf(buf, sizeof(buf), "%d", saved_sleep_sec);
        ui_set_text_safe(ui_Settings_TextAreaSleep, buf);

        log_to_settings("Display sleep set to " + String(saved_sleep_sec) + " s");
    }

    last_activity_time = millis();
}

void backlight_register_callbacks() {
    if (ui_Settings_SliderBrightness) {
        lv_slider_set_range(ui_Settings_SliderBrightness, 1, 100);
        lv_slider_set_value(ui_Settings_SliderBrightness, saved_brightness_pct, LV_ANIM_OFF);
        lv_obj_add_event_cb(ui_Settings_SliderBrightness, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Settings_SliderBrightness, brightness_slider_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(ui_Settings_SliderBrightness, user_beep_cb, LV_EVENT_PRESSED, NULL);
    }

    if (ui_Settings_TextAreaSleep) {
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, settings_numeric_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, user_beep_cb, LV_EVENT_READY, NULL);
    }
}
