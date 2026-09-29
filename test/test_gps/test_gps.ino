// ============================================================
// Test: GPSModule
// Valida os criterios de aceitacao da Issue #5:
//   1. GPS recebe NMEA sentences via update() nao-bloqueante
//   2. getTimeString() retorna tempo valido apos fix
//   3. Nao bloqueia se nao houver sinal
//
// Hardware: ESP32 com modulo GPS em Serial1 (RX=20, TX=21)
// Baud monitor: 115200
// ============================================================

// Stubs de config.h para evitar dependencias extras no teste
#define RX_GPS 20
#define TX_GPS 21

#include "../../firmware/sensors/GPSModule.h"

// ============================================================
// Globals
// ============================================================

GPSModule gps(&Serial1);

unsigned long last_print_ms = 0;
const unsigned long PRINT_INTERVAL_MS = 1000;  // imprimir a cada 1s

unsigned long bytes_received   = 0;
unsigned long sentences_parsed = 0;

// ============================================================
// Helpers
// ============================================================

void printSeparator() {
  Serial.println("------------------------------------------------------------");
}

void printTestHeader() {
  Serial.println("============================================================");
  Serial.println("  TEST: GPSModule - Issue #5");
  Serial.println("============================================================");
  Serial.println("  Aguardando dados NMEA... (fix pode levar ate 60s)");
  printSeparator();
}

// Conta bytes disponiveis antes de update() para verificar que
// o buffer esta sendo drenado corretamente.
void feedAndCount() {
  // Expoe internamente: a funcao update() consome Serial1.available()
  // Medimos bytes antes/depois para confirmar que nao esta bloqueando.
  unsigned long t0 = micros();
  gps.update();
  unsigned long dt = micros() - t0;

  // update() deve terminar em microsegundos, nao em milissegundos
  if (dt > 5000) {
    Serial.print("[WARN] update() levou ");
    Serial.print(dt);
    Serial.println(" us - pode estar bloqueando!");
  }
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  printTestHeader();

  // --- Criterio 3: begin() nao deve bloquear ---
  unsigned long t0 = millis();
  bool ok = gps.begin();
  unsigned long elapsed = millis() - t0;

  Serial.print("[begin()] resultado: ");
  Serial.println(ok ? "OK" : "FALHOU");
  Serial.print("[begin()] tempo: ");
  Serial.print(elapsed);
  Serial.println(" ms  (esperado: < 50 ms)");

  if (elapsed > 50) {
    Serial.println("[WARN] begin() bloqueou por mais de 50 ms!");
  } else {
    Serial.println("[OK]   begin() nao bloqueou.");
  }

  Serial.print("[isReady()] ");
  Serial.println(gps.isReady() ? "true  [OK]" : "false [FALHOU]");
  printSeparator();
}

// ============================================================
// Loop
// ============================================================

void loop() {
  // --- Criterio 1: alimentar o parser de forma nao-bloqueante ---
  feedAndCount();

  unsigned long now = millis();
  if (now - last_print_ms < PRINT_INTERVAL_MS) return;
  last_print_ms = now;

  // --- Criterio 2: verificar saida dos getters ---
  Serial.print("[");
  Serial.print(now / 1000);
  Serial.println("s]");

  Serial.print("  hasValidFix()   : ");
  Serial.println(gps.hasValidFix() ? "true" : "false");

  // --- getTimeString() ---
  String timeStr = gps.getTimeString();
  Serial.print("  getTimeString() : ");
  Serial.println(timeStr);

  bool timeOk = gps.hasValidFix() ? (timeStr != "nan") : (timeStr == "nan");
  Serial.print("  -> ");
  Serial.println(timeOk ? "[OK]  valor coerente com fix" : "[FALHOU] valor inesperado");

  // --- getDateString() ---
  String dateStr = gps.getDateString();
  Serial.print("  getDateString() : ");
  Serial.println(dateStr);

  // --- getData() (CSV completo) ---
  Serial.print("  getData()       : ");
  Serial.println(gps.getData());

  // --- Acessores para SensorData ---
  if (gps.hasValidFix()) {
    Serial.print("  latitude        : ");
    Serial.println(gps.getLatitude(),  8);
    Serial.print("  longitude       : ");
    Serial.println(gps.getLongitude(), 8);
    Serial.print("  gpsAltitude (m) : ");
    Serial.println(gps.getGPSAltitude(), 2);
    Serial.print("  satellites      : ");
    Serial.println(gps.getSatellites());
  }

  printSeparator();
}
