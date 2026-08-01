#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ui.h>
#include "ft6336u.h"
#include <Preferences.h>
#include "HuaweiCAN.h"
#include <math.h>

/* =========================================================
   HARDWARE
   ========================================================= */
#define BUZZER_PIN      1
#define BACKLIGHT_PIN   3

#define BEEP_FREQ         2000
#define BEEP_DURATION_MS  30

#define DEFAULT_DIM_TIMEOUT_MS   30000UL
#define DEFAULT_OFF_TIMEOUT_MS   60000UL

/* =========================================================
   PSU LIMITS
   ========================================================= */
#define PSU_VMAX          58.5f
#define PSU_VMIN          41.5f
#define PSU_VOFFLINE_MIN  48.0f
#define PSU_IMAX          75.0f
#define PSU_IMIN          0.0f

/* =========================================================
   DISPLAY / LVGL
   ========================================================= */
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
toucht_coords_t toucht_coords;

/* =========================================================
   GLOBALS
   ========================================================= */
Preferences preferences;
HuaweiCAN psu;

unsigned long last_activity_time = 0;
int current_backlight_pwm = 255;
bool last_touch_state = false;

/* --- output/fan state tracking --- */
bool suppress_switch_event = false;
bool suppress_fan_event = false;
bool suppress_timer_checkbox_event = false;
bool last_output_switch_state = true;
bool fan_manual_mode = false;

/* --- persisted user settings --- */
float saved_online_v = 53.50f;
float saved_offline_v = 53.50f;
float saved_online_i = 10.00f;
float saved_offline_i = 10.00f;

int saved_brightness_pct = 100;     // 1..100
int saved_sleep_sec = 60;           // screen off timeout in seconds
int saved_dim_sec = 30;             // dim timeout in seconds

bool saved_output_enable = true;
bool saved_fan_manual = false;

/* --- runtime timeouts --- */
unsigned long dim_timeout_ms = DEFAULT_DIM_TIMEOUT_MS;
unsigned long off_timeout_ms = DEFAULT_OFF_TIMEOUT_MS;

/* --- PSU run timer --- */
uint32_t saved_timer_seconds = 0;      // configured timer value
uint32_t timer_remaining_seconds = 0;
bool timer_running = false;
bool saved_use_timer = false;          // checkbox state
unsigned long last_timer_tick_ms = 0;

/* --- Energy metering --- */
double energy_kwh = 0.0;               // persisted energy
unsigned long last_energy_update_ms = 0;
unsigned long last_energy_save_ms = 0;

/* --- screen button tracking --- */
static lv_obj_t* g_screenButtonsComp = nullptr;
static lv_obj_t* g_btnHome = nullptr;
static lv_obj_t* g_btnSetValues = nullptr;
static lv_obj_t* g_btnSettings = nullptr;

/* =========================================================
   SERIAL LVGL LOG
   ========================================================= */
#if LV_USE_LOG != 0
void my_print(const char * buf) {
    Serial.printf("%s", buf);
    Serial.flush();
}
#endif

/* =========================================================
   SAFE UI HELPERS
   ========================================================= */
bool obj_is_textarea(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_textarea_class);
}

bool obj_is_label(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_label_class);
}

bool obj_is_slider(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_slider_class);
}

bool obj_is_checkbox(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_checkbox_class);
}

bool obj_is_btn(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_btn_class);
}

void ui_set_text_safe(lv_obj_t *obj, const char *txt) {
    if (!obj || !txt) return;

    if (obj_is_label(obj)) {
        lv_label_set_text(obj, txt);
    } else if (obj_is_textarea(obj)) {
        lv_textarea_set_text(obj, txt);
    }
}

String ui_get_text_safe(lv_obj_t *obj) {
    if (!obj) return "";

    if (obj_is_textarea(obj)) {
        return String(lv_textarea_get_text(obj));
    } else if (obj_is_label(obj)) {
        return String(lv_label_get_text(obj));
    }

    return "";
}

/* =========================================================
   LOGGING
   ========================================================= */
void append_log_to_textarea(lv_obj_t *ta, const String &msg) {
    if (!ta || !obj_is_textarea(ta)) return;

    String current = lv_textarea_get_text(ta);
    if (current.length() > 1200) {
        current = current.substring(current.length() - 900);
    }

    current += msg;
    current += "\n";
    lv_textarea_set_text(ta, current.c_str());
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
}

void log_to_settings(const String &msg) {
    Serial.println(msg);
    append_log_to_textarea(ui_Settings_TextAreaLogs, msg);
}

/* =========================================================
   BEEP
   IMPORTANT: Must be C-linkage so generated .c files can call it
   ========================================================= */
