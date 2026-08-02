#ifndef OTA_H
#define OTA_H

/* ArduinoOTA -- lets PlatformIO/Arduino IDE flash new firmware over the
   WiFi network already connected by wifi_manager.cpp, discoverable via
   mDNS at OTA_HOSTNAME + ".local" (config.h). Password-protected with
   OTA_PASSWORD. This is the "pio run -t upload" over the network path;
   see web_server.cpp / web_page.h for the browser-upload alternative,
   which doesn't need PlatformIO installed on whatever machine you're
   using at the time.

   Call ota_begin() once from setup(), after wifi_manager_begin() (WiFi
   just needs to be in STA mode by then, not necessarily connected yet --
   OTA simply isn't reachable over mDNS until it is). Call ota_loop()
   every loop(). */
void ota_begin();
void ota_loop();

#endif
