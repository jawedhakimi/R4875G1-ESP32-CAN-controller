#include "psu_profile.h"
#include <stdlib.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"
#include "psu_control.h"
#include "psu_timer.h"   // format_hms()
#include "ui_safe.h"     // log_to_settings()

static Preferences profilePrefs;

static uint32_t duration_sec = 0;

static ProfilePoint voltage_pts[PSU_PROFILE_MAX_POINTS];
static int voltage_count = 0;

static ProfilePoint current_pts[PSU_PROFILE_MAX_POINTS];
static int current_count = 0;

static bool running = false;
static uint32_t elapsed_sec = 0;
static unsigned long last_tick_ms = 0;

static int cmp_point_by_time(const void *a, const void *b) {
    uint32_t ta = ((const ProfilePoint *)a)->t;
    uint32_t tb = ((const ProfilePoint *)b)->t;
    if (ta < tb) return -1;
    if (ta > tb) return 1;
    return 0;
}

// Linear interpolation within the curve; holds flat before the first
// point and after the last one. Returns NAN if the curve has no points --
// callers must check count > 0 first.
static float interpolate(const ProfilePoint *pts, int count, uint32_t t) {
    if (count <= 0) return NAN;
    if (count == 1 || t <= pts[0].t) return pts[0].value;
    if (t >= pts[count - 1].t) return pts[count - 1].value;

    for (int i = 0; i < count - 1; i++) {
        if (t >= pts[i].t && t <= pts[i + 1].t) {
            uint32_t span = pts[i + 1].t - pts[i].t;
            if (span == 0) return pts[i + 1].value;
            float frac = (float)(t - pts[i].t) / (float)span;
            return pts[i].value + frac * (pts[i + 1].value - pts[i].value);
        }
    }
    return pts[count - 1].value; // unreachable, defensive
}

static void persist_profile() {
    JsonDocument doc;
    doc["duration"] = duration_sec;

    JsonArray va = doc["v"].to<JsonArray>();
    for (int i = 0; i < voltage_count; i++) {
        JsonObject o = va.add<JsonObject>();
        o["t"] = voltage_pts[i].t;
        o["v"] = voltage_pts[i].value;
    }

    JsonArray ia = doc["i"].to<JsonArray>();
    for (int i = 0; i < current_count; i++) {
        JsonObject o = ia.add<JsonObject>();
        o["t"] = current_pts[i].t;
        o["v"] = current_pts[i].value;
    }

    String out;
    serializeJson(doc, out);

    // Read-write begin() creates the "profile" namespace on first use --
    // unlike a read-only begin(), it doesn't need the NOT_FOUND guard.
    profilePrefs.begin("profile", false);
    profilePrefs.putString("def", out);
    profilePrefs.end();
}

