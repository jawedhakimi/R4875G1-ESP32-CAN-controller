#ifndef HUAWEI_CAN_H
#define HUAWEI_CAN_H

#include <Arduino.h>
#include "driver/twai.h"

/* =========================================================
   Set HUAWEI_CAN_VERBOSE to 0 (before #include) in a sketch
   that already has its own logging (e.g. the touchscreen UI)
   to silence this driver's own Serial prints.
   ========================================================= */
#ifndef HUAWEI_CAN_VERBOSE
#define HUAWEI_CAN_VERBOSE 1
#endif

// ===== CAN BUS SETTINGS =====
#define CAN_SET_ADDR       0x108180FE
#define CAN_REQUEST_ADDR   0x108040FE
#define CAN_RESPONSE_ADDR  0x1081407F
#define TX_PIN             GPIO_NUM_17
#define RX_PIN             GPIO_NUM_18
#define CAN_BAUD_RATE      125000

// ===== PSU SAFETY LIMITS =====
// Single source of truth - do not redefine these in application code.
#define PSU_VMAX          58.5f
#define PSU_VMIN          41.5f
#define PSU_VOFFLINE_MIN  48.0f
#define PSU_IMAX          75.0f
#define PSU_IMIN          0.0f

// How long without a valid status frame before telemetry is considered stale.
#define PSU_STALE_TIMEOUT_MS  1500UL

// ===== PSU STATUS STRUCT =====
struct PSUStatus {
  float outputVoltage;
  float outputCurrent;
  float inputVoltage;
  float inputCurrent;
  float inputPower;
  float outputPower;
  float efficiency;
  float inputFreq;
  float inputTemp;
  float outputTemp;
  float outputCurrentFast;
  float outputCurrentFiltered;
  float maxOutputCurrentPct;
  uint32_t operatingHours;
  uint32_t alarmBits;
};

class HuaweiCAN {
public:
  HuaweiCAN();
  bool begin();
  bool sendRequest();
  bool sendSetCommand(uint8_t b0, uint8_t b1, uint32_t val);
  void readAndDecodeResponse();
  PSUStatus getStatus();

  // Telemetry health
  bool isStale() const;              // true if no valid frame within PSU_STALE_TIMEOUT_MS
  uint32_t msSinceLastUpdate() const;

  // Bus health: call periodically (e.g. once a second) from the main loop.
  // Detects bus-off / stopped states and attempts recovery. Returns true if the
  // controller is healthy (TWAI_STATE_RUNNING).
  bool checkBusHealth();

  bool setVoltage(float v);
  bool setOfflineVoltage(float v);
  bool setCurrent(float i);
  bool setOfflineCurrent(float i);
  bool setFanMode(bool manual);
  bool enableOutput(bool enable);

private:
  PSUStatus status;
  bool outputState;
  bool fanManualMode;
  uint32_t lastRxMs;

  bool twaiSend(uint32_t id, const uint8_t *data);
};

#endif
