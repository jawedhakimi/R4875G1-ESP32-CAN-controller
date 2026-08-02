#include "web_server.h"
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"
#include "app_state.h"
#include "psu_control.h"
#include "psu_timer.h"
#include "psu_profile.h"
#include "energy_meter.h"
#include "web_page.h"

static WebServer server(WEB_SERVER_PORT);

/* ---------------------------------------------------------------------
   Auth + response helpers
   --------------------------------------------------------------------- */

// Returns false (and has already sent a 401) if the request isn't
// authenticated -- callers should return immediately in that case.
static bool require_auth() {
    if (!server.authenticate(WEB_AUTH_USER, WEB_AUTH_PASS)) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

static void send_json(JsonDocument &doc) {
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void send_error(int code, const char *msg) {
    JsonDocument doc;
    doc["error"] = msg;
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

// Parses the POST body (server.arg("plain")) as JSON into doc. Sends a 400
// and returns false on malformed input.
static bool parse_body(JsonDocument &doc) {
    String body = server.arg("plain");
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        send_error(400, "Malformed JSON body");
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------
   Route handlers
   --------------------------------------------------------------------- */

static void handle_root() {
    if (!require_auth()) return;
    server.send_P(200, "text/html", WEB_INDEX_HTML);
}

static void handle_status() {
    if (!require_auth()) return;

    PSUStatus st = psu.getStatus();
    bool link_ok = !psu.isStale();

    JsonDocument doc;
    doc["link_ok"] = link_ok;

    // Telemetry reads as null (not a stale/misleading last value) once the
    // CAN link has been down for a while -- matches the touchscreen's
    // "--" / "NO LINK" behavior in can_bridge.cpp.
    if (link_ok) {
        doc["vout"] = st.outputVoltage;
        doc["iout"] = st.outputCurrent;
        doc["pout"] = st.outputPower;
        doc["vin"]  = st.inputVoltage;
        doc["iin"]  = st.inputCurrent;
        doc["pin"]  = st.inputPower;
        doc["efficiency"] = st.efficiency;
        doc["freq"] = st.inputFreq;
    } else {
        doc["vout"] = nullptr;
        doc["iout"] = nullptr;
        doc["pout"] = nullptr;
        doc["vin"]  = nullptr;
        doc["iin"]  = nullptr;
        doc["pin"]  = nullptr;
        doc["efficiency"] = nullptr;
        doc["freq"] = nullptr;
    }

    doc["output_on"] = saved_output_enable;
    doc["fan_manual"] = saved_fan_manual;

    doc["online_v"]  = saved_online_v;
    doc["offline_v"] = saved_offline_v;
    doc["online_i"]  = saved_online_i;
    doc["offline_i"] = saved_offline_i;

    doc["energy_kwh"] = energy_kwh;

    doc["timer_enabled"] = saved_use_timer;
    doc["timer_set"] = format_hms(saved_timer_seconds);
    doc["timer_remaining"] = (saved_use_timer && timer_running)
        ? format_hms(timer_remaining_seconds)
        : "--";

    // Lightweight profile run-state, polled at the same 500ms cadence as
    // everything else above. The full point arrays are NOT included here
    // -- those only change on an explicit save, so they're fetched
    // separately via GET /api/profile instead of on every poll.
    doc["profile_running"] = psu_profile_is_running();
    doc["profile_elapsed_sec"] = psu_profile_elapsed_sec();
    doc["profile_duration_sec"] = psu_profile_duration_sec();

    send_json(doc);
}

static void handle_output() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    psu_set_output(doc["on"].as<bool>());

    JsonDocument resp;
    resp["output_on"] = saved_output_enable;
    send_json(resp);
}

static void handle_fan() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    psu_set_fan_manual(doc["manual"].as<bool>());

    JsonDocument resp;
    resp["fan_manual"] = saved_fan_manual;
    send_json(resp);
}

static void handle_voltage_online() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    float v = psu_set_online_voltage(doc["value"].as<float>());

    JsonDocument resp;
    resp["value"] = v;
    send_json(resp);
}

static void handle_voltage_offline() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    float v = psu_set_offline_voltage(doc["value"].as<float>());

    JsonDocument resp;
    resp["value"] = v;
    send_json(resp);
}

static void handle_current_online() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    float i = psu_set_online_current(doc["value"].as<float>());

    JsonDocument resp;
    resp["value"] = i;
    send_json(resp);
}

static void handle_current_offline() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    float i = psu_set_offline_current(doc["value"].as<float>());

    JsonDocument resp;
    resp["value"] = i;
    send_json(resp);
}

static void handle_timer_set() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    String hms = doc["hms"].as<String>();
    uint32_t secs;
    bool ok = parse_hms_string(hms, secs);
    if (ok) {
        psu_timer_configure(secs);
    }

    JsonDocument resp;
    resp["ok"] = ok;
    resp["timer_set"] = format_hms(saved_timer_seconds);
    send_json(resp);
}

static void handle_timer_enabled() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    psu_timer_set_enabled(doc["enabled"].as<bool>());

    JsonDocument resp;
    resp["enabled"] = saved_use_timer;
    send_json(resp);
}

static void handle_energy_reset() {
    if (!require_auth()) return;
    energy_reset();

    JsonDocument resp;
    resp["ok"] = true;
    send_json(resp);
}