extern "C" void user_beep(void);

extern "C" void user_beep(void) {
    tone(BUZZER_PIN, BEEP_FREQ, BEEP_DURATION_MS);
    last_activity_time = millis();
}

static void user_beep_cb(lv_event_t * e) {
    (void)e;
    user_beep();
}

/* =========================================================
   DISPLAY FLUSH
   ========================================================= */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* =========================================================
   TOUCHPAD
   ========================================================= */
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    (void) indev_driver;

    static uint32_t last_touch_time = 0;
    static int16_t last_x = -1;
    static int16_t last_y = -1;

    bool is_touched = get_touch_coords(&toucht_coords);

    if (!is_touched) {
        data->state = LV_INDEV_STATE_REL;
        last_x = -1;
        last_y = -1;
        return;
    }

    int16_t x = toucht_coords.x;
    int16_t y = toucht_coords.y;

    // Validate coordinates
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    // --- FILTER 1: Ignore ultra-fast repeats (debounce) ---
    uint32_t now = millis();
    if (now - last_touch_time < 8) {
    // keep last state instead of forcing release
    data->state = LV_INDEV_STATE_PR;
    data->point.x = last_x;
    data->point.y = last_y;
    return;
    }

    // --- FILTER 2: Ignore tiny jitter ---
    if (abs(x - last_x) < 2 && abs(y - last_y) < 2) {
        // same point -> accept but don't spam LVGL
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        return;
    }

    last_touch_time = now;
    last_x = x;
    last_y = y;

    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;

    last_activity_time = millis();
    last_touch_state = true;
}


/* =========================================================
   PREFERENCES HELPERS
   ========================================================= */
void save_float_pref(const char *key, float val) {
    preferences.begin("psuui", false);
    preferences.putFloat(key, val);
    preferences.end();
}

void save_int_pref(const char *key, int val) {
    preferences.begin("psuui", false);
    preferences.putInt(key, val);
    preferences.end();
}

void save_uint_pref(const char *key, uint32_t val) {
    preferences.begin("psuui", false);
    preferences.putUInt(key, val);
    preferences.end();
}

void save_bool_pref(const char *key, bool val) {
    preferences.begin("psuui", false);
    preferences.putBool(key, val);
    preferences.end();
}

void save_double_pref(const char *key, double val) {
    preferences.begin("psuui", false);
    preferences.putDouble(key, val);
    preferences.end();
}

/* =========================================================
   CLAMP HELPERS
   ========================================================= */
float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* =========================================================
   TIMER PARSE / FORMAT
   ========================================================= */
String format_hms(uint32_t totalSec) {
    uint32_t h = totalSec / 3600;
    uint32_t m = (totalSec % 3600) / 60;
    uint32_t s = totalSec % 60;

    char b[16];
    snprintf(b, sizeof(b), "%02lu.%02lu.%02lu",
             (unsigned long)h,
             (unsigned long)m,
             (unsigned long)s);
    return String(b);
}

bool parse_hms_string(const String &input, uint32_t &outSeconds) {
    String s = input;
    s.trim();

    if (s.length() == 0) return false;

    int p1 = s.indexOf('.');
    int p2 = s.lastIndexOf('.');

    if (p1 <= 0 || p2 <= p1 || p2 >= (int)s.length() - 1) return false;

    String hs = s.substring(0, p1);
    String ms = s.substring(p1 + 1, p2);
    String ss = s.substring(p2 + 1);

    if (hs.length() == 0 || ms.length() == 0 || ss.length() == 0) return false;

    // Only digits and dots allowed
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (!(isdigit(c) || c == '.')) return false;
    }

    long h   = hs.toInt();
    long m   = ms.toInt();
    long sec = ss.toInt();

    if (h < 0 || m < 0 || sec < 0) return false;

    // Convert everything into total seconds
    uint64_t total =
        (uint64_t)h   * 3600ULL +
        (uint64_t)m   * 60ULL +
        (uint64_t)sec;

    // Prevent overflow for uint32_t
    if (total > 0xFFFFFFFFULL) return false;

    outSeconds = (uint32_t)total;
    return true;
}

/* =========================================================
   ENERGY FORMAT / UPDATE
   ========================================================= */
String format_energy_kwh(double kwh) {
    char b[24];
    snprintf(b, sizeof(b), "%.4f", kwh);
    return String(b);
}

void update_energy_ui() {
    if (ui_VarEnergy) {
        ui_set_text_safe(ui_VarEnergy, format_energy_kwh(energy_kwh).c_str());
    }
}

