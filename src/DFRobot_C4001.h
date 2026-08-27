/*!
 * @file DFRobot_C4001.h
 * @brief DFRobot_C4001 class definition: C4001 24 GHz mmWave presence/range radar
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [ZhixinLiu](zhixin.liu@dfrobot.com)
 * @version V1.1
 * @date 2026-08-27
 * @url https://github.com/DFRobot/DFRobot_C4001
 */
#ifndef __DFROBOT_C4001_H__
#define __DFROBOT_C4001_H__

#include <Arduino.h>
#include <Wire.h>

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#include "SoftwareSerial.h"
#else
#include "HardwareSerial.h"
#endif

/* Public configuration constants: I2C addresses selectable on the module.
   Kept as macros (file scope) because sketches use the bare names. */
#define DEVICE_ADDR_0   0x2A   ///< default I2C address
#define DEVICE_ADDR_1   0x2B   ///< alternate I2C address

/**
 * @struct sSensorStatus_t
 * @brief sensor status
 */
typedef struct{
  uint8_t workStatus;   ///< 0 stop / 1 start
  uint8_t workMode;     ///< 0 presence detection / 1 speed and ranging
  uint8_t initStatus;   ///< 0 not initialized / 1 initialized
}sSensorStatus_t;


/**
 * @struct sPrivateData_t
 * @brief speed mode data
 */
typedef struct{
  uint8_t number;   ///< target count
  float speed;      ///< m/s
  float range;      ///< m
  uint32_t energy;  ///< dimensionless
}sPrivateData_t;

/**
 * @struct sResponseData_t
 * @brief response data
 */
typedef struct{
  bool status;        ///< true once a valid "Res..." frame was parsed
  float response1;    ///< first numeric field
  float response2;    ///< second numeric field
  float response3;    ///< third numeric field
}sResponseData_t;


/**
 * @struct sPwmData_t
 * @brief config pwm data param
 * @warning These are stored-configuration values; on some units the OUT pin
 *          does not physically follow them (see HARDWARE_NOTES.md §1).
 */
typedef struct{
  uint8_t pwm1;
  uint8_t pwm2;
  uint8_t timer;
}sPwmData_t;

/**
 * @struct sAllData_t
 * @brief sensor return data
 */
typedef struct{
  uint8_t exist;
  sSensorStatus_t sta;
  sPrivateData_t target;
}sAllData_t;

/**
 * @enum eMode_t
 * @brief sensor work mode
 */
typedef enum{
  eExitMode  = 0x00,
  eSpeedMode = 0x01,
}eMode_t;

/**
 * @enum eSwitch_t
 * @brief Micromotion detection switch
 */
typedef enum{
  eON  = 0x01,
  eOFF = 0x00,
}eSwitch_t;

/**
 * @enum eSetMode_t
 * @brief Set parameters for the sensor working status
 */
typedef enum{
  eStartSen   = 0x55,
  eStopSen    = 0x33,
  eResetSen   = 0xCC,
  eRecoverSen = 0xAA,
  eSaveParams = 0x5C,
  eChangeMode = 0x3B,
}eSetMode_t;

/**
 * @enum C4001Result
 * @brief Result of a configuration write, distinguishing "command accepted"
 *        from "value actually active and confirmed."
 */
enum class C4001Result {
  Success,             ///< written and (if requested) read-back confirmed
  AckedButUnconfirmed, ///< sensor accepted the command but read-back didn't match/confirm
  Rejected,            ///< rejected (out-of-range argument, or sensor Error)
  Timeout,             ///< no response within the wait window
  NotReady,            ///< sensor not ready for config (15s post-boot lockout)
  NotSupported         ///< not supported on the active backend (e.g. I2C PWM)
};

/**
 * @enum eBeginError_t
 * @brief Reason a begin() attempt failed (see beginErrorString()).
 */
typedef enum{
  eBeginNone        = 0,  ///< no error (last begin() succeeded)
  eBeginNoResponse,       ///< sensor never answered
  eBeginWrongAddress,     ///< (I2C) no ACK at the configured address
  eBeginWrongBaud,        ///< (UART) no reply at the configured baud
  eBeginBootLockout,      ///< (reserved) sensor present but still booting
  eBeginBusError,         ///< transport/transaction error
}eBeginError_t;

