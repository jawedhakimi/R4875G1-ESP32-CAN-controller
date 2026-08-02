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

/* =========================================================
   WEB INTERFACE
   HTTP Basic Auth on the local web control panel -- this device can set
   voltage/current on a real power supply over the network, so it's not
   left wide open by default. CHANGE THESE before deploying, especially on
   a shared/untrusted network -- they're sent as HTTP Basic Auth, which is
   only as safe as your LAN (not encrypted; fine for a trusted home
   network, not for anything exposed to the internet).
   ========================================================= */
#define WEB_AUTH_USER   "admin"
#define WEB_AUTH_PASS   "psucontrol"
#define WEB_SERVER_PORT 80

/* =========================================================
   OTA (Over-The-Air) FIRMWARE UPDATE
   Lets you push new firmware over the same WiFi network entered on the
   Connectivity screen -- no USB/physical access needed. Two ways in:
     1. PlatformIO: `pio run -e esp32-s3-devkitc-1-ota -t upload` (see
        platformio.ini) -- uses ArduinoOTA (ota.cpp), discoverable via
        mDNS at OTA_HOSTNAME + ".local".
     2. Browser: open http://<device-ip>/ and use the "Firmware Update"
        panel to upload a compiled .bin -- served by web_server.cpp.
   CHANGE OTA_PASSWORD before deploying, same reasoning as WEB_AUTH_PASS
   above: anyone on the LAN who can push arbitrary firmware onto this
   thing can do anything with your mains-connected rectifier. Keep it in
   sync with platformio.ini's `--auth=` upload flag if you change it.
   ========================================================= */
#define OTA_HOSTNAME  "huawei-psu-controller"
#define OTA_PASSWORD  "psuota"

#endif
