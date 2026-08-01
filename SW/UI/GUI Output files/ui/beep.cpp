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

// BUGFIX: this used to scan only lv_scr_act()'s children once at boot,
// hooking whichever single screen happened to be active at the time (Home).
// The nav bar is a SquareLine "component" instantiated separately per
// screen (ui_ScreenButtons/2/3/4 -- four distinct lv_obj_t trees, not one
// shared widget), so that only ever wired up beeps for taps made from one
// screen; navigating away from anywhere else stayed silent. Each screen's
// container now has a stable extern name (no more heuristic needed), so
// hook all four directly.
void beep_hook_screen_nav_buttons() {
    lv_obj_t *bars[] = { ui_ScreenButtons, ui_ScreenButtons2, ui_ScreenButtons3, ui_ScreenButtons4 };
    int hooked = 0;

    for (lv_obj_t *bar : bars) {
        if (!bar) continue;

        uint32_t cc = lv_obj_get_child_cnt(bar);
        for (uint32_t j = 0; j < cc; j++) {
            lv_obj_t *btn = lv_obj_get_child(bar, j);
            if (btn) lv_obj_add_event_cb(btn, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);
        }
        hooked++;
    }

    Serial.printf("Screen nav beep hooks attached (%d of %d nav bars)\n", hooked, (int)(sizeof(bars) / sizeof(bars[0])));
}

void beep_hook_numpads() {
    // ui_Numpad2 no longer exists -- the current SetValues screen shares a
    // single numpad (ui_Numpad) across all four voltage/current fields.
    if (ui_Numpad) {
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_READY, NULL);
    }
}
