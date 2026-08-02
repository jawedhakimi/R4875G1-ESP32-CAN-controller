#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ui.h>
#include "config.h"
#include "app_state.h"
#include "ui_safe.h"

static Preferences wifiPrefs;

enum WifiConnectState { WIFI_MGR_IDLE, WIFI_MGR_CONNECTING };
static WifiConnectState state = WIFI_MGR_IDLE;
static unsigned long connect_start_ms = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 10000UL;
static String current_ssid;
static String current_pass;
static bool ever_connected = false;

/* ---------------------------------------------------------------------
   Saved-network list. Persisted as one JSON array (key "networks", "wifi"
   namespace) rather than numbered keys -- simpler to add/remove/rewrite as
   a unit, and ArduinoJson is already vendored for the web API. --------- */
#define WIFI_MAX_SAVED_NETWORKS 8

struct SavedNetwork {
    String ssid;
    String pass;
};

static SavedNetwork saved_networks[WIFI_MAX_SAVED_NETWORKS];
static int saved_network_count = 0;

static void wifi_log(const String &msg) {
    Serial.println("[WiFi] " + msg);
    append_log_to_textarea(ui_Connectivity_TextAreaNetworkLog, msg);
}

static int find_saved_network_index(const String &ssid) {
    for (int i = 0; i < saved_network_count; i++) {
        if (saved_networks[i].ssid == ssid) return i;
    }
    return -1;
}

static void refresh_network_dropdown() {
    if (!ui_Connectivity_DropdownSavednetworks) return;

    String options;
    if (saved_network_count == 0) {
        options = "No saved networks";
    } else {
        for (int i = 0; i < saved_network_count; i++) {
            if (i > 0) options += "\n";
            options += saved_networks[i].ssid;
        }
    }
    lv_dropdown_set_options(ui_Connectivity_DropdownSavednetworks, options.c_str());
}

static void persist_saved_networks() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < saved_network_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = saved_networks[i].ssid;
        o["pass"] = saved_networks[i].pass;
    }

    String out;
    serializeJson(doc, out);

    // Read-write begin() creates the "wifi" namespace on first use if it
    // doesn't exist yet -- unlike the read-only begin() used elsewhere,
    // this one doesn't need the NOT_FOUND guard.
    wifiPrefs.begin("wifi", false);
    wifiPrefs.putString("networks", out);
    wifiPrefs.end();
}

// Adds a new saved network, or updates the password if it's already saved.
// Only touches flash when something actually changed (e.g. reconnecting to
// an already-saved network on every boot shouldn't rewrite NVS every time).
static void upsert_saved_network(const String &ssid, const String &pass) {
    int idx = find_saved_network_index(ssid);
    if (idx >= 0) {
        if (saved_networks[idx].pass != pass) {
            saved_networks[idx].pass = pass;
            persist_saved_networks();
        }
        return;
    }

    if (saved_network_count >= WIFI_MAX_SAVED_NETWORKS) {
        wifi_log("Saved network list is full (" + String(WIFI_MAX_SAVED_NETWORKS) +
                  ") -- delete one from the list to save a new one.");
        return;
    }

    saved_networks[saved_network_count].ssid = ssid;
    saved_networks[saved_network_count].pass = pass;
    saved_network_count++;
    persist_saved_networks();
    refresh_network_dropdown();
}

