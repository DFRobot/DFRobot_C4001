/*!
 * @file DFRobot_C4001.cpp
 * @brief Implementation of the DFRobot_C4001 base class and its I2C/UART backends
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [ZhixinLiu](zhixin.liu@dfrobot.com)
 * @version V1.1
 * @date 2026-08-27
 * @url https://github.com/DFRobot/DFRobot_C4001
 */
#include "DFRobot_C4001.h"

DFRobot_C4001::DFRobot_C4001(){}
DFRobot_C4001::~DFRobot_C4001(){}

sSensorStatus_t DFRobot_C4001::getStatus(void)
{
  sSensorStatus_t data;
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_STATUS, &temp, (uint8_t)1);
    data.workStatus = (temp&0x01);
    data.workMode   = (temp&0x02) >> 1;
    data.initStatus = (temp&0x80) >> 7;
  }else{
    sAllData_t allData;
    uint8_t temp[100] = {0};
    /* Parse whatever telemetry has already streamed in. Deliberately do NOT
       issue START_SENSOR: a status read must not silently restart a sensor
       the caller stopped (see the lifecycle notes in the header). */
    uint8_t len = readReg(0, temp, 100);
    allData = analysisData(temp, len);
    if(allData.sta.workStatus == 0){
      /* No live telemetry arrived (sensor deliberately stopped, or silent):
         report it as stopped rather than forcing a start. */
      data.workStatus = 0;
      data.workMode   = 0;
      data.initStatus = 0;
    }else{
      data.workStatus = allData.sta.workStatus;
      data.workMode   = allData.sta.workMode;
      data.initStatus = allData.sta.initStatus;
    }
  }
  return data;
}

bool DFRobot_C4001::motionDetection(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_RESULT_STATUS, &temp, (uint8_t)1);
    return (temp & 0x01) != 0;
  }else{
    uint8_t len = 0;
    uint8_t temp[100] = {0};
    sAllData_t data;
    len = readReg(0, temp, 100);
    data = analysisData(temp, len);
    // Debounce/latch: keep reporting motion for a few consecutive empty frames
    // after the last confirmed target, so the reading doesn't flicker between
    // telemetry bursts. (Debounce length needs hardware verification - Phase 6.)
    if(data.exist){
      _motionLatched = true;
      _motionIdlePolls = 0;
    }else if(_motionLatched && (++_motionIdlePolls >= MOTION_DEBOUNCE_POLLS)){
      _motionLatched = false;
      _motionIdlePolls = 0;
    }
    return _motionLatched;
  }
}

void DFRobot_C4001::setSensor(eSetMode_t mode)
{
  uint8_t temp = mode;
  if(uartI2CFlag == I2C_FLAG){
    if(mode == eStartSen){
      writeReg(REG_CTRL0, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_START_MS);
    }else if(mode == eStopSen){
      writeReg(REG_CTRL0, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_STOP_MS);
    }else if(mode == eResetSen){
      writeReg(REG_CTRL0, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_RESET_MS);
    }else if(mode == eSaveParams){
      writeReg(REG_CTRL1, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_SAVE_MS);
    }else if(mode == eRecoverSen){
      writeReg(REG_CTRL1, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_RECOVER_MS);
    }else if(mode == eChangeMode){
      writeReg(REG_CTRL1, &temp, (uint8_t)1);
      delay(SETTLE_AFTER_RESET_MS);   // mode change reboots like a reset
    }
  }else{
    if(mode == eStartSen){
      writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
      delay(SETTLE_AFTER_START_MS);
    }else if(mode == eStopSen){
      writeReg(0, (uint8_t *)STOP_SENSOR, strlen(STOP_SENSOR));
      delay(SETTLE_AFTER_STOP_MS);
    }else if(mode == eResetSen){
      writeReg(0, (uint8_t *)RESET_SENSOR, strlen(RESET_SENSOR));
      delay(SETTLE_AFTER_RESET_MS);
    }else if(mode == eSaveParams){
      /* The UART firmware only honors saveConfig while stopped. */
      writeReg(0, (uint8_t *)STOP_SENSOR, strlen(STOP_SENSOR));
      delay(SETTLE_AFTER_STOP_MS);
      writeReg(0, (uint8_t *)SAVE_CONFIG, strlen(SAVE_CONFIG));
      delay(UART_SAVE_SETTLE_MS);
      writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
    }else if(mode == eRecoverSen){
      writeReg(0, (uint8_t *)STOP_SENSOR, strlen(STOP_SENSOR));
      delay(SETTLE_AFTER_STOP_MS);
      writeReg(0, (uint8_t *)RECOVER_SENSOR, strlen(RECOVER_SENSOR));
      delay(SETTLE_AFTER_RECOVER_MS);
      writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
      delay(UART_RESTART_SETTLE_MS);
    }
  }
}

