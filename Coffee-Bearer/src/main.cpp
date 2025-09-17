#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

#include "config.h"
#include "RFID_manager.h"
#include "beeps_and_bleeps.h"
#include "auth_manager.h"
#include "logger.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "web_server.h"

// --- Central Application Context ---
struct AppContext {
    AsyncWebServer server;
    WiFiUDP ntpUDP;
    NTPClient timeClient;

    FeedbackManager feedbackManager;
    UserManager userManager;
    AuthManager authManager;
    Logger logger;
    CoffeeController coffeeController;
    WebServerManager webServer;
    RFIDManager rfidManager;

    AppContext() : 
        server(80),
        timeClient(ntpUDP, NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC),
        coffeeController(feedbackManager), 
        webServer(server, authManager, logger, userManager, coffeeController, feedbackManager),
        rfidManager(userManager, coffeeController, logger, feedbackManager, webServer)
    {
        webServer.setRfidManager(&rfidManager);
    }
};

// Create a single global instance of the AppContext
AppContext app;

// --- Global Variables (minimal) ---
bool systemInitialized = false;

// --- Function Prototypes ---
void connectWiFi();
void initializeSystem();
void processSerialCommands();
void updateSystemStatus();
void handleWiFiReconnection();
void checkWeeklyReset();

void connectWiFi() {
    Serial.printf("Conectando ao WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        app.feedbackManager.showStatusInitializing(); 
        app.feedbackManager.update();
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\nWiFi conectado!"));
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        app.logger.info("WiFi conectado - IP: " + WiFi.localIP().toString());

        if (MDNS.begin(MDNS_HOSTNAME)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("Serviço mDNS iniciado. Acesse em: http://%s.local\n", MDNS_HOSTNAME);
            app.logger.info("mDNS iniciado: http://" + String(MDNS_HOSTNAME) + ".local");
        } else {
            Serial.println("Erro ao iniciar mDNS!");
            app.logger.error("Falha ao iniciar mDNS");
        }
    } else {
        Serial.println(F("\nFalha na conexão WiFi!"));
        app.logger.error("Falha na conexão WiFi");
        app.feedbackManager.showStatusError();
    }
}

void initializeSystem() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println(F("\n=================================================="));
    Serial.println(F("     SISTEMA CAFETEIRA RFID v4.0 - INICIANDO     "));
    Serial.println(F("=================================================="));
    
    app.feedbackManager.begin();

    if (!SPIFFS.begin(true)) {
        Serial.println(F("ERRO FATAL: Falha ao montar SPIFFS"));
        while(1) {
            app.feedbackManager.signalError();
            delay(100);
        }
    }
    
    app.logger.begin();
    app.logger.info("Sistema iniciando...");
    
    app.authManager.begin();
    app.userManager.begin();
    app.coffeeController.begin();
    app.rfidManager.begin();
    
    connectWiFi();
    app.webServer.begin(); // Web server now uses the server instance from the context
    app.timeClient.begin();
    app.timeClient.update();
    
    systemInitialized = true;
    app.feedbackManager.showStatusReady();
    
    app.logger.info("Sistema iniciado com sucesso");
    Serial.println("==================================================");
    Serial.println(F("Sistema pronto! Acesse com um dos endereços abaixo:"));
    Serial.printf("   - http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   - http://%s.local\n", MDNS_HOSTNAME);
    Serial.println(F("=================================================="));
}

void performFactoryReset() {
    Serial.println("Executando reset de fábrica...");
    app.feedbackManager.showStatusBusy();
    app.userManager.clearAllData();
    app.coffeeController.clearAllData();
    app.logger.clearLogs();
    app.authManager.resetToDefault();
    app.logger.info("Reset de fábrica executado");
    Serial.println("Reset de fábrica concluído. Reiniciando...");
    
    delay(2000);
    ESP.restart();
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
        if (app.userManager.addUser(uid, name)) {
            Serial.printf("Usuário '%s' adicionado com sucesso!\n", name.c_str());
            app.logger.info("Usuário adicionado via serial: " + name + " (UID: " + uid + ")");
        } else {
            Serial.println("Falha ao adicionar usuário!");
        }
    } else {
        Serial.println("UID e nome não podem estar vazios!");
    }
}

void processSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    String originalCmd = cmd;
    cmd.toLowerCase();

    if (cmd == "list") {
        app.userManager.printUserList();
    }
    
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
        Serial.printf("Usuários: %d/%d\n", app.userManager.getTotalUsers(), MAX_USERS);
        Serial.printf("Cafés servidos: %d\n", app.coffeeController.getTotalServed());
        Serial.printf("Cafés restantes: %d/%d\n", 
            app.coffeeController.getRemainingCoffees(), MAX_COFFEES);
        Serial.printf("Sistema ocupado: %s\n", 
            app.coffeeController.isBusy() ? "Sim" : "Não");
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
        if (app.userManager.removeUser(uid)) {
            Serial.println("Usuário removido com sucesso!");
            app.logger.info("Usuário removido via serial: " + uid);
        } else {
            Serial.println("Usuário não encontrado!");
        }
    }
    else if (cmd == "list") {
        app.userManager.printUserList();
    }
    else if (cmd == "serve") {
        if (app.coffeeController.serveCoffee("MANUAL", nullptr)) {
            Serial.println("Café servido manualmente!");
        } else {
            Serial.println("Não foi possível servir café!");
        }
    }
    else if (cmd == "refill") {
        app.coffeeController.refillContainer();
        Serial.println("Garrafa reabastecida!");
        app.logger.info("Garrafa reabastecida via serial");
    }
    else if (cmd == "stats") {
        app.coffeeController.printStats();
    }
    else if (cmd == "logs") {
        app.logger.printLogs();
    }
    else if (cmd == "clearlogs") {
        app.logger.clearLogs();
        Serial.println("Logs limpos!");
    }
    else if (cmd == "restart") {
        Serial.println("Reiniciando sistema...");
        app.logger.info("Sistema reiniciado via serial");
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


void checkWeeklyReset() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < WEEKLY_RESET_CHECK_INTERVAL) {
        return;
    }
    
    lastCheck = millis();
    
    if (app.userManager.shouldPerformWeeklyReset()) {
        Serial.println("Executando reset semanal de créditos...");
        app.userManager.performWeeklyReset();
        app.logger.info("Reset semanal de créditos executado");
        app.feedbackManager.signalServing();
        delay(2000);
        app.feedbackManager.showStatusReady();
    }
}

void updateSystemStatus() {
    if (app.coffeeController.isEmpty()) {
        app.feedbackManager.showStatusEmpty();
    } else if (app.coffeeController.getRemainingCoffees() < 5) {
        app.feedbackManager.showStatusLow();
    } else {
        app.feedbackManager.showStatusReady();
    }
}

void handleWiFiReconnection() {
    static unsigned long lastReconnectAttempt = 0;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectAttempt > 30000) { // Tenta reconectar a cada 30s
            Serial.println("WiFi desconectado. Tentando reconectar...");
            app.feedbackManager.showStatusInitializing();
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
    app.rfidManager.loop();
    
    // Verificar reset semanal
    checkWeeklyReset();
    
    // Atualizar status do sistema
    updateSystemStatus();

    // Gerenciar reconexão WiFi
    handleWiFiReconnection();
    
    app.feedbackManager.update(); 

    // Atualizar NTP periodicamente
    static unsigned long lastNTPUpdate = 0;
    if (millis() - lastNTPUpdate > 3600000) { // A cada hora
        app.timeClient.update();
        lastNTPUpdate = millis();
    }
    
}