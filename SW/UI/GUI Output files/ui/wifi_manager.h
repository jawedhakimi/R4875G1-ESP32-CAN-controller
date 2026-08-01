#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

/* Loads the saved-network list from NVS, populates the Connectivity
   screen's dropdown with it, and auto-connects to the most recently saved
   network if there is one. Call once from setup(), after ui_init(). */
void wifi_manager_begin();

/* Registers the Connectivity screen's callbacks:
   - submitting the password field (keyboard OK) or pressing the Connect
     button both connect using whatever's typed in the SSID/password
     fields, falling back to the saved-network dropdown's selection if the
     SSID field is empty;
   - the Delete button removes the dropdown's selected saved network.
   A network is only added to the saved list once it's actually connected
   -- a typo'd SSID/password never gets saved. Call once from setup(). */
void wifi_manager_register_callbacks();

/* Non-blocking connect/retry state machine + status logging to
   ui_Connectivity_TextAreaNetworkLog. Call every loop. */
void wifi_manager_loop();

bool wifi_is_connected();
String wifi_ip_address();

#endif
