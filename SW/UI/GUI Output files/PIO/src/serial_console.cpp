#include "serial_console.h"
#include "config.h"
#include "app_state.h"
#include "prefs_store.h"
#include "psu_timer.h"
#include "energy_meter.h"
#include "psu_control.h"   // clampf()

// BUGFIX: the original buffer had no length cap -- if a client sent data
// without a terminating '\n' (or a wire glitch swallowed it), `input` would
// grow forever. Cap it and reset on overflow instead.
#define SERIAL_CMD_MAX_LEN 96

static void printHelp() {
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

static void printStatus() {
    PSUStatus st = psu.getStatus();

    Serial.println("\n---- STATUS ----");
    Serial.println("Output: " + String(saved_output_enable ? "ON" : "OFF"));
    Serial.println("CAN link: " + String(psu.isStale() ? "LOST" : "OK"));

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

static void handle_command(const String &raw) {
    String input = raw;
    input.trim();
    if (input.length() == 0) return;

    if (input == "h") {
        printHelp();
    }
    else if (input == "s") {
        printStatus();
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
        float v = clampf(input.substring(4).toFloat(), PSU_VMIN, PSU_VMAX);
        saved_online_v = v;
        save_float_pref("on_v", v);
        psu.setVoltage(v);
        Serial.println("Von set to " + String(v, 2) + " V");
    }
    else if (input.startsWith("voff=")) {
        float v = clampf(input.substring(5).toFloat(), PSU_VOFFLINE_MIN, PSU_VMAX);
        saved_offline_v = v;
        save_float_pref("off_v", v);
        psu.setOfflineVoltage(v);
        Serial.println("Voff set to " + String(v, 2) + " V");
    }
    else if (input.startsWith("ion=")) {
        float i = clampf(input.substring(4).toFloat(), PSU_IMIN, PSU_IMAX);
        saved_online_i = i;
        save_float_pref("on_i", i);
        psu.setCurrent(i);
        Serial.println("Ion set to " + String(i, 2) + " A");
    }
    else if (input.startsWith("ioff=")) {
        float i = clampf(input.substring(5).toFloat(), PSU_IMIN, PSU_IMAX);
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
        if (parse_hms_string(input.substring(6), secs)) {
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
}

void handle_serial_commands() {
    static String input = "";

    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            handle_command(input);
            input = "";
        } else {
            if (input.length() >= SERIAL_CMD_MAX_LEN) {
                Serial.println("Command too long, discarded.");
                input = "";
            } else {
                input += c;
            }
        }
    }
}
