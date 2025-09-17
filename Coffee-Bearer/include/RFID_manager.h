/*
==================================================
GERENCIADOR RFID
Leitura e processamento de tags RFID
==================================================
*/

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "beeps_and_bleeps.h"
#include "logger.h"
#include "web_server.h"

class WebServerManager;

enum RFIDResult {
    RFID_SUCCESS,
    RFID_ACCESS_DENIED,
    RFID_NO_CREDITS,
    RFID_SYSTEM_BUSY,
    RFID_NO_COFFEE,
    RFID_MASTER_KEY,
    RFID_ERROR
};

enum ScanMode {
    SCAN_NORMAL,
    SCAN_FOR_ADD
};

struct RFIDEvent {
    String uid;
    String userName;
    RFIDResult result;
    unsigned long timestamp;
    int creditsRemaining;
};

class RFIDManager {
private:
    MFRC522* mfrc522;
    String lastUID;
    unsigned long lastReadTime;
    unsigned long cooldownEndTime;
    bool initialized;
    
    // --- References to other managers ---
    UserManager& userManager;
    CoffeeController& coffeeController;
    FeedbackManager& feedbackManager;
    Logger& logger;
    WebServerManager& webServer;
    ScanMode currentMode;
    
    String readUID();
    bool isInCooldown();
    void startCooldown();
    RFIDResult processNormalUser(const String& uid);
    void processMasterKey();
    void handleRFIDResult(const String& uid, const String& userName, RFIDResult result);
    
public:
    RFIDManager(UserManager& users, CoffeeController& coffee, Logger& log, FeedbackManager& feedback, WebServerManager& web);
    ~RFIDManager();
    
    // Inicialização
    bool begin();
    void end();
    void setManagers(UserManager* users, CoffeeController* coffee, Logger* log);
    
    // Loop principal (deve ser chamado no loop principal)
    void loop();
    
    // Configurações
    void setCooldownTime(unsigned long timeMs);
    unsigned long getCooldownTime();
    bool isReady();
    
    // Status
    String getLastUID() { return lastUID; }
    unsigned long getLastReadTime() { return lastReadTime; }
    unsigned long getRemainingCooldown();
    
    // Utilitários
    bool testRFID();
    String formatUID(const String& uid);
    bool isValidUID(const String& uid);
    void setScanMode(ScanMode mode);
    
    // Debug
    void printRFIDInfo();
};