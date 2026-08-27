/*!
 * @file  persistenceRoundtrip.ino
 * @brief Demonstrates the config lifecycle: configure, save(), factoryReset().
 *
 * Shows reading the current config, changing it, persisting with save(), and
 * restoring defaults with factoryReset(). Watch the Serial Monitor.
 * Works over both I2C and UART.
 * @license The MIT License (MIT)
 * @version 1.1.0
 */
#include "DFRobot_C4001.h"

#define I2C_COMMUNICATION   // comment out to use UART

#ifdef I2C_COMMUNICATION
  DFRobot_C4001_I2C radar(&Wire, DEVICE_ADDR_0);
#else
  #if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
    SoftwareSerial mySerial(4, 5);
    DFRobot_C4001_UART radar(&mySerial, 9600);
  #elif defined(ESP32)
    DFRobot_C4001_UART radar(&Serial1, 9600, /*rx*/D2, /*tx*/D3);
  #else
    DFRobot_C4001_UART radar(&Serial1, 9600);
  #endif
#endif

void dumpConfig()
{
  Serial.print("min="); Serial.print(radar.getMinRange());
  Serial.print(" max="); Serial.print(radar.getMaxRange());
  Serial.print(" trig="); Serial.print(radar.getTrigRange());
  Serial.print("  keep="); Serial.print(radar.getKeepTimeout());
  Serial.print("  trigDelay="); Serial.println(radar.getTrigDelay());
}

void show(const char* label, C4001Result r)
{
  Serial.print(label);
  Serial.println(radar.resultToString(r));
}

void setup()
{
  Serial.begin(115200);
  while(!Serial){}
  while(!radar.begin()){
    Serial.print("Not found: ");
    Serial.println(radar.beginErrorString());
    delay(1000);
  }
  Serial.println("Device connected!");
  if(!radar.waitForConfigReady()){
    Serial.println("Timed out waiting for config lockout");
  }

  Serial.println("=== 1. current working config ===");
  dumpConfig();

  Serial.println("=== 2. set some params (persisted automatically) ===");
  show("setDetectionRange:  ", radar.setDetectionRange(60, 1500, 1500));
  show("setTrigSensitivity: ", radar.setTrigSensitivity(3));
  show("setKeepSensitivity: ", radar.setKeepSensitivity(4));

  Serial.println("=== 3. explicit save() (idempotent) ===");
  Serial.print("save issued: ");
  Serial.println(radar.save() ? "true" : "false");
  Serial.println("--- config after change ---");
  dumpConfig();

  Serial.println("=== 4. factoryReset() ===");
  Serial.print("factoryReset issued: ");
  Serial.println(radar.factoryReset() ? "true" : "false");

  Serial.println("=== 5. restore a known-good config + save() ===");
  show("setDetectionRange:  ", radar.setDetectionRange(30, 1000, 1000));
  show("setTrigSensitivity: ", radar.setTrigSensitivity(1));
  show("setKeepSensitivity: ", radar.setKeepSensitivity(2));
  Serial.print("save issued: ");
  Serial.println(radar.save() ? "true" : "false");
  dumpConfig();

  Serial.println("Done. Loop is idle; press reset to re-run.");
}

void loop()
{
  delay(1000);
}