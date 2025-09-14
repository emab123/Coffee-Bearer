/*
==================================================
CONTROLADOR DA CAFETEIRA
Gerencia o preparo e controle de café
==================================================
*/

#pragma once

#include <Arduino.h>
#include "config.h"

enum CoffeeStatus {
    COFFEE_READY,
    COFFEE_BUSY,
    COFFEE_EMPTY,
    COFFEE_ERROR
};

struct CoffeeStats {
    int totalServed;
    int remainingCoffees;
    unsigned long totalServeTime;
    unsigned long lastServed;
    int dailyCount;
    unsigned long dailyResetTime;
};

class CoffeeController {
private:
    bool systemBusy;
    int remainingCoffees;
    int totalServed;
    unsigned long totalServeTime;
    unsigned long lastServed;
    int dailyCount;
    unsigned long dailyResetTime;
    unsigned long lastSave;
    bool dataChanged;
    unsigned long coffeeServeEndTime;
    
    void saveToPreferences();
    void loadFromPreferences();
    void playSuccessTone();
    void playErrorTone();
    void playServingTone();
    void playRefillTone();
    void checkDailyReset();
    
public:
    CoffeeController();
    
    // Inicialização
    bool begin();
    void clearAllData();
    
    // Controle principal
    bool serveCoffee(const String& userName, int* userCredits);
    void refillContainer();
    void emergencyStop();
    
    // Status
    bool isBusy() { return systemBusy; }
    bool isEmpty() { return remainingCoffees <= 0; }
    bool isReady() { return !systemBusy && remainingCoffees > 0; }
    CoffeeStatus getStatus();
    
    // Getters
    int getRemainingCoffees() { return remainingCoffees; }
    int getTotalServed() { return totalServed; }
    int getDailyCount() { return dailyCount; }
    unsigned long getLastServedTime() { return lastServed; }
    unsigned long getTotalServeTime() { return totalServeTime; }
    float getAverageServeTime();
    
    // Setters
    void setRemainingCoffees(int count);
    bool adjustCoffeeCount(int adjustment);
    
    // Estatísticas
    CoffeeStats getStats();
    void printStats();
    void resetDailyStats();
    
    // Configurações
    void setServeTime(unsigned long timeMs);
    unsigned long getServeTime();
    
    // Manutenção (deve ser chamado periodicamente)
    void maintenance();
};