bool DFRobot_C4001::beginConfig(void)
{
  if(!isReadyForConfig()){
    return false;  // 15s post-power-on config lockout, consistent with setters
  }
  if(uartI2CFlag == I2C_FLAG){
    setSensor(eStopSen);
    return true;
  }
  return sensorStop();
}

bool DFRobot_C4001::endConfig(void)
{
  if(!isReadyForConfig()){
    return false;  // config lockout (also prevents resuming an un-ready sensor)
  }
  if(uartI2CFlag == I2C_FLAG){
    setSensor(eSaveParams);
    setSensor(eStartSen);
    return true;
  }
  writeReg(0, (uint8_t *)SAVE_CONFIG, strlen(SAVE_CONFIG));
  delay(SETTLE_AFTER_SAVE_MS);
  writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
  delay(UART_CMD_GAP_MS);
  return true;
}

bool DFRobot_C4001::save(void)
{
  if(!isReadyForConfig()){
    return false;  // config lockout: persist only once configuration is allowed
  }
  // Persist the current working configuration. The setXxx() methods already
  // persist automatically; this is an explicit, idempotent persist-now call.
  setSensor(eSaveParams);
  return true;
}

bool DFRobot_C4001::factoryReset(void)
{
  if(!isReadyForConfig()){
    return false;  // config lockout
  }
  // eRecoverSen: UART = stop -> resetCfg -> start; I2C = recover command.
  setSensor(eRecoverSen);
  return true;
}

