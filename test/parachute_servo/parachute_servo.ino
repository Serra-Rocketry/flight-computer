/**
 * @file parachute_servo.ino
 * @brief Bench test for the parachute release servo under real load (risk #4)
 *
 * The flight firmware (firmware/parachute_module.cpp + firmware/config.h)
 * deploys the parachute by sweeping the servo from SERVO_CLOSED (90°) to
 * SERVO_OPEN (0°) and then sets a logical flag. There is NO feedback in
 * flight: no end-stop switch, no current sensing. If the compartment door
 * jams or the packed chute / spring needs more torque than the servo can
 * deliver, the servo stalls and the firmware never notices.
 *
 * This sketch provides that missing evidence ON THE BENCH:
 *   1. Open sweep: servo CLOSED (90°) -> OPEN (0°) with the REAL load
 *      attached (door + packed chute + spring/rubber band, if used).
 *   2. Timing: command-to-fully-open time, measured with a bench microswitch
 *      (FEEDBACK_PIN) tripped by the door at full travel. If no microswitch
 *      is available (FEEDBACK_PIN = -1), the operator measures with a
 *      stopwatch and types the value in — documented in README.md.
 *   3. Repetition: REPETITIONS trials (default 20) for statistics.
 *   4. Failure reporting: a trial FAILS if the door is not fully open within
 *      OPEN_TIME_LIMIT_MS, or if the door does not stay open during the hold
 *      check (spring/chute back-driving the servo).
 *
 * Serial commands (115200 baud):
 *   'o'  run one open-sweep trial (re-pack the chute and close the door first)
 *   'c'  close sweep: servo back to SERVO_CLOSED (re-arms the mechanism)
 *   'b'  blocked-horn probe: holds at SERVO_OPEN for STALL_HOLD_MS so the
 *        operator can read the locked-rotor stall current (multimeter
 *        peak-hold); servo releases back to CLOSED afterwards
 *   's'  print statistics so far
 *   'r'  reset statistics
 *
 * Wiring (bench fixture — NOT flight hardware):
 *   - Servo signal wire -> SERVO_PIN (GPIO 10, same as firmware config.h)
 *   - Servo power: red wire +5V, brown GND. Put the multimeter (A, peak-hold)
 *     in series with the red wire to measure opening / stall current.
 *   - Bench microswitch mounted so the compartment door trips it when FULLY
 *     open: FEEDBACK_PIN -> switch -> GND (INPUT_PULLUP, tripped = LOW).
 *
 * SERVO_PIN / SERVO_OPEN / SERVO_CLOSED mirror firmware/config.h on purpose.
 * Keep them in sync when the firmware changes (see README.md).
 *
 * @author #11 (Serra Rocketry)
 * @date 2026
 */

#include <ESP32Servo.h>

//==============================================================================
// CONFIGURATION (mirror of firmware/config.h — keep in sync)
//==============================================================================

const int SERVO_PIN    = 10;   ///< Servo PWM pin (config.h: SERVO_PIN)
const int SERVO_OPEN   = 0;    ///< Fully open (config.h: SERVO_OPEN)
const int SERVO_CLOSED = 90;   ///< Fully closed / locked (config.h: SERVO_CLOSED)

//==============================================================================
// BENCH TEST PARAMETERS (criteria — fix BEFORE the test run, see README.md)
//==============================================================================

const int REPETITIONS         = 20;     ///< Number of open-sweep trials
const unsigned long OPEN_TIME_LIMIT_MS = 1000;  ///< Max command-to-fully-open time (pass criterion A)
const unsigned long HOLD_MS            = 2000;  ///< Door must stay open this long after OPEN (criterion B)
const unsigned long STALL_HOLD_MS      = 2000;  ///< Window to read locked-rotor stall current ('b' probe)
const unsigned long CLOSE_SWEEP_MS     = 1000;  ///< Time budget for the close sweep
const unsigned long SETTLE_MS          = 300;   ///< Pause between sweeps

