#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

/* Loads any saved WiFi credentials from NVS, pre-fills the Connectivity
   screen's fields with them, and kicks off an initial connect attempt if
   present. Call once from setup(), after ui_init(). */
void wifi_manager_begin();

/* Registers the Connectivity screen's SSID/password field callbacks
   (submitting the password field triggers a connect attempt with whatever
   is currently in both fields). Call once from setup(). */
void wifi_manager_register_callbacks();

/* Non-blocking connect/retry state machine + status logging to
   ui_Connectivity_TextAreaNetworkLog. Call every loop. */
void wifi_manager_loop();

bool wifi_is_connected();
String wifi_ip_address();

#endif
