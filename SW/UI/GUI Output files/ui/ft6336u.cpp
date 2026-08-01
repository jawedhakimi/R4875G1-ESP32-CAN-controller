/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-07-22 09:19:25
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-07-22 10:30:45
 * @FilePath: \PlatformIO\src\FT6336U_device.cpp
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
#include "ft6336u.h"
#include <Wire.h>
TwoWire touch_i2c = TwoWire(1);

static int s_tp_sda = -1;
static int s_tp_scl = -1;
static uint8_t s_consec_i2c_fails = 0;

/* I2C-bus recovery (per NXP UM10204 3.1.16): if a slave gets interrupted
   mid-byte -- electrical noise, a brownout, ESD -- it can be left holding
   SDA low forever. No amount of retrying Wire's endTransmission()/
   requestFrom() fixes that; the bus has to be manually clocked free.
   Detaches the pins from the I2C peripheral, bit-bangs up to 9 SCL pulses
   with SDA released (a stuck slave shifts out its remaining bits and lets
   go), issues a STOP condition, then re-attaches Wire. Only called after
   several consecutive read failures, not on every touch poll. */
static void i2c_bus_recover(int sda, int scl) {
    touch_i2c.end();

    pinMode(scl, OUTPUT_OPEN_DRAIN);
    pinMode(sda, INPUT);

    for (int i = 0; i < 9; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        if (digitalRead(sda)) break; // slave released SDA early -- done
    }

    // STOP condition: SDA low-to-high while SCL is high.
    pinMode(sda, OUTPUT_OPEN_DRAIN);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);

    touch_i2c.begin(sda, scl, TP_I2C_FREQ);
    touch_i2c.setTimeOut(TP_I2C_TIMEOUT_MS);

    Serial.println("FT6336U: I2C bus recovery performed after repeated read failures.");
}

/* Feeds the consecutive-failure counter and triggers recovery once it
   crosses TP_MAX_CONSEC_FAILS. Call with the result of every I2C
   transaction that represents a real attempt to talk to the chip (not
   "touch_num == 0", which is a successful read reporting no finger down). */
static void note_i2c_result(bool ok) {
    if (ok) {
        s_consec_i2c_fails = 0;
        return;
    }
    if (s_consec_i2c_fails < 255) s_consec_i2c_fails++;
    if (s_consec_i2c_fails >= TP_MAX_CONSEC_FAILS && s_tp_sda >= 0 && s_tp_scl >= 0) {
        i2c_bus_recover(s_tp_sda, s_tp_scl);
        s_consec_i2c_fails = 0;
    }
}

static bool touch_i2c_write(uint8_t driver_addr, uint8_t reg_addr, const uint8_t *data, uint32_t length)
{
    touch_i2c.beginTransmission(driver_addr);
    touch_i2c.write(reg_addr);
    touch_i2c.write(data, length);


    if (touch_i2c.endTransmission() != 0) {
        Serial.println("The I2C transmission fails. - I2C Read\r\n");
        return false;
    }
    return true;
}

static bool touch_i2c_read(uint8_t driver_addr, uint8_t reg_addr, uint8_t *data, uint32_t length)
{
    touch_i2c.beginTransmission(driver_addr);
    touch_i2c.write(reg_addr);
    if (touch_i2c.endTransmission() != 0) {
        Serial.println("The I2C write fails. - I2C Read\r\n");
        return false;
    }

    touch_i2c.requestFrom(driver_addr, length);
    if (touch_i2c.available() != length) {
        Serial.println("The I2C read fails. - I2C Read\r\n");
        return false;
    }
    touch_i2c.readBytes(data, length);
    return true; // 读取成功
}

bool touch_init(int tp_sda, int tp_scl, int tp_rst, int tp_int)
{
    s_tp_sda = tp_sda;
    s_tp_scl = tp_scl;
    s_consec_i2c_fails = 0;

    // Init I2C
    touch_i2c.begin(tp_sda, tp_scl, TP_I2C_FREQ);
    touch_i2c.setTimeOut(TP_I2C_TIMEOUT_MS);
    pinMode(tp_rst, OUTPUT);
    // Reset pulse
    digitalWrite(tp_rst, LOW);
    delay(200);
    digitalWrite(tp_rst, HIGH);
    delay(300);

    uint8_t data[1] = {0};
    bool ok = touch_i2c_read(FT6336U_ADDR, FT6336U_ID_REG, data, 1);

    // BUGFIX: previously this always returned true, even if the chip never
    // responded on I2C. That left the caller (setup()) believing the touch
    // panel was working when it wasn't, and get_touch_coords() would silently
    // read back zeros forever. Now a failed/absent read is reported as init
    // failure so it can be surfaced (e.g. logged / shown in the UI).
    if (!ok) {
        Serial.println("FT6336U: no response on I2C bus (check wiring/address/pins).");
        return false;
    }

    Serial.printf("FT6336U detected, ID reg = 0x%02X\n", data[0]);
    return true;
}


bool get_touch_coords(toucht_coords_t *toucht_coords)
{
    uint8_t x_arr[2] = {0};
    uint8_t y_arr[2] = {0};
    uint8_t touch_num = 0;

    if (!touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_NUM_REG, &touch_num, 1)) {
        note_i2c_result(false);
        return false;
    }
    note_i2c_result(true);

    if (touch_num == 0)
        return false;

    // BUGFIX: previously the return values of these reads were ignored, so a
    // transient I2C error mid-read would leave x_arr/y_arr at their
    // zero-initialized default and still get reported as a valid touch at
    // (0,0) -- a phantom tap in the top-left corner. Now any read failure
    // aborts the sample instead of reporting bogus coordinates.
    bool ok = true;
    ok &= touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_XH_REG, &x_arr[0], 1);
    ok &= touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_XL_REG, &x_arr[1], 1);
    ok &= touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_YH_REG, &y_arr[0], 1);
    ok &= touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_YL_REG, &y_arr[1], 1);
    note_i2c_result(ok);

    if (!ok) return false;

    toucht_coords->x = (uint16_t)(( x_arr[0] & 0x0f) <<  8);
    toucht_coords->x |= x_arr[1];
    toucht_coords->y = (uint16_t)((y_arr[0] & 0x0f) <<  8);
    toucht_coords->y |= y_arr[1];

    return true;
}

