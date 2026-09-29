/**
 * @file gps_diag.ino
 * @brief Raw GPS UART sniffer — find the right baud and pin orientation
 *
 * Sniffs GPIO18 (config RX_GPS) as RX in raw mode at 4800/9600/38400/115200,
 * then swaps (GPIO18 as TX side / GPIO17 as RX) to catch a crossed wiring.
 * Prints raw byte counts and any ASCII runs so NMEA is visible even without
 * full sentence sync.
 *
 * @author Serra Rocketry (#11)
 * @date 2026
 */

#define PIN_A 18  // firmware RX_GPS
#define PIN_B 17  // firmware TX_GPS

static const uint32_t BAUDS[] = {4800, 9600, 38400, 115200};

// sniff: count bytes on rxPin@baud for ms, print ASCII runs >= 6 chars
void sniff(int rxPin, int txPin, uint32_t baud, uint32_t ms) {
  HardwareSerial sn(2);  // UART2 leaves UART1 free
  sn.begin(baud, SERIAL_8N1, rxPin, -1);  // RX only — do not drive the GPS TX line
  uint32_t t0 = millis();
  int bytes = 0, asciiRuns = 0;
  char run[100]; int rl = 0;
  while (millis() - t0 < ms) {
    while (sn.available()) {
      char c = sn.read();
      bytes++;
      if (c >= 32 && c < 127) {
        if (rl < 99) run[rl++] = c;
      } else {
        if (rl >= 6) { run[rl] = 0; Serial.printf("    \"%s\"\n", run); asciiRuns++; }
        rl = 0;
      }
    }
    delay(2);
  }
  if (rl >= 6) { run[rl] = 0; Serial.printf("    \"%s\"\n", run); asciiRuns++; }
  sn.end();
  Serial.printf("  RX=GPIO%d @%lu: %d bytes, %d ascii runs\n", rxPin, baud, bytes, asciiRuns);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== GPS raw sniffer ===");

  Serial.println("[1] orientation A: GPS-TX -> GPIO18 (as config.h)");
  for (uint32_t b : BAUDS) sniff(PIN_A, PIN_B, b, 2000);

  Serial.println("[2] orientation B: GPS-TX -> GPIO17 (swapped)");
  for (uint32_t b : BAUDS) sniff(PIN_B, PIN_A, b, 2000);

  Serial.println("\n=== sniffer done — pick the baud+pin combo with NMEA text ===");
}

void loop() { delay(1000); }
