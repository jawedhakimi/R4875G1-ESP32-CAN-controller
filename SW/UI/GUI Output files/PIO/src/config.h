#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* =========================================================
   CAN DRIVER
   This sketch has its own on-screen/serial logging
   (see ui_safe.h: log_to_settings), so silence HuaweiCAN's own
   verbose Serial prints. HUAWEI_CAN_VERBOSE must be defined
   before HuaweiCAN.h is first included in each translation
   unit -- always pulling the header in through this file keeps
   that consistent everywhere.
   ========================================================= */
#define HUAWEI_CAN_VERBOSE 0
#include <HuaweiCAN.h>
// (also brings in PSU_VMAX / PSU_VMIN / PSU_VOFFLINE_MIN / PSU_IMAX / PSU_IMIN
//  -- the single source of truth for PSU safety limits used throughout the UI)

/* =========================================================
   HARDWARE
   ========================================================= */
#define BUZZER_PIN      1
#define BACKLIGHT_PIN   3

#define BEEP_FREQ         2000
#define BEEP_DURATION_MS  30

#define DEFAULT_DIM_TIMEOUT_MS   30000UL
#define DEFAULT_OFF_TIMEOUT_MS   60000UL

/* Sleep-timeout slider bounds (ui_SliderSleepTimer, Settings screen).
   0 means "never sleep" -- handle_backlight() treats an off_timeout_ms
   of 0 as disabled. Keep in sync with the slider's range set in
   ui_Settings.c (SquareLine Studio). */
#define SLEEP_SEC_MIN   0
#define SLEEP_SEC_MAX   200

/* =========================================================
   DISPLAY / LVGL
   ========================================================= */
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  480

#endif
