/*
==================================================
GERENCIADOR DE USUÁRIOS RFID
Controle de créditos e acesso
==================================================
*/

#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"

class UserManager {
private:
    std::vector<UserCredits> users;
    unsigned long lastWeeklyReset;
    unsigned long lastSave;
    bool dataChanged;
    
    void saveToPreferences();
    void loadFromPreferences();
    int findUserByUID(const String& uid);
    
public:
    UserManager();
    
    // Inicialização
    bool begin();
    void clearAllData();
    
    // Gerenciamento de usuários
    bool addUser(const String& uid, const String& name);
    bool removeUser(const String& uid);
    bool updateUser(const String& uid, const String& newName);
    bool userExists(const String& uid);
    
    // Consulta de usuários
    UserCredits* getUserByUID(const String& uid);
    String getUserName(const String& uid);
    int getUserCredits(const String& uid);
    std::vector<UserCredits> getAllUsers();
    std::vector<UserCredits> getActiveUsers();
    
    // Gerenciamento de créditos
    bool consumeCredit(const String& uid);
    bool addCredits(const String& uid, int credits);
    bool setCredits(const String& uid, int credits);
    int getTotalCreditsInSystem();
    
    // Reset semanal
    bool shouldPerformWeeklyReset();
    void performWeeklyReset();
    unsigned long getTimeSinceLastReset();
    unsigned long getNextResetTime();
    
    // Estatísticas
    int getTotalUsers();
    int getActiveUsersCount();
    int getActiveTodayCount();
    UserCredits getMostActiveUser();
    std::vector<UserCredits> getTopUsers(int count = 5);
    
    // Utilitários
    void printUserList();
    void updateLastUsed(const String& uid);
    bool isValidUID(const String& uid);
    String sanitizeName(const String& name);
    
    // Backup/Restore
    String exportUsers();
    bool importUsers(const String& data);
    
    // Manutenção (deve ser chamado periodicamente)
    void maintenance();
};