/**
 * @class DFRobot_C4001
 * @brief Base class for the C4001 24 GHz mmWave presence/range radar.
 *
 * Lifecycle model:
 *   - The sensor rejects configuration commands for ~15 s after power-on
 *     (the "config lockout"). Check isReadyForConfig() / waitForConfigReady()
 *     before configuring; setters return C4001Result::NotReady in that window.
 *   - Configuration semantics differ by backend. On the UART (ASCII) backend,
 *     settings are applied only while the sensor is stopped; the set...()
 *     methods handle this automatically (stop -> write -> save -> start). On
 *     the register-based I2C backend, register writes apply immediately and
 *     are persisted with a save; the sensor need not be stopped first (this
 *     matches the vendor Python reference). The library hides these details.
 *   - set...() persist automatically; save() is an explicit persist-now and
 *     factoryReset() restores defaults.
 */
class DFRobot_C4001{
public:
  DFRobot_C4001();
  virtual ~DFRobot_C4001();   ///< polymorphic base: virtual destructor

  /**
   * @fn motionDetection
   * @brief motion Detection
   * @return true or false
   * @note I2C reads the live presence bit. UART latches the last confirmed
   *       presence and keeps reporting true for MOTION_DEBOUNCE_POLLS empty
   *       telemetry frames after the target disappears, so the reading does
   *       not flicker between telemetry bursts.
   */
  bool motionDetection(void);

  /**
   * @fn setSensor
   * @brief Set the Sensor object
   * @param mode
   * @n  eStartSen        start collect
   * @n  eStopSen         stop collect
   * @n  eResetSen        reset sensor
   * @n  eRecoverSen      recover params
   * @n  eSaveParams      save config
   * @n  eChangeMode      change mode
   */
  void setSensor(eSetMode_t mode);

  /**
   * @fn setDelay
   * @brief Set the Delay object
   * @param trig Trigger delay, unit 0.01s, range 0~2s (0~200)
   * @param keep Maintain the detection timeout, unit 0.5s, range 2~1500 seconds (4~3000)
   * @return C4001Result: Success when written (and confirmed), see C4001Result.
   */
  C4001Result setDelay(uint8_t trig, uint16_t keep, bool verify = true);

  /**
   * @fn getTrigDelay
   * @brief Get the Trig Delay object
   * @return uint8_t, or 0 when no read-back answer arrives (bare getter: a
   *         genuine 0 value is indistinguishable from "no answer")
   */
  uint8_t getTrigDelay(void);

  /**
   * @fn getKeepTimeout
   * @brief Get the keep (maintain-detection) timeout
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getKeepTimeout(void);

  /**
   * @fn getTrigRange
   * @brief Get the Trig Range object
   * @n     The triggering distance, in cm, ranges from 2.4 to 20m (240 to 2000).
   * @n     The actual configuration range does not exceed the maximum and minimum detection distance.
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getTrigRange(void);

  /**
   * @fn getMaxRange
   * @brief Get the Max Range object
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getMaxRange(void);

  /**
   * @fn getMinRange
   * @brief Get the Min Range object
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getMinRange(void);

  /**
   * @fn setDetectionRange
   * @brief Set the Detection Range object
   * @param min Detection range Minimum distance, unit cm, range 0.3~20m (30~2000), not exceeding max, otherwise the function is abnormal.
   * @param max Detection range Maximum distance, unit cm, range 2.4~20m (240~2000)
   * @param trig The trigger distance (unit: cm) ranges from 2.4 to 20m (240 to 2000). The actual configuration range does not exceed the maximum and minimum detection distance.
   * @return C4001Result: Success for (and read-back) confirmation.
   */
  C4001Result setDetectionRange(uint16_t min, uint16_t max, uint16_t trig, bool verify = true);

