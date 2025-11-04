#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

// Credenciais de acesso à rede.
const char *ssid = "Servidor ESP32";
const char *password = "12345678";

// Instancia um servidor http que escutará na porta 80.
AsyncWebServer server(80);

void setServerRoutes() {
  // Rotas básicas de acesso ao site para o usuário
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/style.css", "text/css");
  });

  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.js", "application/javascript");
  });

  // Rotas API que permitem que o front-end tenha acesso aos dados do back-end.
  // Não é exatamente RESTful por limitações de capacidades da lib, de recursos
  // do ESP32 e de minha própria habilidade lol.

  // Essa rota captura todos os arquivos dispostos na raiz do Sistema de
  // Arquivos e retorna alguns de seus dados. É usado na homepage para popular a
  // tabela.
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument fsFiles; // Cria um objeto JSON.
    File root = LittleFS.open("/"); // Abre o Sistema de Arquivos na Raiz.

    // Para todos os arquivos nesse diretório:
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        // Adicione seu nome e tamanho no objeto JSON.
        JsonObject fsFile = fsFiles.add<JsonObject>();
        fsFile["name"] = String(file.name());
        fsFile["size"] = file.size();
      }
      file = root.openNextFile();
    }
    // Por fim, transforme o objeto JSON em sua representação texto puro e o
    // envie para o cliente.
    char jsonString[8000] = { 0 };
    serializeJson(fsFiles, jsonString);
    request->send(200, "application/json; charset=utf-8", jsonString);
  });

  // Essa rota captura um arquivo específico na raiz do Sistema de Arquivos e o
  // retorna para o cliente. Consumida pelo front-end quando os links de abrir e
  // baixar arquivos são selecionados.
  server.on("/api/file", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Só retormamos com sucesso se o cliente requisitou um arquivo.
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Parâmetro de URL \
          <filename> faltando.");
      return;
    }

    // Só retormamos com sucesso se arquivo requisitado existe no Sistema de
    // Arquivos.
    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();
    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "Arquivo <" + filename + 
          "> não encontrado no Sistema de Arquivos");
      return;
    }

    // Retornamos o arquivo para download ou visualização simples dependendo se
    // o cliente enviou também o parâmetro de URL <download>. (Deve ter uma
    // solução mais elegante pra isso).
    if (request->hasParam("download")) {
      request->send(LittleFS, "/" + filename, String(), true);
    } else {
      request->send(LittleFS, "/" + filename, "text/plain; charset=utf-8");
    }
  });

  // Muito semelhante a rota anterior, só que agora que verbo HTTP mudou de GET
  // para DELETE nós deletamos o arquivo no Sistema de Arquivos ao invés de
  // retorná-lo pro cliente. Consumida pelo front-end quando o cliente seleciona
  // o botão de deletar arquivos.
  server.on("/api/file", HTTP_DELETE, [](AsyncWebServerRequest* request) {
    Serial.println(request->method());
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Parâmetro de URL \
          <filename> faltando.");
      return;
    }

    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();

    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "Erro ao tentar deletar \
          arquivo <" + filename + "> inexistente.");
      return;
    }

    if (LittleFS.remove("/" + filename)) {
      request->send(200, "text/plain; charset=utf-8", "Arquivo deletado com \
          sucesso.");
    } else {
      // A essa altura nada deve dar de errado, mas por via das dúvidas...
      request->send(400, "text/plain; charset=utf-8", "Erro desconhecido ao \
          tentar deletar arquivo <" + filename + ">");
    }
  });
}

void setup() {
  // Inicializa comunicação serial.
  Serial.begin(115200);

  // Inicializa do Sistema de Arquivos Interno.
  if (!LittleFS.begin()) {
    Serial.println("Um erro ocorreu ao montar LittleFS");
    return;
  }

  // Cria ponto de acesso wireless.
  WiFi.softAP(ssid, password);
  Serial.println("Criando ponto de acesso WiFi..");
  Serial.println(WiFi.softAPIP());

  // Configura as rotas do servidor e o inicializa.
  setServerRoutes();
  server.begin();
}

void loop() {
}
