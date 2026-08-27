# DFRobot_C4001
- [中文版](./README_CN.md)

Determine whether the human body exists, and its anti-interference ability is relatively strong, and it is not easy to be affected by factors such as temperature changes, environmental light changes and environmental noise. Whether the human body is sitting, sleeping or moving, the sensor can quickly and sensitively detect its presence.

![效果图](resources/images/mmWaveLadar.jpg)

## Product Link(https://www.dfrobot.com/product-2282.html)
    SKU：SEN0609 SEN0610

## Table of Contents

* [Summary](#Summary)
* [Installation](#Installation)
* [Methods](#Methods)
* [Examples](#Examples)
* [Compatibility](#Compatibility)
* [History](#History)
* [Credits](#Credits)

## Summary

Provides an Arduino library that has the following features:
  Get whether the human body exists
  Get some state of motion of the object

## Installation
There are two methods for using this library:<br>
1. Open Arduino IDE, search for "DFRobot_C4001" on the status bar in Tools ->Manager Libraries and install the library.<br>
2. Download the library file before use, paste it into \Arduino\libraries directory, then open the examples folder and run the demo in the folder.<br>

## Methods

The canonical API docs live in `src/DFRobot_C4001.h`. This section summarizes the
high-level model, ranges, and units.

### Run mode vs. config mode

The C4001 streams telemetry while running. On the UART (ASCII) backend,
configuration commands are only applied while the sensor is stopped; the
library hides most of this:

```
[power-on] --(~15s config lockout)--> [ready]   set...() auto: stop -> write
   -> save -> start (runs atomically, persists) --> [running/telemetry]

Optional explicit bracket: beginConfig() (stop) -> [raw / sendRawCommand usage]
   -> endConfig() (saveConfig + start)
```

The register-based I2C backend applies register writes immediately and
persists them with a save, so its `set...()` methods do not need to stop the
sensor first (this matches the vendor Python reference).

- `set...()` handle the whole stop/write/save/start dance automatically on UART.
- `beginConfig()` / `endConfig()` expose an explicit config-mode window (useful
  with `sendRawCommand()`).
- `save()` persists the current working configuration (idempotent).
- `factoryReset()` restores factory defaults.
- All configuration entry points respect the 15s lockout: setters return
  `C4001Result::NotReady` and the raw/bracket helpers return `false` (or an
  empty String for `sendRawCommand()`) during the window.

### The 15-second lockout

Configuration commands are silently rejected for roughly 15 seconds after
power-on:
- `isReadyForConfig()` reports whether the window has cleared.
- `waitForConfigReady(timeoutMs)` blocks (bounded) until it has.
- Setters return `C4001Result::NotReady` during the window.

### Types

- `C4001Result`: `Success`, `AckedButUnconfirmed`, `Rejected`, `Timeout`,
  `NotReady`, `NotSupported`.
- `eMode_t`: `eExitMode` (presence), `eSpeedMode` (range/speed).
- `eSwitch_t`: `eON`, `eOFF`.
- `eSetMode_t`: `eStartSen`, `eStopSen`, `eResetSen`, `eRecoverSen`,
  `eSaveParams`, `eChangeMode`.
- structs: `sSensorStatus_t`, `sPrivateData_t`, `sResponseData_t`, `sPwmData_t`,
  `sAllData_t`.

### Initialization / status

- `begin(timeoutMs = 0)` - probe and connect; `false` on failure with a reason
  available via `beginErrorString()`.
- `getStatus()` -> `sSensorStatus_t` (`workStatus`, `workMode`, `initStatus`).
- `motionDetection()` - whether motion/presence is currently detected.

### Configuration setters (return `C4001Result`; `verify = true` by default)

| Method | Args (units) | Notes |
| --- | --- | --- |
| `setSensorMode` | `eMode_t` | presence vs. speed mode |
| `setDetectionRange` | `min` (>=30 cm, <=max), `max` (240-2000 cm), `trig` cm | |
| `setTrigSensitivity` | 0-9 | |
| `setKeepSensitivity` | 0-9 | |
| `setDelay` | `trig` 0-200 (x0.01 s), `keep` 4-3000 (x0.5 s) | |
| `setIoPolarity` | 0/1 | UART only (`NotSupported` on I2C) |
| `setPwm` | `pwm1,pwm2` 0-100, `timer` 0-255 (x64 ms) | UART only (`NotSupported` on I2C) |
| `setDetectThres` | `min,max` cm; `thres` 0-6553.5 (x0.1) | speed-mode threshold |
| `setFrettingDetection` | `eSwitch_t` | micromotion on/off |
| `beginConfig/endConfig/save/factoryReset` | - | lifecycle helpers (return `bool`) |

> **Renamed methods:** `setIoPolaity` -> `setIoPolarity`, `getIoPolaity` ->
> `getIoPolarity` and `getKeepTimerout` -> `getKeepTimeout` fix 1.0.0 typos.
> The old spellings still compile (deprecated shims).

> **⚠ Hardware caveat — OUT pin PWM/polarity:** on at least one SEN0610 12 m
> unit, `setPwm` and `setIoPolarity` are accepted (`Success`) and read back
> exactly correct, yet the physical OUT pin stays static HIGH through every
> duty/polarity combination - it behaves as a binary presence trigger only.
> Full evidence, method, and scope: [`HARDWARE_NOTES.md`](HARDWARE_NOTES.md).
> Verify modulation on your own unit before designing around OUT as a PWM output.

### Getters

`getTrigSensitivity`, `getKeepSensitivity`, `getMinRange`, `getMaxRange`,
`getTrigRange`, `getKeepTimeout`, `getTrigDelay`, `getIoPolarity`, `getPwm`,
`getTMinRange`, `getTMaxRange`, `getThresRange`, `getFrettingDetection`,
`getTargetNumber`, `getTargetSpeed`, `getTargetRange`, `getTargetEnergy`.

### Low-level / debugging

- `sendRawCommand(cmd, timeoutMs)` - send a raw ASCII command and return the raw
  response (UART only; empty String on the register-based I2C backend).
- `beginErrorString()` - why the last `begin()` failed.
- `resultToString(C4001Result)` - printable result description.

## Examples

| Example | What it shows |
| --- | --- |
| `motionDetection` | Presence mode end-to-end: arm the detection window, sensitivities, delays and OUT-pin PWM, then poll `motionDetection()`. Start here. |
| `mRangeVelocity` | Speed mode: target range/velocity telemetry. |
| `pwmDebug` | Diagnosis tool for the OUT pin: sweeps `setPwm`/`setIoPolarity` combos and times OUT duty from an input pin ("poor-man's scope"), with `motionDetection()` presence ground truth next to every sample. Config writes persist to flash, so it runs one sweep by default and parks (see the `COMBO_PASSES` note in the sketch). |
| `rawCommandConsole` | Interactive raw ASCII passthrough to the sensor (UART only). |
| `persistenceRoundtrip` | Verifies configuration survives a power cycle (`save()`, readbacks). |

Note: `setPwm`, `setIoPolarity` and raw ASCII commands are UART-only; on the
register-based I2C backend those setters report `NotSupported`.

## Compatibility

MCU                | Work Well    |   Work Wrong    | Untested    | Remarks
------------------ | :----------: | :-------------: | :---------: | :----:
Arduino Uno        |      √       |                 |             |
Arduino MEGA2560   |      √       |                 |             |
Arduino Leonardo   |      √       |                 |             |
FireBeetle-ESP8266 |      √       |                 |             |
FireBeetle-ESP32   |      √       |                 |             |
FireBeetle-M0      |      √       |                 |             |
Micro:bit          |      √       | nonsupport uart |             |


## History

- 2024/02/26 - Version 1.0.0 released.
- 2026/08/27 - Version 1.1.0: robustness fixes (memory-safe parsers, bounded
  waits, per-instance state, real connection probing), a result/verification
  API (`C4001Result`), raw-command passthrough (`sendRawCommand`), persistence
  helpers (`save()`, `factoryReset()`, `beginConfig`/`endConfig`), boot
  diagnostics (`beginErrorString`, `isReadyForConfig`/`waitForConfigReady`),
  baud recovery (`scanBaudRates`), three new examples, a host-side sanitizer
  test harness, and `HARDWARE_NOTES.md`. Typo'd method names were renamed with
  deprecated compatibility shims. Full list: [`CHANGES.md`](CHANGES.md).

## Credits

Written by ZhixinLiu(zhixin.liu@dfrobot.com), 2024. (Welcome to our website)
