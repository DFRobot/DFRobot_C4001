 /*!
  * @file  motionDetection.ino
  * @brief  Example of radar detecting whether an object is moving
  * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
  * @license The MIT License (MIT)
  * @author ZhixinLiu(zhixin.liu@dfrobot.com)
  * @version V1.1
  * @date 2026-08-27
  * @url https://github.com/dfrobot/DFRobot_C4001
  */

#include "DFRobot_C4001.h"

//#define I2C_COMMUNICATION  //use I2C for communication, but use the serial port for communication if the line of codes were masked

#ifdef  I2C_COMMUNICATION
  /*
   * DEVICE_ADDR_0 = 0x2A     default iic_address
   * DEVICE_ADDR_1 = 0x2B
   */
  DFRobot_C4001_I2C radar(&Wire ,DEVICE_ADDR_0);
#else
/* ---------------------------------------------------------------------------------------------------------------------
 *    board   |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |   m0  |
 *     VCC    |            3.3V/5V             |        VCC           |    VCC    |   VCC   |  VCC  |     X      |  vcc  |
 *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |  gnd  |
 *     RX     |              TX                |     Serial1 TX1      |     5     |   5/D6  |  D2   |     X      |  tx1  |
 *     TX     |              RX                |     Serial1 RX1      |     4     |   4/D7  |  D3   |     X      |  rx1  |
 * ----------------------------------------------------------------------------------------------------------------------*/
/* Baud rate cannot be changed */
  #if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
    SoftwareSerial mySerial(4, 5);
    DFRobot_C4001_UART radar(&mySerial ,9600);
  #elif defined(ESP32)
    DFRobot_C4001_UART radar(&Serial1 ,9600 ,/*rx*/D2 ,/*tx*/D3);
  #else
    DFRobot_C4001_UART radar(&Serial1 ,9600);
  #endif
#endif