  /**
   * @fn setTrigSensitivity
   * @brief Set trigger sensitivity, 0~9
   * @param sensitivity
   * @return C4001Result.
   */
  C4001Result setTrigSensitivity(uint8_t sensitivity, bool verify = true);

  /**
   * @fn setKeepSensitivity
   * @brief Set the Keep Sensitivity object，0~9
   * @param sensitivity
   * @return C4001Result.
   */
  C4001Result setKeepSensitivity(uint8_t sensitivity, bool verify = true);

  /**
   * @fn getTrigSensitivity
   * @brief Get the Trig Sensitivity object
   * @return uint8_t, or 0 when no read-back answer arrives
   */
  uint8_t getTrigSensitivity(void);

  /**
   * @fn getKeepSensitivity
   * @brief Get the Keep Sensitivity object
   * @return uint8_t, or 0 when no answer was read back
   */
  uint8_t getKeepSensitivity(void);

  /**
   * @fn getStatus
   * @brief Get the Status object
   * @return sSensorStatus_t
   * @n     workStatus
   * @n       0 stop
   * @n       1 start
   * @n     workMode
   * @n       0 indicates presence detection
   * @n       1 is speed measurement and ranging
   * @n     initStatus
   * @n       0 not init
   * @n       1 init success
   * @note On UART this reports whatever telemetry has streamed in recently;
   *       it never (re)starts the sensor.
   */
  sSensorStatus_t getStatus(void);

  /**
   * @fn setIoPolarity
   * @brief Set the Io Polarity object
   * @param value
   * @n     0：Output low level when there is a target, output high level when there is no target
   * @n     1: Output high level when there is a target, output low level when there is no target (default)
   * @warning HARDWARE CAVEAT (evidence in HARDWARE_NOTES.md §1): on at least one
   *          tested SEN0610 12m unit this setting is accepted and read back
   *          exactly, but the OUT pin itself stays static HIGH and never follows
   *          the configured polarity. Treat OUT as a binary presence trigger
   *          unless measurements on YOUR unit prove otherwise.
   * @return C4001Result.
   */
  C4001Result setIoPolarity(uint8_t value, bool verify = true);

  /**
   * @fn getIoPolarity
   * @brief Get the Io Polarity object
   * @return uint8_t The level of the signal output by the pin after the configured I/O port detects the target,
   *         or 0 when no read-back answer arrives
   */
  uint8_t getIoPolarity(void);

  /**
   * @fn setPwm
   * @brief Set the Pwm object
   * @param pwm1 When no target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * @param pwm2 After the target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * @param timer timer The value ranges from 0 to 255, corresponding to timer x 64ms
   * @n     For example, timer=20, it takes 20*64ms=1.28s for the duty cycle to change from pwm1 to pwm2.
   * @warning HARDWARE CAVEAT (evidence in HARDWARE_NOTES.md §1): on at least one
   *          tested SEN0610 12m unit these values are stored and read back
   *          correctly, yet the OUT pin never modulates - it stays static HIGH
   *          for every duty/polarity combination tried (8-combo instrumented
   *          sweep). Verify on your unit before relying on OUT as a PWM/analog
   *          output; a binary presence trigger has been observed reliably.
   * @return C4001Result.
   */
  C4001Result setPwm(uint8_t pwm1, uint8_t pwm2, uint8_t timer, bool verify = true);


  /**
   * @fn getPwm
   * @brief Get the Pwm object
   * @note A matching read-back only proves the configuration was stored; on
   *       some units it does NOT prove the OUT pin modulates
   *       (see HARDWARE_NOTES.md §1).
   * @return sPwmData_t
   * @retval pwm1  When no target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * @retval pwm2  After the target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * @retval timer The value ranges from 0 to 255, corresponding to timer x 64ms
   * @n      For example, timer=20, it takes 20*64ms=1.28s for the duty cycle to change from pwm1 to pwm2.
   */
  sPwmData_t getPwm(void);

  /**
   * @fn setSensorMode
   * @brief Set the Sensor Mode object
   * @param mode
   * @return C4001Result.
   */
  C4001Result setSensorMode(eMode_t mode, bool verify = true);

