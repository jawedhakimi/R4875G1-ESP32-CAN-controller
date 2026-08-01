#include "serial_console.h"
#include "config.h"
#include "app_state.h"
#include "psu_timer.h"
#include "energy_meter.h"
#include "psu_control.h"   // shared psu_set_*() setters

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
        psu_set_output(true);
        Serial.println("Output ENABLED");
    }
    else if (input == "off") {
        psu_set_output(false);
        Serial.println("Output DISABLED");
    }
    else if (input.startsWith("von=")) {
        float v = psu_set_online_voltage(input.substring(4).toFloat());
        Serial.println("Von set to " + String(v, 2) + " V");
    }
    else if (input.startsWith("voff=")) {
        float v = psu_set_offline_voltage(input.substring(5).toFloat());
        Serial.println("Voff set to " + String(v, 2) + " V");
    }
    else if (input.startsWith("ion=")) {
        float i = psu_set_online_current(input.substring(4).toFloat());
        Serial.println("Ion set to " + String(i, 2) + " A");
    }
    else if (input.startsWith("ioff=")) {
        float i = psu_set_offline_current(input.substring(5).toFloat());
        Serial.println("Ioff set to " + String(i, 2) + " A");
    }
    else if (input == "fanauto") {
        psu_set_fan_manual(false);
        Serial.println("Fan AUTO");
    }
    else if (input == "fanmanual") {
        psu_set_fan_manual(true);
        Serial.println("Fan MANUAL");
    }
    else if (input.startsWith("timer=")) {
        uint32_t secs;
        if (parse_hms_string(input.substring(6), secs)) {
            psu_timer_configure(secs);
            Serial.println("Timer set to " + format_hms(secs));
        } else {
            Serial.println("Invalid format. Use HH.MM.SS");
        }
    }
    else if (input.startsWith("usetimer=")) {
        int v = input.substring(9).toInt();
        psu_timer_set_enabled(v != 0);
        Serial.println(String("Use timer: ") + (saved_use_timer ? "YES" : "NO"));
    }
    else if (input == "energyreset") {
        energy_reset();
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
