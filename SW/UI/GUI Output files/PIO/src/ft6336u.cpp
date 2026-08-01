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
    // Init I2C
    touch_i2c.begin(tp_sda, tp_scl, TP_I2C_FREQ);
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

    if (!touch_i2c_read(FT6336U_ADDR, FT6336U_TOUCH_NUM_REG, &touch_num, 1))
        return false;
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

    if (!ok) return false;

    toucht_coords->x = (uint16_t)(( x_arr[0] & 0x0f) <<  8);
    toucht_coords->x |= x_arr[1];
    toucht_coords->y = (uint16_t)((y_arr[0] & 0x0f) <<  8);
    toucht_coords->y |= y_arr[1];

    return true;
}

