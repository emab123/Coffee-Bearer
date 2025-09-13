#include "coffee_controller.h"
#include <Preferences.h>

CoffeeController::CoffeeController() :
    systemBusy(false),
    remainingCoffees(MAX_COFFEES),
    totalServed(0),
    totalServeTime(0),
    lastServed(0),
    dailyCount(0),
    dailyResetTime(0),
    lastSave(0),
    dataChanged(false) {
}

bool CoffeeController::begin() {
    // Configurar pinos
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Garantir que o relé esteja desligado
    digitalWrite(RELAY_PIN, LOW);
    
    // Carregar dados salvos
    loadFromPreferences();
    
    // Verificar reset diário se necessário
    checkDailyReset();
    
    // Som de inicialização
    playSuccessTone();
    
    DEBUG_PRINTLN("Coffee Controller inicializado");
    DEBUG_PRINTF("Cafés restantes: %d/%d\n", remainingCoffees, MAX_COFFEES);
    DEBUG_PRINTF("Total servido: %d\n", totalServed);
    DEBUG_PRINTF("Contagem diária: %d\n", dailyCount);
    
    return true;
}

void CoffeeController::clearAllData() {
    Preferences prefs;
    prefs.begin("coffee", false);
    prefs.clear();
    prefs.end();
    
    systemBusy = false;
    remainingCoffees = MAX_COFFEES;
    totalServed = 0;
    totalServeTime = 0;
    lastServed = 0;
    dailyCount = 0;
    dailyResetTime = millis();
    dataChanged = true;
    
    DEBUG_PRINTLN("Todos os dados da cafeteira foram limpos");
}

bool CoffeeController::serveCoffee(const String& userName, int* userCredits) {
    // Verificar se o sistema está ocupado
    if (systemBusy) {
        DEBUG_PRINTLN("Sistema ocupado - café não pode ser servido");
        playErrorTone();
        return false;
    }
    
    // Verificar se há café disponível
    if (remainingCoffees <= 0) {
        DEBUG_PRINTLN("Não há café disponível");
        playErrorTone();
        return false;
    }
    
    // Descontar crédito do usuário se fornecido
    if (userCredits != nullptr) {
        if (*userCredits <= 0) {
            DEBUG_PRINTLN("Usuário sem créditos suficientes");
            playErrorTone();
            return false;
        }
        (*userCredits)--;
    }
    
    DEBUG_PRINTF("Servindo café para: %s\n", userName.c_str());
    
    // Marcar sistema como ocupado
    systemBusy = true;
    
    // Som indicando início do preparo
    playServingTone();
    
    unsigned long startTime = millis();
    
    // Ativar relé da cafeteira
    digitalWrite(RELAY_PIN, HIGH);
    DEBUG_PRINTLN("Relé ativado - preparando café...");
    
    // Aguardar o tempo de preparo
    delay(COFFEE_SERVE_TIME_MS);
    
    // Desativar relé
    digitalWrite(RELAY_PIN, LOW);
    DEBUG_PRINTLN("Relé desativado - café pronto!");
    
    unsigned long serveTime = millis() - startTime;
    
    // Atualizar estatísticas
    remainingCoffees--;
    totalServed++;
    dailyCount++;
    totalServeTime += serveTime;
    lastServed = millis();
    dataChanged = true;
    
    // Som de sucesso
    playSuccessTone();
    
    // Sistema não está mais ocupado
    systemBusy = false;
    
    DEBUG_PRINTF("Café servido com sucesso! Restam: %d\n", remainingCoffees);
    DEBUG_PRINTF("Tempo de preparo: %lu ms\n", serveTime);
    
    return true;
}

void CoffeeController::refillContainer() {
    DEBUG_PRINTLN("Reabastecendo recipiente de café...");
    
    remainingCoffees = MAX_COFFEES;
    dataChanged = true;
    
    // Som de reabastecimento
    playRefillTone();
    
    DEBUG_PRINTF("Recipiente reabastecido! Cafés disponíveis: %d\n", remainingCoffees);
}