  /**
   * @fn getTargetNumber
   * @brief Get the Target Number object
   * @return uint8_t, or 0 when there is no answered frame / no target
   * @note I2C caches the whole target record while a target is present and
   *       holds it for ~10 empty polls after it disappears. UART returns the
   *       target count of the most recently parsed $DFDMD frame directly.
   */
  uint8_t getTargetNumber(void);

  /**
   * @fn getTargetSpeed
   * @brief Get the Target Speed object
   * @return float, or 0 when no answered frame has arrived
   * @note Returns the value cached by the last getTargetNumber() call; call
   *       getTargetNumber() first, then read speed/range/energy.
   */
  float getTargetSpeed(void);

  /**
   * @fn getTargetRange
   * @brief Get the Target Range object
   * @return float, or 0 when no answered frame has arrived
   * @note Returns the value cached by the last getTargetNumber() call.
   */
  float getTargetRange(void);

  /**
   * @fn getTargetEnergy
   * @brief Get the Target Energy object
   * @return uint32_t, or 0 when no answered frame has arrived
   * @note Returns the value cached by the last getTargetNumber() call.
   */
  uint32_t getTargetEnergy(void);

  /**
   * @fn setDetectThres
   * @brief Set the Detect Thres object
   * @param min Detection range Minimum distance, unit cm, range 0.3~20m (30~2000), not exceeding max, otherwise the function is abnormal.
   * @param max Detection range Maximum distance, unit cm, range 2.4~20m (240~2000)
   * @param thres Target detection threshold, dimensionless unit 0.1, range 0~6553.5 (0~65535)
   * @return C4001Result.
   */
  C4001Result setDetectThres(uint16_t min, uint16_t max, uint16_t thres, bool verify = true);

  /**
   * @fn getTMinRange
   * @brief get speed Min Range
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getTMinRange(void);

  /**
   * @fn getTMaxRange
   * @brief get speed Max Range
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getTMaxRange(void);

  /**
   * @fn getThresRange
   * @brief Get the Thres Range object
   * @return uint16_t, or 0 when no read-back answer arrives
   */
  uint16_t getThresRange(void);

  /**
   * @fn setFrettingDetection
   * @brief Set the Fretting Detection object
   * @param sta
   * @return C4001Result.
   */
  C4001Result setFrettingDetection(eSwitch_t sta, bool verify = true);

  /**
   * @fn getFrettingDetection
   * @brief Get the Fretting Detection object
   * @return eSwitch_t, or 0 when no answer was read back
   */
  eSwitch_t getFrettingDetection(void);

  /**
   * @fn beginConfig
   * @brief Enter configuration mode (stop the sensor).
   * @return true if accepted, false on failure/timeout.
   * @note The library's setXxx() already handle stop/save/start internally; this
   *       is an explicit convenience for running raw commands while stopped and
   *       then persisting/resuming with endConfig().
   */
  bool beginConfig(void);

  /**
   * @fn endConfig
   * @brief Leave configuration mode: persist current settings and resume.
   * @return true if accepted, false on failure.
   */
  bool endConfig(void);

  /**
   * @fn save
   * @brief Persist the current working configuration to the sensor's flash.
   * @return true if accepted.
   */
  bool save(void);

  /**
   * @fn factoryReset
   * @brief Restore the sensor's factory-default configuration.
   * @return true if accepted.
   */
  bool factoryReset(void);

  /**
   * @fn sendRawCommand
   * @brief Send a raw ASCII command to the sensor and capture the raw response.
   * @note UART backend only. The I2C backend is register-based and returns an
   *       empty String.
   * @param cmd The ASCII command to send. On UART a CR+LF terminator is appended
   *       automatically, matching the documented protocol framing.
   * @param timeoutMs Maximum quiet time (ms) to keep collecting the response.
   * @return The raw response text, or an empty String on timeout / when unsupported.
   */
  String sendRawCommand(const String &cmd, uint32_t timeoutMs = 500);

  /**
   * @fn resultToString
   * @brief Human-readable description of a C4001Result.
   * @param result The result value to describe.
   * @return A static string constant.
   */
  const char *resultToString(C4001Result result);

