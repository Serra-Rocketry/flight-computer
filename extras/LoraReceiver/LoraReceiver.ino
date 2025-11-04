#include <LoRa.h>
#include <SPI.h>

#define LORA_FREQ 868E6
#define SS_LORA 7
#define RST_LORA 1
#define DIO0_LORA 2
#define SYNC_WORD 0xF3
#define BUZZER_PIN 0

String LoRaData;
int lastRSSI = 0;

void setup()
{
  Serial.begin(115200);

  if (!setupLoRa())
  {
    Serial.println("Erro na configuração!");
    buzzSignal("Alerta");
    delay(3000);
    ESP.restart();
  }

  Serial.println("LoRa iniciado com sucesso.");
  buzzSignal("Sucesso");
}

void loop()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    LoRaData = "";
    while (LoRa.available())
    {
      LoRaData += (char)LoRa.read();
    }
    LoRaData.trim();
    String msg = LoRaData + "," + String(LoRa.packetRssi());
    Serial.println(msg);

    int rssi = LoRa.packetRssi();
    if (abs(rssi - lastRSSI) > 3)
    {
      buzzSignal("Beep");
      lastRSSI = rssi;
    }
  }
  delay(50);
}

bool setupLoRa()
{
  LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);
  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("Falha ao inicializar o LoRa.");
    return false;
  }
  LoRa.setSyncWord(SYNC_WORD);
  return true;
}

void buzzSignal(String signal)
{
  int frequency = 500;
  if (signal == "Alerta")
  {
    for (int i = 0; i < 5; i++)
    {
      tone(BUZZER_PIN, frequency, 200);
      delay(350);
    }
  }
  else if (signal == "Sucesso")
  {
    for (int i = 0; i < 3; i++)
    {
      tone(BUZZER_PIN, frequency, 100);
      delay(200);
    }
  }
  else if (signal == "Beep")
  {
    tone(BUZZER_PIN, frequency, 50);
  }
}
