#ifndef WEB_SERVER_H
#define WEB_SERVER_H

/* Local HTTP control panel -- JSON API + embedded page (web_page.h), served
   over WiFi once wifi_manager has a connection. All PSU/timer/energy state
   changes go through the same shared psu_set_*() / psu_timer_*() /
   energy_reset() functions the touchscreen and serial console use (see
   psu_control.h), so all three surfaces stay in sync.

   Protected with HTTP Basic Auth (WEB_AUTH_USER/WEB_AUTH_PASS in config.h)
   -- see the warning there about what that auth does and doesn't protect
   against. */

/* Starts the HTTP server and registers all routes. Call once from setup(),
   after psu/app_state are initialized. Safe to call even before WiFi is
   connected (the server just won't be reachable until then). */
void web_server_begin();

/* Services pending HTTP requests. Call every loop(). Cheap no-op-ish when
   idle (WebServer is poll-based, not blocking). */
void web_server_loop();

#endif
