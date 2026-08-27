/*!
 * @file  rawCommandConsole.ino
 * @brief Interactive raw-command console for the C4001 (UART).
 *
 * Forwards ASCII commands typed in the Serial Monitor to the sensor and prints
 * the raw response. Useful for inspecting the on-wire protocol directly - e.g.
 * investigating command/response framing - and for recovering from a bad baud
 * save via scanBaudRates().
 *
 * @note The C4001 uses a plain ASCII command/response protocol on UART ("Done",
 *       "Error", "Response <v1> <v2> ...", plus "$DF..." telemetry frames). The
 *       I2C backend is register-based, so sendRawCommand() returns empty there;
 *       use the UART backend with this console.
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @version 1.1.0
 * @url https://github.com/dfrobot/DFRobot_C4001
 */
#include "DFRobot_C4001.h"

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  SoftwareSerial mySerial(4, 5);
  DFRobot_C4001_UART radar(&mySerial, 9600);
#elif defined(ESP32)
  DFRobot_C4001_UART radar(&Serial1, 9600, /*rx*/D2, /*tx*/D3);
#else
  DFRobot_C4001_UART radar(&Serial1, 9600);
#endif

// Optional: baud candidates to scan at startup if the sensor was saved at an
// unexpected baud.
static const uint32_t BAUD_CANDIDATES[] = {9600, 19200, 38400, 57600, 115200};

void setup()
{
  Serial.begin(115200);
  while(!Serial){
    ;
  }
  while(!radar.begin()){
    Serial.print("Not found: ");
    Serial.println(radar.beginErrorString());
    delay(1000);
  }
  Serial.println("C4001 detected on UART.");
  Serial.println("Establishing a clean CLI state (sensorStop) so replies are");
  Serial.println("not interleaved with run-mode telemetry...");
  String stopResp = radar.sendRawCommand("sensorStop", 1000);
  Serial.print("auto sensorStop -> [");
  Serial.print(stopResp);
  Serial.println("]");

  Serial.println("Type a command and press Enter, e.g.:");
  Serial.println("  getRange | getSensitivity | getGpioMode 1 | getPwm | sensorStop");
  Serial.println("WARNING: 'setRunApp 1'/saveConfig/resetCfg switch run/config state");
  Serial.println("  and can leave the sensor streaming. Use 'sensorStop' to quiet it,");
  Serial.println("  and avoid raw saveConfig/resetCfg unless you know the recovery.");
  Serial.println("A CR+LF terminator is added automatically; replies print below.");

  // Uncomment to try recovering a sensor whose saved baud is unknown:
  /*==
  uint32_t found = radar.scanBaudRates(BAUD_CANDIDATES,
                                       sizeof(BAUD_CANDIDATES) / sizeof(BAUD_CANDIDATES[0]));
  if(found){
    Serial.print("Sensor answered at baud ");
    Serial.println(found);
  }else{
    Serial.println("No sensor answered at any candidate baud.");
  }
  */
}

String line = "";

void loop()
{
  while(Serial.available()){
    char c = Serial.read();
    if(c == '\n'){
      if(line.length() > 0){
        String resp = radar.sendRawCommand(line);
        Serial.print("> ");
        Serial.println(line);
        Serial.print("response: [");
        Serial.print(resp);
        Serial.println("]");
      }
      line = "";
    }else if(c != '\r'){
      line += c;  // backspace not handled; fine for short commands
    }
  }
  delay(10);
}