void CoffeeController::emergencyStop() {
    DEBUG_PRINTLN("PARADA DE EMERGÊNCIA ATIVADA!");
    
    // Desativar relé imediatamente
    digitalWrite(RELAY_PIN, LOW);
    
    // Marcar sistema como não ocupado
    systemBusy = false;
    
    // Som de erro
    playErrorTone();
    
    DEBUG_PRINTLN("Sistema de café parado com segurança");
}

CoffeeStatus CoffeeController::getStatus() {
    if (systemBusy) {
        return COFFEE_BUSY;
    } else if (remainingCoffees <= 0) {
        return COFFEE_EMPTY;
    } else {
        return COFFEE_READY;
    }
}

float CoffeeController::getAverageServeTime() {
    if (totalServed == 0) {
        return 0.0;
    }
    return (float)totalServeTime / (float)totalServed;
}

void CoffeeController::setRemainingCoffees(int count) {
    if (count < 0) count = 0;
    if (count > MAX_COFFEES) count = MAX_COFFEES;
    
    remainingCoffees = count;
    dataChanged = true;
    
    DEBUG_PRINTF("Cafés restantes definidos para: %d\n", count);
}

bool CoffeeController::adjustCoffeeCount(int adjustment) {
    int newCount = remainingCoffees + adjustment;
    
    if (newCount < 0 || newCount > MAX_COFFEES) {
        return false;
    }
    
    remainingCoffees = newCount;
    dataChanged = true;
    
    DEBUG_PRINTF("Contagem de café ajustada: %+d (total: %d)\n", 
                adjustment, remainingCoffees);
    
    return true;
}

CoffeeStats CoffeeController::getStats() {
    CoffeeStats stats;
    stats.totalServed = totalServed;
    stats.remainingCoffees = remainingCoffees;
    stats.totalServeTime = totalServeTime;
    stats.lastServed = lastServed;
    stats.dailyCount = dailyCount;
    stats.dailyResetTime = dailyResetTime;
    
    return stats;
}

void CoffeeController::printStats() {
    DEBUG_PRINTLN("\n=== ESTATÍSTICAS DA CAFETEIRA ===");
    DEBUG_PRINTF("Status: %s\n", 
        systemBusy ? "Ocupado" : 
        (remainingCoffees > 0 ? "Pronto" : "Vazio"));
    DEBUG_PRINTF("Cafés restantes: %d/%d\n", remainingCoffees, MAX_COFFEES);
    DEBUG_PRINTF("Total servido: %d cafés\n", totalServed);
    DEBUG_PRINTF("Servidos hoje: %d cafés\n", dailyCount);
    DEBUG_PRINTF("Último café: %s\n", 
        lastServed > 0 ? String(millis() - lastServed).c_str() + " ms atrás" : "Nunca");
    DEBUG_PRINTF("Tempo médio de preparo: %.1f ms\n", getAverageServeTime());
    DEBUG_PRINTF("Tempo total de preparo: %lu ms\n", totalServeTime);
    DEBUG_PRINTLN("================================\n");
}

void CoffeeController::resetDailyStats() {
    dailyCount = 0;
    dailyResetTime = millis();
    dataChanged = true;
    
    DEBUG_PRINTLN("Estatísticas diárias resetadas");
}

void CoffeeController::setServeTime(unsigned long timeMs) {
    // Validar tempo (entre 1 segundo e 30 segundos)
    if (timeMs < 1000 || timeMs > 30000) {
        DEBUG_PRINTLN("Tempo de preparo inválido (deve estar entre 1s e 30s)");
        return;
    }
    
    // Esta implementação usa uma constante definida em config.h
    // Para implementar configuração dinâmica, seria necessário
    // usar uma variável global ou Preferences
    DEBUG_PRINTF("Tempo de preparo configurado: %lu ms\n", timeMs);
}

unsigned long CoffeeController::getServeTime() {
    return COFFEE_SERVE_TIME_MS;
}

void CoffeeController::maintenance() {
    // Verificar reset diário
    checkDailyReset();
    
    // Salvar dados se necessário
    if (dataChanged && (millis() - lastSave > DATA_SAVE_INTERVAL_MS)) {
        saveToPreferences();
    }
    
    // Verificar se o relé está travado (possível falha)
    static unsigned long lastBusyCheck = 0;
    if (systemBusy && (millis() - lastBusyCheck > COFFEE_SERVE_TIME_MS * 2)) {
        DEBUG_PRINTLN("AVISO: Sistema ocupado por muito tempo - executando parada de emergência");
        emergencyStop();
        lastBusyCheck = millis();
    }
}

