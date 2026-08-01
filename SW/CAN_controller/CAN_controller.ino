#include <Arduino.h>
#include <HuaweiCAN.h>

HuaweiCAN psu;

void printHelp() {
  Serial.println(F("\n--- Huawei PSU Short Commands ---"));
  Serial.println(F("vonXX   = Set online voltage"));
  Serial.println(F("vofXX   = Set offline voltage"));
  Serial.println(F("ionXX   = Set online current"));
  Serial.println(F("iofXX   = Set offline current"));
  Serial.println(F("fa/fm   = Fan auto/manual"));
  Serial.println(F("on/off  = Output enable/disable"));
  Serial.println(F("s       = Request PSU status"));
  Serial.println(F("h       = Help menu\n"));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Huawei PSU TWAI Controller (modular)");

  if (!psu.begin()) {
    Serial.println("TWAI init failed!");
    while (true);
  }

  Serial.println("TWAI initialized at 125 kbps.\n");
  printHelp();
}

void loop() {
  psu.readAndDecodeResponse();

  static unsigned long last_health_check = 0;
  if (millis() - last_health_check >= 1000) {
    last_health_check = millis();
    psu.checkBusHealth();
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); cmd.toLowerCase();

    if (cmd.startsWith("von")) psu.setVoltage(cmd.substring(3).toFloat());
    else if (cmd.startsWith("vof")) psu.setOfflineVoltage(cmd.substring(3).toFloat());
    else if (cmd.startsWith("ion")) psu.setCurrent(cmd.substring(3).toFloat());
    else if (cmd.startsWith("iof")) psu.setOfflineCurrent(cmd.substring(3).toFloat());
    else if (cmd == "fa") psu.setFanMode(false);
    else if (cmd == "fm") psu.setFanMode(true);
    else if (cmd == "on") psu.enableOutput(true);
    else if (cmd == "off") psu.enableOutput(false);
    else if (cmd == "s") psu.sendRequest();
    else if (cmd == "h") printHelp();
    else Serial.println("Invalid command. Type 'h' for help.");
  }
}