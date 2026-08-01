/* =========================================================
   Huawei R4850/R4875 rectifier touchscreen controller
   ESP32-S3 + LVGL (SquareLine Studio) + FT6336U touch + TWAI (CAN)

   This file is intentionally just wiring: setup()/loop() and the
   LVGL display/log glue. Everything else lives in its own module
   (see the #include list below) -- see each header for what it owns.
   ========================================================= */
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ui.h>
#include "ft6336u.h"
#include <math.h>

#include "config.h"
#include "app_state.h"
#include "ui_safe.h"
#include "prefs_store.h"
#include "beep.h"
#include "backlight.h"
#include "touch_input.h"
#include "energy_meter.h"
#include "psu_timer.h"
#include "psu_control.h"
#include "can_bridge.h"
#include "serial_console.h"
#include "wifi_manager.h"
#include "web_server.h"

/* =========================================================
   DISPLAY / LVGL
   ========================================================= */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * SCREEN_HEIGHT / 10];

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

#if LV_USE_LOG != 0
static void my_print(const char *buf) {
    Serial.printf("%s", buf);
    Serial.flush();
}
#endif

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n=== ESP32-S3 Huawei PSU GUI Boot ===");

    pinMode(BUZZER_PIN, OUTPUT);
    backlight_init();
    last_activity_time = millis();

    if (psu.begin()) {
        Serial.println("CAN Initialized Successfully");
    } else {
        Serial.println("CAN Initialization Failed");
    }

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    tft.begin();
    tft.setRotation(0);
    tft.invertDisplay(true);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    if (!touch_init(TP_SDA, TP_SCL, TP_RST, TP_INT)) {
        Serial.println("FT6336U touch panel init failed -- touch input may not work.");
    }

    ui_init();

    prefs_load_all();
    save_current_settings_to_ui();
    backlight_apply_saved_brightness();

    /* Event callback registration + one-off widget hooks (nav-bar beeps,
       numpad beeps) -- each module wires up only what it owns. */
    backlight_register_callbacks();
    psu_control_register_callbacks();
    psu_timer_register_callbacks();
    energy_meter_register_callbacks();
    wifi_manager_register_callbacks();
    beep_hook_numpads();
    beep_hook_screen_nav_buttons();

    apply_saved_psu_settings();

    update_energy_ui();
    last_energy_update_ms = millis();
    last_energy_save_ms = millis();

    /* WiFi + web control panel. wifi_manager_begin() loads any saved
       credentials from NVS, pre-fills the Connectivity screen, and kicks
       off a non-blocking connect attempt. web_server_begin() can start
       before a connection exists -- it just won't be reachable until
       WiFi comes up. */
    wifi_manager_begin();
    web_server_begin();

    log_to_settings("System boot complete");
    Serial.println("Setup done");
}

/* =========================================================
   LOOP
   ========================================================= */
void loop() {
    static uint32_t last_lvgl = 0;
    uint32_t now = millis();
    if (now - last_lvgl >= 5) {
        lv_timer_handler();
        last_lvgl = now;
    }

    handle_backlight();
    handle_psu_timer();
    handle_can_and_ui();
    handle_serial_commands();
    wifi_manager_loop();
    web_server_loop();
}
