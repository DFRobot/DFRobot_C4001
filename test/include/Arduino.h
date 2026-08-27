/*!
 * @file Arduino.h
 * @brief Minimal host-side stub of the Arduino core types used by
 *        DFRobot_C4001, so the library's parsers compile and run on the PC
 *        under MemorySanitizer/AddressSanitizer.
 */
#ifndef DSH_TEST_ARDUINO_H
#define DSH_TEST_ARDUINO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>

#ifdef __cplusplus
// ---------------------------------------------------------------------------
// millis() / delay(): clock-backed host stand-ins.
// ---------------------------------------------------------------------------
static uint32_t _dsh_test_epoch_ms(void)
{
  static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
  return (uint32_t)ms;
}

static uint32_t millis(void) { return _dsh_test_epoch_ms(); }

static void delay(uint32_t ms)
{
  auto target = std::chrono::steady_clock::now()
    + std::chrono::milliseconds(ms);
  while(std::chrono::steady_clock::now() < target){
    // busy-wait is fine for the host parser tests
  }
}
#endif

/* Arduino's String. The production code uses only a small subset
 * (append, c_str, length, indexOf, element access); modeled with std::string.
 */
class String {
public:
  String() : s_("") {}
  String(const char *c) : s_(c ? c : "") {}
  String(const String &o) : s_(o.s_) {}

  // Numeric constructors.
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(uint8_t v) : s_(std::to_string((unsigned)v)) {}
  String(uint16_t v) : s_(std::to_string((unsigned)v)) {}
  // NOTE: uint32_t aliases 'unsigned int' on this host, so no separate ctor.

  // String(float, decimalPlaces) - Arduino semantics.
  String(float f, uint8_t decimalPlaces) {
    if(decimalPlaces == 0){
      s_ = std::to_string((long)f);
    } else {
      char buf[64];
      snprintf(buf, sizeof(buf), "%.*f", (int)decimalPlaces, (double)f);
      s_ = buf;
    }
  }

  size_t length(void) const { return s_.length(); }
  const char *c_str(void) const { return s_.c_str(); }

  char operator[](size_t idx) const { return s_[idx]; }
  char &operator[](size_t idx) { return s_[idx]; }

  int indexOf(const char *ch) const {
    std::size_t p = s_.find(ch);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(char ch) const {
    std::size_t p = s_.find(ch);
    return p == std::string::npos ? -1 : (int)p;
  }

  bool operator==(const String &o) const { return s_ == o.s_; }

  String operator+(const char *c) const { String r(s_); r.s_ += c; return r; }
  String operator+(const String &o) const { String r(s_); r.s_ += o.s_; return r; }

  String &operator+=(const String &o)   { s_ += o.s_; return *this; }
  String &operator+=(const char *c)     { if(c) s_ += c; return *this; }
  String &operator+=(char c)            { s_ += c; return *this; }
  String &operator+=(int v)             { s_ += std::to_string(v); return *this; }
  String &operator+=(unsigned int v)    { s_ += std::to_string(v); return *this; }
  String &operator+=(uint8_t v)         { s_ += std::to_string((unsigned)v); return *this; }
  String &operator+=(uint16_t v)        { s_ += std::to_string((unsigned)v); return *this; }
  String &operator+=(float f)           { s_ += std::to_string((long)f); return *this; }

private:
  std::string s_;
  // private helper for copy construction of the underlying representation
  String(std::string s) : s_(s) {}
};

#endif // DSH_TEST_ARDUINO_H