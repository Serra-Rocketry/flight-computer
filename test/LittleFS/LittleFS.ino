// Inclusão de bibliotecas
#include "FS.h"
#include "LittleFS.h"

// Definições de pinos e constantes
#define INTERVAL 200

// Variáveis globais
unsigned long previous_millis = 0;
String file_name = "Dados.csv"; // Nome do arquivo para salvar os dados
String file_dir = "";

// Setup da memória LittleFS
bool setupLittleFS()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("Erro ao montar LittleFS.");
        return false;
    }
    return true; // Retorna true se tudo ocorreu bem
}

// Registra e imprime os dados do momento
void logData(unsigned long current_millis)
{
    String data_string = String(current_millis) + ",-22.286898,-42.542294,8,861.80,2025/5/15,12:6:9,0.36,927.76,0.60,-0.01,9.03,-0.01,-0.02,0.02"; // String com os dados atuais
    appendFile(file_dir, data_string);
}

// Escreve os dados no arquivo - escrita
bool writeFile(const String &path, const String &data_string)
{
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) // Se houver falha ao abrir o arquivo
    {
        Serial.println("Falha ao abrir arquivo para gravação.");
        return false;
    }
    if (file.println(data_string)) // Se a escrita no arquivo for bem-sucedida
    {
        Serial.println("Arquivo escrito.");
    }
    else // Se houver falha na escrita
    {
        Serial.println("Falha na gravação do arquivo.");
        file.close();
        return false;
    }
    file.close();
    return true; // Retorna true se tudo ocorreu bem
}

// Escreve os dados no arquivo - anexação
void appendFile(const String &path, const String &message)
{
    File file = LittleFS.open(path, FILE_APPEND);
    if (!file) // Se houver falha ao abrir o arquivo
    {
        Serial.println("Falha ao abrir arquivo para anexar.");
    }
    if (file.print(message + "\n")) // Se a escrita no arquivo for bem-sucedida
    {
        Serial.println("Mensagem anexada.");
    }
    else // Se houver falha na escrita
    {
        Serial.println("Falha ao anexar mensagem.");
        file.close();
    }
    file.close();
}

void setup()
{
    Serial.begin(115200);
    delay(10000); // Aguarda a inicialização do Serial
    Serial.println("Iniciando...");

    file_dir = "/" + file_name; // Diretório do arquivo de dados
    Serial.print("Salvando dados em: ");
    Serial.println(file_dir);
    String data_header = "millis,lat,lon,sat,alt,data,hora,altp,p,ax,ay,az,gx,gy,gz"; // Cabeçalho do arquivo
    if (!(setupLittleFS() && writeFile(file_dir, data_header)))                  // Inicia a memória interna
    {
        Serial.println("Erro no sistema de arquivos!");
        delay(3000);
        ESP.restart();
    }
}

void loop()
{
    unsigned long current_millis = millis();
    if (current_millis - previous_millis >= INTERVAL) // A cada 100ms
    {
        logData(current_millis);
        previous_millis = current_millis;
    }
}