void handle_energy_meter(float outputPowerWatts) {
    unsigned long now = millis();

    if (last_energy_update_ms == 0) {
        last_energy_update_ms = now;
        return;
    }

    unsigned long dt_ms = now - last_energy_update_ms;
    last_energy_update_ms = now;

    if (dt_ms > 5000) return; // skip absurd gap after pauses/reboots/etc

    if (saved_output_enable && outputPowerWatts > 0.0f) {
        double dt_hours = (double)dt_ms / 3600000.0;
        energy_kwh += ((double)outputPowerWatts / 1000.0) * dt_hours;
    }

    static unsigned long last_ui_ms = 0;
    if (now - last_ui_ms >= 1000) {
        update_energy_ui();
        last_ui_ms = now;
    }

    if (now - last_energy_save_ms >= 30000UL) { // save every 30s
        save_double_pref("energy", energy_kwh);
        last_energy_save_ms = now;
    }
}

/* =========================================================
   PSU APPLY + SAVE
   ========================================================= */
void save_current_settings_to_ui() {
    char b[24];

    snprintf(b, sizeof(b), "%.2f", saved_online_v);
    ui_set_text_safe(ui_VarOnlineVout, b);

    snprintf(b, sizeof(b), "%.2f", saved_offline_v);
    ui_set_text_safe(ui_VarOfflineVout, b);

    snprintf(b, sizeof(b), "%.2f", saved_online_i);
    ui_set_text_safe(ui_VarOnlineIout, b);

    snprintf(b, sizeof(b), "%.2f", saved_offline_i);
    ui_set_text_safe(ui_VarOfflineIout, b);

    snprintf(b, sizeof(b), "%d%%", saved_brightness_pct);
    ui_set_text_safe(ui_Settings_LabelBrightness, b);

    snprintf(b, sizeof(b), "%d", saved_sleep_sec);
    ui_set_text_safe(ui_Settings_TextAreaSleep, b);

    ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());

    if (ui_Settings_SliderBrightness && obj_is_slider(ui_Settings_SliderBrightness)) {
        lv_slider_set_value(ui_Settings_SliderBrightness, saved_brightness_pct, LV_ANIM_OFF);
    }

    update_energy_ui();

    if (ui_SetValues_CheckboxUseTimer) {
        if (saved_use_timer) lv_obj_add_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
    }
}

void apply_saved_psu_settings() {
    psu.setVoltage(saved_online_v);
    delay(50);
    psu.setOfflineVoltage(saved_offline_v);
    delay(50);
    psu.setCurrent(saved_online_i);
    delay(50);
    psu.setOfflineCurrent(saved_offline_i);
    delay(50);

    psu.setFanMode(saved_fan_manual);
    delay(50);

    psu.enableOutput(saved_output_enable);
    delay(50);
}

/* =========================================================
   UI EVENT CALLBACKS
   ========================================================= */
static void apply_psu_settings_cb(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_target(e);
    if (!target) return;

    String text = ui_get_text_safe(target);
    float val = text.toFloat();
    char buf[24];

    if (target == ui_VarOnlineVout) {
        float clamped = clampf(val, PSU_VMIN, PSU_VMAX);
        saved_online_v = clamped;
        save_float_pref("on_v", saved_online_v);
        psu.setVoltage(saved_online_v);
        snprintf(buf, sizeof(buf), "%.2f", saved_online_v);
        ui_set_text_safe(ui_VarOnlineVout, buf);
        log_to_settings("Online Vout set to " + String(saved_online_v, 2) + " V");
    }

    else if (target == ui_VarOfflineVout) {
        float clamped = clampf(val, PSU_VOFFLINE_MIN, PSU_VMAX);
        saved_offline_v = clamped;
        save_float_pref("off_v", saved_offline_v);
        psu.setOfflineVoltage(saved_offline_v);
        snprintf(buf, sizeof(buf), "%.2f", saved_offline_v);
        ui_set_text_safe(ui_VarOfflineVout, buf);
        log_to_settings("Offline Vout set to " + String(saved_offline_v, 2) + " V");
    }

    else if (target == ui_VarOnlineIout) {
        float clamped = clampf(val, PSU_IMIN, PSU_IMAX);
        saved_online_i = clamped;
        save_float_pref("on_i", saved_online_i);
        psu.setCurrent(saved_online_i);
        snprintf(buf, sizeof(buf), "%.2f", saved_online_i);
        ui_set_text_safe(ui_VarOnlineIout, buf);
        log_to_settings("Online Iout set to " + String(saved_online_i, 2) + " A");
    }

    else if (target == ui_VarOfflineIout) {
        float clamped = clampf(val, PSU_IMIN, PSU_IMAX);
        saved_offline_i = clamped;
        save_float_pref("off_i", saved_offline_i);
        psu.setOfflineCurrent(saved_offline_i);
        snprintf(buf, sizeof(buf), "%.2f", saved_offline_i);
        ui_set_text_safe(ui_VarOfflineIout, buf);
        log_to_settings("Offline Iout set to " + String(saved_offline_i, 2) + " A");
    }

    last_activity_time = millis();
}

