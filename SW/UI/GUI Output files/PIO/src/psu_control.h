#ifndef PSU_CONTROL_H
#define PSU_CONTROL_H

float clampf(float x, float lo, float hi);

/* Pushes saved_* setpoints to the PSU over CAN. Call once at boot, after
   prefs_load_all(). */
void apply_saved_psu_settings();

/* Reflects saved_* settings into the on-screen widgets (voltage/current
   fields, brightness label+slider, sleep label+slider, timer field, energy,
   "use timer" switch). Call once at boot, after ui_init(). */
void save_current_settings_to_ui();

/* Registers the voltage/current text fields, output switch and fan
   dropdown event callbacks. Call once from setup(). */
void psu_control_register_callbacks();

#endif