// Opening read-only FAILS (NOT_FOUND) if the "wifi" namespace has never
// been written -- true on every first boot. That's normal, not an error:
// calling get*()/end() on a Preferences object after a failed begin()
// corrupts NVS state and crashes the boot (see the earlier fix), so every
// read here is guarded by begin()'s return value.
static void load_saved_networks() {
    saved_network_count = 0;

    String json;
    String legacy_ssid, legacy_pass;
    bool have_legacy = false;

    if (wifiPrefs.begin("wifi", true)) {
        json = wifiPrefs.getString("networks", "");
        if (json.length() == 0) {
            // Migrate the old single ssid/pass keys, from before this
            // device supported more than one saved network.
            legacy_ssid = wifiPrefs.getString("ssid", "");
            legacy_pass = wifiPrefs.getString("pass", "");
            have_legacy = legacy_ssid.length() > 0;
        }
        wifiPrefs.end();
    }

    if (json.length() > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, json)) {
            for (JsonVariant v : doc.as<JsonArray>()) {
                if (saved_network_count >= WIFI_MAX_SAVED_NETWORKS) break;
                JsonObject o = v.as<JsonObject>();
                saved_networks[saved_network_count].ssid = o["ssid"] | "";
                saved_networks[saved_network_count].pass = o["pass"] | "";
                saved_network_count++;
            }
        }
    } else if (have_legacy) {
        saved_networks[0].ssid = legacy_ssid;
        saved_networks[0].pass = legacy_pass;
        saved_network_count = 1;
        persist_saved_networks(); // one-time migration to the new format
    }
}

static void start_connect(const String &ssid, const String &password) {
    if (ssid.length() == 0) {
        wifi_log("No SSID entered.");
        return;
    }

    current_ssid = ssid;
    current_pass = password;
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
    // needs that stack to already exist.
    WiFi.mode(WIFI_STA);

    load_saved_networks();
    refresh_network_dropdown();

    if (saved_network_count > 0) {
        // Most-recently-added saved network -- auto-connect to it, same as
        // before this device supported more than one.
        start_connect(saved_networks[0].ssid, saved_networks[0].pass);
    } else {
        wifi_log("Enter a WiFi network SSID and password, then press Connect.");
    }
}

// Connects using whatever's typed in the SSID/password fields. Falls back
// to whatever's selected in the saved-network dropdown if the SSID field
// is empty -- this is what both the on-screen keyboard's OK button and the
// dedicated Connect button call.
static void connect_from_ui() {
    String ssid = ui_get_text_safe(ui_Connectivity_TextAreaSSID);
    String pass = ui_get_text_safe(ui_Connectivity_TextAreaPassword);
    ssid.trim();

    if (ssid.length() > 0) {
        start_connect(ssid, pass);

        // Clear the input fields right away so the form's ready for the
        // next entry -- connection feedback shows up in the log either way.
        ui_set_text_safe(ui_Connectivity_TextAreaSSID, "");
        ui_set_text_safe(ui_Connectivity_TextAreaPassword, "");
        last_activity_time = millis();
        return;
    }

    // Nothing typed -- use the saved-network dropdown selection instead.
    if (saved_network_count == 0) {
        wifi_log("No saved networks -- enter an SSID and password first.");
        return;
    }

    uint16_t sel = ui_Connectivity_DropdownSavednetworks
        ? lv_dropdown_get_selected(ui_Connectivity_DropdownSavednetworks)
        : 0;
    if (sel >= (uint16_t)saved_network_count) return;

    start_connect(saved_networks[sel].ssid, saved_networks[sel].pass);
    last_activity_time = millis();
}

static void password_ready_cb(lv_event_t *e) {
    (void)e;
    connect_from_ui();
}

static void connect_button_cb(lv_event_t *e) {
    (void)e;
    connect_from_ui();
}

static void delete_button_cb(lv_event_t *e) {
    (void)e;

    if (saved_network_count == 0 || !ui_Connectivity_DropdownSavednetworks) {
        wifi_log("No saved networks to delete.");
        return;
    }

    uint16_t sel = lv_dropdown_get_selected(ui_Connectivity_DropdownSavednetworks);
    if (sel >= (uint16_t)saved_network_count) return;

    String removed_ssid = saved_networks[sel].ssid;
    bool was_current = (removed_ssid == current_ssid) && wifi_is_connected();

    for (int i = sel; i < saved_network_count - 1; i++) {
        saved_networks[i] = saved_networks[i + 1];
    }
    saved_network_count--;
    persist_saved_networks();
    refresh_network_dropdown();

    if (was_current) {
        wifi_log("Removed saved network \"" + removed_ssid +
                  "\" -- still connected until you reconnect or reboot.");
    } else {
        wifi_log("Removed saved network \"" + removed_ssid + "\".");
    }
    last_activity_time = millis();
}

