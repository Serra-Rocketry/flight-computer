// ================================
// 🚀 FMS Completa - Com Reset Automático
// ================================

#include <math.h>

// Constantes de configuração
const float PARACHUTE_ALTITUDE = 100.0;
const float GRAVITY = 9.81;



// Estados da máquina
enum FlightState { 
  IDLE = 0, 
  ASCENT = 1, 
  DESCENT = 2, 
  LANDED = 3 
};

// Variáveis globais
FlightState current_state = IDLE;
float prev_altp = 0.0;
float prev_millis = 0.0;
float prev_filtered_ax = 0.0;
float prev_filtered_ay = 0.0;
float prev_filtered_az = 0.0;

bool first_reading = true;
float prev_vz = 0.0;

// Flags de eventos
bool liftoff_detected = false;
bool burnout_detected = false;
bool apogee_detected = false;
bool freefall_detected = false;
bool parachute_deployed = false;

// Controle de reset automático
unsigned long last_data_time = 0;
bool waiting_for_new_flight = false;
const unsigned long TIMEOUT_MS = 500;  // 5 segundos sem dados = reset

// Filtro
float prev_acc = 0.0;
const float ALPHA = 0.2;

// ================================
// Função de Reset do Sistema
// ================================
void reset_flight_state() {
  Serial.println("\n🔄 RESETANDO MÁQUINA DE ESTADOS PARA NOVO VOO\n");
  
  current_state = IDLE;
  prev_altp = 0.0;
  prev_millis = 0.0;
  prev_filtered_ax = 0.0;
  prev_filtered_ay = 0.0;
  prev_filtered_az = 0.0;
  first_reading = true;
  prev_vz = 0.0;
  liftoff_detected = false;
  burnout_detected = false;
  apogee_detected = false;
  freefall_detected = false;
  parachute_deployed = false;
  prev_acc = 0.0;
  waiting_for_new_flight = false;
  
}

// ================================
// Funções de detecção (mesmas do código anterior)
// ================================

float compute_total_acceleration(float ax, float ay, float az) {
  return sqrt(ax*ax + ay*ay + az*az);
}

float smooth(float value, float prev_value, float alpha) {
  if (prev_value == 0.0 && !first_reading) return value;
  return alpha * value + (1.0 - alpha) * prev_value;
}

bool detect_liftoff(float ax, float ay, float az, float height) {
  float total_acc = compute_total_acceleration(ax, ay, az);
  return (total_acc > 15.0);
}

bool detect_burnout(float ax, float ay, float az, float height, float vz) {
  float total_acc = compute_total_acceleration(ax, ay, az);
  
  if (height < 5.0 || vz <= 0.5) {
    return false;
  }
  
  return (az < -8.0 || total_acc < 2.0);
}

bool detect_apogee(float vz, float az) {
  return (abs(vz) < 1.0 && az < -0.1);
}

bool detect_freefall(float vz, float az, float height, float total_acc) {
  if (height < 5.0 || vz >= -5.0) {
    return false;
  }
  return (total_acc < 11.5);
}

bool detect_parachute(float height, float vz) {
  return (height <= PARACHUTE_ALTITUDE && vz < 0);
}

bool detect_landed(float vz, float height) {
  return (abs(vz) < 0.5 && height < 2.0);
}

// ================================
// Funções de impressão
// ================================

const char* state_name(FlightState state) {
  switch(state) {
    case IDLE: return "IDLE";
    case ASCENT: return "ASCENT";
    case DESCENT: return "DESCENT";
    case LANDED: return "LANDED";
    default: return "UNKNOWN";
  }
}

void print_event(const char* event, float t, float h, float vz, float ax, float ay, float az) {
  float total_acc = compute_total_acceleration(ax, ay, az);
  
  Serial.print(event);
  Serial.print(" @ ");
  Serial.print(t, 2);
  Serial.print("s | h=");
  Serial.print(h, 1);
  Serial.print(" | vz=");
  Serial.print(vz, 2);
  Serial.print(" | ax=");
  Serial.print(ax, 2);
  Serial.print(" | ay=");
  Serial.print(ay, 2);
  Serial.print(" | az=");
  Serial.print(az, 2);
  Serial.print(" | acc=");
  Serial.print(total_acc, 2);
  Serial.print(" | state=State.");
  Serial.println(state_name(current_state));
}

void print_status(float t, float h, float vz, float ax, float ay, float az) {
  float total_acc = compute_total_acceleration(ax, ay, az);
  
  Serial.print("📊 Status @ ");
  Serial.print(t, 2);
  Serial.print("s | h=");
  Serial.print(h, 1);
  Serial.print(" | vz=");
  Serial.print(vz, 2);
  Serial.print(" | acc=");
  Serial.print(total_acc, 2);
  Serial.print(" | state=State.");
  Serial.println(state_name(current_state));
}

