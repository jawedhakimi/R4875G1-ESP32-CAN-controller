#ifndef BEEP_H
#define BEEP_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IMPORTANT: kept as C-linkage in case generated SquareLine .c files ever
   call it directly (that was the intent of the original single-file
   sketch). */
void user_beep(void);

#ifdef __cplusplus
}
#endif

/* LVGL event callback wrapper around user_beep(), for lv_obj_add_event_cb(). */
void user_beep_cb(lv_event_t *e);

/* Finds the ScreenButtons component (Home/SetValues/Settings nav bar) by
   structural heuristic -- SquareLine Studio regenerates ui_*.c on every
   export, so there's no stable extern symbol to hook without hand-editing
   generated files. Attaches a beep to each nav button's click event.
   Call once from setup(), after ui_init(). */
void beep_hook_screen_nav_buttons();

/* Beeps on the on-screen numeric keypad(s) used for entering values.
   Call once from setup(), after ui_init(). */
void beep_hook_numpads();

#endif
