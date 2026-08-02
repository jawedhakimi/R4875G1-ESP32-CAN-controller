#include "ota.h"
#include <ArduinoOTA.h>
#include "config.h"
#include "ui_safe.h"   // log_to_settings()

void ota_begin() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        log_to_settings("OTA update starting (" + type + ")...");
    });

    ArduinoOTA.onEnd([]() {
        log_to_settings("OTA update complete, rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Serial only (not log_to_settings) -- this fires many times a
        // second and would otherwise flood the on-screen log.
        static unsigned int last_pct = 255;
        unsigned int pct = total ? (progress * 100U) / total : 0;
        if (pct != last_pct) {
            last_pct = pct;
            Serial.printf("OTA progress: %u%%\n", pct);
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        String msg;
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "Auth failed"; break;
            case OTA_BEGIN_ERROR:   msg = "Begin failed"; break;
            case OTA_CONNECT_ERROR: msg = "Connect failed"; break;
            case OTA_RECEIVE_ERROR: msg = "Receive failed"; break;
            case OTA_END_ERROR:     msg = "End failed"; break;
            default:                msg = "Unknown error"; break;
        }
        log_to_settings("OTA error: " + msg);
    });

    ArduinoOTA.begin();
    Serial.printf("OTA ready -- hostname \"%s.local\"\n", OTA_HOSTNAME);
}

void ota_loop() {
    ArduinoOTA.handle();
}
