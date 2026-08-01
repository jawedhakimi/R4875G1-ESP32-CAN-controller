#ifndef PSU_TIMER_H
#define PSU_TIMER_H

#include <Arduino.h>

/* "HH.MM.SS" <-> seconds, used by the run-timer text field. */
String format_hms(uint32_t totalSec);
bool parse_hms_string(const String &input, uint32_t &outSeconds);

/* Shared setters -- see psu_control.h for why. Used by the serial console
   and the web API (no touchscreen control exists for these right now). */
void psu_timer_configure(uint32_t seconds);
void psu_timer_set_enabled(bool enabled);

/* Ticks the run-down timer and switches the PSU output off when it hits
   zero. Call every loop. */
void handle_psu_timer();

/* Registers the timer text field + "use timer" slide switch event
   callbacks, and syncs their initial state from saved_timer_seconds /
   saved_use_timer. Call once from setup(). */
void psu_timer_register_callbacks();

#endif
