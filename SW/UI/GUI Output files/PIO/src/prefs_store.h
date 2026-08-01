#ifndef PREFS_STORE_H
#define PREFS_STORE_H

#include <Arduino.h>

void save_float_pref(const char *key, float val);
void save_int_pref(const char *key, int val);
void save_uint_pref(const char *key, uint32_t val);
void save_bool_pref(const char *key, bool val);
void save_double_pref(const char *key, double val);

/* Loads every persisted setting from NVS into the app_state globals and
   derives the runtime dim/off timeout values + initial timer state.
   Call once from setup(), after app_state's defaults are in place. */
void prefs_load_all();

#endif