C4001Result DFRobot_C4001::setSensorMode(eMode_t mode, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(uartI2CFlag == I2C_FLAG){
    sSensorStatus_t data;
    data = getStatus();
    if(data.workMode == (uint8_t)mode){
      return C4001Result::Success;
    }
    setSensor(eChangeMode);
  }else{
    sensorStop();
    if(mode == eExitMode){
      writeReg(0, (uint8_t *)EXIST_MODE, strlen(EXIST_MODE));
    }else{
      writeReg(0, (uint8_t *)SPEED_MODE, strlen(SPEED_MODE));
    }
    delay(2 * UART_MODE_SWITCH_MS);   // let the mode switch take before saving
    writeReg(0, (uint8_t *)SAVE_CONFIG, strlen(SAVE_CONFIG));
    delay(SETTLE_AFTER_SAVE_MS);
    writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
    delay(UART_CMD_GAP_MS);
  }
  if(verify){
    sSensorStatus_t data = getStatus();
    if(data.workMode != (uint8_t)mode){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

C4001Result DFRobot_C4001::setTrigSensitivity(uint8_t sensitivity, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(sensitivity > 9){
    return C4001Result::Rejected;
  }
  uint8_t temp = sensitivity;
  if(uartI2CFlag == I2C_FLAG){
    writeReg(REG_TRIG_SENSITIVITY, &temp, (uint8_t)1);
    setSensor(eSaveParams);
  }else{
    /* setSensitivity <keep> <trig>. "255" is the sensor's documented
       "leave this field unchanged" sentinel, so send the requested trig
       sensitivity and leave keep untouched. */
    String data = "setSensitivity 255 ";
    data += sensitivity;
    writeCMD(data, data, (uint8_t)1);
  }
  if(verify){
    uint8_t actual = getTrigSensitivity();
    if(actual != sensitivity){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

uint8_t DFRobot_C4001::getTrigSensitivity(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_TRIG_SENSITIVITY, &temp, (uint8_t)1);
    return temp;
  }else{
    sResponseData_t responseData;
    uint8_t temp[100] = {0};
    readReg(0, temp, 100);   // drain in-flight telemetry before the stop/query dance
    String data = "getSensitivity";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      // getSensitivity returns: Response <keep> <trig>. Trig acts on the 2nd field.
      return responseData.response2;
    }
    return 0;
  }
}

C4001Result DFRobot_C4001::setKeepSensitivity(uint8_t sensitivity, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(sensitivity > 9){
    return C4001Result::Rejected;
  }
  uint8_t temp = sensitivity;
  if(uartI2CFlag == I2C_FLAG){
    writeReg(REG_KEEP_SENSITIVITY, &temp, (uint8_t)1);
    setSensor(eSaveParams);
  }else{
    /* setSensitivity <keep> <trig>. "255" leaves a field unchanged (see
       setTrigSensitivity), so send keep with trig untouched. */
    String data = "setSensitivity ";
    data += sensitivity;
    data += " 255";
    writeCMD(data, data, (uint8_t)1);
  }
  if(verify){
    uint8_t actual = getKeepSensitivity();
    if(actual != sensitivity){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

uint8_t DFRobot_C4001::getKeepSensitivity(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_KEEP_SENSITIVITY, &temp, (uint8_t)1);
    return temp;
  }else{
    sResponseData_t responseData;
    uint8_t temp[100] = {0};
    readReg(0, temp, 100);   // drain in-flight telemetry before the stop/query dance
    String data = "getSensitivity";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      // getSensitivity returns: Response <keep> <trig>. Keep acts on the 1st field.
      return responseData.response1;
    }
    return 0;
  }
}

C4001Result DFRobot_C4001::setDelay(uint8_t trig, uint16_t keep, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(trig > 200){
    return C4001Result::Rejected;
  }
  if(keep < 4 || keep > 3000){
    return C4001Result::Rejected;
  }
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[3] = {0};
    temp[0] = trig;
    temp[1] = keep;
    temp[2] = keep>>8;
    writeReg(REG_TRIG_DELAY, temp, (uint8_t)3);
    setSensor(eSaveParams);
  }else{
    String data = "setLatency ";
    data += String((float)trig*0.01, 1);
    data += " ";
    data += String((float)keep*0.5, 1);
    writeCMD(data, data, (uint8_t)1);
  }
  if(verify){
    uint8_t trigActual = getTrigDelay();
    uint16_t keepActual = getKeepTimeout();
    if(trigActual != trig || keepActual != keep){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

uint8_t DFRobot_C4001::getTrigDelay(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_TRIG_DELAY, &temp, (uint8_t)1);
    return temp;
  }else{
    sResponseData_t responseData;
    String data = "getLatency";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      return responseData.response1*100;
    }
    return 0;
  }
}

uint16_t DFRobot_C4001::getKeepTimeout(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[2] = {0};
    readReg(REG_KEEP_TIMEOUT_L, temp, (uint8_t)2);
    return (((uint16_t)temp[1]) << 8) | temp[0];
  }else{
    sResponseData_t responseData;
    String data = "getLatency";
    responseData = wRCMD(data, (uint8_t)2);
    if(responseData.status){
      return responseData.response2*2;
    }
    return 0;
  }
}

C4001Result DFRobot_C4001::setDetectionRange(uint16_t min, uint16_t max, uint16_t trig, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(max < 240 || max > 2000){
    return C4001Result::Rejected;
  }
  if(min < 30 || min > max){
    return C4001Result::Rejected;
  }
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[10] = {0};
    temp[0] = (uint8_t)(min);
    temp[1] = (uint8_t)(min >> 8);
    temp[2] = (uint8_t)(max);
    temp[3] = (uint8_t)(max >> 8);
    temp[4] = (uint8_t)(trig);
    temp[5] = (uint8_t)(trig >> 8);
    writeReg(REG_E_MIN_RANGE_L, temp, (uint8_t)6);
    setSensor(eSaveParams);
  }else{
    String data1 = "setRange ";
    String data2 = "setTrigRange ";
    data1 += String(((float)min)/100.0, 1);
    data1 += " ";
    data1 += String(((float)max)/100.0, 1);
    data2 += String(((float)trig)/100.0, 1);
    writeCMD(data1, data2, (uint8_t)2);
  }
  if(verify){
    uint16_t minActual = getMinRange();
    uint16_t maxActual = getMaxRange();
    uint16_t trigActual = getTrigRange();
    if(minActual != min || maxActual != max || trigActual != trig){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

uint16_t DFRobot_C4001::getTrigRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_E_TRIG_RANGE_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getTrigRange";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      return responseData.response1*100;
    }
    return 0;
  }
}

uint16_t DFRobot_C4001::getMaxRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_E_MAX_RANGE_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getRange";
    responseData = wRCMD(data, (uint8_t)2);
    if(responseData.status){
      return responseData.response2*100;
    }
    return 0;
  }
}

uint16_t DFRobot_C4001::getMinRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_E_MIN_RANGE_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getRange";
    responseData = wRCMD(data, (uint8_t)2);
    if(responseData.status){
      return responseData.response1*100;
    }
    return 0;
  }
}

