/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-07-22 09:19:25
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-07-22 10:29:29
 * @FilePath: \PlatformIO\src\FT6336U_device.h
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */

#pragma once

#include <Arduino.h>

#define MAX_TOUCH_MAX_POINTS    1

#define TP_INT 47
#define TP_RST 21
#define TP_SDA 45
#define TP_SCL 48


#define TP_I2C_FREQ 400000

/* Bounds how long any single I2C transaction can block for. Without this,
   a glitched/noisy bus (a slave interrupted mid-byte, holding SDA low) can
   wedge Wire's endTransmission()/requestFrom() forever -- since touch
   reads happen synchronously inside the same loop() that also drives
   LVGL, CAN polling, etc., that hang freezes the entire device, not just
   the touch panel. */
#define TP_I2C_TIMEOUT_MS    50

/* After this many consecutive failed transactions, assume the bus is
   wedged (not just a transient glitch) and run the recovery sequence in
   i2c_bus_recover() below. */
#define TP_MAX_CONSEC_FAILS  5

#define FT6336U_ADDR 0x38
#define FT6336U_ID_REG 0xa8

#define FT6336U_TOUCH_NUM_REG 0x02
#define FT6336U_TOUCH_XH_REG 0x03
#define FT6336U_TOUCH_XL_REG 0x04
#define FT6336U_TOUCH_YH_REG 0x05
#define FT6336U_TOUCH_YL_REG 0x06
typedef struct{
    uint16_t x;
    uint16_t y;
}toucht_coords_t;


bool get_touch_coords(toucht_coords_t *toucht_coords);

bool touch_init(int tp_sda, int tp_scl, int tp_rst, int tp_int);