static void settings_numeric_cb(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_target(e);
    if (!target) return;

    String text = ui_get_text_safe(target);
    int val = text.toInt();
    char buf[16];

    if (target == ui_Settings_TextAreaSleep) {
        val = constrain(val, 5, 3600);
        saved_sleep_sec = val;
        saved_dim_sec = max(3, saved_sleep_sec / 2);

        save_int_pref("sleep", saved_sleep_sec);
        save_int_pref("dim", saved_dim_sec);

        off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;
        dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;

        snprintf(buf, sizeof(buf), "%d", saved_sleep_sec);
        ui_set_text_safe(ui_Settings_TextAreaSleep, buf);

        log_to_settings("Display sleep set to " + String(saved_sleep_sec) + " s");
    }

    last_activity_time = millis();
}

static void brightness_slider_cb(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_target(e);
    if (!target || !obj_is_slider(target)) return;

    int val = lv_slider_get_value(target);
    val = constrain(val, 1, 100);

    saved_brightness_pct = val;
    save_int_pref("bright", saved_brightness_pct);

    int pwm = map(saved_brightness_pct, 0, 100, 0, 255);
    analogWrite(BACKLIGHT_PIN, pwm);
    current_backlight_pwm = pwm;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", saved_brightness_pct);
    ui_set_text_safe(ui_Settings_LabelBrightness, buf);

    last_activity_time = millis();
}

static void timer_setting_cb(lv_event_t * e) {
    lv_obj_t * target = lv_event_get_target(e);
    if (!target) return;

    String text = ui_get_text_safe(target);
    uint32_t secs = 0;

    if (!parse_hms_string(text, secs)) {
        log_to_settings("Invalid timer format. Use HH.MM.SS");
        ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());
        return;
    }

    saved_timer_seconds = secs;
    timer_remaining_seconds = secs;
    timer_running = (saved_use_timer && secs > 0 && saved_output_enable);
    last_timer_tick_ms = millis();

    save_uint_pref("timer_sec", saved_timer_seconds);
    ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(saved_timer_seconds).c_str());

    if (saved_timer_seconds == 0) {
        log_to_settings("Timer disabled (00.00.00)");
    } else {
        log_to_settings("Timer set to " + format_hms(saved_timer_seconds));
    }

    last_activity_time = millis();
}

static void timer_checkbox_cb(lv_event_t * e) {
    if (suppress_timer_checkbox_event) return;

    lv_obj_t * cb = lv_event_get_target(e);
    if (!cb) return;

    bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    saved_use_timer = checked;
    save_bool_pref("use_timer", saved_use_timer);

    if (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable) {
        timer_remaining_seconds = saved_timer_seconds;
        timer_running = true;
        last_timer_tick_ms = millis();
        log_to_settings("Timer ENABLED");
    } else {
        timer_running = false;
        log_to_settings("Timer DISABLED");
    }

    last_activity_time = millis();
}

static void reset_energy_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    energy_kwh = 0.0;
    save_double_pref("energy", energy_kwh);
    update_energy_ui();
    log_to_settings("Energy counter reset");
    user_beep();
    last_activity_time = millis();
}

static void output_switch_cb(lv_event_t * e) {
    if (suppress_switch_event) return;

    lv_obj_t * sw = lv_event_get_target(e);
    if (!sw) return;

    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    psu.enableOutput(checked);
    saved_output_enable = checked;
    save_bool_pref("out_en", saved_output_enable);
    last_output_switch_state = checked;

    if (ui_VarOutputState) {
        ui_set_text_safe(ui_VarOutputState, checked ? "ON" : "OFF");
    }

    if (checked && saved_use_timer && saved_timer_seconds > 0) {
        timer_remaining_seconds = saved_timer_seconds;
        timer_running = true;
        last_timer_tick_ms = millis();
    }

    if (!checked) {
        timer_running = false;
    }

    log_to_settings(String("Output ") + (checked ? "ENABLED" : "DISABLED"));
    last_activity_time = millis();
}

static void fan_dropdown_cb(lv_event_t * e) {
    if (suppress_fan_event) return;

    lv_obj_t * dd = lv_event_get_target(e);
    if (!dd) return;

    uint16_t sel = lv_dropdown_get_selected(dd);

    bool manual = (sel == 1); // 0=Auto, 1=Manual
    fan_manual_mode = manual;
    saved_fan_manual = manual;
    save_bool_pref("fan_man", saved_fan_manual);

    psu.setFanMode(manual);

    log_to_settings(String("Fan mode: ") + (manual ? "MANUAL" : "AUTO"));
    last_activity_time = millis();
}

