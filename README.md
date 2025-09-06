# Dispenser de Café Automatizado com RFID

![ESP32](https://img.shields.io/badge/ESP32-E23237?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)

## Visão Geral

O "Coffee-Bearer" é um sistema inteligente de gerenciamento de cafeteira, projetado para controlar o acesso e o consumo de café por meio de tags RFID. Esta versão (v3.0) introduz uma interface web completa, permitindo o gerenciamento de usuários e o monitoramento do sistema diretamente de um navegador, além de manter a operação via comandos seriais para depuração.

O projeto é construído sobre a plataforma ESP32, utilizando o framework Arduino e PlatformIO para gerenciamento de dependências e compilação.

## ✨ Funcionalidades Principais

* **Autenticação por RFID:** Acesso rápido e seguro para servir café. Apenas usuários com tags RFID cadastradas podem utilizar a máquina.
* **Interface Web Completa:** Um servidor web assíncrono embarcado no ESP32 fornece uma interface de usuário moderna e responsiva para:
    * Visualizar o status do sistema em tempo real (Ativo/Ocupado).
    * Adicionar e remover usuários associando um nome a um UID de uma tag RFID.
    * Listar todos os usuários cadastrados no sistema.
    * Monitorar estatísticas, como o total de usuários e o número de cafés servidos.
    * Acionar o serviço de café manualmente.
    * Realizar backups da lista de usuários e limpar todos os dados do sistema de forma segura.
* **Persistência de Dados:** Utiliza a memória flash do ESP32 (através da biblioteca `Preferences`) para salvar a lista de usuários e as estatísticas, garantindo que os dados não sejam perdidos ao reiniciar o dispositivo.
* **Feedback Sonoro:** Um buzzer integrado fornece feedback audível para diferentes ações, como acesso autorizado, acesso negado, inicialização do sistema e café pronto.
* **Controle via Serial:** Mantém a compatibilidade com comandos via monitor serial para depuração e gerenciamento rápido sem a necessidade de acesso à rede.

## 🛠️ Hardware Necessário

* **Placa de Desenvolvimento:** ESP32 (o projeto está configurado para uma `upesy_wrover`).
* **Leitor RFID:** MFRC522 para leitura das tags.
* **Atuador:** Módulo Relé para acionar a bomba ou mecanismo da cafeteira.
* **Feedback:** Buzzer para os sinais sonoros.
* **Tags/Cartões RFID:** Para os usuários.

### Mapeamento de Pinos

Conforme definido em `src/main.cpp`:
* **RST_PIN (Reset do RFID):** `GPIO 4`
* **SS_PIN (Slave Select do RFID):** `GPIO 5`
* **BUZZER_PIN:** `GPIO 15`
* **RELAY_PIN:** `GPIO 13`

## 📚 Software e Dependências

Este projeto é gerenciado com o [PlatformIO](https://platformio.org/). As bibliotecas necessárias estão listadas no arquivo `platformio.ini` e são gerenciadas automaticamente:

* `miguelbalboa/MFRC522`: Para comunicação com o leitor RFID.
* `esphome/ESPAsyncWebServer-esphome`: Para criar o servidor web assíncrono.
* `bblanchon/ArduinoJson`: Para manipulação eficiente de dados JSON na comunicação entre o ESP32 e a interface web.



## 📡 API Endpoints

O sistema expõe uma API REST para ser consumida pela interface web.

| Método | Endpoint             | Descrição                                                                        |
| :----- | :------------------- | :------------------------------------------------------------------------------- |
| `GET`  | `/`                  | Serve a página principal `index.html`.                                             |
| `GET`  | `/style.css`         | Serve o arquivo de estilos CSS.                                                  |
| `GET`  | `/script.js`         | Serve o arquivo JavaScript da aplicação.                                         |
| `GET`  | `/api/status`        | Retorna o status atual do sistema, incluindo contadores e o último evento.         |
| `GET`  | `/api/usuarios`      | Retorna uma lista de todos os usuários cadastrados com seus nomes e UIDs.          |
| `POST` | `/api/usuarios`      | Adiciona um novo usuário. Requer um corpo JSON com `uid` e `nome`.                 |
| `DELETE`| `/api/usuarios`     | Remove um usuário existente. Requer um corpo JSON com o `uid` do usuário a ser removido. |
| `POST` | `/api/servir-cafe`   | Aciona o relé para servir um café manualmente.                                     |
| `DELETE`| `/api/limpar-dados`  | Apaga permanentemente todos os usuários e estatísticas da memória.               |
| `GET`  | `/api/backup`        | Retorna os dados dos usuários em um formato JSON para backup.                    |



## ⌨️ Comandos via Serial

Para fins de depuração e gerenciamento, os seguintes comandos estão disponíveis através do monitor serial (baud rate: 115200):

* `HELP`: Exibe a lista de todos os comandos disponíveis.
* `ADD <UID> <NOME>`: Adiciona um novo usuário. Ex: `ADD A1 B2 C3 D4 João Silva`.
* `RM <UID>`: Remove um usuário pelo seu UID.
* `LIST`: Lista todos os usuários cadastrados.
* `CLEAR`: Inicia o processo para apagar todos os dados (requer confirmação).
* `STATS`: Mostra as estatísticas atuais do sistema.
* `SERVIR`: Serve um café manualmente, como se fosse acionado pela interface web.



## 🚀 Como Começar

1.  **Clone o Repositório:**
    ```bash
    git clone <URL_DO_REPOSITORIO>
    ```
2.  **Abra no PlatformIO:** Abra a pasta do projeto em um editor com a extensão do PlatformIO (ex: VS Code).
3.  **Configure o Wi-Fi:** Edite o arquivo `src/main.cpp` e insira as credenciais da sua rede Wi-Fi nas variáveis `ssid` e `password`.
4.  **Compile e Envie:** Use as funções do PlatformIO para compilar e enviar o firmware para a sua placa ESP32.
5.  **Acesse a Interface:** Após o upload, abra o Monitor Serial para ver o endereço IP atribuído ao dispositivo. Acesse esse IP em um navegador na mesma rede para utilizar a interface web.

## 🗺️ Roadmap de Futuras Melhorias

* **Sistema de Log:** Implementar um registro detalhado de cada uso, armazenando data, hora e usuário.
* **Sistema de Créditos:** Atribuir um limite semanal de cafés por usuário.
* **Controle de Nível da Garrafa:** Monitorar a quantidade de cafés restantes e usar uma "chave mestra" para reabastecer o contador.
* **Reset Semanal de Créditos:** Automatizar o processo de renovação dos créditos de todos os usuários a cada semana.
* **Interface de Usuário Aprimorada:** Criar uma área para que os usuários possam consultar seus créditos restantes e histórico de consumo.
