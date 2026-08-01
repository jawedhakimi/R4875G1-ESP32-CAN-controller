#include "beep.h"
#include <ui.h>
#include "config.h"
#include "app_state.h"

extern "C" void user_beep(void) {
    tone(BUZZER_PIN, BEEP_FREQ, BEEP_DURATION_MS);
    last_activity_time = millis();
}

void user_beep_cb(lv_event_t *e) {
    (void)e;
    user_beep();
}

static void screen_nav_beep_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        user_beep();
    }
}

void beep_hook_screen_nav_buttons() {
    if (!lv_scr_act()) return;

    lv_obj_t *scr = lv_scr_act();
    uint32_t child_count = lv_obj_get_child_cnt(scr);

    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (!child) continue;

        lv_coord_t h = lv_obj_get_height(child);

        if (h >= 50 && h <= 90) {
            uint32_t cc = lv_obj_get_child_cnt(child);
            if (cc == 3) {
                lv_obj_t *c0 = lv_obj_get_child(child, 0);
                lv_obj_t *c1 = lv_obj_get_child(child, 1);
                lv_obj_t *c2 = lv_obj_get_child(child, 2);

                if (c0 && c1 && c2) {
                    lv_obj_add_event_cb(c0, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);
                    lv_obj_add_event_cb(c1, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);
                    lv_obj_add_event_cb(c2, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);

                    Serial.println("Screen nav beep hooks attached");
                    return;
                }
            }
        }
    }

    Serial.println("ScreenButtons component not found");
}

void beep_hook_numpads() {
    if (ui_Numpad) {
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_READY, NULL);
    }
}
