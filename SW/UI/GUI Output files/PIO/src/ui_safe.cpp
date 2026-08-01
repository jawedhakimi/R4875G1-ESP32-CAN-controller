#include "ui_safe.h"
#include <ui.h>

bool obj_is_textarea(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_textarea_class);
}

bool obj_is_label(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_label_class);
}

bool obj_is_slider(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_slider_class);
}

bool obj_is_checkbox(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_checkbox_class);
}

bool obj_is_btn(lv_obj_t *obj) {
    return obj && lv_obj_check_type(obj, &lv_btn_class);
}

void ui_set_text_safe(lv_obj_t *obj, const char *txt) {
    if (!obj || !txt) return;

    if (obj_is_label(obj)) {
        lv_label_set_text(obj, txt);
    } else if (obj_is_textarea(obj)) {
        lv_textarea_set_text(obj, txt);
    }
}

String ui_get_text_safe(lv_obj_t *obj) {
    if (!obj) return "";

    if (obj_is_textarea(obj)) {
        return String(lv_textarea_get_text(obj));
    } else if (obj_is_label(obj)) {
        return String(lv_label_get_text(obj));
    }

    return "";
}

void append_log_to_textarea(lv_obj_t *ta, const String &msg) {
    if (!ta || !obj_is_textarea(ta)) return;

    String current = lv_textarea_get_text(ta);
    if (current.length() > 1200) {
        current = current.substring(current.length() - 900);
    }

    current += msg;
    current += "\n";
    lv_textarea_set_text(ta, current.c_str());
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
}

void log_to_settings(const String &msg) {
    Serial.println(msg);
    append_log_to_textarea(ui_Settings_TextAreaLogs, msg);
}
