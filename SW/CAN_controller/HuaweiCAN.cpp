#include "HuaweiCAN.h"

// ===== CONSTANTS =====
const float VMAX = 58.5;
const float VMIN = 41.5;
const float VOFFLINE_MIN = 48.0;
const float IMAX = 75.0;
const float IMIN = 0.0;

HuaweiCAN::HuaweiCAN() : outputState(true), fanManualMode(false) {}

bool HuaweiCAN::begin() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_config  = TWAI_TIMING_CONFIG_125KBITS();
  twai_filter_config_t  f_config  = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
  if (twai_start() != ESP_OK) return false;
  return true;
}

// ===== SEND UTILS =====
void HuaweiCAN::twaiSend(uint32_t id, uint8_t *data) {
  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = 1;
  msg.data_length_code = 8;
  memcpy(msg.data, data, 8);
  twai_transmit(&msg, pdMS_TO_TICKS(100));
}

void HuaweiCAN::sendSetCommand(uint8_t b0, uint8_t b1, uint32_t val) {
  uint8_t d[8] = {b0, b1, 0, 0,
                  (uint8_t)(val >> 24),
                  (uint8_t)(val >> 16),
                  (uint8_t)(val >> 8),
                  (uint8_t)val};
  twaiSend(CAN_SET_ADDR, d);
  Serial.printf("CMD: [%02X %02X] = %lu (data: ", b0, b1, val);
  for (int i = 0; i < 8; i++) Serial.printf("%02X ", d[i]);
  Serial.println(")");
}

void HuaweiCAN::sendRequest() {
  uint8_t d[8] = {0};
  twaiSend(CAN_REQUEST_ADDR, d);
  Serial.println("Status request sent.");
}

// ===== RESPONSE DECODER =====
void HuaweiCAN::readAndDecodeResponse() {
  twai_message_t m;
  if (twai_receive(&m, 0) != ESP_OK || !m.extd || m.identifier != CAN_RESPONSE_ADDR) return;

  uint8_t *r = m.data;
  uint32_t val = ((uint32_t)r[4] << 24) | ((uint32_t)r[5] << 16) | ((uint32_t)r[6] << 8) | r[7];
  float decodedVal = 0;
  char dataStr[32];
  sprintf(dataStr, "%02X %02X %02X %02X %02X %02X %02X %02X",
          r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);

  switch (r[1]) {
    case 0x0E:
      status.operatingHours = val;
      Serial.printf("Operating Hours = %lu Hrs\tRaw = %lu\tID=0x0E\tData=%s\n", val, val, dataStr);
      break;
    case 0x70:
      status.inputPower = val / 1024.0;
      Serial.printf("Input Power = %.2f W\tRaw = %lu\tID=0x70\tData=%s\n", status.inputPower, val, dataStr);
      break;
    case 0x71:
      status.inputFreq = val / 1024.0;
      Serial.printf("Input Frequency = %.2f Hz\tRaw = %lu\tID=0x71\tData=%s\n", status.inputFreq, val, dataStr);
      break;
    case 0x72:
      status.inputCurrent = val / 1024.0;
      Serial.printf("Input Current = %.2f A\tRaw = %lu\tID=0x72\tData=%s\n", status.inputCurrent, val, dataStr);
      break;
    case 0x73:
      status.outputPower = val / 1024.0;
      Serial.printf("Output Power = %.2f W\tRaw = %lu\tID=0x73\tData=%s\n", status.outputPower, val, dataStr);
      break;
    case 0x74:
      status.efficiency = (val / 1024.0) * 100.0;
      Serial.printf("Efficiency = %.2f %%\tRaw = %lu\tID=0x74\tData=%s\n", status.efficiency, val, dataStr);
      break;
    case 0x75:
      status.outputVoltage = val / 1024.0;
      Serial.printf("Output Voltage = %.2f V\tRaw = %lu\tID=0x75\tData=%s\n", status.outputVoltage, val, dataStr);
      break;
    case 0x76:
      status.maxOutputCurrentPct = (val / 1250.0) * 100.0;
      Serial.printf("Max Output Current = %.2f %%\tRaw = %lu\tID=0x76\tData=%s\n", status.maxOutputCurrentPct, val, dataStr);
      break;
    case 0x78:
      status.inputVoltage = val / 1024.0;
      Serial.printf("Input Voltage = %.2f V\tRaw = %lu\tID=0x78\tData=%s\n", status.inputVoltage, val, dataStr);
      break;
    case 0x7F:
      status.outputTemp = val / 1024.0;
      Serial.printf("Output Temperature = %.2f °C\tRaw = %lu\tID=0x7F\tData=%s\n", status.outputTemp, val, dataStr);
      break;
    case 0x80:
      status.inputTemp = val / 1024.0;
      Serial.printf("Input Temperature = %.2f °C\tRaw = %lu\tID=0x80\tData=%s\n", status.inputTemp, val, dataStr);
      break;
    case 0x81:
      status.outputCurrent = val / 1024.0;
      status.outputCurrentFast = status.outputCurrent;
      Serial.printf("Output Current 1 (fast) = %.2f A\tRaw = %lu\tID=0x81\tData=%s\n", status.outputCurrentFast, val, dataStr);
      break;
    case 0x82:
      status.outputCurrentFiltered = val / 1024.0;
      Serial.printf("Output Current 2 (filtered) = %.2f A\tRaw = %lu\tID=0x82\tData=%s\n", status.outputCurrentFiltered, val, dataStr);
      break;
    case 0x83:
      status.alarmBits = val;
      Serial.printf("Alarm/Status Bits = 0x%08lX\tRaw = %lu\tID=0x83\tData=%s\n", val, val, dataStr);
      break;
    default:
      Serial.printf("Unknown ID 0x%02X\tRaw = %lu\tID=0x%02X\tData=%s\n", r[1], val, r[1], dataStr);
      break;
  }
}

