// Bibliotecas para conexão WiFi e Firebase
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Biblioteca para obter o tempo
#include "time.h"

// Parâmetros e biblioteca para conexão com o GPS
#include <TinyGPS++.h>
#define RXD2 16
#define TXD2 17
HardwareSerial neogps(1);
TinyGPSPlus gps;
float lati, lon;
int sat;

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Parâmetros para conexão WiFi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Firebase API Key, chave de um projeto no Firebase
#define API_KEY ""

// Credenciais de conexão Firebase
#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "admin1"

// URL do projeto Firebase
#define DATABASE_URL ""

// Define os objetos no Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variável que salva o USER UID
String uid;

// ID do dispositivo
String ID = String("A002");

// Caminhos para os dados no Firebase-> Main e Child
// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String latPath = "/latitude";
String lngPath = "/longitude";
String satPath = "/satelites";
String timePath = "/timestamp";
// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;

FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

// Variáveis de intervalos da leitura
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 60000;

// Built in LED
int LED = 2;

// Função para conectar ao WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Função que obtem o horário
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return (0);
  }
  time(&now);
  return now;
}

void setup() {
  // Inicia o serial, o GPS e o LED
  Serial.begin(115200);
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(LED, OUTPUT);

  // Inicia o WiFi
  initWiFi();

  // Configura o horário
  configTime(0, 0, ntpServer);

  // Configura o api key
  config.api_key = API_KEY;

  // Configura as credenciais de usuário
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Configura o RTDB URL
  config.database_url = DATABASE_URL;

  // Conecta ao Firebase
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Configura o máximo de tentativas de conexão
  config.max_token_generation_retry = 5;

  // Inicializa o Firebase com as credenciais
  Firebase.begin(&config, &auth);

  // Obtem o UID do usuário
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Imprime o user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Configura o caminho database com o user UID
  databasePath = "/Data/" + ID;
}

void loop() {
  // Verifica se a leitura do sensor está ativa
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
      {
        newData = true;
      }
    }
  }
  //Se newData é verdadeiro
  if (newData == true)
  {
    newData = false;
    // Se a leitura é válida
    if (gps.location.isValid() == 1)
    {
      sat = (gps.satellites.value());
      lati = (gps.location.lat());
      lon = (gps.location.lng());

      // Envia novas leituras ao database
      if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
        sendDataPrevMillis = millis();

        digitalWrite (LED, HIGH);
        delay(1000);
        digitalWrite (LED, LOW);

        // Obtem o horário atual
        timestamp = getTime();
        Serial.print ("Horário: ");
        Serial.println (timestamp);

        parentPath = databasePath + "/" + String(timestamp);

        json.set(latPath.c_str(), String(lati, 6));
        json.set(lngPath.c_str(), String(lon, 6));
        json.set(satPath.c_str(), String(sat));
        json.set(timePath, String(timestamp));
        Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
      }
    }
  }
}
