#include "can_bridge.h"
#include <ui.h>
#include "app_state.h"
#include "ui_safe.h"
#include "energy_meter.h"

void handle_can_and_ui() {
    static unsigned long last_can_req = 0;
    static unsigned long last_ui_update = 0;
    static unsigned long last_bus_health_check = 0;
    static bool was_stale = false;

    if (millis() - last_can_req > 200) {
        psu.sendRequest();
        last_can_req = millis();
    }

    psu.readAndDecodeResponse();

    // Cheap periodic check for bus-off / stopped controller; attempts
    // recovery on its own (see HuaweiCAN::checkBusHealth()).
    if (millis() - last_bus_health_check >= 1000) {
        last_bus_health_check = millis();
        psu.checkBusHealth();
    }

    bool stale = psu.isStale();
    PSUStatus status = psu.getStatus();

    // Don't keep "spending" the last known output power once the link is
    // down -- that would silently inflate the energy counter forever if the
    // PSU drops off CAN mid-session while still physically outputting.
    handle_energy_meter(stale ? 0.0f : status.outputPower);

    if (millis() - last_ui_update < 100) return;
    last_ui_update = millis();

    // --- BUGFIX: previously there was no notion of "stale" telemetry at
    // all -- if the CAN link died, the UI just kept showing whatever
    // numbers it last received, forever, with no indication anything was
    // wrong. Now a dead link is shown explicitly. ---
    if (stale) {
        if (!was_stale) {
            log_to_settings("CAN link lost -- PSU not responding");
            was_stale = true;
        }

        ui_set_text_safe(ui_VarVout, "--");
        ui_set_text_safe(ui_VarIout, "--");
        ui_set_text_safe(ui_VarPout, "--");
        ui_set_text_safe(ui_VarVin, "--");
        ui_set_text_safe(ui_VarIin, "--");
        ui_set_text_safe(ui_VarPin, "--");
        ui_set_text_safe(ui_VarTin, "--");
        ui_set_text_safe(ui_VarTout, "--");
        ui_set_text_safe(ui_VarEffi, "--");
        ui_set_text_safe(ui_VarFin, "--");
        ui_set_text_safe(ui_VarOutputState, "NO LINK");
        return;
    }

    if (was_stale) {
        log_to_settings("CAN link restored");
        was_stale = false;
    }

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
        ui_set_text_safe(ui_VarTin, valBuffer);
    }

    if (status.outputTemp > -50.0f && status.outputTemp < 150.0f) {
        snprintf(valBuffer, sizeof(valBuffer), "%.1f", status.outputTemp);
        ui_set_text_safe(ui_VarTout, valBuffer);
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