// Bench microswitch (door fully open). Set to -1 for manual stopwatch mode.
const int FEEDBACK_PIN = 4;
const int TRIPPED_LEVEL = LOW;   ///< Microswitch to GND with INPUT_PULLUP

const unsigned long SERIAL_BAUD = 115200;

const int MAX_TRIALS = 100;      ///< Stats buffer size

//==============================================================================
// GLOBAL STATE
//==============================================================================

Servo parachuteServo;
unsigned long openTimesMs[MAX_TRIALS];
bool trippedFlags[MAX_TRIALS];
bool openFailFlags[MAX_TRIALS];
bool holdFailFlags[MAX_TRIALS];
int trialCount = 0;

//==============================================================================
// HELPERS
//==============================================================================

/**
 * @brief Print the bench test banner and command help.
 */
void printBanner() {
  Serial.println();
  Serial.println("============================================================");
  Serial.println("PARACHUTE SERVO BENCH TEST — real load (risk #4)");
  Serial.println("============================================================");
  Serial.println("Commands:");
  Serial.println("  'o'  run one open-sweep trial (re-pack chute + close door first)");
  Serial.println("  'c'  close sweep (servo back to SERVO_CLOSED)");
  Serial.println("  'b'  blocked-horn stall probe (multimeter peak-hold)");
  Serial.println("  's'  print statistics");
  Serial.println("  'r'  reset statistics");
  Serial.println("------------------------------------------------------------");
  Serial.print("Criteria: open time < ");
  Serial.print(OPEN_TIME_LIMIT_MS);
  Serial.print(" ms, hold ");
  Serial.print(HOLD_MS);
  Serial.print(" ms, trials: ");
  Serial.println(REPETITIONS);
  if (FEEDBACK_PIN < 0) {
    Serial.println("Feedback: MANUAL (stopwatch — type the measured time)");
  } else {
    Serial.print("Feedback: microswitch on pin ");
    Serial.println(FEEDBACK_PIN);
  }
  Serial.println();
}

/**
 * @brief Wait for the bench microswitch to trip (door fully open).
 *
 * Polls FEEDBACK_PIN until it reads TRIPPED_LEVEL or the timeout expires.
 *
 * @param timeoutMs Max time to wait in milliseconds
 * @return true if the switch tripped within the timeout, false otherwise
 */
bool waitForFeedback(unsigned long timeoutMs) {
  if (FEEDBACK_PIN < 0) {
    return false;
  }
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(FEEDBACK_PIN) == TRIPPED_LEVEL) {
      return true;
    }
    delay(1);
  }
  return false;
}

/**
 * @brief Read a long integer from the serial line with a timeout.
 *
 * Used in manual (stopwatch) mode to collect the operator-measured open time.
 * Consumes characters until newline or timeout.
 *
 * @param timeoutMs Max time to wait for the value
 * @return Parsed value, or -1 on timeout
 */
long readLongFromSerial(unsigned long timeoutMs) {
  unsigned long start = millis();
  long value = 0;
  bool gotDigit = false;
  while (millis() - start < timeoutMs) {
    while (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (gotDigit) {
          return value;
        }
      } else if (c >= '0' && c <= '9') {
        value = value * 10 + (c - '0');
        gotDigit = true;
      }
    }
    delay(1);
  }
  return -1;
}

/**
 * @brief Print one CSV result row (machine-parseable for the Python analyzer).
 *
 * Format: RUN,OPEN_MS,TRIPPED,OPEN_FAIL,HOLD_FAIL
 */
void printResultRow(int run, unsigned long openMs, bool tripped,
                    bool openFail, bool holdFail) {
  Serial.print(run);
  Serial.print(',');
  Serial.print(openMs);
  Serial.print(',');
  Serial.print(tripped ? 1 : 0);
  Serial.print(',');
  Serial.print(openFail ? 1 : 0);
  Serial.print(',');
  Serial.println(holdFail ? 1 : 0);
}

//==============================================================================
// TEST SEQUENCES
//==============================================================================

