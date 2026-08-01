#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include <lvgl.h>

/* LVGL indev read callback. Register with lv_indev_drv_register() in
   setup(). Handles debouncing, jitter filtering, and wake-on-touch (see
   touch_input.cpp for details). */
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

#endif