uint8_t DFRobot_C4001::getTargetNumber(void)
{
  uint8_t temp[10] = {0};
  if(uartI2CFlag == I2C_FLAG){
    readReg(REG_RESULT_OBJ_NUM, temp, (uint8_t)7);
    if(temp[0] == 1){
      _flashNumber = 0;
      _buffer.number = 1;
      _buffer.range  = (float)( int16_t((uint16_t)(temp[1] | ((uint16_t)temp[2]) << 8))) / 100.0;
      _buffer.speed  = (float)( int16_t((uint16_t)(temp[3] | ((uint16_t)temp[4]) << 8))) / 100.0;
      _buffer.energy = (uint16_t)(temp[5] | ((uint16_t)temp[6]) << 8);
    }else{
      // Debounce (per-instance counter): hold the last target for ~10 empty
      // polls before reporting none, so brief dropouts don't read as "gone".
      if(_flashNumber++ > 10){
        _buffer.number = 0;
        _buffer.range  = 0;
        _buffer.speed  = 0;
        _buffer.energy = 0;
      }
    }
    return _buffer.number;
  }else{
    uint8_t len = 0;
    uint8_t temp[100] = {0};
    sAllData_t data;
    len = readReg(0, temp, 100);
    data = analysisData(temp, len);
    if(data.target.number != 0){
      _flashNumber = 0;
      _buffer.number = data.target.number;
      _buffer.range  = data.target.range;   // already in meters (see analysisData)
      _buffer.speed  = data.target.speed;   // already in m/s (see analysisData)
      _buffer.energy = data.target.energy;
    }else{
      _buffer.number = 0;
      if(_flashNumber++ > 10){
        _buffer.range  = 0;
        _buffer.speed  = 0;
        _buffer.energy = 0;
      }
    }
    return data.target.number;
  }
}

float DFRobot_C4001::getTargetSpeed(void)
{
  return _buffer.speed;
}

float DFRobot_C4001::getTargetRange(void)
{
  return _buffer.range;
}

uint32_t DFRobot_C4001::getTargetEnergy(void)
{
  return _buffer.energy;
}

C4001Result DFRobot_C4001::setDetectThres(uint16_t min, uint16_t max, uint16_t thres, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(max > 2500){
    return C4001Result::Rejected;
  }
  if(min > max){
    return C4001Result::Rejected;
  }
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[10] = {0};
    temp[0] = (uint8_t)(thres);
    temp[1] = (uint8_t)(thres >> 8);
    temp[2] = (uint8_t)(min);
    temp[3] = (uint8_t)(min >> 8);
    temp[4] = (uint8_t)(max);
    temp[5] = (uint8_t)(max >> 8);
    writeReg(REG_CFAR_THR_L, temp, (uint8_t)6);
    setSensor(eSaveParams);
  }else{
    String data1 = "setRange ";
    String data2 = "setThrFactor ";
    data1 += String(((float)min)/100.0, 1);
    data1 += " ";
    data1 += String(((float)max)/100.0, 1);
    data2 += thres;
    writeCMD(data1, data2, (uint8_t)2);
  }
  if(verify){
    uint16_t minActual = getTMinRange();
    uint16_t maxActual = getTMaxRange();
    uint16_t thrActual = getThresRange();
    if(minActual != min || maxActual != max || thrActual != thres){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

C4001Result DFRobot_C4001::setIoPolarity(uint8_t value, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(value > 1){
    return C4001Result::Rejected;
  }
  if(uartI2CFlag == I2C_FLAG){
    // Not implemented on the register-based I2C backend.
    return C4001Result::NotSupported;
  }
  String data = "setGpioMode 1 ";
  data += value;
  writeCMD(data, data, (uint8_t)1);
  if(verify){
    uint8_t actual = getIoPolarity();
    if(actual != value){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

uint8_t DFRobot_C4001::getIoPolarity(void)
{
  if(uartI2CFlag == I2C_FLAG){
    return 0;  // not implemented on register-based I2C backend
  }else{
    sResponseData_t responseData;
    String data = "getGpioMode 1";
    responseData = wRCMD(data, (uint8_t)2);
    if(responseData.status){
      return responseData.response2;
    }
    return 0;
  }
}

C4001Result DFRobot_C4001::setPwm(uint8_t pwm1, uint8_t pwm2, uint8_t timer, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(pwm1 > 100 || pwm2 > 100){
    return C4001Result::Rejected;
  }
  if(uartI2CFlag == I2C_FLAG){
    // Not implemented on the register-based I2C backend.
    return C4001Result::NotSupported;
  }
  String data = "setPwm ";
  data += pwm1;
  data += " ";
  data += pwm2;
  data += " ";
  data += timer;
  writeCMD(data, data, (uint8_t)1);
  if(verify){
    sPwmData_t actual = getPwm();
    if(actual.pwm1 != pwm1 || actual.pwm2 != pwm2 || actual.timer != timer){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

sPwmData_t DFRobot_C4001::getPwm(void)
{
  sPwmData_t pwmData;
  memset(&pwmData, 0, sizeof(sPwmData_t));
  if(uartI2CFlag == I2C_FLAG){
    return pwmData;  // zeroed: PWM not implemented on register-based I2C backend
  }else{
    sResponseData_t responseData;
    String data = "getPwm";
    responseData = wRCMD(data, (uint8_t)3);
    if(responseData.status){
      pwmData.pwm1 = responseData.response1;
      pwmData.pwm2 = responseData.response2;
      pwmData.timer = responseData.response3;
    }
    return pwmData;
  }
}

uint16_t DFRobot_C4001::getTMinRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_T_MIN_RANGE_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getRange";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      return responseData.response1*100;
    }
    return 0;
  }
}

uint16_t DFRobot_C4001::getTMaxRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_T_MAX_RANGE_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getRange";
    responseData = wRCMD(data, (uint8_t)2);
    if(responseData.status){
      return responseData.response2*100;
    }
    return 0;
  }
}

uint16_t DFRobot_C4001::getThresRange(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp[4] = {0};
    readReg(REG_CFAR_THR_L, temp, (uint8_t)2);
    return (uint16_t)(temp[0] | ((uint16_t)temp[1]) << 8);
  }else{
    sResponseData_t responseData;
    String data = "getThrFactor";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      return responseData.response1;
    }
    return 0;
  }
}

C4001Result DFRobot_C4001::setFrettingDetection(eSwitch_t sta, bool verify)
{
  if(!isReadyForConfig()){
    return C4001Result::NotReady;  // 15s post-power-on lockout
  }
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = sta;
    writeReg(REG_MICRO_MOTION, &temp, (uint8_t)1);
    setSensor(eSaveParams);
  }else{
    String data = "setMicroMotion ";
    data += sta;
    writeCMD(data, data, (uint8_t)1);
  }
  if(verify){
    eSwitch_t actual = getFrettingDetection();
    if(actual != sta){
      return C4001Result::AckedButUnconfirmed;
    }
  }
  return C4001Result::Success;
}

