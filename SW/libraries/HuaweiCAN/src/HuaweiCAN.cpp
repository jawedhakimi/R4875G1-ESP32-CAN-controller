#include "HuaweiCAN.h"
#include "esp_err.h"

HuaweiCAN::HuaweiCAN() : outputState(true), fanManualMode(false), lastRxMs(0) {}

bool HuaweiCAN::begin() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_config  = TWAI_TIMING_CONFIG_125KBITS();
  twai_filter_config_t  f_config  = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
  if (twai_start() != ESP_OK) return false;
  return true;
}

// ===== SEND UTILS =====
bool HuaweiCAN::twaiSend(uint32_t id, const uint8_t *data) {
  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = 1;
  msg.data_length_code = 8;
  memcpy(msg.data, data, 8);

  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(100));
  if (err != ESP_OK) {
#if HUAWEI_CAN_VERBOSE
    Serial.printf("CAN TX failed (id=0x%08lX): %s\n", (unsigned long)id, esp_err_to_name(err));
#endif
    return false;
  }
  return true;
}

bool HuaweiCAN::sendSetCommand(uint8_t b0, uint8_t b1, uint32_t val) {
  uint8_t d[8] = {b0, b1, 0, 0,
                  (uint8_t)(val >> 24),
                  (uint8_t)(val >> 16),
                  (uint8_t)(val >> 8),
                  (uint8_t)val};
  bool ok = twaiSend(CAN_SET_ADDR, d);
#if HUAWEI_CAN_VERBOSE
  Serial.printf("CMD: [%02X %02X] = %lu (data: ", b0, b1, (unsigned long)val);
  for (int i = 0; i < 8; i++) Serial.printf("%02X ", d[i]);
  Serial.println(ok ? ") OK" : ") TX FAILED");
#endif
  return ok;
}

bool HuaweiCAN::sendRequest() {
  uint8_t d[8] = {0};
  bool ok = twaiSend(CAN_REQUEST_ADDR, d);
#if HUAWEI_CAN_VERBOSE
  Serial.println(ok ? "Status request sent." : "Status request FAILED to send.");
#endif
  return ok;
}

