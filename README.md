# Dispenser de Café Automatizado com RFID (v2.2)

![ESP32](https://img.shields.io/badge/ESP32-E23237?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)

## 📄 Resumo do Projeto

Este repositório contém o firmware e a documentação técnica para um protótipo funcional de um **Dispenser de Café Automatizado**. A solução utiliza um microcontrolador ESP32 e tecnologia RFID para controlar o acesso e a dispensação de café, resolvendo problemas de controle e desperdício em ambientes compartilhados.

O sistema é robusto, com persistência de dados na memória flash interna (NVS) e gerenciamento completo de usuários via linha de comando (Monitor Serial).

## ✨ Funcionalidades Principais

* ✅ **Controle de Acesso via RFID:** Liberação da dose de café apenas para usuários autorizados.
* 💾 **Persistência de Dados:** Usuários e estatísticas são salvos na memória flash do ESP32, sobrevivendo a reinicializações.
* 👨‍💼 **Gerenciamento de Usuários:** Adicione, remova e liste usuários facilmente.
* 🔊 **Feedback Audiovisual:** Um buzzer e um LED fornecem feedback instantâneo sobre as operações (acesso autorizado, negado, café servindo, etc.).
* 📊 **Estatísticas de Uso:** O sistema contabiliza o número total de cafés servidos.
* ⚙️ **Interface de Gestão via Linha de Comando:** Controle total do sistema através do Monitor Serial da IDE Arduino.

## 🏛️ Arquitetura do Sistema

O projeto é dividido em três camadas principais:

1.  **Camada de Hardware:** Componentes eletrônicos que interagem com o mundo físico (leitor RFID, bomba, relé).
2.  **Camada de Firmware:** O software embarcado no ESP32, escrito em C++, que contém toda a lógica operacional.
3.  **Camada de Gestão:** A interface de controle, atualmente implementada via Monitor Serial, com planos de evoluir para uma interface web.

## 🛠️ Hardware Utilizado

### Lista de Componentes

* Microcontrolador ESP32
* Módulo Leitor RFID-RC522 com tag ou cartão
* Módulo Relé de 1 Canal
* Buzzer Ativo
* LED de Status
* Bomba de líquidos 6V
* Fonte de alimentação externa 6V
* Resistor (para o LED, ex: 220Ω)
* Jumpers e protoboard

### Mapeamento de Pinos (Pinout)

| Componente          | Pino do Componente                               | Pino no ESP32                                              |
| ------------------- | ------------------------------------------------ | ---------------------------------------------------------- |
| **Módulo RFID-RC522** | `3.3V` / `RST` / `GND` / `MISO` / `MOSI` / `SCK` / `SDA(CS)` | `3V3` / `GPIO 4` / `GND` / `GPIO 19` / `GPIO 23` / `GPIO 18` / `GPIO 5` |
| **Módulo Relé** | `VCC` / `GND` / `IN`                               | `5V (VIN)` / `GND` / `GPIO 2`                                |
| **Buzzer Ativo** | Positivo (`+`) / Negativo (`-`)                    | `GPIO 15` / `GND`                                          |
| **LED de Status** | Positivo (`+`) / Negativo (`-`)                    | `GPIO 16` / `GND` (via resistor)                           |

> **Nota sobre a Bomba 6V:** A alimentação da bomba é interrompida pelo relé. Conecte o positivo da fonte 6V ao terminal `COM` do relé e o positivo da bomba ao terminal `NO` (Normalmente Aberto).

## 💾 Detalhes do Firmware

O firmware foi desenvolvido em **C++** utilizando a **IDE Arduino**.

### Bibliotecas Essenciais

* `<SPI.h>` e `<MFRC522.h>`: Para comunicação com o leitor RFID.
* `<Preferences.h>`: Para salvar e carregar dados na memória flash NVS (Non-Volatile Storage) do ESP32.

### Lógica de Operação

1.  **`setup()`**: Ao iniciar, o ESP32 inicializa os componentes de hardware (Serial, RFID, pinos) e carrega os usuários e estatísticas salvos da memória flash.
2.  **`loop()`**: O sistema entra em um ciclo contínuo, monitorando duas coisas:
    * O leitor RFID, para detectar a aproximação de uma tag.
    * A porta serial, para receber comandos de gerenciamento.

## 💻 Como Utilizar (Gestão via Monitor Serial)

Abra o Monitor Serial na IDE Arduino com a velocidade (`baud rate`) de **115200**. Use os seguintes comandos para gerenciar a cafeteira:

| Comando               | Descrição                                         | Exemplo                                   |
| --------------------- | ------------------------------------------------- | ----------------------------------------- |
| `HELP`                | Mostra a lista de todos os comandos disponíveis.    | `HELP`                                    |
| `ADD <UID> <NOME>`    | Adiciona um novo usuário ao sistema.              | `ADD 1A 2B 3C 4D JOAO SILVA`              |
| `RM <UID>`            | Remove um usuário pelo seu UID.                   | `RM 1A 2B 3C 4D`                          |
| `LIST`                | Lista todos os usuários cadastrados.              | `LIST`                                    |
| `STATS`               | Exibe as estatísticas de uso do sistema.          | `STATS`                                   |
| `SERVIR`              | Serve um café manualmente, sem precisar de tag.   | `SERVIR`                                  |
| `CLEAR`               | **CUIDADO:** Apaga todos os dados (pede confirmação). | `CLEAR`                                   |

## 🗺️ Roadmap do Projeto

-   [x] **Fase 1: Prova de Conceito (Hardware):** ✅ CONCLUÍDA
-   [x] **Fase 2: Gestão de Utilizadores e Persistência de Dados:** ✅ CONCLUÍDA
-   [ ] **Fase 3: Interface Web de Gestão:** 📝 PRÓXIMOS PASSOS

## 👥 Autores

* **Artur Gemaque & EMAB**

---
*Relatório base datado de 04 de setembro de 2025.*