const char *DFRobot_C4001::resultToString(C4001Result result)
{
  switch(result){
    case C4001Result::Success:             return "Success";
    case C4001Result::AckedButUnconfirmed: return "AckedButUnconfirmed";
    case C4001Result::Rejected:            return "Rejected";
    case C4001Result::Timeout:             return "Timeout";
    case C4001Result::NotReady:            return "NotReady";
    case C4001Result::NotSupported:        return "NotSupported";
    default:                               return "Unknown";
  }
}

bool DFRobot_C4001::isReadyForConfig(void)
{
  if(_readyMark == 0){
    return false;  // no confirmed device yet
  }
  return (uint32_t)(millis() - _readyMark) >= CONFIG_READY_DELAY_MS;
}

bool DFRobot_C4001::waitForConfigReady(uint32_t timeoutMs)
{
  uint32_t start = millis();
  while(!isReadyForConfig()){
    if((uint32_t)(millis() - start) >= timeoutMs){
      return false;
    }
    delay(CONFIG_POLL_STEP_MS);
  }
  return true;
}

const char* DFRobot_C4001::beginErrorString(void)
{
  switch(_beginError){
    case eBeginWrongAddress: return "wrong I2C address (no ACK at configured addr)";
    case eBeginWrongBaud:    return "no reply at configured UART baud";
    case eBeginBootLockout:  return "sensor present but still booting";
    case eBeginBusError:     return "transport/bus error";
    case eBeginNoResponse:
    default:                 return "no response from sensor (check wiring/power)";
  }
}

eSwitch_t DFRobot_C4001::getFrettingDetection(void)
{
  if(uartI2CFlag == I2C_FLAG){
    uint8_t temp = 0;
    readReg(REG_MICRO_MOTION, &temp, (uint8_t)1);
    return (eSwitch_t)temp;
  }else{
    sResponseData_t responseData;
    String data = "getMicroMotion";
    responseData = wRCMD(data, (uint8_t)1);
    if(responseData.status){
      return (eSwitch_t)responseData.response1;
    }
    return (eSwitch_t)0;
  }
}

/* Deprecated 1.0.0 spellings: thin forwarders to the renamed methods. */
C4001Result DFRobot_C4001::setIoPolaity(uint8_t value, bool verify)
{
  return setIoPolarity(value, verify);
}

uint8_t DFRobot_C4001::getIoPolaity(void)
{
  return getIoPolarity();
}

uint16_t DFRobot_C4001::getKeepTimerout(void)
{
  return getKeepTimeout();
}

