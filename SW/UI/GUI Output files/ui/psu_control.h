#ifndef PSU_CONTROL_H
#define PSU_CONTROL_H

float clampf(float x, float lo, float hi);

/* ---------------------------------------------------------------------
   Shared PSU setters -- single source of truth for "change a PSU setting":
   clamp to safety limits, update saved_*, persist to NVS, send over CAN,
   log, and (for output/fan) reflect into the touchscreen widgets. Called
   from the touchscreen callbacks, the serial console, and the web API, so
   the three surfaces can't drift out of sync with each other. Each
   returns the value actually applied (post-clamp). --------------------- */
float psu_set_online_voltage(float v);
float psu_set_offline_voltage(float v);
float psu_set_online_current(float i);
float psu_set_offline_current(float i);
void  psu_set_output(bool on);
void  psu_set_fan_manual(bool manual);

/* Pushes saved_* setpoints to the PSU over CAN. Call once at boot, after
   prefs_load_all(). */
void apply_saved_psu_settings();

/* Reflects saved_* settings into the on-screen widgets (voltage/current
   fields, output switch/state). Call once at boot, after ui_init(). */
void save_current_settings_to_ui();

/* Registers the voltage/current text fields, output switch and fan
   dropdown event callbacks. Call once from setup(). */
void psu_control_register_callbacks();

#endif
