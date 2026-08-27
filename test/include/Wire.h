/*!
 * @file Wire.h
 * @brief Host stub of the Arduino Wiring TwoWire class with the methods the
 *        DFRobot_C4001 I2C backend uses. This is a non-functional stand-in:
 *        the parser tests do not drive real I2C traffic.
 * @see test/harness.cpp
 */
#ifndef DSH_TEST_WIRE_H
#define DSH_TEST_WIRE_H

#include <stddef.h>
#include <stdint.h>

class TwoWire {
public:
  void begin(void) {}
  void beginTransmission(uint8_t /*address*/) {}
  uint8_t write(uint8_t /*data*/) { return 1; }
  size_t  write(uint8_t * /*data*/, uint8_t /*size*/) { return 0; }
  uint8_t endTransmission(void) { return 0; }
  uint8_t endTransmission(bool /*sendStop*/) { return 0; }
  uint8_t requestFrom(uint8_t /*address*/, uint8_t /*quantity*/) { return 0; }
  int  available(void) { return 0; }
  int  read(void) { return -1; }
};

extern TwoWire Wire;   // global instance (declared in a .cpp below)
#endif // DSH_TEST_WIRE_H