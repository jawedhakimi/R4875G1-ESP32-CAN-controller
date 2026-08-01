#include "touch_input.h"
#include "ft6336u.h"
#include "config.h"
#include "app_state.h"
#include "backlight.h"

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    (void)indev_driver;

    static toucht_coords_t toucht_coords;
    static uint32_t last_touch_time = 0;
    static int16_t last_x = -1;
    static int16_t last_y = -1;

    // True while we're "eating" the touch that woke the screen up -- reset
    // as soon as the finger lifts, so the *next* touch behaves normally.
    static bool swallowing_wake_touch = false;

    bool is_touched = get_touch_coords(&toucht_coords);

    if (!is_touched) {
        data->state = LV_INDEV_STATE_REL;
        last_x = -1;
        last_y = -1;
        swallowing_wake_touch = false;
        return;
    }

    int16_t x = toucht_coords.x;
    int16_t y = toucht_coords.y;

    // Validate coordinates
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    // --- WAKE-ON-TOUCH: if the screen is currently asleep (backlight timed
    // all the way off), this touch -- and the rest of this same continuous
    // press, until the finger lifts -- only wakes the backlight. It must
    // never reach LVGL as a press, otherwise whatever happens to be under
    // the finger (a button, the output switch, ...) gets activated the
    // instant the screen turns back on. ---
    if (backlight_is_asleep()) {
        swallowing_wake_touch = true;
    }
    if (swallowing_wake_touch) {
        last_activity_time = millis();   // handle_backlight() will wake the panel
        data->state = LV_INDEV_STATE_REL;
        last_x = x;
        last_y = y;
        return;
    }

    uint32_t now = millis();

    // --- FILTER 1: ignore ultra-fast repeats (debounce) ---
    if (now - last_touch_time < 8) {
        // Keep last state instead of forcing release. Also refresh the
        // activity timer: a finger held perfectly still on a button/slider
        // hits this branch on every read, and previously never refreshed
        // last_activity_time, so a long press could let the screen dim/sleep
        // underneath the held finger.
        last_activity_time = millis();
        data->state = LV_INDEV_STATE_PR;
        data->point.x = last_x;
        data->point.y = last_y;
        return;
    }

    // --- FILTER 2: ignore tiny jitter ---
    if (abs(x - last_x) < 2 && abs(y - last_y) < 2) {
        // Same point -> accept but don't spam LVGL. Same activity-refresh
        // reasoning as filter 1 above.
        last_activity_time = millis();
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        return;
    }

    last_touch_time = now;
    last_x = x;
    last_y = y;

    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;

    last_activity_time = millis();
}
