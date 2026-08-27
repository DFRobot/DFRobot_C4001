/*!
 * @file  pwmDebug.ino
 * @brief Map the C4001's OUT-pin behavior across setPwm/setIoPolarity configs.
 *
 * Instead of a scope, this sketch uses an Arduino input pin to time the OUT
 * signal and estimate its duty cycle (a "poor-man's scope"). Useful for
 * confirming Manual Issue #2: what does the OUT pin actually do (duty / level)
 * as a function of pwm1/pwm2/timer and polarity, and in the presence mode.
 *
 * DETECTION is armed exactly like the motionDetection example (same mode,
 * window, sensitivities), and each combo is also probed with the same
 * read-only radar.motionDetection() poll, so you get presence ground truth
 * next to every duty sample. Sequence per combo matters: on the UART backend
 * EVERY setter AND every config readback (getPwm/getIoPolarity) restarts the
 * sensor internally (stop -> write -> save -> start). This sketch therefore
 * does all writes and readbacks FIRST, then lets the sensor settle (PWM ramp
 * + existence-engine warm-up) and only afterwards samples OUT and polls
 * presence - sampling right after a restart is what makes OUT look dead.
 *
 * WIRING (UART backend is REQUIRED: setPwm/setIoPolarity are UART-only here):
 *   C4001 OUT  ->  Arduino digital input (OUT_PIN, default pin 2; override
 *                  with -DC4001_PWM_OUT_PIN=n when your board needs another)
 *   C4001 RX/TX -> see the board table below; GND tie between boards.
 *   UNO/ESP8266: SoftwareSerial on 4/5.  ESP32: Serial1 remapped to GPIO25(rx)
 *   / GPIO26(tx) - plain numbers, NOT Dx macros (not defined by every ESP32
 *   core) and NOT GPIO2 (boot strapping + collides with OUT_PIN).
 * The input pin is a plain INPUT (no pull-up): the tested module actively
 * drives OUT high (see HARDWARE_NOTES.md). If your unit's OUT looks like an
 * open-drain/active-low line, switch to INPUT_PULLUP and re-run.
 *
 * FIRMWARE CAVEAT (see HARDWARE_NOTES.md at the repo root): on at least one
 * SEN0610 12m unit, EVERY combo below - both polarities, all duty values -
 * measured a static ~100% OUT level: config accepted + read back exactly, but
 * the pin never modulates. On such units OUT works only as a binary presence
 * trigger. If your sweep shows per-combo duties that actually track pwm1/pwm2,
 * please report it (append to HARDWARE_NOTES.md §1) - valuable counter-evidence.
 *
 * FLASH WEAR: on UART, each setPwm()/setIoPolarity() call issues SAVE_CONFIG
 * (a flash write). The default sweep therefore runs each combo exactly ONCE
 * (COMBO_PASSES = 1), prints a summary and parks. Raise COMBO_PASSES or set
 * it to 0 for endless stepping - but only for short sessions.
 * @license The MIT License (MIT)
 * @version 1.1.0
 */
#include "DFRobot_C4001.h"

#ifndef C4001_PWM_OUT_PIN
#define C4001_PWM_OUT_PIN 2
#endif
const int OUT_PIN = C4001_PWM_OUT_PIN;   // wire the sensor's OUT pin here

// Full sweeps to run before parking (see FLASH WEAR note above). 0 = forever.
const uint8_t COMBO_PASSES = 1;

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  SoftwareSerial mySerial(4, 5);
  DFRobot_C4001_UART radar(&mySerial, 9600);
#elif defined(ESP32)
  DFRobot_C4001_UART radar(&Serial1, 9600, /*rx*/25, /*tx*/26);
#else
  DFRobot_C4001_UART radar(&Serial1, 9600);
#endif

// pwm1 = duty (0-100) when NOT detected, pwm2 = duty when detected,
// timer = transition ticks (x64 ms), pol = io polarity (0: low-on-target, 1: high-on-target).
struct combo_t { uint8_t pwm1, pwm2, timer, pol; };
static const combo_t COMBOS[] = {
  { 50,  0, 10, 0 },
  { 50,  0, 10, 1 },
  {100, 10, 10, 1 },
  {100, 10, 10, 0 },
  {100,  0, 10, 0 },
  {100,  0, 10, 1 },
  {  0,100, 10, 0 },
  {  0,100, 10, 1 },
};
static const int NUM_COMBOS = sizeof(COMBOS) / sizeof(COMBOS[0]);

// Per-combo results for the end-of-run summary.
static float    s_bestDuty[NUM_COMBOS];
static uint16_t s_presHits[NUM_COMBOS];

