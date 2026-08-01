#ifndef HUAWEI_CAN_H
#define HUAWEI_CAN_H

#include <Arduino.h>
#include "driver/twai.h"

// ===== CAN BUS SETTINGS =====
#define CAN_SET_ADDR       0x108180FE
#define CAN_REQUEST_ADDR   0x108040FE
#define CAN_RESPONSE_ADDR  0x1081407F
#define TX_PIN             GPIO_NUM_17
#define RX_PIN             GPIO_NUM_18
#define CAN_BAUD_RATE      125000

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
  void sendRequest();
  void sendSetCommand(uint8_t b0, uint8_t b1, uint32_t val);
  void readAndDecodeResponse();
  PSUStatus getStatus();
  void setVoltage(float v);
  void setOfflineVoltage(float v);
  void setCurrent(float i);
  void setOfflineCurrent(float i);
  void setFanMode(bool manual);
  void enableOutput(bool enable);

private:
  PSUStatus status;
  bool outputState;
  bool fanManualMode;

  void twaiSend(uint32_t id, uint8_t *data);
};

#endif