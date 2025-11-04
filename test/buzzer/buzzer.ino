#define BUZZER_PIN 10 // Pino do buzzer

// Sinalização com o buzzer
void buzzSignal(String signal)
{
    if (signal == "Alerta") // Alerta de erro em alguma configuração
    {
        for (int i = 0; i < 5; i++)
        {
            tone(BUZZER_PIN, 1000, 200);
            delay(200 + 150);
        }
    }
    else if (signal == "Sucesso") // Sinal de sucesso na configuração
    {
        for (int i = 0; i < 3; i++)
        {
            tone(BUZZER_PIN, 1000, 100);
            delay(100 + 100);
        }
    }
    else if (signal == "Ativado")
    {
        tone(BUZZER_PIN, 1000, 500);
    }
    else if (signal == "Beep") // Beep de funcionamento padrão
    {
        tone(BUZZER_PIN, 1000, 50);
    }
    else
    {
        Serial.println("Sinal inválido!");
    }
}

void setup()
{
    pinMode(BUZZER_PIN, OUTPUT);
    Serial.begin(9600);
}

void loop()
{
    Serial.println("Alerta");
    buzzSignal("Alerta");
    delay(1000);
    Serial.println("Sucesso");
    buzzSignal("Sucesso");
    delay(1000);
    Serial.println("Ativado");
    buzzSignal("Ativado");
    delay(1000);
    Serial.println("Beep");
    buzzSignal("Beep");
    delay(1000);
}
