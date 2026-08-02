#include "app_state.h"

HuaweiCAN psu;

unsigned long last_activity_time = 0;

bool suppress_switch_event = false;
bool suppress_fan_event = false;
bool suppress_timer_switch_event = false;

/* --- persisted user settings: compile-time defaults, overwritten by
   prefs_load_all() at boot from whatever was last saved to NVS --- */
float saved_online_v = 48.0f;
float saved_offline_v = 48.0f;
float saved_online_i = 20.00f;
float saved_offline_i = 10.00f;

int saved_brightness_pct = 50;
int saved_sleep_sec = 60;
int saved_dim_sec = 30;

bool saved_output_enable = true;
bool saved_fan_manual = false;

unsigned long dim_timeout_ms = DEFAULT_DIM_TIMEOUT_MS;
unsigned long off_timeout_ms = DEFAULT_OFF_TIMEOUT_MS;

uint32_t saved_timer_seconds = 0;
uint32_t timer_remaining_seconds = 0;
bool timer_running = false;
bool saved_use_timer = false;
unsigned long last_timer_tick_ms = 0;

double energy_kwh = 0.0;
unsigned long last_energy_update_ms = 0;
unsigned long last_energy_save_ms = 0;