  /**
   * @fn isReadyForConfig
   * @brief Report whether the sensor is past the 15-second post-power-on
   *        configuration lockout.
   * @note The lockout baseline is the first successful begin(); a fresh
   *       MCU+sensor co-boot maps well. Poll it, or just call waitForConfigReady().
   * @return true once configuration is allowed.
   */
  bool isReadyForConfig(void);

  /**
   * @fn waitForConfigReady
   * @brief Block (up to timeoutMs) until the sensor is ready for configuration.
   * @param timeoutMs Maximum time to wait, in ms.
   * @return true once ready, false if the timeout elapsed first.
   */
  bool waitForConfigReady(uint32_t timeoutMs = CONFIG_READY_TIMEOUT_MS);

  /**
   * @fn beginErrorString
   * @brief Textual description of why the last begin() attempt failed.
   * @return A static string constant.
   */
  const char* beginErrorString(void);

  /* Deprecated 1.0.0 spellings, kept so existing sketches keep compiling
     (and highlighting) unchanged. They forward to the renamed methods. */
  [[deprecated("renamed to setIoPolarity")]]
  C4001Result setIoPolaity(uint8_t value, bool verify = true);
  [[deprecated("renamed to getIoPolarity")]]
  uint8_t getIoPolaity(void);
  [[deprecated("renamed to getKeepTimeout")]]
  uint16_t getKeepTimerout(void);

protected:
  /* ================= constants ================= */

  /* Backend selector; set by the subclass constructors. */
  static constexpr uint8_t I2C_FLAG  = 0x01;
  static constexpr uint8_t UART_FLAG = 0x02;

  /* --- I2C register map ---
   *
   * The C4001 exposes ONE mode-dependent register file: the shared control
   * registers below always work, but the 0x10..0x2A window is reinterpreted
   * depending on the active work mode. The presence-mode and speed-mode
   * groups therefore alias the same hardware addresses BY DESIGN (e.g. 0x20
   * is trig sensitivity in presence mode, CFAR threshold low byte in speed
   * mode). This is correct hardware behavior, not a bug.
   */

  /* Shared control registers (both modes). */
  static constexpr uint8_t REG_STATUS       = 0x00;
  static constexpr uint8_t REG_CTRL0        = 0x01;
  static constexpr uint8_t REG_CTRL1        = 0x02;
  static constexpr uint8_t REG_SOFT_VERSION = 0x03;

  /* Presence-mode map (eExitMode): used by setDetectionRange / setDelay /
   * setTrigSensitivity / setKeepSensitivity and their getters. */
  static constexpr uint8_t REG_RESULT_STATUS     = 0x10;
  static constexpr uint8_t REG_TRIG_SENSITIVITY  = 0x20;
  static constexpr uint8_t REG_KEEP_SENSITIVITY  = 0x21;
  static constexpr uint8_t REG_TRIG_DELAY        = 0x22;
  static constexpr uint8_t REG_KEEP_TIMEOUT_L    = 0x23;
  static constexpr uint8_t REG_KEEP_TIMEOUT_H    = 0x24;
  static constexpr uint8_t REG_E_MIN_RANGE_L     = 0x25;
  static constexpr uint8_t REG_E_MIN_RANGE_H     = 0x26;
  static constexpr uint8_t REG_E_MAX_RANGE_L     = 0x27;
  static constexpr uint8_t REG_E_MAX_RANGE_H     = 0x28;
  static constexpr uint8_t REG_E_TRIG_RANGE_L    = 0x29;
  static constexpr uint8_t REG_E_TRIG_RANGE_H    = 0x2A;

