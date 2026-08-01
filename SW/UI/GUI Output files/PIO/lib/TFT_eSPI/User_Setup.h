/***************************************************************************************
**  TFT_eSPI User Setup File for ESP32 + ST7796 (320x480)
***************************************************************************************/

// ================== DRIVER ==================
#define ST7796_DRIVER

// ================== DISPLAY SIZE ==================
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// ================== ESP32 SPI PINS ==================
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  46

// ================== BACKLIGHT ==================
#define TFT_BL   3
#define TFT_BACKLIGHT_ON HIGH

// ================== SPI FREQUENCY ==================
#define USE_FSPI_PORT
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

// ================== FONT SUPPORT ==================
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// ================== COLOR ORDER ==================
#define TFT_RGB_ORDER TFT_BGR

// ================== TRANSACTION SUPPORT ==================
#define SUPPORT_TRANSACTIONS