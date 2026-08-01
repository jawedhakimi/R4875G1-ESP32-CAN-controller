#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ui.h>
#include "app_state.h"
#include "ui_safe.h"

static Preferences wifiPrefs;

enum WifiConnectState { WIFI_MGR_IDLE, WIFI_MGR_CONNECTING };
static WifiConnectState state = WIFI_MGR_IDLE;
static unsigned long connect_start_ms = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 10000UL;
static String current_ssid;
static bool ever_connected = false;

static void wifi_log(const String &msg) {
    Serial.println("[WiFi] " + msg);
    append_log_to_textarea(ui_Connectivity_TextAreaNetworkLog, msg);
}

static void save_credentials(const String &ssid, const String &password) {
    wifiPrefs.begin("wifi", false);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("pass", password);
    wifiPrefs.end();
}

static void start_connect(const String &ssid, const String &password) {
    if (ssid.length() == 0) {
        wifi_log("No SSID entered.");
        return;
    }

    current_ssid = ssid;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    state = WIFI_MGR_CONNECTING;
    connect_start_ms = millis();
    wifi_log("Connecting to \"" + ssid + "\"...");
}

void wifi_manager_begin() {
    // WiFi.mode() is what actually brings up the underlying lwIP/TCP-IP
    // stack (starts the tcpip task, creates its sync semaphores). Do this
    // unconditionally, even with no saved credentials -- web_server_begin()
    // runs right after this in setup() and opens a listening socket, which
    // needs that stack to already exist. Without saved credentials this
    // used to skip WiFi.mode() entirely, leaving the stack uninitialized
    // and crashing web_server_begin() on its first socket call.
    WiFi.mode(WIFI_STA);

    // Opening read-only FAILS (NOT_FOUND) if the "wifi" namespace has never
    // been written -- true on every first boot, before any credentials are
    // saved. That's normal, not an error: skip straight to the "no saved
    // credentials" path instead of touching the unopened Preferences object
    // (calling getString()/end() on it after a failed begin() corrupts NVS
    // state and crashes the boot -- this is what the earlier fix addressed).
    String ssid, pass;
    if (wifiPrefs.begin("wifi", true)) {
        ssid = wifiPrefs.getString("ssid", "");
        pass = wifiPrefs.getString("pass", "");
        wifiPrefs.end();
    }

    if (ssid.length() > 0) {
        ui_set_text_safe(ui_Connectivity_TextAreaSSID, ssid.c_str());
        ui_set_text_safe(ui_Connectivity_TextAreaPassword, pass.c_str());
        start_connect(ssid, pass);
    } else {
        wifi_log("Enter a WiFi network SSID and password, then press OK.");
    }
}

static void password_ready_cb(lv_event_t *e) {
    (void)e;

    String ssid = ui_get_text_safe(ui_Connectivity_TextAreaSSID);
    String pass = ui_get_text_safe(ui_Connectivity_TextAreaPassword);
    ssid.trim();

    if (ssid.length() == 0) {
        wifi_log("Enter a network SSID first.");
        return;
    }

    save_credentials(ssid, pass);
    start_connect(ssid, pass);
    last_activity_time = millis();
}

void wifi_manager_register_callbacks() {
    // The generated ui_event_Connectivity_TextAreaPassword handler only
    // shows/targets the keyboard on CLICKED -- this is a separate callback
    // for LV_EVENT_READY (keyboard "OK"), so both coexist without conflict.
    if (ui_Connectivity_TextAreaPassword) {
        lv_obj_add_event_cb(ui_Connectivity_TextAreaPassword, password_ready_cb, LV_EVENT_READY, NULL);
    }
}

void wifi_manager_loop() {
    static unsigned long last_retry_ms = 0;

    if (state == WIFI_MGR_CONNECTING) {
        wl_status_t st = WiFi.status();

        if (st == WL_CONNECTED) {
            state = WIFI_MGR_IDLE;
            ever_connected = true;
            wifi_log("Connected. IP address: " + WiFi.localIP().toString());
        } else if (millis() - connect_start_ms > WIFI_CONNECT_TIMEOUT_MS) {
            state = WIFI_MGR_IDLE;
            wifi_log("Connection to \"" + current_ssid + "\" failed or timed out.");
        }
        return;
    }

    // Previously connected this session but the link dropped (router
    // reboot, out of range, etc.) -- retry periodically with the same
    // credentials rather than requiring the user to re-enter them.
    if (ever_connected && WiFi.status() != WL_CONNECTED) {
        if (millis() - last_retry_ms > WIFI_RETRY_INTERVAL_MS) {
            last_retry_ms = millis();
            wifi_log("Connection lost, retrying...");
            WiFi.reconnect();
        }
    }
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifi_ip_address() {
    if (WiFi.status() != WL_CONNECTED) return "";
    return WiFi.localIP().toString();
}