// Métodos privados

void CoffeeController::saveToPreferences() {
    if (!dataChanged) return;
    
    Preferences prefs;
    prefs.begin("coffee", false);
    
    prefs.putInt("remaining", remainingCoffees);
    prefs.putInt("totalServed", totalServed);
    prefs.putULong("totalTime", totalServeTime);
    prefs.putULong("lastServed", lastServed);
    prefs.putInt("dailyCount", dailyCount);
    prefs.putULong("dailyReset", dailyResetTime);
    
    prefs.end();
    
    dataChanged = false;
    lastSave = millis();
    
    DEBUG_PRINTLN("Dados da cafeteira salvos");
}

void CoffeeController::loadFromPreferences() {
    Preferences prefs;
    prefs.begin("coffee", true);
    
    remainingCoffees = prefs.getInt("remaining", MAX_COFFEES);
    totalServed = prefs.getInt("totalServed", 0);
    totalServeTime = prefs.getULong("totalTime", 0);
    lastServed = prefs.getULong("lastServed", 0);
    dailyCount = prefs.getInt("dailyCount", 0);
    dailyResetTime = prefs.getULong("dailyReset", millis());
    
    prefs.end();
    
    // Validar dados carregados
    if (remainingCoffees < 0) remainingCoffees = 0;
    if (remainingCoffees > MAX_COFFEES) remainingCoffees = MAX_COFFEES;
    if (totalServed < 0) totalServed = 0;
    if (dailyCount < 0) dailyCount = 0;
    
    DEBUG_PRINTLN("Dados da cafeteira carregados");
}

void CoffeeController::playSuccessTone() {
    // Dois tons ascendentes para indicar sucesso
    tone(BUZZER_PIN, TONE_SUCCESS_FREQ1, TONE_SUCCESS_DURATION);
    delay(TONE_SUCCESS_DURATION + 20);
    tone(BUZZER_PIN, TONE_SUCCESS_FREQ2, TONE_SUCCESS_DURATION);
    delay(TONE_SUCCESS_DURATION + 20);
    noTone(BUZZER_PIN);
}

void CoffeeController::playErrorTone() {
    // Tom baixo e longo para indicar erro
    tone(BUZZER_PIN, TONE_ERROR_FREQ, TONE_ERROR_DURATION);
    delay(TONE_ERROR_DURATION + 50);
    noTone(BUZZER_PIN);
}

void CoffeeController::playServingTone() {
    // Dois tons rápidos para indicar que está preparando
    tone(BUZZER_PIN, TONE_COFFEE_FREQ1, TONE_COFFEE_DURATION);
    delay(TONE_COFFEE_DURATION + 30);
    tone(BUZZER_PIN, TONE_COFFEE_FREQ2, TONE_COFFEE_DURATION);
    delay(TONE_COFFEE_DURATION + 30);
    noTone(BUZZER_PIN);
}

void CoffeeController::playRefillTone() {
    // Três tons ascendentes para indicar reabastecimento
    tone(BUZZER_PIN, TONE_REFILL_FREQ1, TONE_SUCCESS_DURATION);
    delay(TONE_SUCCESS_DURATION + 20);
    tone(BUZZER_PIN, TONE_REFILL_FREQ2, TONE_SUCCESS_DURATION);
    delay(TONE_SUCCESS_DURATION + 20);
    tone(BUZZER_PIN, TONE_REFILL_FREQ3, TONE_SUCCESS_DURATION);
    delay(TONE_SUCCESS_DURATION + 20);
    noTone(BUZZER_PIN);
}

void CoffeeController::checkDailyReset() {
    unsigned long currentTime = millis();
    
    // Verificar overflow do millis()
    if (currentTime < dailyResetTime) {
        dailyResetTime = currentTime;
        dataChanged = true;
    }
    
    // Verificar se passou um dia desde o último reset
    if ((currentTime - dailyResetTime) >= MILLIS_PER_DAY) {
        DEBUG_PRINTLN("Executando reset diário das estatísticas");
        resetDailyStats();
    }
}