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

/* Attaches a beep to every nav button's click event, across all four
   per-screen nav bar instances (ui_ScreenButtons/2/3/4 -- Home/SetValues/
   Settings/Connectivity each get their own copy of the component).
   Call once from setup(), after ui_init(). */
void beep_hook_screen_nav_buttons();

/* Beeps on the on-screen numeric keypad(s) used for entering values.
   Call once from setup(), after ui_init(). */
void beep_hook_numpads();

#endif