sResponseData_t DFRobot_C4001::analysisResponse(uint8_t *data, uint8_t len ,uint8_t count)
{
  sResponseData_t responseData;
  /* Deterministic defaults: only set fields we positively parse. "status"
     becomes true only once a genuine, in-bounds "Res" token is located. */
  responseData.status    = false;
  responseData.response1 = 0.0f;
  responseData.response2 = 0.0f;
  responseData.response3 = 0.0f;
  if(data == NULL || len == 0){
    return responseData;
  }
  uint8_t space[5] = {0};   // byte offsets of the first up-to-5 field starts
  uint8_t i = 0;
  uint8_t j = 0;
  /* Locate the "Res" token. The i+2 < len guard ensures data[i+1] /
     data[i+2] stay in-bounds even when a "Res" lands at the buffer end (or
     the buffer is shorter than 3 bytes). */
  for(i = 0; i + 2 < len; i++){
    if(data[i] == 'R' && data[i+1] == 'e' && data[i+2] == 's'){
      break;
    }
  }
  if(i >= len || (i + 2) >= len){
    return responseData;  // no "Res" token fully in bounds -> nothing to parse
  }
  responseData.status = true;
  /* Record the byte right after each space (a numeric field start), stopping
     at 5 entries. Without the j<5 bound a >5-space response would overflow the
     fixed-size space[] array. */
  for(uint8_t k = i; k < len && j < 5; k++){
    if(data[k] == ' ' && (k + 1) < len){
      space[j++] = k + 1;
    }
  }
  if(j == 0){
    return responseData;  // "Res" present but no space-delimited fields
  }
  responseData.response1 = atof((const char*)(data + space[0]));
  if(j >= 2){
    responseData.response2 = atof((const char*)(data + space[1]));
  }
  if(count == 3 && j >= 3){
    responseData.response3 = atof((const char*)(data + space[2]));
  }
  return responseData;
}

sAllData_t DFRobot_C4001::analysisData(uint8_t * data, uint8_t len)
{
  sAllData_t allData;
  uint8_t location = 0;
  memset(&allData, 0, sizeof(sAllData_t));
  for(uint8_t i = 0; i < len; i++){
    if(data[i] == '$'){
      location = i;
      break;
    }
  }
  if(location == len){
    return allData;
  }
  const char* rest = (const char*)(data + location);
  /* Guard the strncmp() lookahead: each type token is 6 bytes, so require at
     least 6 readable bytes remain before comparing. */
  size_t rem = (size_t)(len - location);
  if(rem >= 6 && 0 == strncmp(rest, "$DFHPD", strlen("$DFHPD"))){
    allData.sta.workMode = eExitMode;
    allData.sta.workStatus = 1;
    allData.sta.initStatus = 1;
    /* Exist byte sits right after the "$DFHPD" type token; guard the read. */
    if((location + 7) < len && data[location+7] == '1'){
      allData.exist = 1;
    }else{
      allData.exist = 0;
    }
  }else if(rem >= 6 && 0 == strncmp(rest, "$DFDMD", strlen("$DFDMD"))){
    // $DFDMD,par1,par2,par3,par4,par5,par6,par7*
    allData.sta.workMode = eSpeedMode;
    allData.sta.workStatus = 1;
    allData.sta.initStatus = 1;
    // The frame is parsed into a private local copy so strtok()'s in-place
    // ',' -> NUL mutation never touches the caller's buffer.
    char localBuf[64] = {0};
    size_t n = ((size_t)(len - location)) < (size_t)(sizeof(localBuf) - 1)
             ? (size_t)(len - location) : (size_t)(sizeof(localBuf) - 1);
    memcpy(localBuf, data + location, n);
    char *parts[7] = {0};
    int index = 0;
    char *token = strtok(localBuf, ",");
    while(token != NULL && index < 7){
      parts[index++] = token;
      token = strtok(NULL, ",");
    }
    // A complete $DFDMD frame yields 7 comma-separated tokens. If the frame is
    // truncated, leave allData zeroed (from the memset above) rather than
    // dereferencing an uninitialized/garbage parts[] entry.
    if(index >= 7){
      allData.target.number = (uint8_t)atoi(parts[1]);
      // NOTE: telemetry range/speed are stored as-is; the getters return
      // meters / m/s consistent with the I2C path. (Exact on-wire unit needs
      // hardware confirm - Phase 6.)
      allData.target.range  = (float)atof(parts[3]);
      allData.target.speed  = (float)atof(parts[4]);
      allData.target.energy = (uint32_t)atof(parts[5]);
    }
  }
  return allData;
}

/* Send an ASCII query on UART and parse its "Response ..." reply. The sensor
   is stopped first (queries are only answered while stopped) and restarted
   afterwards. */
sResponseData_t DFRobot_C4001::wRCMD(String cmd1, uint8_t count)
{
  uint8_t len = 0;
  uint8_t temp[200] = {0};
  sResponseData_t responseData;
  sensorStop();
  writeReg(0, (uint8_t *)cmd1.c_str(), cmd1.length());
  delay(UART_CMD_GAP_MS);
  len = readReg(0, temp, 200);
  responseData = analysisResponse(temp, len, count);
  delay(UART_CMD_GAP_MS);
  writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
  delay(UART_CMD_GAP_MS);
  return responseData;
}

/* Send one or two ASCII set commands on UART, wrapped in the required
   stop -> write -> save -> start sequence. */