// ===== PSU CONTROL FUNCTIONS =====
void HuaweiCAN::setVoltage(float v) {
  if (v < VMIN || v > VMAX) { Serial.println("Voltage range error."); return; }
  sendSetCommand(0x01, 0x00, round(v * 1020));
  Serial.printf("Set online voltage %.2f V (raw=%lu)\n", v, (uint32_t)round(v * 1020));
}

void HuaweiCAN::setOfflineVoltage(float v) {
  if (v < VOFFLINE_MIN || v > VMAX) { Serial.println("Offline voltage range error."); return; }
  sendSetCommand(0x01, 0x01, round(v * 1020));
  Serial.printf("Set offline voltage %.2f V (raw=%lu)\n", v, (uint32_t)round(v * 1020));
}

void HuaweiCAN::setCurrent(float i) {
  if (i < IMIN || i > IMAX) { Serial.println("Current range error."); return; }
  sendSetCommand(0x01, 0x03, round(i * 20));
  Serial.printf("Set online current %.2f A (raw=%lu)\n", i, (uint32_t)round(i * 20));
}

void HuaweiCAN::setOfflineCurrent(float i) {
  if (i < IMIN || i > IMAX) { Serial.println("Offline current range error."); return; }
  sendSetCommand(0x01, 0x04, round(i * 20));
  Serial.printf("Set offline current %.2f A (raw=%lu)\n", i, (uint32_t)round(i * 20));
}

void HuaweiCAN::setFanMode(bool manual) {
  uint8_t cmd = outputState ? 0x34 : 0x35;
  uint8_t d[8] = {0x01, cmd, 0, manual ? 1 : 0, 0, 0, 0, 0};
  twaiSend(CAN_SET_ADDR, d);
  fanManualMode = manual;
  Serial.printf("Fan %s mode (data: ", manual ? "MANUAL" : "AUTO");
  for (int i = 0; i < 8; i++) Serial.printf("%02X ", d[i]);
  Serial.println(")");
}

void HuaweiCAN::enableOutput(bool en) {
  uint8_t d[8] = {0x01, 0x32, 0, en ? 0x00 : 0x01, 0, 0, 0, 0};
  twaiSend(CAN_SET_ADDR, d);
  outputState = en;
  Serial.printf("Output %s (data: ", en ? "ENABLED" : "DISABLED");
  for (int i = 0; i < 8; i++) Serial.printf("%02X ", d[i]);
  Serial.println(")");
}

PSUStatus HuaweiCAN::getStatus() { return status; }