#include "RFID_manager.h"

extern UserManager userManager;
extern CoffeeController coffeeController;
extern LEDController ledController;
extern Logger logger;

RFIDManager::RFIDManager() :
    mfrc522(nullptr),
    lastUID(""),
    lastReadTime(0),
    cooldownEndTime(0),
    initialized(false),
    userManager(nullptr),
    coffeeController(nullptr),
    ledController(nullptr),
    logger(nullptr) {
}

RFIDManager::~RFIDManager() {
    end();
}

bool RFIDManager::begin() {
    if (initialized) {
        end();
    }
    
    // Configurar pinos SPI
    SPI.begin();
    
    // Inicializar MFRC522
    mfrc522 = new MFRC522(RFID_SS_PIN, RFID_RST_PIN);
    
    if (!mfrc522) {
        DEBUG_PRINTLN("ERRO: Falha ao criar instância MFRC522");
        return false;
    }
    
    mfrc522->PCD_Init();
    
    // Testar comunicação com o leitor
    if (!testRFID()) {
        DEBUG_PRINTLN("ERRO: Falha na comunicação com leitor RFID");
        delete mfrc522;
        mfrc522 = nullptr;
        return false;
    }
    
    // Configurar referências para outros managers
    this->userManager = &::userManager;
    this->coffeeController = &::coffeeController;
    this->ledController = &::ledController;
    this->logger = &::logger;
    
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
    DEBUG_PRINTLN("RFID Manager finalizado");
}

void RFIDManager::setManagers(UserManager* users, CoffeeController* coffee, LEDController* led, Logger* log) {
    this->userManager = users;
    this->coffeeController = coffee;
    this->ledController = led;
    this->logger = log;
}

void RFIDManager::loop() {
    if (!initialized || !mfrc522) {
        return;
    }
    
    // Verificar se está no período de cooldown
    if (isInCooldown()) {
        return;
    }
    
    // Verificar se há uma nova tag presente
    if (!mfrc522->PICC_IsNewCardPresent()) {
        return;
    }
    
    // Tentar ler a tag
    if (!mfrc522->PICC_ReadCardSerial()) {
        DEBUG_PRINTLN("Falha ao ler cartão RFID");
        return;
    }
    
    // Extrair UID da tag
    String uid = readUID();
    
    if (uid.length() == 0) {
        DEBUG_PRINTLN("UID inválido lido");
        mfrc522->PICC_HaltA();
        return;
    }
    
    // Evitar leituras repetidas da mesma tag
    if (uid == lastUID && (millis() - lastReadTime) < 2000) {
        mfrc522->PICC_HaltA();
        return;
    }
    
    DEBUG_PRINTF("Tag RFID detectada: %s\n", uid.c_str());
    
    // Atualizar informações da última leitura
    lastUID = uid;
    lastReadTime = millis();
    
    RFIDResult result;
    String userName = "";
    
    // Verificar se é a chave mestra
    if (uid.equalsIgnoreCase(MASTER_UID)) {
        result = RFID_MASTER_KEY;
        userName = "MASTER";
        processMasterKey();
    } else {
        // Processar usuário normal
        result = processNormalUser(uid);
        userName = userManager->getUserName(uid);
        if (userName.length() == 0) {
            userName = "DESCONHECIDO";
        }
    }
    
    // Tratar o resultado
    handleRFIDResult(uid, userName, result);
    
    // Iniciar cooldown para evitar leituras repetidas
    startCooldown();
    
    // Parar comunicação com a tag
    mfrc522->PICC_HaltA();
    mfrc522->PCD_StopCrypto1();
}

void RFIDManager::setCooldownTime(unsigned long timeMs) {
    // Para esta implementação, usamos a constante COOLDOWN_TIME_MS
    // Uma implementação mais avançada poderia armazenar em uma variável
    DEBUG_PRINTF("Cooldown configurado: %lu ms\n", timeMs);
}

unsigned long RFIDManager::getCooldownTime() {
    return COOLDOWN_TIME_MS;
}

bool RFIDManager::isReady() {
    return initialized && !isInCooldown() && coffeeController && !coffeeController->isBusy();
}