/* =========================================================
   SCREEN BUTTON BEEP PATCH
   No generated file edits required
   ========================================================= */
static void screen_nav_beep_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        user_beep();
    }
}

void find_and_hook_screen_buttons() {
    if (!lv_scr_act()) return;

    lv_obj_t *scr = lv_scr_act();
    uint32_t child_count = lv_obj_get_child_cnt(scr);

    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (!child) continue;

        lv_coord_t h = lv_obj_get_height(child);

        if (h >= 50 && h <= 90) {
            uint32_t cc = lv_obj_get_child_cnt(child);
            if (cc == 3) {
                lv_obj_t *c0 = lv_obj_get_child(child, 0);
                lv_obj_t *c1 = lv_obj_get_child(child, 1);
                lv_obj_t *c2 = lv_obj_get_child(child, 2);

                if (c0 && c1 && c2) {
                    g_screenButtonsComp = child;
                    g_btnHome = c0;
                    g_btnSetValues = c1;
                    g_btnSettings = c2;

                    lv_obj_add_event_cb(g_btnHome, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);
                    lv_obj_add_event_cb(g_btnSetValues, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);
                    lv_obj_add_event_cb(g_btnSettings, screen_nav_beep_cb, LV_EVENT_CLICKED, NULL);

                    Serial.println("Screen nav beep hooks attached");
                    return;
                }
            }
        }
    }

    Serial.println("ScreenButtons component not found");
}

/* =========================================================
   BACKLIGHT
   ========================================================= */
void handle_backlight() {
    unsigned long idle_time = millis() - last_activity_time;

    if (idle_time > off_timeout_ms && off_timeout_ms > 0) {
        if (current_backlight_pwm != 0) {
            analogWrite(BACKLIGHT_PIN, 0);
            current_backlight_pwm = 0;
        }
    }
    else if (idle_time > dim_timeout_ms && dim_timeout_ms > 0) {
        int full_brightness = map(saved_brightness_pct, 0, 100, 0, 255);
        int dim_pwm = full_brightness / 4;
        if (dim_pwm < 5) dim_pwm = 5;
        if (current_backlight_pwm != dim_pwm) {
            analogWrite(BACKLIGHT_PIN, dim_pwm);
            current_backlight_pwm = dim_pwm;
        }
    }
    else {
        int full_pwm = map(saved_brightness_pct, 0, 100, 0, 255);
        if (current_backlight_pwm != full_pwm) {
            analogWrite(BACKLIGHT_PIN, full_pwm);
            current_backlight_pwm = full_pwm;
        }
    }
}

/* =========================================================
   PSU TIMER RUNTIME
   ========================================================= */
void handle_psu_timer() {
    if (!saved_use_timer) return;
    if (!timer_running) return;
    if (!saved_output_enable) {
        timer_running = false;
        return;
    }

    if (millis() - last_timer_tick_ms >= 1000) {
        last_timer_tick_ms += 1000;

        if (timer_remaining_seconds > 0) {
            timer_remaining_seconds--;
        }

        if (ui_SetValues_TextAreaVarTimer && obj_is_textarea(ui_SetValues_TextAreaVarTimer)) {
            ui_set_text_safe(ui_SetValues_TextAreaVarTimer, format_hms(timer_remaining_seconds).c_str());
        }

        if (timer_remaining_seconds == 0) {
            psu.enableOutput(false);
            saved_output_enable = false;
            save_bool_pref("out_en", false);
            timer_running = false;

            suppress_switch_event = true;
            if (ui_Home_SwitchEnableOutput) {
                lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
            }
            suppress_switch_event = false;

            if (ui_VarOutputState) {
                ui_set_text_safe(ui_VarOutputState, "OFF");
            }

            log_to_settings("Timer expired -> PSU output OFF");
        }
    }
}

/* =========================================================
   CAN + UI
   ========================================================= */
