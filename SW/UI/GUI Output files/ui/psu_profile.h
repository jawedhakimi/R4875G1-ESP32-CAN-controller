#ifndef PSU_PROFILE_H
#define PSU_PROFILE_H

#include <Arduino.h>

/* Output profile -- a scheduled voltage/current curve run over a fixed
   duration, executed live against the PSU through the same shared online
   setters the touchscreen/serial console/web setpoints use (psu_control.h),
   so nothing can drift out of sync. Each curve (voltage, current) is an
   independent, ordered list of (time_sec, value) points, linearly
   interpolated between them and held flat before the first / after the
   last point. There's no touchscreen editor for this -- a curve editor
   doesn't really work on a 320x480 touchscreen -- it's authored from the
   web control panel (see web_page.h / web_server.cpp).

   The curve definition (points + duration) persists to NVS and survives a
   reboot. Whether a profile was RUNNING does not -- on boot a profile is
   always stopped, never auto-resumed. An unattended power cycle silently
   continuing to drive voltage/current on its own is exactly the kind of
   surprise a device that outputs real power shouldn't produce. Press Run
   to (re)start; it always starts fresh from t=0. */

#define PSU_PROFILE_MAX_POINTS 40

struct ProfilePoint {
    uint32_t t;    // seconds from profile start, 0..duration
    float value;   // volts or amps depending on which curve it's in
};

/* Loads the persisted profile definition (if any) from NVS. Call once
   from setup(), after prefs_load_all(). */
void psu_profile_load();

/* Ticks a running profile: advances elapsed time, applies the
   interpolated voltage/current (persist=false -- see psu_control.h),
   stops and does one final persist=true apply when the duration is
   reached. Call every loop(). */
void handle_psu_profile();

uint32_t psu_profile_duration_sec();
int psu_profile_voltage_count();
int psu_profile_current_count();
const ProfilePoint *psu_profile_voltage_points();
const ProfilePoint *psu_profile_current_points();

/* Replaces the whole profile definition in one shot and persists it to
   NVS. Points are clamped to the PSU's safety limits (PSU_VMIN/VMAX,
   PSU_IMIN/IMAX) and to [0, duration_sec], then sorted by time. Fails
   (returns false, fills *error) if a curve has more than
   PSU_PROFILE_MAX_POINTS points, if duration_sec is 0, or if a profile is
   currently running -- stop it first. */
bool psu_profile_set(uint32_t duration_sec,
                      const ProfilePoint *voltage_pts, int voltage_count,
                      const ProfilePoint *current_pts, int current_count,
                      String &error);

/* Starts executing from t=0. Fails (returns false) if both curves are
   empty -- there'd be nothing to run. */
bool psu_profile_start();

/* Stops advancing. The PSU is left at whatever it was last commanded to
   -- stopping the schedule doesn't revert the output. */
void psu_profile_stop();

bool psu_profile_is_running();
uint32_t psu_profile_elapsed_sec();

#endif
