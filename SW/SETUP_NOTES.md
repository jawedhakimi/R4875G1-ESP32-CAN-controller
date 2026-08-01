# Build setup

The touchscreen UI firmware source lives in `UI/GUI Output files/ui/`.
There's also a small standalone bench-test sketch, `CAN_controller/`, for
exercising the PSU over CAN without the display.

## HuaweiCAN library

`HuaweiCAN.h/.cpp` (the CAN driver) used to be duplicated across sketches,
with the copies silently drifting apart (one had a real protocol bug the
other didn't). It's now a single library at `libraries/HuaweiCAN/`, and
everything else reaches it via that one copy -- either directly or through a
symlink -- so there's nothing left to fall out of sync:

- `CAN_controller/CAN_controller.ino` -- Arduino IDE, uses `libraries/HuaweiCAN`
  directly.
- `UI/GUI Output files/PIO/lib/HuaweiCAN` -- a symlink to `libraries/HuaweiCAN`
  (not a copy), used by the PlatformIO build.

## Building the UI: PlatformIO (VS Code) -- primary path

Open `UI/GUI Output files/PIO/` as a PlatformIO project. Its
`platformio.ini`:
- points `src_dir` at `../ui`, so it builds `UI/GUI Output files/ui/`
  directly -- there's no separate PIO copy of the firmware to keep in sync;
- libraries (ui, lvgl, TFT_eSPI, ArduinoJson, HuaweiCAN) live directly in
  `PIO/lib/`, PlatformIO's default library location -- no extra config
  needed for discovery;
- targets `esp32-s3-devkitc-1` configured for 16MB flash / 8MB octal PSRAM
  (`qio_opi`).

`CAN_controller/` doesn't have a PlatformIO project of its own -- it's a
quick Arduino-IDE bench tool. Ask if you'd like one added.

### If Serial shows nothing after flashing

Some ESP32-S3-DevKitC-1 boards have a separate USB-UART bridge chip
(CP2102/CH340, a second "UART" USB port) and need nothing extra. Boards
that expose only the S3's native USB port need
`-DARDUINO_USB_CDC_ON_BOOT=1` (commented out in `platformio.ini` -- uncomment
if `pio device monitor` stays blank).

## Building the UI: Arduino IDE

The libraries (ui, lvgl, TFT_eSPI, ArduinoJson, HuaweiCAN) now live in
`UI/GUI Output files/PIO/lib/`, not in a folder named `libraries` -- and
Arduino IDE only auto-discovers a folder literally named `libraries`. To
build with Arduino IDE:

- Point your Arduino IDE's `libraries/` folder (or sketchbook) at
  `UI/GUI Output files/PIO/lib/` -- e.g. copy/symlink its contents into your
  existing Arduino `libraries/` folder, and
- Open `UI/GUI Output files/ui/ui.ino` directly.

For `CAN_controller/CAN_controller.ino` (separate sketch, not under `UI/`),
either also set `SW/` itself as the sketchbook location (which makes
top-level `SW/libraries/` auto-discovered for every sketch under it,
including this one), or copy/symlink `libraries/HuaweiCAN/` into whatever
Arduino libraries folder you're already using.

## UI sketch layout

`UI/GUI Output files/ui/` has multiple `.h`/`.cpp` files alongside `ui.ino`
(Arduino/PlatformIO both compile every `.cpp`/`.h` in that folder together,
so this is a normal multi-file sketch, not a separate library). `ui.ino`
itself is just `setup()`/`loop()` + LVGL glue; each other file owns one
concern (backlight, touch input, PSU control, CAN bridge, WiFi, web server,
etc.) -- see the comment at the top of each header for what it does.

## WiFi + web control panel

The Connectivity screen (SSID/password fields, submit with the on-screen
keyboard's OK) is backed by `wifi_manager.h/.cpp`: it saves credentials to
NVS, connects non-blockingly, and auto-retries if the link drops. Once
connected, `web_server.h/.cpp` serves a control panel at `http://<device
IP>/` (IP is logged to the Connectivity screen and to Serial) offering the
same PSU controls as the touchscreen/serial console, plus a run-timer
control that has no touchscreen widget yet. All three surfaces
(touchscreen, serial console, web) go through the same shared setter
functions in `psu_control.h`/`psu_timer.h`/`energy_meter.h`, so they can't
drift out of sync with each other.

The web panel is protected with HTTP Basic Auth
(`WEB_AUTH_USER`/`WEB_AUTH_PASS` in `config.h`, default `admin`/
`psucontrol`). **Change these before deploying** -- Basic Auth isn't
encrypted, so treat it as "keep casual access off your LAN," not real
security, and don't expose the device to the internet.
