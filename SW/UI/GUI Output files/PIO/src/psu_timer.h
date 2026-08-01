#ifndef PSU_TIMER_H
#define PSU_TIMER_H

#include <Arduino.h>

/* "HH.MM.SS" <-> seconds, used by the run-timer text field. */
String format_hms(uint32_t totalSec);
bool parse_hms_string(const String &input, uint32_t &outSeconds);

/* Ticks the run-down timer and switches the PSU output off when it hits
   zero. Call every loop. */
void handle_psu_timer();

/* Registers the timer text field + "use timer" slide switch event callbacks.
   Call once from setup(). */
void psu_timer_register_callbacks();

#endif