void wifi_manager_register_callbacks() {
    // The generated ui_event_Connectivity_TextAreaPassword handler only
    // shows/targets the keyboard on CLICKED -- this is a separate callback
    // for LV_EVENT_READY (keyboard "OK"), so both coexist without conflict.
    if (ui_Connectivity_TextAreaPassword) {
        lv_obj_add_event_cb(ui_Connectivity_TextAreaPassword, password_ready_cb, LV_EVENT_READY, NULL);
    }

    // Neither image button has a generated click handler (SquareLine only
    // wires up widgets it has a built-in "action" for).
    if (ui_Connectivity_ImgButtonConnectNetwork) {
        lv_obj_add_event_cb(ui_Connectivity_ImgButtonConnectNetwork, connect_button_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui_Connectivity_ImgButtonDeleteNetwork) {
        lv_obj_add_event_cb(ui_Connectivity_ImgButtonDeleteNetwork, delete_button_cb, LV_EVENT_CLICKED, NULL);
    }
}

void wifi_manager_loop() {
    static unsigned long last_retry_ms = 0;

    if (state == WIFI_MGR_CONNECTING) {
        wl_status_t st = WiFi.status();

        if (st == WL_CONNECTED) {
            state = WIFI_MGR_IDLE;
            ever_connected = true;

            String ip = WiFi.localIP().toString();
            wifi_log("Connected to \"" + current_ssid + "\". IP address: " + ip);
            wifi_log("Web panel: http://" + ip + "/  (user: " + String(WEB_AUTH_USER) +
                      ", pass: " + String(WEB_AUTH_PASS) + ")");

            // OTA reference info -- printed here (once per fresh connection)
            // so it's readable straight off this screen without needing
            // physical/serial access or digging through the source. The pio
            // command below has this session's live IP baked in, ready to
            // copy as-is.
            wifi_log("OTA (PlatformIO): pio run -e esp32-s3-devkitc-1-ota -t upload --upload-port " + ip);
            wifi_log("OTA auth: " + String(OTA_PASSWORD) + "  (mDNS host: " + String(OTA_HOSTNAME) + ".local)");
            wifi_log("OTA (browser): open the web panel above, use its Firmware Update section.");

            // Only actually saved once the connection is confirmed good --
            // a typo'd SSID/password never makes it into the saved list.
            upsert_saved_network(current_ssid, current_pass);

            int idx = find_saved_network_index(current_ssid);
            if (idx >= 0 && ui_Connectivity_DropdownSavednetworks) {
                lv_dropdown_set_selected(ui_Connectivity_DropdownSavednetworks, idx);
            }
        }
        // Specific feedback for the common failure modes instead of a
        // generic "failed or timed out" for everything -- wrong password
        // and a nonexistent/mistyped SSID surface as distinct WiFi statuses
        // well before the connect timeout, so no need to wait for it.
        else if (st == WL_NO_SSID_AVAIL) {
            state = WIFI_MGR_IDLE;
            wifi_log("Network \"" + current_ssid + "\" not found -- check the name.");
        }
        else if (st == WL_CONNECT_FAILED) {
            state = WIFI_MGR_IDLE;
            wifi_log("Failed to connect to \"" + current_ssid + "\" -- check the password.");
        }
        else if (millis() - connect_start_ms > WIFI_CONNECT_TIMEOUT_MS) {
            state = WIFI_MGR_IDLE;
            wifi_log("Connection to \"" + current_ssid + "\" timed out.");
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
