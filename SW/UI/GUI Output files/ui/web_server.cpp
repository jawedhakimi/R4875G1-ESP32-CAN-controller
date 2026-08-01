#include "web_server.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "app_state.h"
#include "psu_control.h"
#include "psu_timer.h"
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

    server.onNotFound(handle_not_found);
    server.begin();
}

void web_server_loop() {
    server.handleClient();
}
