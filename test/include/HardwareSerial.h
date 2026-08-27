/* @file HardwareSerial.h
 * Host stub for the Arduino HardwareSerial class, compiled for the UART
 * backend of DFRobot_C4001. Non-functional: parser tests never drive real
 * serial traffic.
 */
#ifndef DSH_TEST_HARDWARESERIAL_H
#define DSH_TEST_HARDWARESERIAL_H

#include <stdint.h>

#ifndef SERIAL_8N1
#define SERIAL_8N1 0x0f
#endif

class HardwareSerial {
public:
  void begin(uint32_t /*baud*/) {}
  void begin(uint32_t /*baud*/, uint32_t /*config*/, int8_t /*rxPin*/, int8_t /*txPin*/) {}
  size_t write(const uint8_t * /*buffer*/, size_t /*size*/) { return 0; }
  size_t write(uint8_t /*b*/) { return 0; }
  int print(const char * /*s*/) { return 0; }
  int available(void) { return 0; }
  int read(void) { return -1; }
};

#endif // DSH_TEST_HARDWARESERIAL_H