#include "energy_meter.h"
#include <ui.h>
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"

String format_energy_kwh(double kwh) {
    char b[24];
    snprintf(b, sizeof(b), "%.4f", kwh);
    return String(b);
}

void update_energy_ui() {
    if (ui_VarEnergy) {
        ui_set_text_safe(ui_VarEnergy, format_energy_kwh(energy_kwh).c_str());
    }
}

void handle_energy_meter(float outputPowerWatts) {
    unsigned long now = millis();

    if (last_energy_update_ms == 0) {
        last_energy_update_ms = now;
        return;
    }

    unsigned long dt_ms = now - last_energy_update_ms;
    last_energy_update_ms = now;

    if (dt_ms > 5000) return; // skip absurd gap after pauses/reboots/etc

    if (saved_output_enable && outputPowerWatts > 0.0f) {
        double dt_hours = (double)dt_ms / 3600000.0;
        energy_kwh += ((double)outputPowerWatts / 1000.0) * dt_hours;
    }

    static unsigned long last_ui_ms = 0;
    if (now - last_ui_ms >= 1000) {
        update_energy_ui();
        last_ui_ms = now;
    }

    if (now - last_energy_save_ms >= 30000UL) { // save every 30s
        save_double_pref("energy", energy_kwh);
        last_energy_save_ms = now;
    }
}

static void reset_energy_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    energy_kwh = 0.0;
    save_double_pref("energy", energy_kwh);
    update_energy_ui();
    log_to_settings("Energy counter reset");
    user_beep();
    last_activity_time = millis();
}

void energy_meter_register_callbacks() {
    if (ui_Settings_ButtonResetEnergy) {
        // BUGFIX: the original sketch registered *both* reset_energy_cb
        // (which itself calls user_beep()) and user_beep_cb for
        // LV_EVENT_CLICKED on this same button, so the buzzer sounded
        // twice per tap. reset_energy_cb's own beep is enough.
        lv_obj_add_event_cb(ui_Settings_ButtonResetEnergy, reset_energy_cb, LV_EVENT_CLICKED, NULL);
    }
}
