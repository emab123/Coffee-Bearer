#include "RFID_manager.h"
#include "web_server.h"

// --- Constructor ---
RFIDManager::RFIDManager(UserManager& users, CoffeeController& coffee, Logger& log, FeedbackManager& feedback, WebServerManager& web) : 
    mfrc522(nullptr),
    userManager(users),
    coffeeController(coffee),
    logger(log),
    feedbackManager(feedback),
    webServer(&web), // Store the web server reference
    lastUID(""),
    lastReadTime(0),
    cooldownEndTime(0),
    initialized(false),
    currentMode(SCAN_NORMAL)
{}

RFIDManager::~RFIDManager() {
    end();
}

bool RFIDManager::begin() {
    if (initialized) {
        return true;
    }
    
    SPI.begin();
    mfrc522 = new MFRC522(RFID_SS_PIN, RFID_RST_PIN);
    
    if (!mfrc522) {
        DEBUG_PRINTLN("ERRO: Falha ao criar instância MFRC522");
        return false;
    }
    
    mfrc522->PCD_Init();
    
    if (!testRFID()) {
        DEBUG_PRINTLN("ERRO: Falha na comunicação com leitor RFID");
        delete mfrc522;
        mfrc522 = nullptr;
        return false;
    }
    
    initialized = true;
    DEBUG_PRINTLN("RFID Manager inicializado com sucesso");
    printRFIDInfo();
    
    return true;
}

void RFIDManager::end() {
    if (mfrc522) {
        delete mfrc522;
        mfrc522 = nullptr;
    }
    initialized = false;
}

void RFIDManager::loop() {
    // Exit immediately if not initialized or if no new card is present
    if (!initialized || !mfrc522 || !mfrc522->PICC_IsNewCardPresent()) {
        return;
    }

    // Try to read the card. If it fails, exit.
    if (!mfrc522->PICC_ReadCardSerial()) {
        return;
    }

    // --- Card has been successfully read, now process it ---

    // First, check if we are in a cooldown period to prevent rapid reads
    if (isInCooldown()) {
        mfrc522->PICC_HaltA(); // Halt communication to release the card
        return;
    }

    String uid = readUID();
    if (uid.isEmpty()) {
        mfrc522->PICC_HaltA();
        return;
    }
    
    lastUID = uid;
    lastReadTime = millis();
    DEBUG_PRINTF("Tag RFID detectada: %s\n", uid.c_str());

    // Handle scan-to-add mode for the web UI
    if (currentMode == SCAN_FOR_ADD) {
        if (!userManager.userExists(uid)) {
            DEBUG_PRINTF("Novo UID capturado para adicionar: %s\n", uid.c_str());
            if (webServer) { // Check if the pointer is valid
                webServer->pushScannedUID(uid); 
            }
        } else {
            DEBUG_PRINTLN("Cartão já cadastrado, ignorando.");
            feedbackManager.signalError();
        }
        currentMode = SCAN_NORMAL;
    }

    // Handle normal operation
    else {
        RFIDResult result;
        String userName = "";
        
        if (uid.equalsIgnoreCase(MASTER_UID)) {
            result = RFID_MASTER_KEY;
            userName = "MASTER";
            processMasterKey();
        } else {
            userName = userManager.getUserName(uid);
            result = processNormalUser(uid);
            if (userName.isEmpty()) {
                userName = "DESCONHECIDO";
            }
        }
        handleRFIDResult(uid, userName, result);
    }
    
    startCooldown();
    mfrc522->PICC_HaltA();
    mfrc522->PCD_StopCrypto1();
}

void RFIDManager::setScanMode(ScanMode mode) {
    this->currentMode = mode;
    if (mode == SCAN_FOR_ADD) {
        DEBUG_PRINTLN("RFID Manager: Modo de leitura para adicionar usuário ativado.");
    } else {
        DEBUG_PRINTLN("RFID Manager: Modo de leitura normal ativado.");
    }
}

// --- Private Methods (Unchanged) ---

RFIDResult RFIDManager::processNormalUser(const String& uid) {
    if (coffeeController.isBusy()) return RFID_SYSTEM_BUSY;
    if (coffeeController.isEmpty()) return RFID_NO_COFFEE;
    
    UserCredits* user = userManager.getUserByUID(uid);
    if (!user) return RFID_ACCESS_DENIED;
    if (user->credits <= 0) return RFID_NO_CREDITS;
    
    if (coffeeController.serveCoffee(user->name, &user->credits)) {
        userManager.updateLastUsed(uid);
        logger.logRFIDEvent(uid, user->name, "CAFE_SERVIDO", true);
        return RFID_SUCCESS;
    } else {
        logger.logRFIDEvent(uid, user->name, "FALHA_SERVIR", false);
        return RFID_ERROR;
    }
}

void RFIDManager::processMasterKey() {
    DEBUG_PRINTLN("CHAVE MESTRA DETECTADA!");
    coffeeController.refillContainer();
    logger.logRFIDEvent(MASTER_UID, "MASTER", "REABASTECIMENTO", true);
}

void RFIDManager::handleRFIDResult(const String& uid, const String& userName, RFIDResult result) {
    switch (result) {
        case RFID_SUCCESS:
            // The success signal is handled by CoffeeController upon completion
            break;
        case RFID_MASTER_KEY:
            feedbackManager.signalMasterKey();
            break;
        case RFID_ACCESS_DENIED:
            feedbackManager.signalUnknownUser();
            break;
        case RFID_NO_CREDITS:
            feedbackManager.signalNoCredits();
            break;
        default:
            feedbackManager.signalError();
            break;
    }
}

// --- Helper and Debug Methods (Unchanged) ---

String RFIDManager::readUID() {
    String uid = "";
    for (byte i = 0; i < mfrc522->uid.size; i++) {
        if (i > 0) uid += " ";
        if (mfrc522->uid.uidByte[i] < 0x10) uid += "0";
        uid += String(mfrc522->uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

bool RFIDManager::isInCooldown() {
    return millis() < cooldownEndTime;
}

void RFIDManager::startCooldown() {
    cooldownEndTime = millis() + COOLDOWN_TIME_MS;
}

bool RFIDManager::testRFID() {
    if (!mfrc522) return false;
    byte version = mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    return !(version == 0x00 || version == 0xFF);
}

void RFIDManager::printRFIDInfo() {
    if (!mfrc522) {
        DEBUG_PRINTLN("RFID não inicializado");
        return;
    }
    DEBUG_PRINTLN("\n=== INFORMAÇÕES DO LEITOR RFID ===");
    byte version = mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    DEBUG_PRINTF("Versão do chip: 0x%02X\n", version);
    
    switch (version) {
        case 0x91: DEBUG_PRINTLN("Chip: MFRC522 v1.0"); break;
        case 0x92: DEBUG_PRINTLN("Chip: MFRC522 v2.0"); break;
        default: DEBUG_PRINTLN("Chip: Desconhecido"); break;
    }
    
    DEBUG_PRINTF("Pino SS: %d\n", RFID_SS_PIN);
    DEBUG_PRINTF("Pino RST: %d\n", RFID_RST_PIN);
    DEBUG_PRINTF("Cooldown: %lu ms\n", COOLDOWN_TIME_MS);
    DEBUG_PRINTF("Master UID: %s\n", MASTER_UID);
    DEBUG_PRINTLN("=================================\n");
}