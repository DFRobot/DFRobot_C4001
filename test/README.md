# DFRobot_C4001 parser test harness

Host-runnable AddressSanitizer/UBSan harness for the two near-pure
buffer&#8594;struct parsers in `src/DFRobot_C4001.cpp`:

- `DFRobot_C4001::analysisData(B[t], len)` — `$DFHPD` / `$DFDMD` telemetry frames
- `DFRobot_C4001::analysisResponse(B[t], len, count)` — `Response ...` command replies

It compiles the *actual* library `.cpp` (not a copy) against minimal stubs of
`Arduino.h` / `Wire.h` / `HardwareSerial.h`, then feeds it hostile frames
(empty, truncated, oversized, "Res" at the buffer boundary, no spaces, leading
garbage) so the memory-safety fixes can be verified under ASan.

## Files

- `harness.cpp` — the tests (exposes the two protected parsers via a subclass).
- `include/Arduino.h` — host `String` / `millis` / `delay` stand-ins.
- `include/Wire.h` — host `TwoWire` stub (I2C backend, not exercised).
- `include/HardwareSerial.h` — host `HardwareSerial` stub (UART backend).
- `Makefile` — build/run.

## Build

```sh
mkdir -p build/asan
# Copy real sanitizer runtimes in if the toolchain lacks them (see below).
g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined \
    -Isrc -Itest/include -Lbuild/asan \
    src/DFRobot_C4001.cpp test/harness.cpp -o build/radar_test

LD_LIBRARY_PATH=$(pwd)/build/asan build/radar_test
```

A clean run prints `=== 23 checks, 0 failures ===` with no
`AddressSanitizer:` / `runtime error:` diagnostics. A non-zero exit code
(`failures > 0`) or an ASan report means a regression.

## Sanitizer runtime (this toolbox)

This machine's GCC installs the ASan and UBSan runtime shared libraries only
as linker *scripts* that reference `/usr/lib64/libasan.so.8.0.0` /
`/usr/lib64/libubsan.so.1.0.0` (absent). The real objects ship inside the
Flatpak runtimes; stage them where the linker and loader will find them:

```sh
FP=$(find /var/lib/flatpak -name 'libasan.so.8.0.0'  | head -1); cp "$FP" build/asan/libasan.so;        cp "$FP" build/asan/libasan.so.8
FP=$(find /var/lib/flatpak -name 'libubsan.so.1.0.0' | head -1); cp "$FP" build/asan/libubsan.so;       cp "$FP" build/asan/libubsan.so.1
```

On any normal installation (standard `libasan.so`/`libubsan.so` in the linker
path) step 1 is unnecessary and drop `-Lbuild/asan`.