// ================================
// Processamento principal
// ================================

void process_sensor_data(float millis, float altp, float ax, float ay, float az) {
  float vz = 0.0;
  float dt = 0.0;
  
  // Reset se houver gap de dados (nova execução)
  static float last_millis = -1;
  if (last_millis > 0 && millis < last_millis) {
    // Se o tempo reiniciou, é uma nova execução
    reset_flight_state();
  }
  last_millis = millis;
  
  // Calcular velocidade vertical
  if (!first_reading) {
    dt = millis - prev_millis;
    if (dt > 0) {
      vz = (altp - prev_altp) / dt;
      vz = constrain(vz, -200.0, 200.0);
    }
  } else {
    first_reading = false;
  }
  
  // Aplicar filtro
  float filtered_ax = smooth(ax, prev_filtered_ax, ALPHA);
  float filtered_ay = smooth(ay, prev_filtered_ay, ALPHA);
  float filtered_az = smooth(az, prev_filtered_az, ALPHA);
  prev_filtered_ax = filtered_ax;
  prev_filtered_ay = filtered_ay;
  prev_filtered_az = filtered_az;

  float total_acc = compute_total_acceleration(filtered_ax, filtered_ay, filtered_az);
  
  // ===== MÁQUINA DE ESTADOS =====
  
  switch(current_state) {
    case IDLE:
      if (!liftoff_detected && detect_liftoff(filtered_ax, filtered_ay, filtered_az, altp)) {
        current_state = ASCENT;
        liftoff_detected = true;
        print_event("🚀 LIFTOFF", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      break;
      
    case ASCENT:
      if (!burnout_detected && detect_burnout(filtered_ax, filtered_ay, filtered_az, altp, vz)) {
        burnout_detected = true;
        print_event("🔥 BURNOUT", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      
      if (!apogee_detected && detect_apogee(vz, filtered_az)) {
        current_state = DESCENT;
        apogee_detected = true;
        print_event("⛰️ APOGEE", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      break;
      
    case DESCENT:
      if (!freefall_detected && detect_freefall(vz, filtered_az, altp, total_acc)) {
        freefall_detected = true;
        print_event("🪂 FREEFALL", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      
      if (!parachute_deployed && detect_parachute(altp, vz)) {
        parachute_deployed = true;
        print_event("🪂 PARACHUTE DEPLOYED", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      
      if (detect_landed(vz, altp)) {
        current_state = LANDED;
        print_event("🏁 LANDED", millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
      }
      break;
      
    case LANDED:
      // Estado final, aguarda reset
      break;
  }
  
  // Status periódico
  static int counter = 0;
  counter++;
  if (counter >= 50 && current_state != LANDED) {
    counter = 0;
    print_status(millis, altp, vz, filtered_ax, filtered_ay, filtered_az);
  }
  
  // Atualizar variáveis
  prev_altp = altp;
  prev_millis = millis;
  prev_vz = vz;
  prev_acc = total_acc;
}

// ================================
// Setup e Loop
// ================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=========================================");
  Serial.println("🚀 FMS Completa - Ready to receive data");
  Serial.println("=========================================");
  Serial.println("Formato esperado: millis,altp,ax,ay,az");
  Serial.println("Sistema irá resetar automaticamente para cada novo voo");
  Serial.println("=========================================\n");
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();



    // Comando de reset
    if (data == "RESET") {
      reset_flight_state();
      Serial.println("✅ Sistema resetado para novo voo!");
      return;
    }

  
    
    // Parse CSV: millis,altp,ax,ay,az
    int i1 = data.indexOf(',');
    int i2 = data.indexOf(',', i1 + 1);
    int i3 = data.indexOf(',', i2 + 1);
    int i4 = data.indexOf(',', i3 + 1);
    
    if (i1 != -1 && i2 != -1 && i3 != -1 && i4 != -1) {
      float millis = data.substring(0, i1).toFloat();
      float altp   = data.substring(i1 + 1, i2).toFloat();
      float ax     = data.substring(i2 + 1, i3).toFloat();
      float ay     = data.substring(i3 + 1, i4).toFloat();
      float az     = data.substring(i4 + 1).toFloat();
      
      // Validar dados
      if (isnan(millis) || isnan(altp) || isnan(ax) || isnan(ay) || isnan(az)) {
        Serial.println("⚠️ Dados inválidos recebidos!");
        return;
      }
      
      process_sensor_data(millis, altp, ax, ay, az);
    }
  }
}