/**
 * @brief Run one open-sweep trial under the real load.
 *
 * Sweeps the servo SERVO_CLOSED -> SERVO_OPEN, measures the command-to-open
 * time (microswitch or manual), then verifies the door stays open for
 * HOLD_MS (spring/chute must not back-drive the servo). Auto-returns to
 * SERVO_CLOSED so the operator can re-arm the mechanism.
 */
void runOpenTrial() {
  if (trialCount >= MAX_TRIALS) {
    Serial.println("ERROR: trial buffer full, reset with 'r'");
    return;
  }

  if (FEEDBACK_PIN >= 0 && digitalRead(FEEDBACK_PIN) == TRIPPED_LEVEL) {
    Serial.println("WARNING: door already open (switch tripped). Close it first.");
  }

  Serial.print("Trial ");
  Serial.print(trialCount + 1);
  Serial.println(": opening...");

  // --- Open sweep: measure command-to-fully-open time ---------------------
  unsigned long t0 = millis();
  parachuteServo.write(SERVO_OPEN);
  bool tripped = waitForFeedback(OPEN_TIME_LIMIT_MS);
  unsigned long openMs = tripped ? (millis() - t0) : OPEN_TIME_LIMIT_MS;

  if (FEEDBACK_PIN < 0) {
    // Manual mode: operator measured with a stopwatch. Ask for the value.
    Serial.print("Enter measured open time (ms) [limit ");
    Serial.print(OPEN_TIME_LIMIT_MS);
    Serial.println("]:");
    long manualMs = readLongFromSerial(20000);
    if (manualMs >= 0) {
      openMs = (unsigned long)manualMs;
      tripped = (openMs < OPEN_TIME_LIMIT_MS);
    } else {
      Serial.println("WARNING: no input received, using sweep window as open time");
    }
  }

  bool openFail = !tripped;

  // --- Hold check: door must stay open (no back-drive) ---------------------
  bool holdFail = false;
  if (FEEDBACK_PIN >= 0 && tripped) {
    unsigned long holdStart = millis();
    while (millis() - holdStart < HOLD_MS) {
      if (digitalRead(FEEDBACK_PIN) != TRIPPED_LEVEL) {
        holdFail = true;
        break;
      }
      delay(1);
    }
  }

  // --- Record and report ----------------------------------------------------
  openTimesMs[trialCount] = openMs;
  trippedFlags[trialCount] = tripped;
  openFailFlags[trialCount] = openFail;
  holdFailFlags[trialCount] = holdFail;
  trialCount++;

  printResultRow(trialCount, openMs, tripped, openFail, holdFail);

  if (openFail) {
    Serial.print("FAIL: door not fully open within ");
    Serial.print(OPEN_TIME_LIMIT_MS);
    Serial.println(" ms (stall or jam suspected)");
  }
  if (holdFail) {
    Serial.println("FAIL: door did not stay open during hold check (back-drive)");
  }
  if (!openFail && !holdFail) {
    Serial.println("PASS");
  }

  // Re-arm the mechanism for the next trial.
  delay(SETTLE_MS);
  parachuteServo.write(SERVO_CLOSED);
  Serial.println("Servo back to CLOSED. Re-pack the chute and close the door, then 'o'.");
  Serial.println();
}

/**
 * @brief Close sweep: move the servo back to SERVO_CLOSED.
 *
 * Used to re-arm the mechanism between trials or before power-off.
 */
void runCloseSweep() {
  Serial.println("Closing...");
  parachuteServo.write(SERVO_CLOSED);
  delay(CLOSE_SWEEP_MS);
  Serial.println("Servo at CLOSED.");
}

/**
 * @brief Blocked-horn stall probe.
 *
 * The operator blocks the servo horn by hand while the servo drives to
 * SERVO_OPEN and holds for STALL_HOLD_MS. Read the locked-rotor stall
 * current with the multimeter in peak-hold mode during the window. The
 * servo then releases back to CLOSED. Run this briefly and LAST — a locked
 * servo draws maximum current and heats up.
 */