void DFRobot_C4001::writeCMD(String cmd1 , String cmd2, uint8_t count)
{
  sensorStop();
  writeReg(0, (uint8_t *)cmd1.c_str(), cmd1.length());
  delay(UART_CMD_GAP_MS);
  if(count > 1){
    delay(UART_CMD_GAP_MS);
    writeReg(0, (uint8_t *)cmd2.c_str(), cmd2.length());
    delay(UART_CMD_GAP_MS);
  }
  writeReg(0, (uint8_t *)SAVE_CONFIG, strlen(SAVE_CONFIG));
  delay(UART_CMD_GAP_MS);
  writeReg(0, (uint8_t *)START_SENSOR, strlen(START_SENSOR));
  delay(UART_CMD_GAP_MS);
}

String DFRobot_C4001::sendRawCommand(const String &cmd, uint32_t timeoutMs)
{
  if(!isReadyForConfig()){
    // Config lockout: raw writes are rejected exactly like the setters, so a
    // caller in the 15s post-power-on window gets an empty response instead of
    // silently reaching a not-yet-ready sensor.
    return String("");
  }
  if(uartI2CFlag == I2C_FLAG){
    // The I2C backend is register-based and does not accept ASCII commands.
    return String("");
  }
  // Drain stale bytes with a bounded window so a sensor that is actively
  // streaming telemetry (run mode) cannot hang this call forever.
  uint8_t flushBuf[32] = {0};
  uint32_t flushStart = millis();
  while(readReg(0, flushBuf, (uint8_t)sizeof(flushBuf)) > 0){
    if((uint32_t)(millis() - flushStart) > UART_DRAIN_WINDOW_MS){
      break;
    }
  }
  // Send the command with the documented CR+LF terminator.
  String fullCmd = cmd + "\r\n";
  writeReg(0, (uint8_t *)fullCmd.c_str(), fullCmd.length());
  delay(RAW_REPLY_GRACE_MS);  // give the sensor time to begin responding
  String response;
  uint32_t start = millis();
  while((uint32_t)(millis() - start) < timeoutMs){
    uint8_t c = 0;
    int16_t n = readReg(0, &c, (uint8_t)1);
    if(n > 0){
      response += (char)c;
      start = millis();  // reset the idle timeout while data keeps arriving
    }
  }
  return response;
}

/* Stop the sensor (UART) and wait for its "sensorStop" echo. Retries until
   the deadline; false means the sensor never acknowledged. */
bool DFRobot_C4001::sensorStop(void)
{
  uint8_t len = 0;
  uint8_t temp[200] = {0};
  uint32_t stopWaitStart = millis();
  writeReg(0, (uint8_t *)STOP_SENSOR, strlen(STOP_SENSOR));
  delay(STOP_ACK_INITIAL_MS);
  len = readReg(0, temp, 200);
  while(1){
    if(len != 0){
      if (strstr((const char *)temp, "sensorStop") != NULL) {
        return true;
      }
    }
    if(millis() - stopWaitStart > SENSOR_STOP_TIMEOUT_MS){
      return false;  // bounded wait: don't block forever if the sensor never ACKs
    }
    memset(temp, 0, 200);
    delay(STOP_ACK_RETRY_MS);
    writeReg(0, (uint8_t *)STOP_SENSOR, strlen(STOP_SENSOR));
    len = readReg(0, temp, 200);
  }
}

DFRobot_C4001_I2C::DFRobot_C4001_I2C(TwoWire *pWire, uint8_t addr)
{
  _pWire = pWire;
  this->_I2C_addr = addr;
  uartI2CFlag = I2C_FLAG;
}

bool DFRobot_C4001_I2C::begin(uint32_t timeoutMs)
{
  (void)timeoutMs;  // the I2C probe is fast; a timeout parameter is not applicable
  _pWire->begin();
  _pWire->beginTransmission(_I2C_addr);
  if(_pWire->endTransmission() != 0){
    _beginError = eBeginWrongAddress;
    return false;  // nothing ACKs at this address
  }
  // The address ACK alone is not a reliable "connected" check on all cores
  // (a NACK can be misreported, causing a false "Device connected!"). Confirm
  // real radar traffic by reading a register and checking bytes come back.
  uint8_t probe = 0;
  int16_t n = readReg(REG_STATUS, &probe, (uint8_t)1);
  if(n <= 0){
    _beginError = eBeginNoResponse;
    return false;  // no data returned -> not the radar (or a bus error)
  }
  _beginError = eBeginNone;
  _readyMark = millis();
  return true;
}

void DFRobot_C4001_I2C::writeReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  _pWire->beginTransmission(this->_I2C_addr);
  _pWire->write(reg);
  _pWire->write(data, len);
  _pWire->endTransmission();
}