// ===== RESPONSE DECODER =====
void HuaweiCAN::readAndDecodeResponse() {
  twai_message_t m;
  if (twai_receive(&m, 0) != ESP_OK || !m.extd || m.identifier != CAN_RESPONSE_ADDR) return;

  // Any recognised response frame counts as "link alive", regardless of which
  // register it carries.
  lastRxMs = millis();

  uint8_t *r = m.data;
  uint32_t val = ((uint32_t)r[4] << 24) | ((uint32_t)r[5] << 16) | ((uint32_t)r[6] << 8) | r[7];
  char dataStr[32];
  snprintf(dataStr, sizeof(dataStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
           r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);

  switch (r[1]) {
    case 0x0E:
      status.operatingHours = val;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Operating Hours = %lu Hrs\tRaw = %lu\tID=0x0E\tData=%s\n", (unsigned long)val, (unsigned long)val, dataStr);
#endif
      break;
    case 0x70:
      status.inputPower = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Input Power = %.2f W\tRaw = %lu\tID=0x70\tData=%s\n", status.inputPower, (unsigned long)val, dataStr);
#endif
      break;
    case 0x71:
      status.inputFreq = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Input Frequency = %.2f Hz\tRaw = %lu\tID=0x71\tData=%s\n", status.inputFreq, (unsigned long)val, dataStr);
#endif
      break;
    case 0x72:
      status.inputCurrent = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Input Current = %.2f A\tRaw = %lu\tID=0x72\tData=%s\n", status.inputCurrent, (unsigned long)val, dataStr);
#endif
      break;
    case 0x73:
      status.outputPower = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Output Power = %.2f W\tRaw = %lu\tID=0x73\tData=%s\n", status.outputPower, (unsigned long)val, dataStr);
#endif
      break;
    case 0x74:
      status.efficiency = (val / 1024.0f) * 100.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Efficiency = %.2f %%\tRaw = %lu\tID=0x74\tData=%s\n", status.efficiency, (unsigned long)val, dataStr);
#endif
      break;
    case 0x75:
      status.outputVoltage = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Output Voltage = %.2f V\tRaw = %lu\tID=0x75\tData=%s\n", status.outputVoltage, (unsigned long)val, dataStr);
#endif
      break;
    case 0x76:
      status.maxOutputCurrentPct = (val / 1250.0f) * 100.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Max Output Current = %.2f %%\tRaw = %lu\tID=0x76\tData=%s\n", status.maxOutputCurrentPct, (unsigned long)val, dataStr);
#endif
      break;
    case 0x78:
      status.inputVoltage = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Input Voltage = %.2f V\tRaw = %lu\tID=0x78\tData=%s\n", status.inputVoltage, (unsigned long)val, dataStr);
#endif
      break;
    case 0x7F:
      status.outputTemp = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Output Temperature = %.2f C\tRaw = %lu\tID=0x7F\tData=%s\n", status.outputTemp, (unsigned long)val, dataStr);
#endif
      break;
    case 0x80:
      status.inputTemp = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Input Temperature = %.2f C\tRaw = %lu\tID=0x80\tData=%s\n", status.inputTemp, (unsigned long)val, dataStr);
#endif
      break;
    case 0x81:
      status.outputCurrent = val / 1024.0f;
      status.outputCurrentFast = status.outputCurrent;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Output Current 1 (fast) = %.2f A\tRaw = %lu\tID=0x81\tData=%s\n", status.outputCurrentFast, (unsigned long)val, dataStr);
#endif
      break;
    case 0x82:
      status.outputCurrentFiltered = val / 1024.0f;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Output Current 2 (filtered) = %.2f A\tRaw = %lu\tID=0x82\tData=%s\n", status.outputCurrentFiltered, (unsigned long)val, dataStr);
#endif
      break;
    case 0x83:
      status.alarmBits = val;
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Alarm/Status Bits = 0x%08lX\tRaw = %lu\tID=0x83\tData=%s\n", (unsigned long)val, (unsigned long)val, dataStr);
#endif
      break;
    default:
#if HUAWEI_CAN_VERBOSE
      Serial.printf("Unknown ID 0x%02X\tRaw = %lu\tID=0x%02X\tData=%s\n", r[1], (unsigned long)val, r[1], dataStr);
#endif
      break;
  }
}

bool HuaweiCAN::isStale() const {
  if (lastRxMs == 0) return true; // never received a frame
  return (millis() - lastRxMs) > PSU_STALE_TIMEOUT_MS;
}

uint32_t HuaweiCAN::msSinceLastUpdate() const {
  if (lastRxMs == 0) return UINT32_MAX;
  return millis() - lastRxMs;
}

bool HuaweiCAN::checkBusHealth() {
  twai_status_info_t info;
  if (twai_get_status_info(&info) != ESP_OK) return false;

  switch (info.state) {
    case TWAI_STATE_RUNNING:
      return true;

    case TWAI_STATE_BUS_OFF:
#if HUAWEI_CAN_VERBOSE
      Serial.println("CAN bus-off detected -- initiating recovery");
#endif
      twai_initiate_recovery();
      return false;

    case TWAI_STATE_STOPPED:
      // Bus-off recovery lands here; the controller must be restarted.
#if HUAWEI_CAN_VERBOSE
      Serial.println("CAN controller stopped -- restarting");
#endif
      twai_start();
      return false;

    default: // TWAI_STATE_RECOVERING
      return false;
  }
}

// ===== PSU CONTROL FUNCTIONS =====
bool HuaweiCAN::setVoltage(float v) {
  if (v < PSU_VMIN || v > PSU_VMAX) {
#if HUAWEI_CAN_VERBOSE
    Serial.println("Voltage range error.");
#endif
    return false;
  }
  uint32_t raw = (uint32_t)round(v * 1020.0f);
  bool ok = sendSetCommand(0x01, 0x00, raw);
#if HUAWEI_CAN_VERBOSE
  Serial.printf("Set online voltage %.2f V (raw=%lu)\n", v, (unsigned long)raw);
#endif
  return ok;
}

bool HuaweiCAN::setOfflineVoltage(float v) {
  if (v < PSU_VOFFLINE_MIN || v > PSU_VMAX) {
#if HUAWEI_CAN_VERBOSE
    Serial.println("Offline voltage range error.");
#endif
    return false;
  }
  uint32_t raw = (uint32_t)round(v * 1020.0f);
  bool ok = sendSetCommand(0x01, 0x01, raw);
#if HUAWEI_CAN_VERBOSE
  Serial.printf("Set offline voltage %.2f V (raw=%lu)\n", v, (unsigned long)raw);
#endif
  return ok;
}

bool HuaweiCAN::setCurrent(float i) {
  if (i < PSU_IMIN || i > PSU_IMAX) {
#if HUAWEI_CAN_VERBOSE
    Serial.println("Current range error.");
#endif
    return false;
  }
  uint32_t raw = (uint32_t)round(i * 20.0f);
  bool ok = sendSetCommand(0x01, 0x03, raw);
#if HUAWEI_CAN_VERBOSE
  Serial.printf("Set online current %.2f A (raw=%lu)\n", i, (unsigned long)raw);
#endif
  return ok;
}

bool HuaweiCAN::setOfflineCurrent(float i) {
  if (i < PSU_IMIN || i > PSU_IMAX) {
#if HUAWEI_CAN_VERBOSE
    Serial.println("Offline current range error.");
#endif
    return false;
  }
  uint32_t raw = (uint32_t)round(i * 20.0f);
  bool ok = sendSetCommand(0x01, 0x04, raw);
#if HUAWEI_CAN_VERBOSE
  Serial.printf("Set offline current %.2f A (raw=%lu)\n", i, (unsigned long)raw);
#endif
  return ok;
}

// NOTE (bugfix): the previous implementation picked between the 0x34 ("On-line")
// and 0x35 ("Off-line/Default") fan registers based on `outputState`. Per the
// Huawei protocol doc, "On-line" means "CAN communication is present" and
// "Off-line" means "CAN comms have timed out for ~60s" -- it has nothing to do
// with whether output is enabled. Since this driver polls the rectifier every
// 200ms, it is *always* On-line while running, so the old code silently only
// ever touched the On-line register when the output happened to be on, and
// only the Off-line/Default register (mostly irrelevant while we're connected)
// when the output was off. Fix: write both registers so the requested mode is
// honoured no matter what output state we're in.
bool HuaweiCAN::setFanMode(bool manual) {
  uint8_t val = manual ? 0x01 : 0x00;
  uint8_t dOnline[8]  = {0x01, 0x34, 0, val, 0, 0, 0, 0};
  uint8_t dOffline[8] = {0x01, 0x35, 0, val, 0, 0, 0, 0};

  bool ok1 = twaiSend(CAN_SET_ADDR, dOnline);
  bool ok2 = twaiSend(CAN_SET_ADDR, dOffline);
  fanManualMode = manual;

#if HUAWEI_CAN_VERBOSE
  Serial.printf("Fan %s mode (On-line+Off-line regs written, %s)\n",
                manual ? "MANUAL" : "AUTO",
                (ok1 && ok2) ? "OK" : "TX FAILED");
#endif
  return ok1 && ok2;
}

bool HuaweiCAN::enableOutput(bool en) {
  uint8_t d[8] = {0x01, 0x32, 0, (uint8_t)(en ? 0x00 : 0x01), 0, 0, 0, 0};
  bool ok = twaiSend(CAN_SET_ADDR, d);
  outputState = en;
#if HUAWEI_CAN_VERBOSE
  Serial.printf("Output %s (%s)\n", en ? "ENABLED" : "DISABLED", ok ? "OK" : "TX FAILED");
#endif
  return ok;
}

PSUStatus HuaweiCAN::getStatus() { return status; }
