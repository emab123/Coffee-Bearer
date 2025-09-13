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
#include "led_controller.h"
#include "logger.h"

enum RFIDResult {
    RFID_SUCCESS,
    RFID_ACCESS_DENIED,
    RFID_NO_CREDITS,
    RFID_SYSTEM_BUSY,
    RFID_NO_COFFEE,
    RFID_MASTER_KEY,
    RFID_ERROR
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
    
    // Referências para outros managers
    UserManager* userManager;
    CoffeeController* coffeeController;
    LEDController* ledController;
    Logger* logger;
    
    String readUID();
    bool isInCooldown();
    void startCooldown();
    RFIDResult processNormalUser(const String& uid);
    void processMasterKey();
    void handleRFIDResult(const String& uid, const String& userName, RFIDResult result);
    void playResultSound(RFIDResult result);
    void showResultLED(RFIDResult result);
    
public:
    RFIDManager();
    ~RFIDManager();
    
    // Inicialização
    bool begin();
    void end();
    void setManagers(UserManager* users, CoffeeController* coffee, LEDController* led, Logger* log);
    
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
    
    // Debug
    void printRFIDInfo();
};