  /* Speed-mode map (eSpeedMode), aliased onto the same addresses as the
   * presence-mode block above: used by setDetectThres / setFrettingDetection,
   * the target getters, and the getTMinRange / getTMaxRange / getThresRange
   * getters. */
  static constexpr uint8_t REG_RESULT_OBJ_NUM   = 0x10;
  static constexpr uint8_t REG_RESULT_RANGE_L   = 0x11;
  static constexpr uint8_t REG_RESULT_RANGE_H   = 0x12;
  static constexpr uint8_t REG_RESULT_SPEED_L   = 0x13;
  static constexpr uint8_t REG_RESULT_SPEED_H   = 0x14;
  static constexpr uint8_t REG_RESULT_ENERGY_L  = 0x15;
  static constexpr uint8_t REG_RESULT_ENERGY_H  = 0x16;
  static constexpr uint8_t REG_CFAR_THR_L       = 0x20;
  static constexpr uint8_t REG_CFAR_THR_H       = 0x21;
  static constexpr uint8_t REG_T_MIN_RANGE_L    = 0x22;
  static constexpr uint8_t REG_T_MIN_RANGE_H    = 0x23;
  static constexpr uint8_t REG_T_MAX_RANGE_L    = 0x24;
  static constexpr uint8_t REG_T_MAX_RANGE_H    = 0x25;
  static constexpr uint8_t REG_MICRO_MOTION     = 0x26;

  /* --- UART ASCII protocol commands --- */
  static constexpr const char *START_SENSOR  = "sensorStart";
  static constexpr const char *STOP_SENSOR   = "sensorStop";
  static constexpr const char *SAVE_CONFIG   = "saveConfig";
  static constexpr const char *RECOVER_SENSOR = "resetCfg";      ///< factory data reset
  static constexpr const char *RESET_SENSOR  = "resetSystem";
  static constexpr const char *SPEED_MODE    = "setRunApp 1";
  static constexpr const char *EXIST_MODE    = "setRunApp 0";

  /* --- Timing (empirical firmware requirements: do not retune without
   *     hardware verification) --- */

  /* Bounded waits: every blocking helper has a deadline so a silent sensor
     cannot hang the sketch. */
  static constexpr uint32_t UART_READ_BLOCK_MS      = 100;    ///< UART readReg collect window
  static constexpr uint32_t GET_STATUS_TIMEOUT_MS   = 5000;   ///< reserved: planned bound for getStatus()
  static constexpr uint32_t SENSOR_STOP_TIMEOUT_MS  = 5000;   ///< sensorStop() ack deadline
  static constexpr uint32_t UART_BEGIN_PROBE_MS     = 1500;   ///< begin() reply window
  static constexpr uint32_t CONFIG_READY_DELAY_MS   = 15000;  ///< 15s post-power-on config lockout
  static constexpr uint32_t CONFIG_READY_TIMEOUT_MS = 30000;  ///< waitForConfigReady() default
  static constexpr uint32_t CONFIG_POLL_STEP_MS     = 50;     ///< waitForConfigReady() poll step

  /* Settle pauses after control writes (sensor firmware needs them). */
  static constexpr uint32_t SETTLE_AFTER_START_MS   = 200;    ///< after sensorStart
  static constexpr uint32_t SETTLE_AFTER_STOP_MS    = 200;    ///< after sensorStop
  static constexpr uint32_t SETTLE_AFTER_RESET_MS   = 1500;   ///< after resetSystem / mode change
  static constexpr uint32_t SETTLE_AFTER_SAVE_MS    = 500;    ///< after saveConfig (I2C path, endConfig)
  static constexpr uint32_t SETTLE_AFTER_RECOVER_MS = 800;    ///< after resetCfg
  static constexpr uint32_t UART_SAVE_SETTLE_MS     = 800;    ///< after saveConfig in setSensor() UART sequence
  static constexpr uint32_t UART_RESTART_SETTLE_MS  = 500;    ///< after START_SENSOR closes a UART recover sequence

