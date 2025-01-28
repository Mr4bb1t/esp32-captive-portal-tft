# ESP32 Captive Portal TFT

Este repositório contém um firmware para ESP32 que implementa um **captive portal** com suporte para uma tela TFT. O objetivo é fornecer uma interface visual interativa para conexão Wi-Fi e outras funcionalidades.

## Recursos
- **Captive Portal** para simulção de ataque Phishing.
- **Suporte a telas TFT** para exibição de informações.
- **Interface interativa** com botões físicos
- **Servidor Web integrado** para configuração via navegador.
- **Spiffs** para armazenamento de credenciais e exibição direta na tela.

## Hardware Necessário
- ESP32
- Tela TFT compatível
- Componentes adicionais (como botões, fios, solda, etc.)

## Instalação
### Requisitos
- **Arduino IDE** ou **PlatformIO**
- **Bibliotecas Necessárias:**
  - `TFT_eSPI`
  - `WiFiManager`
  - `ESPAsyncWebServer`
  - `SPIFFS.h`
  - `WiFi.h`
  - `DNSServer.h`

### Passos
1. Clone este repositório:
   ```sh
   git clone https://github.com/Mr4bb1t/esp32-captive-portal-tft.git
   ```
2. Abra o código na Arduino IDE ou no PlatformIO.
3. Instale as bibliotecas listadas acima.
4. Compile e envie para o ESP32.

## Estrutura do Repositório
```
/
├── src/                # Código-fonte principal
│   ├── main.cpp        # Arquivo principal do firmware
│   ├── display.cpp     # Funções para controle da tela TFT
│   ├── wifi.cpp        # Gerenciamento da conexão Wi-Fi
│   ├── captive_portal/ # Arquivos do portal cativo
├── lib/                # Bibliotecas externas (se necessário)
├── data/               # Arquivos de SPIFFS ou LittleFS (HTML, CSS, JS)
├── include/            # Headers (.h)
├── .gitignore          # Arquivos a serem ignorados pelo Git
├── README.md           # Este arquivo
└── LICENSE             # Licença do projeto
```

## Como Contribuir
1. Fork o repositório
2. Crie uma branch com sua feature: `git checkout -b minha-feature`
3. Commit suas alterações: `git commit -m 'Adiciona nova funcionalidade'`
4. Push para sua branch: `git push origin minha-feature`
5. Abra um Pull Request

## Licença
Este projeto está sob a licença MIT. Veja o arquivo `LICENSE` para mais detalhes.

## Contato
Se tiver alguma dúvida ou sugestão, entre em contato pelo GitHub ou abra uma issue.