void handle_can_and_ui() {
    static unsigned long last_can_req = 0;
    static unsigned long last_ui_update = 0;

    if (millis() - last_can_req > 200) {
        psu.sendRequest();
        last_can_req = millis();
    }

    psu.readAndDecodeResponse();
    PSUStatus status = psu.getStatus();

    handle_energy_meter(status.outputPower);

    if (millis() - last_ui_update < 100) return;
    last_ui_update = millis();

    char valBuffer[24];

    if (status.outputVoltage >= 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.outputVoltage);
        ui_set_text_safe(ui_VarVout, valBuffer);
    }

    if (status.outputCurrent >= 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.outputCurrent);
        ui_set_text_safe(ui_VarIout, valBuffer);
    }

    if (status.outputPower >= 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.outputPower);
        ui_set_text_safe(ui_VarPout, valBuffer);
    }

    if (status.inputTemp > -50.0f && status.inputTemp < 150.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.1f", status.inputTemp);
        ui_set_text_safe(ui_VarInputTemp, valBuffer);
    }

    if (status.outputTemp > -50.0f && status.outputTemp < 150.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.1f", status.outputTemp);
        ui_set_text_safe(ui_VarOutputTemp, valBuffer);
    }

    if (status.inputVoltage > 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.inputVoltage);
        ui_set_text_safe(ui_VarVin, valBuffer);
    }

    if (status.inputCurrent >= 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.inputCurrent);
        ui_set_text_safe(ui_VarIin, valBuffer);
    }

    if (status.inputPower >= 0.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.2f", status.inputPower);
        ui_set_text_safe(ui_VarPin, valBuffer);
    }

    if (status.efficiency >= 0.0f && status.efficiency <= 100.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.1f", status.efficiency);
        ui_set_text_safe(ui_VarEffi, valBuffer);
    }

    if (status.inputFreq >= 40.0f && status.inputFreq <= 70.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.1f", status.inputFreq);
        ui_set_text_safe(ui_VarFin, valBuffer);
    }

    suppress_switch_event = true;
    if (ui_Home_SwitchEnableOutput) {
        if (saved_output_enable) lv_obj_add_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
    }
    suppress_switch_event = false;

    ui_set_text_safe(ui_VarOutputState, saved_output_enable ? "ON" : "OFF");
}

