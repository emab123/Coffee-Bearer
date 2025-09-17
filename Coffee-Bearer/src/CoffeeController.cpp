#include "coffee_controller.h"
#include "beeps_and_bleeps.h"
#include <Preferences.h>

CoffeeController::CoffeeController(FeedbackManager& feedback) :
    feedbackManager(feedback),
    systemBusy(false),
    remainingCoffees(MAX_COFFEES),
    totalServed(0),
    totalServeTime(0),
    lastServed(0),
    dailyCount(0),
    dailyResetTime(0),
    lastSave(0),
    dataChanged(false) 
{
}

bool CoffeeController::begin() {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    loadFromPreferences();
    checkDailyReset();
    feedbackManager.signalSuccess();
    
    DEBUG_PRINTLN("Coffee Controller inicializado");
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
    if (systemBusy || remainingCoffees <= 0 || (userCredits != nullptr && *userCredits <= 0)) {
        feedbackManager.signalError(); // Now directly calls the correct instance
        return false;
    }
    
    if (userCredits != nullptr) {
        (*userCredits)--;
    }
    
    systemBusy = true;
    feedbackManager.signalServing(); // Correctly signals serving start

    digitalWrite(RELAY_PIN, HIGH);
    coffeeServeEndTime = millis() + COFFEE_SERVE_TIME_MS;
    
    return true;
}

void CoffeeController::refillContainer() {
    DEBUG_PRINTLN("Reabastecendo recipiente de café...");
    remainingCoffees = MAX_COFFEES;
    dataChanged = true;
    feedbackManager.signalRefill();
    DEBUG_PRINTF("Recipiente reabastecido! Cafés disponíveis: %d\n", remainingCoffees);
}

void CoffeeController::emergencyStop() {
    digitalWrite(RELAY_PIN, LOW);
    systemBusy = false;
    feedbackManager.signalError();
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
    String lastCoffeeText = lastServed > 0 ? 
        String(millis() - lastServed) + " ms atrás" : 
        "Nunca";
    DEBUG_PRINTF("Último café: %s\n", lastCoffeeText.c_str());    DEBUG_PRINTF("Tempo médio de preparo: %.1f ms\n", getAverageServeTime());
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
    if (systemBusy && millis() >= coffeeServeEndTime) {
        digitalWrite(RELAY_PIN, LOW);
        remainingCoffees--;
        totalServed++;
        dailyCount++;
        lastServed = millis();
        dataChanged = true;
        feedbackManager.signalSuccess();
        systemBusy = false;
        DEBUG_PRINTF("Café servido com sucesso! Restam: %d\n", remainingCoffees);
    }
    
    checkDailyReset();
    
    if (dataChanged && (millis() - lastSave > DATA_SAVE_INTERVAL_MS)) {
        saveToPreferences();
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