// Presence ground truth: poll motionDetection() ~30x100 ms (3 s window),
// like the loop() of the motionDetection example.
static const uint16_t PRESENCE_POLLS    = 30;
// After the last config write/readback (each restarts the sensor), give the
// PWM ramp and the existence engine time to stabilize before sampling OUT.
static const uint32_t SETTLE_WARMUP_MS  = 1500;

// Estimate OUT duty cycle (%) by timing HIGH vs total time over windowMs.
float measureDuty(uint8_t pin, uint32_t windowMs)
{
  uint32_t totalUs = (uint32_t)windowMs * 1000UL;
  uint32_t startUs = micros();
  uint32_t highUs = 0;
  int last = digitalRead(pin);
  uint32_t segStart = micros();
  uint32_t nowUs;
  do{
    int v = digitalRead(pin);
    nowUs = micros();
    if(v != last){
      if(last == HIGH){
        highUs += nowUs - segStart;
      }
      segStart = nowUs;
      last = v;
    }
  }while(nowUs - startUs < totalUs);
  if(last == HIGH){
    highUs += nowUs - segStart;
  }
  uint32_t elapsed = nowUs - startUs;
  if(elapsed == 0) return 0.0f;
  return 100.0f * (float)highUs / (float)elapsed;
}

// Wait until live telemetry shows the sensor running again after a restart.
// getStatus() on UART only parses streamed frames - it never restarts the
// sensor - so it is safe to poll here.
void waitRunning(const char *why)
{
  for(int tries = 0; tries < 5; tries++){
    sSensorStatus_t st = radar.getStatus();
    if(st.workStatus == 1){
      return;
    }
    delay(1000);
  }
  Serial.print("WARN: sensor not reporting running (");
  Serial.print(why);
  Serial.println(") - OUT samples below may read 0% duty");
}

void setup()
{
  Serial.begin(115200);
  while(!Serial){}

  while(!radar.begin()){
    Serial.print("Not found: ");
    Serial.println(radar.beginErrorString());
    delay(1000);
  }
  Serial.println("C4001 connected.");

  // Wait out the ~15s power-on config lockout.
  if(!radar.waitForConfigReady()){
    Serial.println("WARN: config lockout did not clear in time");
  }

  pinMode(OUT_PIN, INPUT);  // plain input; the tested module actively drives OUT

  // ---- Detection arming: same system/sequence as the motionDetection example ----
  C4001Result r;
  r = radar.setSensorMode(eExitMode);   // presence mode: OUT reflects presence/PWM
  Serial.print("set presence (exit) mode : ");
  Serial.println(radar.resultToString(r));

  // Prove the mode actually took (workMode 0 = presence, 1 = speed). Without
  // this check a silently-ignored mode change looks exactly like "OUT dead".
  waitRunning("after setSensorMode");
  sSensorStatus_t st = radar.getStatus();
  Serial.print("status: work=");
  Serial.print(st.workStatus);
  Serial.print(" mode=");
  Serial.print(st.workMode ? "SPEED(!)" : "presence");
  Serial.print(" init=");
  Serial.println(st.initStatus);

  // Detection window (centimeters): min >= 30, max 240..2000, trig = distance
  // that must be crossed to trigger. Same values the motionDetection example
  // uses by default.
  r = radar.setDetectionRange(/*min*/30, /*max*/500, /*trig*/500);
  Serial.print("set detect window (min/max/trig) : ");
  Serial.println(radar.resultToString(r));

  // Sensitivity 0..9: 1/1 = the proven-easy defaults from motionDetection.
  r = radar.setTrigSensitivity(1);
  Serial.print("set trigger sensitivity (start) : ");
  Serial.println(radar.resultToString(r));
  r = radar.setKeepSensitivity(1);
  Serial.print("set hold sensitivity    (stay)  : ");
  Serial.println(radar.resultToString(r));

  // OUT switching delays: trig 0..200 (x0.01 s turn-ON delay), keep 4..3000
  // (x0.5 s turn-OFF hold). 50/4 = 0.5 s on / 2 s off: snappy enough that the
  // 600 ms duty windows below catch the switch, but still debounced.
  // (keep minimum is 4 - smaller values are rejected by the library.)
  r = radar.setDelay(/*trig*/50, /*keep*/4);
  Serial.print("set turn-on / turn-off delays : ");
  Serial.println(radar.resultToString(r));

  Serial.println();
  Serial.print("PWM/OUT map tool: ");
  Serial.print(NUM_COMBOS);
  Serial.print(" combos x ");
  Serial.print(COMBO_PASSES == 0 ? "infinite" : "1 pass");
  Serial.println(" sweep. Walk in/out of the window while each combo runs.");
  Serial.println("presence = motionDetection() hits per 30 polls; duty = max of 3 OUT samples.");
  Serial.println("NOTE: on some units OUT stays static HIGH regardless of these");
  Serial.println("settings (binary trigger only) - see HARDWARE_NOTES.md section 1.");

  // Zero the summary bookkeeping.
  for(int i = 0; i < NUM_COMBOS; i++){
    s_bestDuty[i] = 0.0f;
    s_presHits[i] = 0;
  }
}

