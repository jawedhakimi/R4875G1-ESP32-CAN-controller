#ifndef ENERGY_METER_H
#define ENERGY_METER_H

#include <Arduino.h>
#include <lvgl.h>

String format_energy_kwh(double kwh);
void update_energy_ui();

/* Accumulates energy_kwh from the current output power reading. Pass 0 (or
   don't call) when telemetry is stale/unavailable so a dead CAN link doesn't
   silently keep "spending" the last known power figure. Call every loop. */
void handle_energy_meter(float outputPowerWatts);

/* "Reset energy counter" button handler + beep. Registers itself. */
void energy_meter_register_callbacks();

#endif