static void handle_profile_get() {
    if (!require_auth()) return;

    JsonDocument doc;
    doc["duration_sec"] = psu_profile_duration_sec();
    doc["running"] = psu_profile_is_running();
    doc["elapsed_sec"] = psu_profile_elapsed_sec();

    JsonArray va = doc["voltage_points"].to<JsonArray>();
    const ProfilePoint *vp = psu_profile_voltage_points();
    for (int i = 0; i < psu_profile_voltage_count(); i++) {
        JsonObject o = va.add<JsonObject>();
        o["t"] = vp[i].t;
        o["v"] = vp[i].value;
    }

    JsonArray ia = doc["current_points"].to<JsonArray>();
    const ProfilePoint *ip = psu_profile_current_points();
    for (int i = 0; i < psu_profile_current_count(); i++) {
        JsonObject o = ia.add<JsonObject>();
        o["t"] = ip[i].t;
        o["v"] = ip[i].value;
    }

    send_json(doc);
}

// Reads a JSON array of {t, v} objects into a fixed-size ProfilePoint
// buffer. Returns false (fills *error) if there are more than `max`.
static bool parse_profile_points(JsonArray arr, ProfilePoint *out, int max, int &count, String &error) {
    count = 0;
    for (JsonVariant v : arr) {
        if (count >= max) {
            error = "Too many points (max " + String(max) + " per curve).";
            return false;
        }
        JsonObject o = v.as<JsonObject>();
        out[count].t = o["t"] | 0;
        out[count].value = o["v"] | 0.0f;
        count++;
    }
    return true;
}

static void handle_profile_set() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    uint32_t duration = doc["duration_sec"] | 0;

    ProfilePoint vpts[PSU_PROFILE_MAX_POINTS];
    ProfilePoint ipts[PSU_PROFILE_MAX_POINTS];
    int vcount = 0, icount = 0;
    String error;

    if (!parse_profile_points(doc["voltage_points"].as<JsonArray>(), vpts, PSU_PROFILE_MAX_POINTS, vcount, error) ||
        !parse_profile_points(doc["current_points"].as<JsonArray>(), ipts, PSU_PROFILE_MAX_POINTS, icount, error)) {
        send_error(400, error.c_str());
        return;
    }

    if (!psu_profile_set(duration, vpts, vcount, ipts, icount, error)) {
        send_error(409, error.c_str());
        return;
    }

    JsonDocument resp;
    resp["ok"] = true;
    send_json(resp);
}

static void handle_profile_run() {
    if (!require_auth()) return;
    JsonDocument doc;
    if (!parse_body(doc)) return;

    bool run = doc["run"] | false;
    bool ok = true;
    if (run) {
        ok = psu_profile_start();
    } else {
        psu_profile_stop();
    }

    JsonDocument resp;
    resp["ok"] = ok;
    resp["running"] = psu_profile_is_running();
    resp["elapsed_sec"] = psu_profile_elapsed_sec();
    send_json(resp);
}

/* ---------------------------------------------------------------------
   Browser-based OTA upload -- lets you flash a compiled .bin from the
   "Firmware Update" panel on the web page without needing PlatformIO/
   Arduino IDE installed on whatever machine you're using (see ota.cpp
   for the PlatformIO-network-upload alternative). Auth is checked in
   UPLOAD_FILE_START, before Update.begin() -- an unauthenticated POST
   never gets as far as writing to flash. --------------------------------- */
static bool s_update_authorized = false;

static void handle_update_upload() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        s_update_authorized = server.authenticate(WEB_AUTH_USER, WEB_AUTH_PASS);
        if (!s_update_authorized) {
            Serial.println("OTA (web): unauthorized upload attempt, ignoring.");
            return;
        }
        Serial.printf("OTA (web): receiving \"%s\"\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!s_update_authorized) return;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!s_update_authorized) return;
        if (Update.end(true)) {
            Serial.printf("OTA (web): update OK, %u bytes -- rebooting\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        Serial.println("OTA (web): upload aborted");
    }
}

// Runs once the upload body above has been fully received/written.
static void handle_update_result() {
    server.sendHeader("Connection", "close");

    if (!s_update_authorized) {
        server.requestAuthentication();
        return;
    }
    if (Update.hasError()) {
        server.send(200, "text/plain", "Update FAILED -- device NOT rebooting. Check the serial log.");
        return;
    }

    server.send(200, "text/plain", "Update OK -- rebooting now.");
    delay(200); // give the response time to actually go out before restarting
    ESP.restart();
}

static void handle_not_found() {
    server.send(404, "text/plain", "Not found");
}

/* ---------------------------------------------------------------------
   Public API
   --------------------------------------------------------------------- */

void web_server_begin() {
    server.on("/", HTTP_GET, handle_root);
    server.on("/api/status", HTTP_GET, handle_status);

    server.on("/api/output", HTTP_POST, handle_output);
    server.on("/api/fan", HTTP_POST, handle_fan);

    server.on("/api/voltage/online", HTTP_POST, handle_voltage_online);
    server.on("/api/voltage/offline", HTTP_POST, handle_voltage_offline);
    server.on("/api/current/online", HTTP_POST, handle_current_online);
    server.on("/api/current/offline", HTTP_POST, handle_current_offline);

    server.on("/api/timer", HTTP_POST, handle_timer_set);
    server.on("/api/timer/enabled", HTTP_POST, handle_timer_enabled);

    server.on("/api/energy/reset", HTTP_POST, handle_energy_reset);

    server.on("/api/profile", HTTP_GET, handle_profile_get);
    server.on("/api/profile", HTTP_POST, handle_profile_set);
    server.on("/api/profile/run", HTTP_POST, handle_profile_run);

    server.on("/update", HTTP_POST, handle_update_result, handle_update_upload);

    server.onNotFound(handle_not_found);
    server.begin();
}

void web_server_loop() {
    server.handleClient();
}