void handle_serial_commands() {
    static String input = "";

    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            input.trim();
            if (input.length() == 0) {
                input = "";
                continue;
            }

            if (input == "h") {
                Serial.println("\n--- COMMANDS ---");
                Serial.println("h                : help");
                Serial.println("s                : status");
                Serial.println("on / off         : output control");
                Serial.println("von=XX.X         : set ONLINE voltage");
                Serial.println("voff=XX.X        : set OFFLINE voltage");
                Serial.println("ion=XX.X         : set ONLINE current");
                Serial.println("ioff=XX.X        : set OFFLINE current");
                Serial.println("fanauto          : fan AUTO mode");
                Serial.println("fanmanual        : fan MANUAL mode");
                Serial.println("timer=HH.MM.SS   : set timer");
                Serial.println("usetimer=0/1     : disable/enable timer");
                Serial.println("energyreset      : reset kWh counter");
                Serial.println("----------------\n");
            }

            else if (input == "s") {
                PSUStatus st = psu.getStatus();

                Serial.println("\n---- STATUS ----");
                Serial.println("Output: " + String(saved_output_enable ? "ON" : "OFF"));

                Serial.println("Von: " + String(saved_online_v, 2));
                Serial.println("Voff: " + String(saved_offline_v, 2));
                Serial.println("Ion: " + String(saved_online_i, 2));
                Serial.println("Ioff: " + String(saved_offline_i, 2));

                Serial.println("Fan: " + String(saved_fan_manual ? "MANUAL" : "AUTO"));

                Serial.println("Use Timer: " + String(saved_use_timer ? "YES" : "NO"));
                Serial.println("Timer set: " + format_hms(saved_timer_seconds));
                Serial.println("Remaining: " + format_hms(timer_remaining_seconds));

                Serial.println("Energy (kWh): " + format_energy_kwh(energy_kwh));

                Serial.println("--- LIVE ---");
                Serial.println("Vout: " + String(st.outputVoltage, 2));
                Serial.println("Iout: " + String(st.outputCurrent, 2));
                Serial.println("Pout: " + String(st.outputPower, 2));
                Serial.println("Temp(in/out): " + String(st.inputTemp, 1) + " / " + String(st.outputTemp, 1));

                Serial.println("----------------\n");
            }

            else if (input == "on") {
                psu.enableOutput(true);
                saved_output_enable = true;
                save_bool_pref("out_en", true);
                Serial.println("Output ENABLED");
            }
            else if (input == "off") {
                psu.enableOutput(false);
                saved_output_enable = false;
                save_bool_pref("out_en", false);
                Serial.println("Output DISABLED");
            }

            else if (input.startsWith("von=")) {
                float v = input.substring(4).toFloat();
                v = clampf(v, PSU_VMIN, PSU_VMAX);

                saved_online_v = v;
                save_float_pref("on_v", v);
                psu.setVoltage(v);

                Serial.println("Von set to " + String(v, 2) + " V");
            }

            else if (input.startsWith("voff=")) {
                float v = input.substring(5).toFloat();
                v = clampf(v, PSU_VOFFLINE_MIN, PSU_VMAX);

                saved_offline_v = v;
                save_float_pref("off_v", v);
                psu.setOfflineVoltage(v);

                Serial.println("Voff set to " + String(v, 2) + " V");
            }

            else if (input.startsWith("ion=")) {
                float i = input.substring(4).toFloat();
                i = clampf(i, PSU_IMIN, PSU_IMAX);

                saved_online_i = i;
                save_float_pref("on_i", i);
                psu.setCurrent(i);

                Serial.println("Ion set to " + String(i, 2) + " A");
            }

            else if (input.startsWith("ioff=")) {
                float i = input.substring(5).toFloat();
                i = clampf(i, PSU_IMIN, PSU_IMAX);

                saved_offline_i = i;
                save_float_pref("off_i", i);
                psu.setOfflineCurrent(i);

                Serial.println("Ioff set to " + String(i, 2) + " A");
            }

            else if (input == "fanauto") {
                saved_fan_manual = false;
                save_bool_pref("fan_man", false);
                psu.setFanMode(false);
                Serial.println("Fan AUTO");
            }

            else if (input == "fanmanual") {
                saved_fan_manual = true;
                save_bool_pref("fan_man", true);
                psu.setFanMode(true);
                Serial.println("Fan MANUAL");
            }

            else if (input.startsWith("timer=")) {
                uint32_t secs;
                String val = input.substring(6);

                if (parse_hms_string(val, secs)) {
                    saved_timer_seconds = secs;
                    timer_remaining_seconds = secs;
                    timer_running = (saved_use_timer && secs > 0 && saved_output_enable);
                    save_uint_pref("timer_sec", secs);

                    Serial.println("Timer set to " + format_hms(secs));
                } else {
                    Serial.println("Invalid format. Use HH.MM.SS");
                }
            }

            else if (input.startsWith("usetimer=")) {
                int v = input.substring(9).toInt();
                saved_use_timer = (v != 0);
                save_bool_pref("use_timer", saved_use_timer);
                timer_running = (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable);
                Serial.println(String("Use timer: ") + (saved_use_timer ? "YES" : "NO"));
            }

            else if (input == "energyreset") {
                energy_kwh = 0.0;
                save_double_pref("energy", energy_kwh);
                Serial.println("Energy counter reset");
            }

            else {
                Serial.println("Unknown command (type h)");
            }

            input = "";
        } else {
            input += c;
        }
    }
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n=== ESP32-S3 Huawei PSU GUI Boot ===");

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BACKLIGHT_PIN, OUTPUT);

    analogWrite(BACKLIGHT_PIN, 255);
    current_backlight_pwm = 255;
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

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    touch_init(TP_SDA, TP_SCL, TP_RST, TP_INT);

    ui_init();

    preferences.begin("psuui", true);

    saved_online_v       = preferences.getFloat("on_v", 53.50f);
    saved_offline_v      = preferences.getFloat("off_v", 53.50f);
    saved_online_i       = preferences.getFloat("on_i", 10.00f);
    saved_offline_i      = preferences.getFloat("off_i", 10.00f);

    saved_brightness_pct = preferences.getInt("bright", 100);
    saved_sleep_sec      = preferences.getInt("sleep", 30);
    saved_dim_sec        = preferences.getInt("dim", max(3, saved_sleep_sec / 2));

    saved_output_enable  = preferences.getBool("out_en", true);
    saved_fan_manual     = preferences.getBool("fan_man", false);

    saved_timer_seconds  = preferences.getUInt("timer_sec", 0);
    saved_use_timer      = preferences.getBool("use_timer", false);

    energy_kwh           = preferences.getDouble("energy", 0.0);

    preferences.end();

    saved_brightness_pct = constrain(saved_brightness_pct, 1, 100);
    saved_sleep_sec      = constrain(saved_sleep_sec, 5, 3600);
    saved_dim_sec        = constrain(saved_dim_sec, 3, saved_sleep_sec);

    dim_timeout_ms = (unsigned long)saved_dim_sec * 1000UL;
    off_timeout_ms = (unsigned long)saved_sleep_sec * 1000UL;

    timer_remaining_seconds = saved_timer_seconds;
    timer_running = (saved_use_timer && saved_timer_seconds > 0 && saved_output_enable);
    last_timer_tick_ms = millis();

    save_current_settings_to_ui();

    if (ui_Settings_DropdownFanControll) {
        lv_dropdown_set_options(ui_Settings_DropdownFanControll, "Auto\nManual");

        suppress_fan_event = true;
        lv_dropdown_set_selected(ui_Settings_DropdownFanControll, saved_fan_manual ? 1 : 0);
        suppress_fan_event = false;

        fan_manual_mode = saved_fan_manual;
    }

    if (ui_Home_SwitchEnableOutput) {
        suppress_switch_event = true;
        if (saved_output_enable) lv_obj_add_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_Home_SwitchEnableOutput, LV_STATE_CHECKED);
        suppress_switch_event = false;

        last_output_switch_state = saved_output_enable;
    }

    if (ui_SetValues_CheckboxUseTimer) {
        suppress_timer_checkbox_event = true;
        if (saved_use_timer) lv_obj_add_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui_SetValues_CheckboxUseTimer, LV_STATE_CHECKED);
        suppress_timer_checkbox_event = false;
    }

    if (ui_VarOutputState) {
        ui_set_text_safe(ui_VarOutputState, saved_output_enable ? "ON" : "OFF");
    }

    {
        int pwm = map(saved_brightness_pct, 0, 100, 0, 255);
        analogWrite(BACKLIGHT_PIN, pwm);
        current_backlight_pwm = pwm;
    }

    if (ui_VarOnlineVout) {
        lv_obj_add_event_cb(ui_VarOnlineVout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineVout) {
        lv_obj_add_event_cb(ui_VarOfflineVout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOnlineIout) {
        lv_obj_add_event_cb(ui_VarOnlineIout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineIout) {
        lv_obj_add_event_cb(ui_VarOfflineIout, apply_psu_settings_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Settings_TextAreaSleep) {
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, settings_numeric_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Settings_SliderBrightness) {
        lv_slider_set_range(ui_Settings_SliderBrightness, 1, 100);
        lv_slider_set_value(ui_Settings_SliderBrightness, saved_brightness_pct, LV_ANIM_OFF);
        lv_obj_add_event_cb(ui_Settings_SliderBrightness, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_SetValues_TextAreaVarTimer) {
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, timer_setting_cb, LV_EVENT_READY, NULL);
    }

    if (ui_SetValues_CheckboxUseTimer) {
        lv_obj_add_event_cb(ui_SetValues_CheckboxUseTimer, timer_checkbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Settings_ButtonResetEnergy) {
        lv_obj_add_event_cb(ui_Settings_ButtonResetEnergy, reset_energy_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_Home_SwitchEnableOutput) {
        lv_obj_add_event_cb(ui_Home_SwitchEnableOutput, output_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Settings_DropdownFanControll) {
        lv_obj_add_event_cb(ui_Settings_DropdownFanControll, fan_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* --- Existing beep events --- */
    if (ui_Home_SwitchEnableOutput) {
        lv_obj_add_event_cb(ui_Home_SwitchEnableOutput, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Settings_DropdownFanControll) {
        lv_obj_add_event_cb(ui_Settings_DropdownFanControll, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_SetValues_CheckboxUseTimer) {
        lv_obj_add_event_cb(ui_SetValues_CheckboxUseTimer, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_Settings_ButtonResetEnergy) {
        lv_obj_add_event_cb(ui_Settings_ButtonResetEnergy, user_beep_cb, LV_EVENT_CLICKED, NULL);
    }

    if (ui_VarOnlineVout) {
        lv_obj_add_event_cb(ui_VarOnlineVout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOnlineVout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineVout) {
        lv_obj_add_event_cb(ui_VarOfflineVout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOfflineVout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOnlineIout) {
        lv_obj_add_event_cb(ui_VarOnlineIout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOnlineIout, user_beep_cb, LV_EVENT_READY, NULL);
    }
    if (ui_VarOfflineIout) {
        lv_obj_add_event_cb(ui_VarOfflineIout, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_VarOfflineIout, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Settings_TextAreaSleep) {
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_Settings_TextAreaSleep, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_SetValues_TextAreaVarTimer) {
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, user_beep_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ui_SetValues_TextAreaVarTimer, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Settings_SliderBrightness) {
        lv_obj_add_event_cb(ui_Settings_SliderBrightness, user_beep_cb, LV_EVENT_PRESSED, NULL);
    }

    if (ui_Numpad) {
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Numpad, user_beep_cb, LV_EVENT_READY, NULL);
    }

    if (ui_Numpad2) {
        lv_obj_add_event_cb(ui_Numpad2, user_beep_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Numpad2, user_beep_cb, LV_EVENT_READY, NULL);
    }

    /* --- Attach reliable beep hooks to screen nav buttons --- */
    find_and_hook_screen_buttons();

    apply_saved_psu_settings();

    update_energy_ui();
    last_energy_update_ms = millis();
    last_energy_save_ms = millis();

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
}