void psu_profile_load() {
    duration_sec = 0;
    voltage_count = 0;
    current_count = 0;

    // Opening read-only FAILS (NOT_FOUND) if the "profile" namespace has
    // never been written -- true until the first profile is ever saved.
    // That's normal, not an error -- see wifi_manager.cpp for the same
    // pattern and why it matters (touching the Preferences object after a
    // failed begin() corrupts NVS state and crashes the boot).
    String json;
    if (profilePrefs.begin("profile", true)) {
        json = profilePrefs.getString("def", "");
        profilePrefs.end();
    }
    if (json.length() == 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return; // corrupt/empty -- start fresh

    duration_sec = doc["duration"] | 0;

    for (JsonVariant v : doc["v"].as<JsonArray>()) {
        if (voltage_count >= PSU_PROFILE_MAX_POINTS) break;
        JsonObject o = v.as<JsonObject>();
        voltage_pts[voltage_count].t = o["t"] | 0;
        voltage_pts[voltage_count].value = o["v"] | 0.0f;
        voltage_count++;
    }
    for (JsonVariant v : doc["i"].as<JsonArray>()) {
        if (current_count >= PSU_PROFILE_MAX_POINTS) break;
        JsonObject o = v.as<JsonObject>();
        current_pts[current_count].t = o["t"] | 0;
        current_pts[current_count].value = o["v"] | 0.0f;
        current_count++;
    }
}

bool psu_profile_set(uint32_t new_duration,
                      const ProfilePoint *v_pts, int v_count,
                      const ProfilePoint *i_pts, int i_count,
                      String &error) {
    if (running) {
        error = "Stop the profile before editing it.";
        return false;
    }
    if (new_duration == 0) {
        error = "Duration must be greater than zero.";
        return false;
    }
    if (v_count > PSU_PROFILE_MAX_POINTS || i_count > PSU_PROFILE_MAX_POINTS) {
        error = "Too many points (max " + String(PSU_PROFILE_MAX_POINTS) + " per curve).";
        return false;
    }

    duration_sec = new_duration;

    voltage_count = v_count;
    for (int i = 0; i < v_count; i++) {
        voltage_pts[i].t = min(v_pts[i].t, duration_sec);
        voltage_pts[i].value = clampf(v_pts[i].value, PSU_VMIN, PSU_VMAX);
    }
    qsort(voltage_pts, voltage_count, sizeof(ProfilePoint), cmp_point_by_time);

    current_count = i_count;
    for (int i = 0; i < i_count; i++) {
        current_pts[i].t = min(i_pts[i].t, duration_sec);
        current_pts[i].value = clampf(i_pts[i].value, PSU_IMIN, PSU_IMAX);
    }
    qsort(current_pts, current_count, sizeof(ProfilePoint), cmp_point_by_time);

    persist_profile();
    return true;
}

bool psu_profile_start() {
    if (running) return true;
    if (voltage_count == 0 && current_count == 0) return false;

    running = true;
    elapsed_sec = 0;
    last_tick_ms = millis();

    log_to_settings("Profile started (" + format_hms(duration_sec) + ").");
    return true;
}

void psu_profile_stop() {
    if (!running) return;
    running = false;
    elapsed_sec = 0;
    log_to_settings("Profile stopped.");
}

void handle_psu_profile() {
    if (!running) return;

    unsigned long now = millis();
    uint32_t dt = (uint32_t)((now - last_tick_ms) / 1000UL);
    if (dt == 0) return; // wait for a full second to pass -- no point hammering CAN faster than that
    last_tick_ms += dt * 1000UL;
    elapsed_sec += dt;

    bool finished = elapsed_sec >= duration_sec;
    if (finished) elapsed_sec = duration_sec;

    // persist=false: this can tick once a second for the whole profile
    // duration -- see psu_control.h for why that must not hit flash.
    if (voltage_count > 0) {
        psu_set_online_voltage(interpolate(voltage_pts, voltage_count, elapsed_sec), false);
    }
    if (current_count > 0) {
        psu_set_online_current(interpolate(current_pts, current_count, elapsed_sec), false);
    }

    if (finished) {
        running = false;

        // One final persist=true apply -- the resting setpoint the
        // profile leaves behind is saved same as any manually-entered
        // one, so it survives a reboot and is what apply_saved_psu_settings()
        // pushes back out next boot.
        if (voltage_count > 0) {
            psu_set_online_voltage(interpolate(voltage_pts, voltage_count, elapsed_sec), true);
        }
        if (current_count > 0) {
            psu_set_online_current(interpolate(current_pts, current_count, elapsed_sec), true);
        }

        elapsed_sec = 0;
        log_to_settings("Profile finished.");
    }
}

uint32_t psu_profile_duration_sec() { return duration_sec; }
int psu_profile_voltage_count() { return voltage_count; }
int psu_profile_current_count() { return current_count; }
const ProfilePoint *psu_profile_voltage_points() { return voltage_pts; }
const ProfilePoint *psu_profile_current_points() { return current_pts; }
bool psu_profile_is_running() { return running; }
uint32_t psu_profile_elapsed_sec() { return elapsed_sec; }
