#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#define FILESYSTEM LittleFS

const char *ssid = "File Browser";
const char *password = "12345678";

WebServer server(80);

String formatBytes(size_t bytes)
{
  if (bytes < 1024)
    return String(bytes) + " B";
  else if (bytes < 1024 * 1024)
    return String(bytes / 1024.0, 2) + " KB";
  else
    return String(bytes / 1024.0 / 1024.0, 2) + " MB";
}

String getContentType(String filename, bool download = false)
{
  if (download)
    return "application/octet-stream";
  else if (filename.endsWith(".htm") || filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".pdf"))
    return "application/pdf";
  else if (filename.endsWith(".zip"))
    return "application/zip";
  return "text/plain";
}

// Página HTML gerada dinamicamente
String generateFileManagerPage()
{
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Gerenciador de Arquivos</title>
      <style>
        body { font-family: Arial; background: #1e1e1e; color: #fff; padding: 20px; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { border: 1px solid #444; padding: 10px; text-align: left; }
        a, button { color: #61dafb; text-decoration: none; padding: 4px 8px; }
        button { background: none; border: 1px solid #61dafb; cursor: pointer; }
        button:hover, a:hover { background-color: #333; }
      </style>
    </head>
    <body>
      <h1>Gerenciador de Arquivos (LittleFS)</h1>
      <table>
        <tr><th>Nome</th><th>Tamanho</th><th>Ações</th></tr>
  )rawliteral";

  File root = FILESYSTEM.open("/");
  File file = root.openNextFile();
  while (file)
  {
    if (!file.isDirectory())
    {
      String filename = String(file.name());
      String size = formatBytes(file.size());
      html += "<tr><td>" + filename + "</td><td>" + size + "</td><td>";
      html += "<a href=\"" + filename + "\" target=\"_blank\">Abrir</a> ";
      html += "<a href=\"" + filename + "?download=1\">Baixar</a> ";
      html += "<button onclick=\"deleteFile(encodeURIComponent('" + filename + "'))\">Excluir</button>";
      html += "</td></tr>";
    }
    file = root.openNextFile();
  }

  html += R"rawliteral(
      </table>
      <script>
        function deleteFile(filename) {
          if (confirm("Deseja realmente excluir " + filename + "?")) {
            fetch('/delete?file=' + filename).then(res => {
              if (res.ok) location.reload();
              else alert("Erro ao excluir.");
            });
          }
        }
      </script>
    </body>
    </html>
  )rawliteral";

  return html;
}

// Lê o arquivo do LittleFS
bool handleFileRead(String path, bool download = false)
{
  if (path.endsWith("/"))
    path += "index.html";
  if (!FILESYSTEM.exists(path))
    return false;
  File file = FILESYSTEM.open(path, "r");
  server.streamFile(file, getContentType(path, download));
  file.close();
  return true;
}

// Rota principal
void handleRoot()
{
  String page = generateFileManagerPage();
  server.send(200, "text/html", page);
}

// Deleta o arquivo
void handleDeleteFile()
{
  if (!server.hasArg("file"))
  {
    server.send(400, "text/plain", "Faltando argumento 'file'");
    return;
  }
  String path = server.arg("file");
  path = "/" + urlDecode(path); // Ensure path starts with '/'
  if (FILESYSTEM.exists(path))
  {
    FILESYSTEM.remove(path);
    server.send(200, "text/plain", "Arquivo excluído");
  }
  else
  {
    server.send(404, "text/plain", "Arquivo não encontrado: " + path);
  }
}

// Baixar arquivos via ?download=1
void handleDownloadOrOpen()
{
  String path = server.uri();
  bool download = server.hasArg("download");
  if (!handleFileRead(path, download))
  {
    server.send(404, "text/plain", "Arquivo não encontrado");
  }
}

String urlDecode(String input)
{
  String decoded = "";
  char c;
  for (int i = 0; i < input.length(); i++)
  {
    if (input[i] == '%')
    {
      int code;
      sscanf(input.substring(i + 1, i + 3).c_str(), "%x", &code);
      c = static_cast<char>(code);
      decoded += c;
      i += 2;
    }
    else if (input[i] == '+')
    {
      decoded += ' ';
    }
    else
    {
      decoded += input[i];
    }
  }
  return decoded;
}

void setup()
{
  Serial.begin(115200);
  delay(5000);
  FILESYSTEM.begin();

  WiFi.softAP(ssid, password);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/delete", HTTP_GET, handleDeleteFile);
  server.onNotFound(handleDownloadOrOpen);

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop()
{
  server.handleClient();
}
