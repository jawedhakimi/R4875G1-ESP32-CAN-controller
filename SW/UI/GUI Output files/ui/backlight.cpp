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

/* BUGFIX (unchanged from before): only persist to flash once the drag ends
   (LV_EVENT_RELEASED), not on every VALUE_CHANGED tick during the drag. */
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
    ui_set_text_safe(ui_VarBrightness, buf);

    last_activity_time = millis();

    if (code == LV_EVENT_RELEASED) {
        save_int_pref("bright", saved_brightness_pct);
    }
}

/* Same debounce pattern as brightness_slider_cb above: PWM/timeout + label
   update on every tick (cheap, RAM-only), persisted to flash only once, on
   release. Range is SLEEP_SEC_MIN..SLEEP_SEC_MAX (config.h), matching
   ui_SliderSleepTimer's range set below; 0 means "never sleep". */
static void sleep_slider_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!target || !obj_is_slider(target)) return;

    lv_event_code_t code = lv_event_get_code(e);

    int val = lv_slider_get_value(target);
    val = constrain(val, SLEEP_SEC_MIN, SLEEP_SEC_MAX);
    saved_sleep_sec = val;
    saved_dim_sec = (saved_sleep_sec > 0) ? max(3, saved_sleep_sec / 2) : 0;

    off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;
    dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;

    char buf[32];
    snprintf(buf, sizeof(buf), "Sleep after: %d s", saved_sleep_sec);
    ui_set_text_safe(ui_VarSleepTimer, buf);

    last_activity_time = millis();

    if (code == LV_EVENT_RELEASED) {
        save_int_pref("sleep", saved_sleep_sec);
        save_int_pref("dim", saved_dim_sec);

        if (saved_sleep_sec == 0) {
            log_to_settings("Display sleep disabled");
        } else {
            log_to_settings("Display sleep set to " + String(saved_sleep_sec) + " s");
        }
    }
}

/* Settings now has properly named ui_SliderBrightness/ui_VarBrightness and
   ui_SliderSleepTimer/ui_VarSleepTimer pairs (re-exported from SquareLine
   Studio, replacing the old "SliderTemp"/"VarTemp" widget the brightness
   slider used to be repurposed from, and adding a sleep slider where a
   textarea used to be). SquareLine's generated ui_event_SliderBrightness /
   ui_event_SliderSleepTimer only update the label on LV_EVENT_CLICKED and
   don't touch PWM output, timeouts, or persistence, so both are detached
   here and replaced with our own handlers that do. */
void backlight_register_callbacks() {
    if (ui_SliderBrightness) {
        lv_obj_remove_event_cb(ui_SliderBrightness, ui_event_SliderBrightness);

        lv_slider_set_range(ui_SliderBrightness, 1, 100);
        lv_slider_set_value(ui_SliderBrightness, saved_brightness_pct, LV_ANIM_OFF);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", saved_brightness_pct);
        ui_set_text_safe(ui_VarBrightness, buf);

        lv_obj_add_event_cb(ui_SliderBrightness, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_SliderBrightness, brightness_slider_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(ui_SliderBrightness, user_beep_cb, LV_EVENT_PRESSED, NULL);
    }

    if (ui_SliderSleepTimer) {
        lv_obj_remove_event_cb(ui_SliderSleepTimer, ui_event_SliderSleepTimer);

        lv_slider_set_range(ui_SliderSleepTimer, SLEEP_SEC_MIN, SLEEP_SEC_MAX);
        lv_slider_set_value(ui_SliderSleepTimer, saved_sleep_sec, LV_ANIM_OFF);

        char buf[32];
        snprintf(buf, sizeof(buf), "Sleep after: %d s", saved_sleep_sec);
        ui_set_text_safe(ui_VarSleepTimer, buf);

        lv_obj_add_event_cb(ui_SliderSleepTimer, sleep_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_SliderSleepTimer, sleep_slider_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(ui_SliderSleepTimer, user_beep_cb, LV_EVENT_PRESSED, NULL);
    }
}
