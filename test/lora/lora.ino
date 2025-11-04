#include <LoRa.h>
#include <SPI.h>

#define LORA_FREQ 868E6

#define SS 7
#define RST 1
#define DIO0 2

int counter = 0;

void setup() {
  Serial.begin(115200);

  if (!setupLoRa())
  {
    Serial.println("Erro na configuração!");
    delay(3000);
    ESP.restart();
  }

}

void loop() {
  sendLoRa(String(counter));
  counter++;
  delay(5000);
}

bool setupLoRa()
{
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("Falha ao inicializar o LoRa!");
    return false;
  }
  LoRa.setSyncWord(0xF3);
  return true;
}

void sendLoRa(const String &message)
{
  LoRa.beginPacket();
  LoRa.print(message);
  if (LoRa.endPacket())
  {
    Serial.println("Mensagem LoRa enviada.");
  }
  else
  {
    Serial.println("ERRO ao enviar mensagem LoRa!");
  }
}