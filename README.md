# Coffee Bearer - Dispenser de Café Automatizado com RFID

![ESP32](https://img.shields.io/badge/ESP32-E23237?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-FF7F00?style=for-the-badge&logo=platformio&logoColor=white)

## Visão Geral

O "Coffee Bearer" é um sistema inteligente de gerenciamento para cafeteiras, projetado para controlar o acesso e o consumo de café por meio de tags RFID. Esta versão (v3.4) é uma solução completa que inclui um sistema de créditos, controle de nível da garrafa, reset semanal de créditos e uma interface web moderna para gerenciamento total do sistema.

Construído sobre a plataforma ESP32, o projeto utiliza o framework Arduino e é gerenciado pelo PlatformIO, garantindo um desenvolvimento robusto e organizado.

<p align="center">
  <img src="Landing Page.png" alt="Lading Page" width="700"/>
</p>

##  Funcionalidades Principais

* **Autenticação por RFID:** Acesso rápido e seguro para servir café. Apenas usuários com tags RFID cadastradas podem utilizar a máquina.
* **Sistema de Créditos:** Cada usuário possui um número de créditos (`CREDITOS_INICIAIS`) que são debitados a cada uso.
* **Reset Semanal de Créditos:** Os créditos de todos os usuários são restaurados para o valor inicial automaticamente a cada 7 dias, garantindo o uso justo do equipamento.
* **Controle de Nível e Tag Mestra:** O sistema monitora a quantidade de cafés restantes. Uma tag RFID especial (`UID_MESTRE`) é usada para "reabastecer" o contador de cafés da garrafa.
* **Interface Web Completa:** Um servidor web assíncrono embarcado no ESP32 fornece uma interface de usuário moderna e responsiva para:
    * Visualizar o status do sistema em tempo real (Ativo/Ocupado, cafés restantes, último evento).
    * Adicionar e remover usuários associando um nome a um UID de uma tag RFID.
    * Listar todos os usuários cadastrados e seus créditos restantes.
    * Visualizar logs detalhados de eventos do sistema com data e hora (via NTP).
    * Acionar o serviço de café manualmente.
    * Realizar backups da lista de usuários e limpar todos os dados do sistema de forma segura.
* **Persistência de Dados:** Utiliza a memória flash do ESP32 (através da biblioteca `Preferences`) para salvar a lista de usuários, estatísticas e o estado do sistema, garantindo que os dados não sejam perdidos ao reiniciar.
* **Registro de Logs (Logging):** Todos os eventos importantes (cafés servidos, usuários adicionados/removidos, resets) são registrados em um arquivo `datalog.txt` na memória SPIFFS com data e hora sincronizadas via NTP.
* **Feedback Sonoro:** Um buzzer integrado fornece feedback audível para diferentes ações: acesso autorizado, negado, sistema inicializado, café pronto, sem café na garrafa e reabastecimento.
* **Controle via Serial:** Mantém a compatibilidade com comandos via monitor serial para depuração e gerenciamento rápido.

## Hardware Necessário

* **Placa de Desenvolvimento:** ESP32 (o projeto está configurado para uma `upesy_wrover`).
* **Leitor RFID:** MFRC522 para leitura das tags.
* **Atuador:** Módulo Relé para acionar a bomba ou mecanismo da cafeteira.
* **Feedback:** Buzzer para os sinais sonoros.
* **Tags/Cartões RFID:** Para os usuários e para a Tag Mestra.

### Mapeamento de Pinos

Conforme definido em `src/main.cpp`:

| Componente | Pino no ESP32 |
| :--- | :--- |
| **MFRC522 RST** | GPIO 4 |
| **MFRC522 SS (SDA)** | GPIO 5 |
| **BUZZER** | GPIO 15 |
| **RELÉ** | GPIO 13 |
| **MFRC522 MOSI** | GPIO 23 |
| **MFRC522 MISO** | GPIO 19 |
| **MFRC522 SCK** | GPIO 18 |

## Software e Dependências

Este projeto é gerenciado com o [PlatformIO](https://platformio.org/). As bibliotecas necessárias estão listadas no arquivo `platformio.ini` e são instaladas automaticamente:

* `miguelbalboa/MFRC522`: Para comunicação com o leitor RFID.
* `esphome/ESPAsyncWebServer-esphome`: Para criar o servidor web assíncrono.
* `bblanchon/ArduinoJson`: Para manipulação eficiente de dados JSON na API.
* `arduino-libraries/NTPClient`: Para sincronização do relógio via internet.

## 📡 API Endpoints

O sistema expõe uma API REST para ser consumida pela interface web.

| Método | Endpoint | Descrição |
| :--- | :--- | :--- |
| `GET` | `/` | Serve a página principal `index.html`. |
| `GET` | `/style.css` | Serve o arquivo de estilos CSS. |
| `GET` | `/script.js` | Serve o arquivo JavaScript da aplicação. |
| `GET` | `/api/status` | Retorna o status atual do sistema (contadores, último evento, etc.). |
| `GET` | `/api/usuarios` | Retorna uma lista de todos os usuários cadastrados com seus dados. |
| `POST` | `/api/usuarios` | Adiciona um novo usuário. Requer um corpo JSON com `uid` e `nome`. |
| `DELETE`| `/api/usuarios` | Remove um usuário. Requer um corpo JSON com o `uid`. |
| `POST` | `/api/servir-cafe` | Aciona o relé para servir um café manualmente. |
| `DELETE`| `/api/limpar-dados` | Apaga permanentemente todos os usuários e estatísticas da memória. |
| `GET` | `/api/backup` | Retorna os dados dos usuários em um formato JSON para backup. |
| `GET` | `/api/logs` | Retorna o conteúdo do arquivo de log de eventos do sistema. |

## Comandos via Serial

Para fins de depuração e gerenciamento, os seguintes comandos estão disponíveis através do monitor serial (baud rate: 115200):

* `help`: Exibe a lista de todos os comandos disponíveis.
* `add <uid> <nome>`: Adiciona um novo usuário. Ex: `add DC 11 C7 B2 João Silva`.
* `rm <uid>`: Remove um usuário pelo seu UID.
* `list`: Lista todos os usuários cadastrados.
* `clear`: Inicia o processo para apagar todos os dados (requer confirmação).
* `stats`: Mostra as estatísticas atuais do sistema.
* `servir`: Serve um café manualmente.
* `logs`: Exibe o log completo de eventos.
* `reset`: Força o reset de créditos para todos os usuários.

## 🚀 Como Começar

1.  **Clone o Repositório:**
    ```bash
    git clone https://github.com/ArturGRS/Coffee-Bearer
    ```

2.  **Abra no PlatformIO:** Abra a pasta do projeto no VS Code com a extensão do PlatformIO instalada.

3.  **Configure as Credenciais:**
    * Na pasta `include/`, renomeie o arquivo `credentials.h.example` para `credentials.h`.
    * Abra o novo arquivo `credentials.h` e preencha suas credenciais de Wi-Fi e o UID da sua Tag Mestra.
    * Você também pode ajustar outras configurações do sistema, como `MAX_USUARIOS` e `CREDITOS_INICIAIS`, neste mesmo arquivo.

4.  **Compile e Envie:** Use os comandos da barra de status do PlatformIO:
    * `PlatformIO: Build` para compilar o projeto.
    * `PlatformIO: Upload` para enviar o firmware para o ESP32.
    * **`PlatformIO: Upload File System image`** para enviar os arquivos da interface web (`data/`) para a memória do ESP32. **Este passo é essencial!**

5.  **Acesse a Interface:** Após o upload, abra o Monitor Serial (`115200`) para ver o endereço IP atribuído ao dispositivo. Acesse esse IP em um navegador na mesma rede para utilizar a interface web.
