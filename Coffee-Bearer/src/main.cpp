/*
==================================================
SISTEMA CAFETEIRA RFID - v4.0 REFATORADO
Controle Inteligente com Interface Web Separada
Admin/Usuário, Autenticação e LED Neopixel
==================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Inclusão dos módulos customizados
#include "config.h"
#include "RFID_manager.h"
#include "led_controller.h"
#include "auth_manager.h"
#include "logger.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "web_server.h"


// Instâncias globais
AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);

// Managers
UserManager userManager;
CoffeeController coffeeController;
AuthManager authManager;
Logger logger;
WebServerManager webServer(authManager, logger, userManager, coffeeController);
RFIDManager rfidManager;
LEDController ledController;

// Variáveis de controle
unsigned long lastResetCheck = 0;
unsigned long lastStatusUpdate = 0;
bool systemInitialized = false;

void initializeSystem() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println(F("\n=================================================="));
    Serial.println(F("     SISTEMA CAFETEIRA RFID v4.0 - INICIANDO     "));
    Serial.println(F("=================================================="));
    
    // Inicializar SPIFFS primeiro
    if (!SPIFFS.begin(true)) {
        Serial.println(F("ERRO FATAL: Falha ao montar SPIFFS"));
        while(1) {
            ledController.update();
            delay(100);
        }
    }
    
    // Inicializar componentes
    ledController.begin();
    
    logger.begin();
    logger.info("Sistema iniciando...");
    
    authManager.begin();
    rfidManager.begin();
    userManager.begin();
    coffeeController.begin();
    
    // Conectar WiFi
    connectWiFi();
    
    // Inicializar servidor web
    webServer.begin();
    
    // Configurar NTP
    timeClient.begin();
    timeClient.update();
    
    systemInitialized = true;
    ledController.showStatusOK();
    
    logger.info("Sistema iniciado com sucesso");
    Serial.print(F("Sistema pronto! Acesse: http://"));
    Serial.println(WiFi.localIP());
    Serial.println(F("Digite 'help' para comandos disponíveis"));
    Serial.println(F("=================================================="));
}

void connectWiFi() {
    
    Serial.printf("Conectando ao WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        ledController.update();
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\nWiFi conectado!"));
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        logger.info("WiFi conectado - IP: " + WiFi.localIP().toString());
    } else {
        Serial.println(F("\nFalha na conexão WiFi!"));
        logger.error("Falha na conexão WiFi");
        ledController.showStatusEmpty();
    }
}

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    String originalCmd = cmd;
    cmd.toLowerCase();
    
    if (cmd == "help") {
        Serial.println(F("\n========== COMANDOS DISPONÍVEIS =========="));
        Serial.println(F("Sistema:"));
        Serial.println(F("  help              - Mostra este menu"));
        Serial.println(F("  status            - Status do sistema"));
        Serial.println(F("  restart           - Reinicia o sistema"));
        Serial.println(F("  factory           - Reset de fábrica"));
        Serial.println(F(""));
        Serial.println(F("Usuários:"));
        Serial.println(F("  add <uid> <nome>  - Adiciona usuário"));
        Serial.println(F("  remove <uid>      - Remove usuário"));
        Serial.println(F("  list              - Lista usuários"));
        Serial.println(F("  credits <uid>     - Mostra créditos"));
        Serial.println(F(""));
        Serial.println(F("Café:"));
        Serial.println(F("  serve             - Serve café manual"));
        Serial.println(F("  refill            - Reabastece garrafa"));
        Serial.println(F("  stats             - Estatísticas"));
        Serial.println(F(""));
        Serial.println(F("Logs:"));
        Serial.println(F("  logs              - Mostra logs"));
        Serial.println(F("  clearlogs         - Limpa logs"));
        Serial.println(F("==========================================\n"));
    }
    else if (cmd == "status") {
        Serial.printf("\n=== STATUS DO SISTEMA ===\n");
        Serial.printf("WiFi: %s (IP: %s)\n", 
            WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado",
            WiFi.localIP().toString().c_str());
        Serial.printf("Usuários: %d/%d\n", userManager.getTotalUsers(), MAX_USERS);
        Serial.printf("Cafés servidos: %d\n", coffeeController.getTotalServed());
        Serial.printf("Cafés restantes: %d/%d\n", 
            coffeeController.getRemainingCoffees(), MAX_COFFEES);
        Serial.printf("Sistema ocupado: %s\n", 
            coffeeController.isBusy() ? "Sim" : "Não");
        Serial.printf("Uptime: %lu ms\n", millis());
        Serial.println("========================\n");
    }
    else if (cmd.startsWith("add ")) {
        handleAddUserCommand(originalCmd);
    }
    else if (cmd.startsWith("remove ")) {
        String uid = originalCmd.substring(7);
        uid.trim();
        uid.toUpperCase();
        if (userManager.removeUser(uid)) {
            Serial.println("Usuário removido com sucesso!");
            logger.info("Usuário removido via serial: " + uid);
        } else {
            Serial.println("Usuário não encontrado!");
        }
    }
    else if (cmd == "list") {
        userManager.printUserList();
    }
    else if (cmd == "serve") {
        if (coffeeController.serveCoffee("MANUAL", nullptr)) {
            Serial.println("Café servido manualmente!");
        } else {
            Serial.println("Não foi possível servir café!");
        }
    }
    else if (cmd == "refill") {
        coffeeController.refillContainer();
        Serial.println("Garrafa reabastecida!");
        logger.info("Garrafa reabastecida via serial");
    }
    else if (cmd == "stats") {
        coffeeController.printStats();
    }
    else if (cmd == "logs") {
        logger.printLogs();
    }
    else if (cmd == "clearlogs") {
        logger.clearLogs();
        Serial.println("Logs limpos!");
    }
    else if (cmd == "restart") {
        Serial.println("Reiniciando sistema...");
        logger.info("Sistema reiniciado via serial");
        delay(1000);
        ESP.restart();
    }
    else if (cmd == "factory") {
        Serial.println("ATENÇÃO: Reset de fábrica! Digite 'CONFIRMAR' para continuar:");
        while (!Serial.available()) delay(10);
        String confirm = Serial.readStringUntil('\n');
        confirm.trim();
        if (confirm == "CONFIRMAR") {
            performFactoryReset();
        } else {
            Serial.println("Operação cancelada.");
        }
    }
    else if (cmd.length() > 0) {
        Serial.println("Comando desconhecido. Digite 'help' para ver comandos disponíveis.");
    }
}

void handleAddUserCommand(String originalCmd) {
    int firstSpace = originalCmd.indexOf(' ');
    if (firstSpace == -1) {
        Serial.println("Formato: add <uid> <nome>");
        return;
    }
    
    String remainder = originalCmd.substring(firstSpace + 1);
    int secondSpace = remainder.indexOf(' ');
    
    if (secondSpace == -1) {
        Serial.println("Formato: add <uid> <nome>");
        return;
    }
    
    String uid = remainder.substring(0, secondSpace);
    String name = remainder.substring(secondSpace + 1);
    
    uid.trim();
    uid.toUpperCase();
    name.trim();
    
    if (uid.length() > 0 && name.length() > 0) {
        if (userManager.addUser(uid, name)) {
            Serial.printf("Usuário '%s' adicionado com sucesso!\n", name.c_str());
            logger.info("Usuário adicionado via serial: " + name + " (UID: " + uid + ")");
        } else {
            Serial.println("Falha ao adicionar usuário!");
        }
    } else {
        Serial.println("UID e nome não podem estar vazios!");
    }
}

void performFactoryReset() {
    Serial.println("Executando reset de fábrica...");
    ledController.signalServing();
    
    // Limpar dados dos usuários
    userManager.clearAllData();
    
    // Limpar dados do controlador de café
    coffeeController.clearAllData();
    
    // Limpar logs
    logger.clearLogs();
    
    // Limpar autenticação
    authManager.resetToDefault();
    
    logger.info("Reset de fábrica executado");
    Serial.println("Reset de fábrica concluído. Reiniciando...");
    
    delay(2000);
    ESP.restart();
}

void checkWeeklyReset() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < WEEKLY_RESET_CHECK_INTERVAL) {
        return;
    }
    
    lastCheck = millis();
    
    if (userManager.shouldPerformWeeklyReset()) {
        Serial.println("Executando reset semanal de créditos...");
        userManager.performWeeklyReset();
        logger.info("Reset semanal de créditos executado");
        ledController.signalServing();
        delay(2000);
        ledController.showStatusOK();
    }
}

void updateSystemStatus() {
    // Esta função define a cor de fundo se NENHUMA animação estiver acontecendo.
    // A prioridade é mostrar se o café está vazio
    if (coffeeController.isEmpty()) {
        ledController.showStatusEmpty();
    } 
    // Depois, se está acabando (ex: menos de 5 cafés)
    else if (coffeeController.getRemainingCoffees() < 5) {
        ledController.showStatusLow();
    }
    // Se não, tudo OK
    else {
        ledController.showStatusOK();
    }
}

void handleWiFiReconnection() {
    static unsigned long lastReconnectAttempt = 0;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectAttempt > 30000) { // Tenta reconectar a cada 30s
            Serial.println("WiFi desconectado. Tentando reconectar...");
            ledController.showStatusInitializing();
            WiFi.reconnect();
            lastReconnectAttempt = millis();
        }
    }
}

void setup() {
    initializeSystem();
}

void loop() {
    if (!systemInitialized) {
        delay(100);
        return;
    }
    
    // Processar comandos seriais
    processSerialCommands();
    
    // Processar RFID
    rfidManager.loop();
    
    // Verificar reset semanal
    checkWeeklyReset();
    
    // Atualizar status do sistema
    updateSystemStatus();

    // Gerenciar reconexão WiFi
    handleWiFiReconnection();
    
    ledController.update(); 

    // Atualizar NTP periodicamente
    static unsigned long lastNTPUpdate = 0;
    if (millis() - lastNTPUpdate > 3600000) { // A cada hora
        timeClient.update();
        lastNTPUpdate = millis();
    }
    
}