unsigned long RFIDManager::getRemainingCooldown() {
    if (!isInCooldown()) {
        return 0;
    }
    return cooldownEndTime - millis();
}

bool RFIDManager::testRFID() {
    if (!mfrc522) {
        return false;
    }
    
    // Testar comunicação básica
    byte version = mfrc522->PCD_ReadRegister(MFRC522::VersionReg);
    
    // Versões conhecidas do MFRC522: 0x91, 0x92
    if (version == 0x00 || version == 0xFF) {
        DEBUG_PRINTF("RFID: Comunicação falhou (version: 0x%02X)\n", version);
        return false;
    }
    
    DEBUG_PRINTF("RFID: Comunicação OK (version: 0x%02X)\n", version);
    return true;
}

String RFIDManager::formatUID(const String& uid) {
    String formatted = uid;
    formatted.toUpperCase();
    formatted.trim();
    return formatted;
}

bool RFIDManager::isValidUID(const String& uid) {
    return userManager ? userManager->isValidUID(uid) : false;
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
    DEBUG_PRINTF("Status: %s\n", isReady() ? "Pronto" : "Não Pronto");
    
    DEBUG_PRINTLN("=================================\n");
}

// Métodos privados

String RFIDManager::readUID() {
    if (!mfrc522) {
        return "";
    }
    
    String uid = "";
    
    for (byte i = 0; i < mfrc522->uid.size; i++) {
        if (i > 0) uid += " ";
        
        if (mfrc522->uid.uidByte[i] < 0x10) {
            uid += "0";
        }
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

RFIDResult RFIDManager::processNormalUser(const String& uid) {
    if (!userManager || !coffeeController) {
        return RFID_ERROR;
    }
    
    // Verificar se o sistema está ocupado
    if (coffeeController->isBusy()) {
        DEBUG_PRINTLN("Sistema ocupado");
        return RFID_SYSTEM_BUSY;
    }
    
    // Verificar se há café disponível
    if (coffeeController->isEmpty()) {
        DEBUG_PRINTLN("Sem café disponível");
        return RFID_NO_COFFEE;
    }
    
    // Verificar se o usuário existe
    UserCredits* user = userManager->getUserByUID(uid);
    if (!user) {
        DEBUG_PRINTF("Usuário não encontrado: %s\n", uid.c_str());
        return RFID_ACCESS_DENIED;
    }
    
    // Verificar se o usuário tem créditos
    if (user->credits <= 0) {
        DEBUG_PRINTF("Usuário sem créditos: %s\n", user->name.c_str());
        return RFID_NO_CREDITS;
    }
    
    // Tentar servir café
    if (coffeeController->serveCoffee(user->name, &user->credits)) {
        // Atualizar dados do usuário
        userManager->updateLastUsed(uid);
        
        DEBUG_PRINTF("Café servido para %s (créditos restantes: %d)\n", 
                    user->name.c_str(), user->credits);
        
        // Log do evento
        if (logger) {
            logger->logRFIDEvent(uid, user->name, "CAFE_SERVIDO", true);
            logger->logCoffeeServed(user->name, coffeeController->getRemainingCoffees());
        }
        
        return RFID_SUCCESS;
    } else {
        DEBUG_PRINTF("Falha ao servir café para %s\n", user->name.c_str());
        
        if (logger) {
            logger->logRFIDEvent(uid, user->name, "FALHA_SERVIR", false);
        }
        
        return RFID_ERROR;
    }
}

void RFIDManager::processMasterKey() {
    DEBUG_PRINTLN("CHAVE MESTRA DETECTADA!");
    
    if (!coffeeController) {
        return;
    }
    
    // Reabastecer a cafeteira
    coffeeController->refillContainer();
    
    // Log do evento
    if (logger) {
        logger->logRFIDEvent(MASTER_UID, "MASTER", "REABASTECIMENTO", true);
        logger->logSystemEvent("Reabastecimento via chave mestra");
    }
    
    DEBUG_PRINTLN("Sistema reabastecido via chave mestra");
}

void RFIDManager::handleRFIDResult(const String& uid, const String& userName, RFIDResult result) {
    // Feedback visual
    showResultLED(result);
    
    // Feedback sonoro
    playResultSound(result);
    
    // Log detalhado do resultado
    String resultText = "";
    int creditsRemaining = -1;
    
    if (userManager) {
        UserCredits* user = userManager->getUserByUID(uid);
        if (user) {
            creditsRemaining = user->credits;
        }
    }
    
    switch (result) {
        case RFID_SUCCESS:
            resultText = "Café servido com sucesso";
            DEBUG_PRINTF("✅ %s - %s (Créditos: %d)\n", 
                        userName.c_str(), resultText.c_str(), creditsRemaining);
            break;
            
        case RFID_ACCESS_DENIED:
            resultText = "Acesso negado - usuário não cadastrado";
            DEBUG_PRINTF("❌ %s - %s\n", uid.c_str(), resultText.c_str());
            break;
            
        case RFID_NO_CREDITS:
            resultText = "Sem créditos suficientes";
            DEBUG_PRINTF("💳 %s - %s\n", userName.c_str(), resultText.c_str());
            break;
            
        case RFID_SYSTEM_BUSY:
            resultText = "Sistema ocupado";
            DEBUG_PRINTF("⏳ %s - %s\n", userName.c_str(), resultText.c_str());
            break;
            
        case RFID_NO_COFFEE:
            resultText = "Sem café disponível";
            DEBUG_PRINTF("☕ %s - %s\n", userName.c_str(), resultText.c_str());
            break;
            
        case RFID_MASTER_KEY:
            resultText = "Chave mestra - sistema reabastecido";
            DEBUG_PRINTF("🔑 %s - %s\n", userName.c_str(), resultText.c_str());
            break;
            
        case RFID_ERROR:
        default:
            resultText = "Erro do sistema";
            DEBUG_PRINTF("🚨 %s - %s\n", userName.c_str(), resultText.c_str());
            break;
    }
    
    // Log adicional se disponível
    if (logger) {
        String details = "UID: " + uid + ", Resultado: " + resultText;
        if (creditsRemaining >= 0) {
            details += ", Créditos restantes: " + String(creditsRemaining);
        }
        logger->logSystemEvent("Evento RFID", details);
    }
}

void RFIDManager::playResultSound(RFIDResult result) {
    // Usar os tons definidos em config.h
    switch (result) {
        case RFID_SUCCESS:
            // Tom de sucesso (definido no CoffeeController)
            tone(BUZZER_PIN, TONE_SUCCESS_FREQ1, TONE_SUCCESS_DURATION);
            delay(TONE_SUCCESS_DURATION + 20);
            tone(BUZZER_PIN, TONE_SUCCESS_FREQ2, TONE_SUCCESS_DURATION);
            delay(TONE_SUCCESS_DURATION + 20);
            noTone(BUZZER_PIN);
            break;
            
        case RFID_MASTER_KEY:
            // Sequência especial para chave mestra
            tone(BUZZER_PIN, TONE_REFILL_FREQ1, 100);
            delay(120);
            tone(BUZZER_PIN, TONE_REFILL_FREQ2, 100);
            delay(120);
            tone(BUZZER_PIN, TONE_REFILL_FREQ3, 100);
            delay(120);
            noTone(BUZZER_PIN);
            break;
            
        case RFID_ACCESS_DENIED:
        case RFID_NO_CREDITS:
        case RFID_SYSTEM_BUSY:
        case RFID_NO_COFFEE:
        case RFID_ERROR:
        default:
            // Tom de erro
            tone(BUZZER_PIN, TONE_ERROR_FREQ, TONE_ERROR_DURATION);
            delay(TONE_ERROR_DURATION + 50);
            noTone(BUZZER_PIN);
            break;
    }
}

void RFIDManager::showResultLED(RFIDResult result) {
    if (!ledController) {
        return;
    }
    
    switch (result) {
        case RFID_SUCCESS:
            ledController->signalServing();
            break;
        case RFID_MASTER_KEY:
            ledController->signalMasterKey();
            break;
        case RFID_ACCESS_DENIED:
            ledController->signalUnknownUser();
            break;
        case RFID_NO_CREDITS:
            ledController->signalNoCredits();
            break;
        default:
            ledController->signalError();
            break;
    }
}