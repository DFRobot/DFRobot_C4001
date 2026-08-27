# HARDWARE_NOTES — empirically observed hardware/firmware caveats

Field notes from real C4001 units, kept so future users do not re-derive them
the hard way. Everything below is **observation + cited sources**; scope of
evidence is stated per item. Corrections and additional unit reports are
welcome — append to the relevant section with your SKU, firmware behavior, and
test conditions.

---

## 1. OUT pin may not follow `setPwm()` / `setIoPolarity()` at all
*(observed on a **SEN0610 12 m** unit during v1.1.0 development; see scope note)*

### Symptom

The documented behavior (vendor API docs) is:

| Command | Documented effect |
|---|---|
| `setPwm(pwm1, pwm2, timer)` | OUT duty = `pwm1`% when no target, `pwm2`% when a target is present; duty ramps over `timer × 64 ms` |
| `setIoPolarity(0/1)` | polarity: `0` → OUT low while target present, high otherwise; `1` → inverse (default) |

**Measured instead:** the OUT pin sat at one static HIGH level through every
combination tried. Configuration commands are *accepted* (`Success`) and
*read back identically* (`getPwm()` / `getIoPolarity()` return exactly what was
written) — but the physical pin never modulates.

### Evidence summary

Two controlled sweeps using `examples/pwmDebug/pwmDebug.ino` (UART backend,
presence mode armed exactly like `motionDetection`, target continuously
present). Each combo was followed by exact readbacks and then three
600 ms duty samples taken from an MCU input pin wired to OUT:

- Run A: input configured `INPUT_PULLUP` → duty ≈ 100.0 % in all 8 combos
  spanning `pwm1 ∈ {0, 50, 100}` × `pwm2 ∈ {0, 10, 100}` × polarity `{0,1}`
- Run B: input floating (`INPUT`) → identical ≈ 100.0 % in all connected steps;
  the deliberately disconnected control step read a clean 0.0 %

Run B's contrast (attached: firm HIGH vs disconnected: clean LOW) rules out the
MCU pull-up as the cause of the reading and shows the **module itself sources
the node high**. If it were an undriven open-drain line it would have floated
like the disconnected step.

Why static-HIGH cannot be reconciled with the documented semantics under either
run's conditions: combos with `pwm2 = 0` must hold OUT low while the target is
continuously present, and flipping polarity must change *something*. Neither ever
happened. Low-duty combos (`pwm2 = 10`) would also be indistinguishable from 0 %
at these sample rates yet still never deviated from 100 %.

### What this means practically

- Treat OUT as, at best, a **binary presence-trigger output**, not an analog/PWM
  output — until proven otherwise on your unit. Vendor marketing language
  matches this reading: DFRobot's CN product page advertises only
  “I/O口高低电平输出控制” (high/low level output), and the datasheet describes
  OUT-as-trigger-switch usage ("only using the OUT pin") including re-trigger
  blocking-time semantics — never demonstrates PWM.
- Do NOT design an RC-filter/analog application around OUT without first
  verifying your specific unit.
- The configuration channel itself is fine: commands persist and survive power
  cycles (`persistenceRoundtrip` style checks pass). The failure is confined to
  the physical output stage / firmware interpretation.
- Presence/speed telemetry, detection-window config, sensitivity, delays, and
  raw command passthrough all worked normally on the same unit throughout.

### Scope-of-evidence disclaimer

Single SEN0610 (12 m) unit, firmware revision unknown/unreadable via the
protocol. This note does **not** claim every unit behaves this way — no public
report confirming or refuting PWM-on-OUT was found anywhere online at the time
of writing (searches covered the DFRobot forum, GitHub issues for
`DFRobot_C4001`, and general web). If you test OUT modulation, please record
your result here (SKU + firmware batch + method).

### Reproduce

Upload `examples/pwmDebug/pwmDebug.ino` (single sweep by default; warns on any
mismatch between requested and read-back config) and compare its per-combo
`OUTduty~` values against the table in the vendor docs above. To rule out
wiring, temporarily lift the jumper between sensor OUT and the MCU input:
a healthy drive changes with config, an unconnected/floating pin reads ~0 %.

### References consulted

- DFRobot API docs describing the PWM contract:
  [SEN0610 wiki](https://wiki.dfrobot.com/sen0610/docs/20949),
  [PlatformIO library page](https://registry.platformio.org/libraries/dfrobot/DFRobot_C4001)
- CN product page advertising OUT as level output only:
  [wiki.dfrobot.com.cn SEN0609](https://wiki.dfrobot.com.cn/sku_sen0609_c4001_mmwave_presence_sensor_25m)
- SEN0609 datasheet, OUT-as-switch framing & blocking time:
  [PDF](https://dfimg.dfrobot.com/wiki/20522/SEN0609_gravity-c4001-24ghz-mmwave-human-presence-detection-sensor_datasheet_V1.pdf)
- Third-party tutorial using OUT binary-only:
  [makerguides.com](https://www.makerguides.com/human-presence-detection-with-mmwave-c4001/)
- Forum threads checked (other failure modes, none about OUT modulation):
  [399073](https://www.dfrobot.com/forum/topic/399073),
  [401984](https://www.dfrobot.com/forum/topic/401984),
  [334728](https://www.dfrobot.com/forum/topic/334728)

---

*Keep sections numbered so links stay stable.*
