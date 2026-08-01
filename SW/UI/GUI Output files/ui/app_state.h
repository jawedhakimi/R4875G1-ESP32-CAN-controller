#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include "config.h"   // pulls in HuaweiCAN.h

/* Shared PSU driver instance. */
extern HuaweiCAN psu;

/* --- activity / idle tracking (touched by touch input, beeps, every
   settings event -- read by the backlight module) --- */
extern unsigned long last_activity_time;

/* --- event-suppression flags. Set while we programmatically change a
   widget's state (e.g. syncing the output switch from CAN status) so its
   event callback doesn't re-trigger and re-send the command it just
   reflected. --- */
extern bool suppress_switch_event;
extern bool suppress_fan_event;
extern bool suppress_timer_checkbox_event;

/* --- persisted user settings (mirrors of NVS, loaded at boot by
   prefs_load_all()) --- */
extern float saved_online_v;
extern float saved_offline_v;
extern float saved_online_i;
extern float saved_offline_i;

extern int saved_brightness_pct;   // 1..100
extern int saved_sleep_sec;        // screen-off timeout, seconds
extern int saved_dim_sec;          // dim timeout, seconds

extern bool saved_output_enable;
extern bool saved_fan_manual;

/* --- runtime timeouts derived from saved_sleep_sec / saved_dim_sec --- */
extern unsigned long dim_timeout_ms;
extern unsigned long off_timeout_ms;

/* --- PSU run timer --- */
extern uint32_t saved_timer_seconds;      // configured timer value
extern uint32_t timer_remaining_seconds;
extern bool timer_running;
extern bool saved_use_timer;              // checkbox state
extern unsigned long last_timer_tick_ms;

/* --- energy metering --- */
extern double energy_kwh;
extern unsigned long last_energy_update_ms;
extern unsigned long last_energy_save_ms;

#endif