void setup()
{
  Serial.begin(115200);
  while(!Serial){}

  Serial.println(F("=============================="));
  Serial.println(F("DFRobot C4001 mmWave radar"));
  Serial.println(F("=============================="));

  // ---- 1) Connect ----
  while(!radar.begin()){
    Serial.print(F("Not found: "));
    Serial.println(radar.beginErrorString());
    delay(1000);
  }
  Serial.println(F("C4001 connected."));

  // ---- 2) Wait out the ~15s power-on lockout ----
  // Config commands are ignored for ~15s after the sensor powers on, so wait.
  if(!radar.waitForConfigReady()){
    Serial.println(F("WARN: config lockout did not clear in time"));
  }

  // ---- 3) Operating mode ----
  // eExitMode  = presence: "is anyone / is anything there?" (OUT pin reflects it)
  // eSpeedMode = speed: reports target speed & range (see mRangeVelocity)
  radar.setSensorMode(eExitMode);

  // ---- 4) Sensor status ----
  sSensorStatus_t st = radar.getStatus();
  Serial.println(F("status (work: 0=stopped 1=running | mode: 0=presence 1=speed | init: 0=no 1=yes)"));
  Serial.print(F("  work status  = ")); Serial.println(st.workStatus);
  Serial.print(F("  work mode    = ")); Serial.println(st.workMode);
  Serial.print(F("  init status  = ")); Serial.println(st.initStatus);

  // ---- 5) Detection window (centimeters) ----
  // min  : closest distance to detect (>= 30 cm)
  // max  : farthest distance (240 .. 2000 cm, i.e. ~2.4m .. 20m)
  // trig : distance that must be crossed to TRIGGER; often set = max
  C4001Result r;
  r = radar.setDetectionRange(/*min*/30, /*max*/240, /*trig*/240);
  Serial.print(F("set detect window (min/max/trig) : "));
  Serial.println(radar.resultToString(r));

  // ---- 6) Sensitivity ----
  // "trig sensitivity" (0..9): how easily the radar STARTS detecting a NEW
  //   person / motion. Higher = senses weaker motion and from farther away,
  //   but is also more prone to noise and false triggers.
  // "hold (keep) sensitivity" (0..9): the threshold used to KEEP the presence
  //   once it has triggered, so a person who enters and then sits / stands
  //   still is NOT dropped. This one matters most for "presence even when
  //   still" applications.
  r = radar.setTrigSensitivity(1);
  Serial.print(F("set trigger sensitivity (start) : "));
  Serial.println(radar.resultToString(r));
  r = radar.setKeepSensitivity(1);
  Serial.print(F("set hold sensitivity    (stay)  : "));
  Serial.println(radar.resultToString(r));

  // ---- 7) Delays ----
  // trig (turn-ON delay): 0..200, each unit = 0.01s. 100 = 1.0s delay between
  //   detecting a target and the output switching ON (de-bounces quick blips).
  // keep (turn-OFF delay): 4..3000, each unit = 0.5s. 4 = 2s that the output
  //   stays ON after the target leaves (hides momentary dropouts).
  //   NOTE: 4 is the sensor's minimum - smaller values are rejected.
  r = radar.setDelay(/*trig*/100, /*keep*/4);
  Serial.print(F("set turn-on / turn-off delays : "));
  Serial.println(radar.resultToString(r));

  // ---- 8) OUT pin PWM (UART backend) ----
  // pwm1 = duty (0..100%) when NO target is present
  // pwm2 = duty (0..100%) when a target IS present
  // timer = 0..255; ramp length between pwm1<->pwm2 = timer*64ms (e.g. 10 -> 0.64s)
  //
  // !! HARDWARE CAVEAT (HARDWARE_NOTES.md §1 at the repo root): on at least one
  // tested SEN0610 12m unit the OUT pin NEVER modulates - all values below are
  // accepted and read back correctly, but the pin stays static HIGH. On such
  // units treat OUT as a binary presence trigger only.
  r = radar.setPwm(/*pwm1*/50, /*pwm2*/0, /*timer*/10);
  Serial.print(F("set OUT pwm (no-target/target) : "));
  Serial.println(radar.resultToString(r));

  // 0 : OUT LOW with a target present, HIGH when none  (active on absence)
  // 1 : OUT HIGH with a target present, LOW when none   (active on presence)
  r = radar.setIoPolarity(1);
  Serial.print(F("set OUT polarity  : "));
  Serial.println(radar.resultToString(r));

  // ---- 9) Read it all back (what the sensor is actually using) ----
  Serial.println(F("-- active config --"));
  Serial.print(F("  trig sensitivity    = ")); Serial.println(radar.getTrigSensitivity());
  Serial.print(F("  hold sensitivity    = ")); Serial.println(radar.getKeepSensitivity());
  Serial.print(F("  min range           = ")); Serial.println(radar.getMinRange());
  Serial.print(F("  max range           = ")); Serial.println(radar.getMaxRange());
  Serial.print(F("  trig range          = ")); Serial.println(radar.getTrigRange());
  Serial.print(F("  hold time (x0.5s)   = ")); Serial.println(radar.getKeepTimeout());
  Serial.print(F("  trig delay (x0.01s) = ")); Serial.println(radar.getTrigDelay());
  Serial.print(F("  OUT polarity        = ")); Serial.println(radar.getIoPolarity());
  sPwmData_t pw = radar.getPwm();
  Serial.print(F("  pwm (no-target)     = ")); Serial.println(pw.pwm1);
  Serial.print(F("  pwm (target)        = ")); Serial.println(pw.pwm2);
  Serial.print(F("  pwm ramp timer      = ")); Serial.println(pw.timer);

  Serial.println(F(""));
  Serial.println(F("Setup done. Walk into the detection window to trigger the output."));
}

void loop()
{
  if(radar.motionDetection()){
    Serial.println(F(">> presence detected"));
    delay(300);   // short print-debounce so the log isn't spammy
  }
  delay(100);
}