#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <lvgl.h>

/* Sets up the backlight pin and turns it fully on. Call once from setup(),
   before anything else needs the display to be visible. */
void backlight_init();

/* Applies saved_brightness_pct as the current PWM level. Call once from
   setup() after prefs_load_all(). */
void backlight_apply_saved_brightness();

/* Call every loop() iteration. Drives the backlight PWM through
   full -> dim -> off based on idle time. */
void handle_backlight();

/* True once the backlight has timed all the way off (screen asleep).
   Used by touch_input.cpp to implement wake-on-touch. */
bool backlight_is_asleep();

/* Register the brightness slider + sleep-timeout slider event callbacks.
   Call once from setup(). */
void backlight_register_callbacks();

#endif