void runStallProbe() {
  Serial.println("BLOCK the servo horn NOW and hold it (do not let it move).");
  Serial.print("Probing in 2 s... window: ");
  Serial.print(STALL_HOLD_MS);
  Serial.println(" ms. Read the multimeter peak!");
  delay(2000);
  parachuteServo.write(SERVO_OPEN);
  delay(STALL_HOLD_MS);
  parachuteServo.write(SERVO_CLOSED);
  Serial.println("Probe done. Servo released back to CLOSED.");
  Serial.println();
}

//==============================================================================
// STATISTICS
//==============================================================================

/**
 * @brief Print statistics over all completed trials.
 *
 * Reports totals, failures, and mean/min/max/std of the open time (only
 * successful, tripped runs contribute to the timing stats).
 */
void printStatistics() {
  if (trialCount == 0) {
    Serial.println("No trials recorded yet.");
    return;
  }

  int nTripped = 0;
  int nOpenFail = 0;
  int nHoldFail = 0;
  unsigned long sum = 0;
  unsigned long minMs = 0;
  unsigned long maxMs = 0;

  for (int i = 0; i < trialCount; i++) {
    if (trippedFlags[i]) {
      nTripped++;
      unsigned long v = openTimesMs[i];
      sum += v;
      if (nTripped == 1 || v < minMs) minMs = v;
      if (v > maxMs) maxMs = v;
    }
    if (openFailFlags[i]) nOpenFail++;
    if (holdFailFlags[i]) nHoldFail++;
  }

  // Mean and population standard deviation over successful runs.
  float meanMs = (nTripped > 0) ? ((float)sum / nTripped) : 0.0f;
  float varSum = 0.0f;
  for (int i = 0; i < trialCount; i++) {
    if (trippedFlags[i]) {
      float d = (float)openTimesMs[i] - meanMs;
      varSum += d * d;
    }
  }
  float stdMs = (nTripped > 0) ? sqrt(varSum / nTripped) : 0.0f;

  Serial.println("------------------------------------------------------------");
  Serial.println("STATISTICS:");
  Serial.print("  trials total : "); Serial.println(trialCount);
  Serial.print("  fully open   : "); Serial.println(nTripped);
  Serial.print("  open fails   : "); Serial.println(nOpenFail);
  Serial.print("  hold fails   : "); Serial.println(nHoldFail);
  Serial.print("  open time ms : mean="); Serial.print(meanMs, 1);
  Serial.print(" min="); Serial.print(minMs);
  Serial.print(" max="); Serial.print(maxMs);
  Serial.print(" std="); Serial.println(stdMs, 1);
  Serial.print("  worst case   : "); Serial.println(maxMs);

  // Verdict against the fixed criteria.
  bool pass = (nOpenFail == 0) && (nHoldFail == 0) &&
              (trialCount >= REPETITIONS);
  Serial.print("  VERDICT      : ");
  Serial.println(pass ? "PASS (20/20, all within limits)" : "FAIL (see README.md 'If it fails')");
  Serial.println("------------------------------------------------------------");
  Serial.println();
}

//==============================================================================
// ARDUINO LIFECYCLE
//==============================================================================

/**
 * @brief Initialize serial, attach the servo, and arm the mechanism closed.
 */
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  if (FEEDBACK_PIN >= 0) {
    pinMode(FEEDBACK_PIN, INPUT_PULLUP);
  }

  if (!parachuteServo.attach(SERVO_PIN)) {
    Serial.println("ERROR: servo attach failed — check SERVO_PIN wiring");
    while (1) {
      delay(1000);
    }
  }

  parachuteServo.write(SERVO_CLOSED);
  delay(CLOSE_SWEEP_MS);

  printBanner();
  Serial.println("Mechanism armed at CLOSED. Ready.");
  Serial.println();
}

/**
 * @brief Dispatch serial commands.
 */
void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    switch (cmd) {
      case 'o':
        runOpenTrial();
        break;
      case 'c':
        runCloseSweep();
        break;
      case 'b':
        runStallProbe();
        break;
      case 's':
        printStatistics();
        break;
      case 'r':
        trialCount = 0;
        Serial.println("Statistics reset.");
        break;
      default:
        break;  // ignore whitespace / unknown chars
    }
  }
}
