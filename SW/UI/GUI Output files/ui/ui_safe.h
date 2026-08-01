#ifndef UI_SAFE_H
#define UI_SAFE_H

#include <lvgl.h>
#include <Arduino.h>

/* Type checks so we never call the wrong lv_*_set/get_text on an object. */
bool obj_is_textarea(lv_obj_t *obj);
bool obj_is_label(lv_obj_t *obj);
bool obj_is_slider(lv_obj_t *obj);
bool obj_is_checkbox(lv_obj_t *obj);
bool obj_is_btn(lv_obj_t *obj);

void ui_set_text_safe(lv_obj_t *obj, const char *txt);
String ui_get_text_safe(lv_obj_t *obj);

/* On-screen log (Settings screen text area) + mirrored Serial output. */
void append_log_to_textarea(lv_obj_t *ta, const String &msg);
void log_to_settings(const String &msg);

#endif