  /* UART command pacing. */
  static constexpr uint32_t UART_CMD_GAP_MS     = 100;   ///< spacing between raw ASCII commands
  static constexpr uint32_t UART_DRAIN_WINDOW_MS = 100;  ///< bounded drain of stale RX bytes (scan/raw)
  static constexpr uint32_t UART_BEGIN_DRAIN_MS  = 200;  ///< bounded drain of stale RX bytes (begin)
  static constexpr uint32_t UART_MODE_SWITCH_MS = 50;    ///< after setRunApp n
  static constexpr uint32_t RAW_REPLY_GRACE_MS  = 50;    ///< sendRawCommand(): sensor turnaround before the reply
  static constexpr uint32_t STOP_ACK_INITIAL_MS = 1000;  ///< sensorStop(): first wait for the echo
  static constexpr uint32_t STOP_ACK_RETRY_MS   = 400;   ///< sensorStop(): retry spacing
  static constexpr uint32_t UART_OPEN_DELAY_MS  = 1000;  ///< SoftwareSerial open (AVR/ESP8266)
  static constexpr uint32_t BAUD_SWITCH_DELAY_MS = 20;   ///< after re-opening the UART at a new baud

  /* Telemetry debounce. */
  static constexpr uint8_t MOTION_DEBOUNCE_POLLS = 3;    ///< empty UART frames held before presence drops

  /* ================= protected helpers ================= */
  sResponseData_t wRCMD(String cmd1, uint8_t count);
  void writeCMD(String cmd1 , String cmd2, uint8_t count);
  sAllData_t analysisData(uint8_t * data, uint8_t len);
  sResponseData_t analysisResponse(uint8_t *data, uint8_t len ,uint8_t count);
  bool sensorStop(void);

  /* ================= protected state ================= */
  uint32_t _readyMark = 0;             ///< millis() of first successful begin()
  uint8_t  _beginError = eBeginNone;   ///< reason the last begin() failed
  uint8_t  uartI2CFlag = 0;  ///< backend flag (I2C_FLAG / UART_FLAG); set only
                             ///< by subclass constructors

private:
  sPrivateData_t _buffer;
  uint8_t _flashNumber = 0;        ///< per-instance debounce counter (getTargetNumber)
  bool _motionLatched = false;     ///< latched presence state for motionDetection()
  uint8_t _motionIdlePolls = 0;    ///< consecutive empty polls since last presence
  virtual void writeReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len) = 0;
};

class DFRobot_C4001_I2C:public DFRobot_C4001{
public:
  DFRobot_C4001_I2C(TwoWire *pWire=&Wire, uint8_t addr = DEVICE_ADDR_0);
  bool begin(uint32_t timeoutMs = 0);
protected:
  virtual void writeReg(uint8_t reg, uint8_t *data, uint8_t len);
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len);
private:
  TwoWire *_pWire;
  uint8_t _I2C_addr;

};

class DFRobot_C4001_UART:public DFRobot_C4001{
public:
#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  DFRobot_C4001_UART(SoftwareSerial *sSerial, uint32_t Baud);
#else
  DFRobot_C4001_UART(HardwareSerial *hSerial, uint32_t Baud ,uint8_t rxpin = 0, uint8_t txpin = 0);
#endif

  bool begin(uint32_t timeoutMs = 0);

  /**
   * @fn scanBaudRates
   * @brief Try a list of baud rates to find one the sensor responds on. Recovery
   *        helper for the case where the sensor was saved at an unexpected baud
   *        and is no longer reachable at the configured one. Reconfigures this
   *        instance's UART to the baud that answers.
   * @param candidates Array of baud rates to try (e.g. {9600,19200,...}).
   * @param count Number of entries in candidates.
   * @param probeTimeoutMs Time (ms) to listen for a reply at each baud.
   * @note The probe looks for recognizable protocol tokens ("Response", "Done",
   *       "sensorStop", "$DF..."), so a wrong-baud garbage stream is not a match.
   * @return The working baud rate, or 0 if none responded.
   */
  uint32_t scanBaudRates(const uint32_t* candidates, size_t count, uint32_t probeTimeoutMs = 200);
protected:
  virtual void writeReg(uint8_t reg, uint8_t *data, uint8_t len);
  virtual int16_t readReg(uint8_t reg, uint8_t *data, uint8_t len);
private:

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  SoftwareSerial *_serial;
#else
  HardwareSerial *_serial;
#endif
  uint32_t _baud;
  uint8_t _rxpin;
  uint8_t _txpin;
};
#endif
