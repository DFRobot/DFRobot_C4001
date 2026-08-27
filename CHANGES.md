# Changelog

All notable changes since the 1.0.0 release currently on GitHub.

## 1.1.0 (2026-08-27)

### Fixed

- **Memory safety in the UART protocol parsers** (`analysisData` /
  `analysisResponse`), verified under ASan/UBSan by the new test harness:
  - the `Response` parser capped its field-offset table at 5 entries (a
    longer reply used to overflow it), bounds-checks the `Res` token, and
    returns deterministic defaults instead of parsing garbage;
  - `$DFDMD` frames are parsed from a local copy, so `strtok()` no longer
    mutates the caller's buffer, and truncated frames leave the result zeroed
    instead of reading uninitialized `parts[]` entries;
  - the `$DFHPD` exist-byte read and the type-token `strncmp()` lookahead are
    bounds-guarded.
- **I2C `readReg()` clamps the copy to the requested length** — a device
  answering with more bytes than requested can no longer overflow the
  caller's buffer.
- **`getStatus()` (UART) no longer blocks forever.** 1.0.0 issued
  `sensorStart` and spun in `while(len == 0)` — an endless hang when the
  sensor had been deliberately stopped. It now reports the sensor as stopped
  and never silently restarts it.
- **`motionDetection()` (UART) latches per instance.** 1.0.0 used a
  function-local `static bool` shared by every radar object (and dead code),
  so presence flickered between telemetry frames. Presence is now latched
  with a small empty-frame debounce (value pending hardware verification).
- **`getTargetNumber()` debounce counter is per-instance** (was a `static`
  shared across all radar objects).
- **`sensorStop()` has a bounded retry deadline** (was an unbounded
  `while(1)`).
- **`begin()` actually probes the sensor.** UART `begin()` used to return
  `true` unconditionally (false "Device connected!" with nothing attached);
  it now sends a benign query and requires a reply. I2C `begin()` verifies a
  real register read after the address ACK (an ACK alone is not reliable on
  all cores). The failure reason is available via `beginErrorString()`.
- **Virtual destructor** on the polymorphic base (safe polymorphic delete);
  the backend-flag member moved from `public` to `protected`.

### Added

- `C4001Result` (Success / AckedButUnconfirmed / Rejected / Timeout /
  NotReady / NotSupported) returned by every setter, with optional read-back
  verification (`verify = true` by default) and `resultToString()` for
  printing.
- 15-second post-power-on **config lockout** handling: `isReadyForConfig()`,
  `waitForConfigReady()`; all configuration entry points return
  `NotReady`/`false` during the window instead of being silently ignored.
- **Config lifecycle helpers**: `beginConfig()` / `endConfig()` (explicit
  config-mode bracket), `save()` (explicit persist-now), `factoryReset()`.
- `sendRawCommand(cmd, timeoutMs)` — raw ASCII passthrough with the
  documented CR+LF framing (UART only).
- `scanBaudRates(...)` — recovery helper for a sensor saved at an unexpected
  baud (UART only).
- `eBeginError_t` + `beginErrorString()` boot diagnostics.
- Examples: `pwmDebug` (OUT-pin duty sweep, "poor-man's scope"),
  `rawCommandConsole` (interactive protocol console),
  `persistenceRoundtrip` (config-survives-power-cycle check).
- `test/` — host-runnable ASan/UBSan parser harness (23 checks; see
  `test/README.md`).
- `HARDWARE_NOTES.md` — empirically observed hardware/firmware caveats. §1
  documents that on at least one SEN0610 12 m unit the OUT pin never
  modulates despite `setPwm`/`setIoPolarity` being accepted and read back
  correctly (binary presence trigger only).
- `keywords.txt` entries for the new API.

### Changed (breaking)

- **All setters return `C4001Result` instead of `bool`** (and
  `setFrettingDetection`, formerly `void`, does too). Since `C4001Result` is
  a scoped enum, code like `if(radar.setDelay(...))` must become
  `if(radar.setDelay(...) == C4001Result::Success)`. Every example shows the
  new pattern.
- `begin()` takes an optional `timeoutMs` probe window.
- `setPwm` / `setIoPolarity` on the I2C backend now return `NotSupported`
  (1.0.0 pretended success); `getPwm`/`getIoPolarity` return zeroes there.
- Setters persist configuration automatically (stop → write → save → start
  on UART; register write + save on I2C), matching the vendor Python
  reference semantics.
- README rewritten around the run/config lifecycle, units, and caveats;
  README_CN updated for the renamed API.

### Renamed (old spellings still compile via deprecated shims)

| 1.0.0 | 1.1.0 |
| --- | --- |
| `setIoPolaity()` | `setIoPolarity()` |
| `getIoPolaity()` | `getIoPolarity()` |
| `getKeepTimerout()` | `getKeepTimeout()` |

The protected parser methods `anaysisData`/`anaysisResponse` were renamed to
`analysisData`/`analysisResponse` (internal only).

### Internal cleanup (no behavior change)

- In-class `#define`s replaced with named `static constexpr` constants;
  `TIME_OUT` became `UART_READ_BLOCK_MS`; every magic `delay()`/timeout
  literal is now a named constant (all values unchanged).
- The two mode-dependent I2C register maps (presence vs. speed, which alias
  the same addresses by design) are grouped and documented; typo fix
  `REG_RESULT_OBJ_MUN` → `REG_RESULT_OBJ_NUM`.
- Dead members (`_M_Flag`, `_addr`) and dead code removed; getter
  latch/debounce semantics documented in the header.
- `.gitignore` added (`build/`, `__pycache__/`, `*.pyc`); compiled artifacts
  are no longer part of the tree, and the Python bytecode file accidentally
  committed in 1.0.0 (`python/raspberrypi/__pycache__/...pyc`) was removed.
- File-header metadata aligned to V1.1 across `src/` and examples.

## 1.0.0 (2024-02-26)

Initial release.