int16_t DFRobot_C4001_I2C::readReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  uint8_t i = 0;
  _pWire->beginTransmission(this->_I2C_addr);
  _pWire->write(reg);
  if(_pWire->endTransmission(false) != 0){
    return -1;
  }
  _pWire->requestFrom((uint8_t)this->_I2C_addr,(uint8_t)len);
  /* Clamp the copy to the caller's requested length: a device that returns
     more bytes than asked for must not overflow the caller's buffer. */
  while (_pWire->available() && i < len){
    data[i++]=_pWire->read();
  }
  return i;
}

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  DFRobot_C4001_UART::DFRobot_C4001_UART(SoftwareSerial *sSerial, uint32_t Baud)
  {
    this->_serial = sSerial;
    this->_baud = Baud;
    uartI2CFlag = UART_FLAG;
  }
#else
  DFRobot_C4001_UART::DFRobot_C4001_UART(HardwareSerial *hSerial, uint32_t Baud ,uint8_t rxpin, uint8_t txpin)
  {
    this->_serial = hSerial;
    this->_baud = Baud;
    uartI2CFlag = UART_FLAG;
    this->_rxpin = rxpin;
    this->_txpin = txpin;
  }
#endif

bool DFRobot_C4001_UART::begin(uint32_t timeoutMs)
{
  #ifdef ESP32
    _serial->begin(this->_baud, SERIAL_8N1, _rxpin, _txpin);
  #elif defined(ARDUINO_AVR_UNO) || defined(ESP8266)
    _serial->begin(this->_baud);
    delay(UART_OPEN_DELAY_MS);
  #else
    _serial->begin(this->_baud);  // M0 cannot create a begin in a construct
  #endif

  // Probe for a live sensor: UART begin() used to return true
  // unconditionally, which produced a false "Device connected!" when nothing
  // was attached. Send a benign read command and require a reply within a
  // bounded window. NOTE: during the sensor's 15s post-power-on lockout it
  // may stay silent, so a connected-but-booting unit can report false until
  // the lockout ends; the caller's retry loop then succeeds.
  uint32_t waitMs = (timeoutMs == 0) ? UART_BEGIN_PROBE_MS : timeoutMs;
  uint32_t t0 = millis();
  while(_serial->available() && (uint32_t)(millis() - t0) < UART_BEGIN_DRAIN_MS){
    _serial->read();  // bounded flush of stale bytes
  }
  _serial->print("getSensitivity\r\n");
  t0 = millis();
  while((uint32_t)(millis() - t0) < waitMs){
    if(_serial->available() > 0){
      _beginError = eBeginNone;
      _readyMark = millis();
      return true;  // any reply means a live sensor is present
    }
  }
  _beginError = eBeginNoResponse;  // nothing answered (wiring/baud/power)
  return false;
}

uint32_t DFRobot_C4001_UART::scanBaudRates(const uint32_t* candidates, size_t count, uint32_t probeTimeoutMs)
{
  for(size_t i = 0; i < count; i++){
    uint32_t baud = candidates[i];
    // Reconfigure this UART at the candidate baud (mirrors begin() pin layout).
    #ifdef ESP32
      _serial->begin(baud, SERIAL_8N1, _rxpin, _txpin);
    #elif defined(ARDUINO_AVR_UNO) || defined(ESP8266)
      _serial->begin(baud);
    #else
      _serial->begin(baud);
    #endif
    delay(BAUD_SWITCH_DELAY_MS);
    // Bounded flush of any stale bytes.
    uint32_t t0 = millis();
    while(_serial->available() && (uint32_t)(millis() - t0) < UART_DRAIN_WINDOW_MS){
      _serial->read();
    }
    // Probe with a benign command and collect the reply window.
    _serial->print("getSensitivity\r\n");
    t0 = millis();
    String resp;
    while((uint32_t)(millis() - t0) < probeTimeoutMs){
      while(_serial->available() && resp.length() < 512){
        resp += (char)_serial->read();
      }
    }
    // A wrong baud turns the incoming stream into garbage; only a correct baud
    // yields recognizable protocol tokens.
    if(resp.indexOf("Response") >= 0 || resp.indexOf("Done") >= 0 ||
       resp.indexOf("sensorStop") >= 0 || resp.indexOf("$DF") >= 0){
      _baud = baud;  // remember the working baud for subsequent traffic
      return baud;
    }
  }
  return 0;  // none responded at any candidate
}

void DFRobot_C4001_UART::writeReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  (void)reg;  // UART has no register address; the command text carries it
  _serial->write(data, len);
}

int16_t DFRobot_C4001_UART::readReg(uint8_t reg, uint8_t *data, uint8_t len)
{
  (void)reg;  // UART has no register address; reads return the stream
  uint16_t i = 0;
  uint32_t nowtime = millis();
  while(millis() - nowtime < UART_READ_BLOCK_MS){
    while(_serial->available() > 0){
      if(i == len) return len;
      data[i++] = _serial->read();
    }
  }
  return i;
}