// Apply one combo, then measure presence + OUT duty in the RIGHT order:
// write first, readback next (both restart the sensor internally), settle,
// and only then sample OUT.
void runCombo(int idx)
{
  const combo_t &c = COMBOS[idx];

  Serial.println();
  Serial.print("=== step ");
  Serial.print(idx);
  Serial.print(": set(p1=");
  Serial.print(c.pwm1);
  Serial.print(",p2=");
  Serial.print(c.pwm2);
  Serial.print(",tm=");
  Serial.print(c.timer);
  Serial.print(",pol=");
  Serial.print(c.pol);
  Serial.println(") ===");

  C4001Result r;
  r = radar.setPwm(c.pwm1, c.pwm2, c.timer);
  Serial.print("setPwm    : ");
  Serial.println(radar.resultToString(r));
  r = radar.setIoPolarity(c.pol);
  Serial.print("setIoPolarity: ");
  Serial.println(radar.resultToString(r));

  // Readbacks run through the same stop/save/start cycle, so do them now,
  // BEFORE the settle window - not between the settle and the measurement.
  sPwmData_t p = radar.getPwm();
  uint8_t pol = radar.getIoPolarity();
  Serial.print("readback(p1=");
  Serial.print(p.pwm1);
  Serial.print(",p2=");
  Serial.print(p.pwm2);
  Serial.print(",tm=");
  Serial.print(p.timer);
  Serial.print(",pol=");
  Serial.print(pol);
  Serial.print(")");
  if(p.pwm1 != c.pwm1 || p.pwm2 != c.pwm2 || p.timer != c.timer || pol != c.pol){
    Serial.print("  <-- MISMATCH vs requested");
  }
  Serial.println();

  // Let the duty transition (timer x 64 ms) finish and the existence engine
  // warm up fully before sampling.
  delay((uint32_t)c.timer * 64UL + SETTLE_WARMUP_MS);
  waitRunning("before sampling");

  // Presence ground truth, read-only (never restarts the sensor).
  uint16_t hits = 0;
  for(uint16_t i = 0; i < PRESENCE_POLLS; i++){
    if(radar.motionDetection()){
      hits++;
    }
    delay(100);
  }
  s_presHits[idx] = hits;

  // OUT duty: three samples, keep the max (transient engine blips only lower
  // individual windows).
  float best = 0.0f;
  for(int s = 0; s < 3; s++){
    float d = measureDuty(OUT_PIN, 600);
    if(d > best){
      best = d;
    }
    delay(200);
  }
  s_bestDuty[idx] = best;

  Serial.print("presence: ");
  Serial.print(hits);
  Serial.print("/");
  Serial.print(PRESENCE_POLLS);
  Serial.print(" polls | OUTduty~");
  Serial.print(best, 1);
  Serial.println("%");

  if(hits > PRESENCE_POLLS / 2 && best < 1.0f){
    Serial.println("  -> target seen but OUT flat: pwm/polarity combo may not drive OUT (check readback).");
  }
  if(hits == 0){
    Serial.println("  -> no target during this window; re-run this combo with someone present.");
  }
}

void printSummary()
{
  Serial.println();
  Serial.println("==== sweep summary ====");
  for(int i = 0; i < NUM_COMBOS; i++){
    Serial.print("step ");
    Serial.print(i);
    Serial.print(": set(p1=");
    Serial.print(COMBOS[i].pwm1);
    Serial.print(",p2=");
    Serial.print(COMBOS[i].pwm2);
    Serial.print(",tm=");
    Serial.print(COMBOS[i].timer);
    Serial.print(",pol=");
    Serial.print(COMBOS[i].pol);
    Serial.print(") presence ");
    Serial.print(s_presHits[i]);
    Serial.print("/");
    Serial.print(PRESENCE_POLLS);
    Serial.print(" OUTduty~");
    Serial.print(s_bestDuty[i], 1);
    Serial.println("%");
  }
  Serial.println("Sweep done. LAST combo stays active (saved to flash). Reset to re-run.");
}

void loop()
{
  static int step = 0;      // combo index within the current pass
  static int pass = 0;      // completed passes
  static bool done = false;

  if(done){
    delay(1000);            // parked; sensor keeps running with the last combo
    return;
  }

  runCombo(step);
  step++;

  if(step >= NUM_COMBOS){
    step = 0;
    pass++;
    printSummary();
    if(COMBO_PASSES != 0 && pass >= COMBO_PASSES){
      done = true;          // park: no more config writes, no more flash wear
    }
  }
}
