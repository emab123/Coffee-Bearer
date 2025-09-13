#include "user_manager.h"
#include <Preferences.h>
#include <algorithm>
#include <ArduinoJson.h>

UserManager::UserManager() : 
    lastWeeklyReset(0),
    lastSave(0),
    dataChanged(false) {
}

bool UserManager::begin() {
    loadFromPreferences();
    
    // Se não há registro de reset semanal, definir para agora
    if (lastWeeklyReset == 0) {
        lastWeeklyReset = millis();
        dataChanged = true;
        saveToPreferences();
    }
    
    DEBUG_PRINTLN("User Manager inicializado");
    DEBUG_PRINTF("Usuários carregados: %d\n", users.size());
    DEBUG_PRINTF("Último reset semanal: %lu\n", lastWeeklyReset);
    
    return true;
}

void UserManager::clearAllData() {
    Preferences prefs;
    prefs.begin("users", false);
    prefs.clear();
    prefs.end();
    
    users.clear();
    lastWeeklyReset = millis();
    dataChanged = true;
    
    DEBUG_PRINTLN("Todos os dados de usuários foram limpos");
}

bool UserManager::addUser(const String& uid, const String& name) {
    if (users.size() >= MAX_USERS) {
        DEBUG_PRINTLN("Máximo de usuários atingido");
        return false;
    }
    
    if (!isValidUID(uid) || name.length() == 0) {
        DEBUG_PRINTLN("UID ou nome inválido");
        return false;
    }
    
    // Verificar se usuário já existe
    if (userExists(uid)) {
        DEBUG_PRINTLN("Usuário já existe");
        return false;
    }
    
    UserCredits newUser;
    newUser.uid = uid.c_str();
    newUser.name = sanitizeName(name);
    newUser.credits = INITIAL_CREDITS;
    newUser.lastUsed = 0;
    newUser.isActive = true;
    
    users.push_back(newUser);
    dataChanged = true;
    
    DEBUG_PRINTF("Usuário adicionado: %s (UID: %s)\n", 
                newUser.name.c_str(), newUser.uid.c_str());
    
    return true;
}

bool UserManager::removeUser(const String& uid) {
    int index = findUserByUID(uid);
    if (index == -1) {
        return false;
    }
    
    String userName = users[index].name;
    users.erase(users.begin() + index);
    dataChanged = true;
    
    DEBUG_PRINTF("Usuário removido: %s (UID: %s)\n", 
                userName.c_str(), uid.c_str());
    
    return true;
}

bool UserManager::updateUser(const String& uid, const String& newName) {
    UserCredits* user = getUserByUID(uid);
    if (!user || newName.length() == 0) {
        return false;
    }
    
    String oldName = user->name;
    user->name = sanitizeName(newName);
    dataChanged = true;
    
    DEBUG_PRINTF("Usuário atualizado: %s -> %s (UID: %s)\n", 
                oldName.c_str(), user->name.c_str(), uid.c_str());
    
    return true;
}

bool UserManager::userExists(const String& uid) {
    return findUserByUID(uid) != -1;
}

UserCredits* UserManager::getUserByUID(const String& uid) {
    int index = findUserByUID(uid);
    return (index != -1) ? &users[index] : nullptr;
}

String UserManager::getUserName(const String& uid) {
    UserCredits* user = getUserByUID(uid);
    return user ? user->name : "";
}

int UserManager::getUserCredits(const String& uid) {
    UserCredits* user = getUserByUID(uid);
    return user ? user->credits : -1;
}

std::vector<UserCredits> UserManager::getAllUsers() {
    return users;
}

std::vector<UserCredits> UserManager::getActiveUsers() {
    std::vector<UserCredits> activeUsers;
    for (const auto& user : users) {
        if (user.isActive && user.credits > 0) {
            activeUsers.push_back(user);
        }
    }
    return activeUsers;
}

bool UserManager::consumeCredit(const String& uid) {
    UserCredits* user = getUserByUID(uid);
    if (!user || user->credits <= 0) {
        return false;
    }
    
    user->credits--;
    updateLastUsed(uid);
    dataChanged = true;
    
    DEBUG_PRINTF("Crédito consumido: %s (%d restantes)\n", 
                user->name.c_str(), user->credits);
    
    return true;
}

bool UserManager::addCredits(const String& uid, int credits) {
    if (credits <= 0) return false;
    
    UserCredits* user = getUserByUID(uid);
    if (!user) return false;
    
    user->credits += credits;
    dataChanged = true;
    
    DEBUG_PRINTF("Créditos adicionados: %s (+%d = %d total)\n", 
                user->name.c_str(), credits, user->credits);
    
    return true;
}

bool UserManager::setCredits(const String& uid, int credits) {
    if (credits < 0) return false;
    
    UserCredits* user = getUserByUID(uid);
    if (!user) return false;
    
    int oldCredits = user->credits;
    user->credits = credits;
    dataChanged = true;
    
    DEBUG_PRINTF("Créditos definidos: %s (%d -> %d)\n", 
                user->name.c_str(), oldCredits, credits);
    
    return true;
}

int UserManager::getTotalCreditsInSystem() {
    int total = 0;
    for (const auto& user : users) {
        total += user.credits;
    }
    return total;
}

bool UserManager::shouldPerformWeeklyReset() {
    unsigned long currentTime = millis();
    
    // Verificar overflow do millis()
    if (currentTime < lastWeeklyReset) {
        lastWeeklyReset = currentTime;
        dataChanged = true;
    }
    
    return (currentTime - lastWeeklyReset) >= WEEKLY_RESET_INTERVAL_MS;
}

void UserManager::performWeeklyReset() {
    int usersReset = 0;
    
    for (auto& user : users) {
        if (user.credits < INITIAL_CREDITS) {
            user.credits = INITIAL_CREDITS;
            usersReset++;
        }
    }
    
    lastWeeklyReset = millis();
    dataChanged = true;
    
    DEBUG_PRINTF("Reset semanal executado: %d usuários resetados\n", usersReset);
}

unsigned long UserManager::getTimeSinceLastReset() {
    unsigned long currentTime = millis();
    if (currentTime < lastWeeklyReset) {
        return 0; // Overflow detectado
    }
    return currentTime - lastWeeklyReset;
}

unsigned long UserManager::getNextResetTime() {
    return lastWeeklyReset + WEEKLY_RESET_INTERVAL_MS;
}

int UserManager::getTotalUsers() {
    return users.size();
}

int UserManager::getActiveUsersCount() {
    int count = 0;
    for (const auto& user : users) {
        if (user.isActive && user.credits > 0) {
            count++;
        }
    }
    return count;
}

int UserManager::getActiveTodayCount() {
    unsigned long oneDayAgo = millis() - MILLIS_PER_DAY;
    int count = 0;
    
    for (const auto& user : users) {
        if (user.lastUsed > oneDayAgo) {
            count++;
        }
    }
    return count;
}

UserCredits UserManager::getMostActiveUser() {
    UserCredits mostActive;
    mostActive.uid = "";
    mostActive.lastUsed = 0;
    
    for (const auto& user : users) {
        if (user.lastUsed > mostActive.lastUsed) {
            mostActive = user;
        }
    }
    
    return mostActive;
}

std::vector<UserCredits> UserManager::getTopUsers(int count) {
    std::vector<UserCredits> sortedUsers = users;
    
    // Ordenar por última utilização (mais recente primeiro)
    std::sort(sortedUsers.begin(), sortedUsers.end(), 
        [](const UserCredits& a, const UserCredits& b) {
            return a.lastUsed > b.lastUsed;
        });
    
    // Limitar ao número solicitado
    if (sortedUsers.size() > count) {
        sortedUsers.resize(count);
    }
    
    return sortedUsers;
}

void UserManager::printUserList() {
    DEBUG_PRINTF("\n=== LISTA DE USUÁRIOS (%d/%d) ===\n", users.size(), MAX_USERS);
    
    if (users.empty()) {
        DEBUG_PRINTLN("Nenhum usuário cadastrado");
        return;
    }
    
    for (const auto& user : users) {
        DEBUG_PRINTF("UID: %s | Nome: %s | Créditos: %d | Ativo: %s\n",
                    user.uid.c_str(),
                    user.name.c_str(),
                    user.credits,
                    user.isActive ? "Sim" : "Não");
    }
    
    DEBUG_PRINTF("Total de créditos no sistema: %d\n", getTotalCreditsInSystem());
    DEBUG_PRINTLN("===============================\n");
}

void UserManager::updateLastUsed(const String& uid) {
    UserCredits* user = getUserByUID(uid);
    if (user) {
        user->lastUsed = millis();
        user->isActive = true;
        dataChanged = true;
    }
}

bool UserManager::isValidUID(const String& uid) {
    if (uid.length() < 8 || uid.length() > 23) {
        return false;
    }
    
    // Verificar se contém apenas caracteres hexadecimais e espaços
    for (char c : uid) {
        if (!isxdigit(c) && c != ' ') {
            return false;
        }
    }
    
    return true;
}

String UserManager::sanitizeName(const String& name) {
    String clean = name;
    clean.trim();
    
    // Remover caracteres especiais perigosos
    clean.replace("<", "");
    clean.replace(">", "");
    clean.replace("\"", "");
    clean.replace("'", "");
    clean.replace("&", "");
    
    // Limitar tamanho
    if (clean.length() > 50) {
        clean = clean.substring(0, 50);
    }
    
    return clean;
}

String UserManager::exportUsers() {
    // Implementar exportação JSON dos usuários para backup
    String json = "{\"users\":[";
    
    for (size_t i = 0; i < users.size(); i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"uid\":\"" + users[i].uid + "\",";
        json += "\"name\":\"" + users[i].name + "\",";
        json += "\"credits\":" + String(users[i].credits) + ",";
        json += "\"lastUsed\":" + String(users[i].lastUsed) + ",";
        json += "\"isActive\":" + String(users[i].isActive ? "true" : "false");
        json += "}";
    }
    
    json += "],\"lastWeeklyReset\":" + String(lastWeeklyReset) + "}";
    return json;
}

bool UserManager::importUsers(const String& data) {
    // Implementar importação JSON dos usuários (backup restore)
    // Esta é uma implementação simplificada
    DEBUG_PRINTLN("Importação de usuários não implementada completamente");
    return false;
}

void UserManager::maintenance() {
    // Salvar dados se necessário
    if (dataChanged && (millis() - lastSave > DATA_SAVE_INTERVAL_MS)) {
        saveToPreferences();
    }
    
    // Verificar e executar reset semanal se necessário
    if (shouldPerformWeeklyReset()) {
        performWeeklyReset();
    }
}

// Métodos privados

void UserManager::saveToPreferences() {
    if (!dataChanged) return;
    
    Preferences prefs;
    prefs.begin("users", false);
    
    // Limpar dados antigos
    prefs.clear();
    
    // Salvar configurações gerais
    prefs.putULong("lastReset", lastWeeklyReset);
    prefs.putUInt("userCount", users.size());
    
    // Salvar cada usuário
    for (size_t i = 0; i < users.size(); i++) {
        String prefix = "u" + String(i) + "_";
        
        prefs.putString((prefix + "uid").c_str(), users[i].uid);
        prefs.putString((prefix + "name").c_str(), users[i].name);
        prefs.putInt((prefix + "credits").c_str(), users[i].credits);
        prefs.putULong((prefix + "lastUsed").c_str(), users[i].lastUsed);
        prefs.putBool((prefix + "isActive").c_str(), users[i].isActive);
    }
    
    prefs.end();
    
    dataChanged = false;
    lastSave = millis();
    
    DEBUG_PRINTF("Dados de usuários salvos (%d usuários)\n", users.size());
}

void UserManager::loadFromPreferences() {
    Preferences prefs;
    prefs.begin("users", true);
    
    // Carregar configurações gerais
    lastWeeklyReset = prefs.getULong("lastReset", 0);
    unsigned int userCount = prefs.getUInt("userCount", 0);
    
    // Carregar usuários
    users.clear();
    users.reserve(userCount);
    
    for (unsigned int i = 0; i < userCount; i++) {
        String prefix = "u" + String(i) + "_";
        
        UserCredits user;
        user.uid = prefs.getString((prefix + "uid").c_str(), "");
        user.name = prefs.getString((prefix + "name").c_str(), "");
        user.credits = prefs.getInt((prefix + "credits").c_str(), INITIAL_CREDITS);
        user.lastUsed = prefs.getULong((prefix + "lastUsed").c_str(), 0);
        user.isActive = prefs.getBool((prefix + "isActive").c_str(), true);
        
        if (user.uid.length() > 0 && user.name.length() > 0) {
            users.push_back(user);
        }
    }
    
    prefs.end();
    
    DEBUG_PRINTF("Dados de usuários carregados (%d usuários)\n", users.size());
}

int UserManager::findUserByUID(const String& uid) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].uid.equalsIgnoreCase(uid)) {
            return i;
        }
    }
    return -1;
}

// Converte um único usuário para JSON
String UserManager::userToJson(const UserCredits &user) {
    StaticJsonDocument<256> doc;
    doc["uid"] = user.uid;
    doc["name"] = user.name;
    doc["credits"] = user.credits;
    doc["lastUsed"] = user.lastUsed;
    doc["isActive"] = user.isActive;
    String out;
    serializeJson(doc, out);
    return out;
}

// Lista todos os usuários em JSON
String UserManager::listUsersJson() {
    StaticJsonDocument<1024> doc;   // tamanho ajustável
    JsonArray arr = doc.createNestedArray("users");

    for (const auto &user : users) {
        JsonObject obj = arr.createNestedObject();
        obj["uid"] = user.uid;
        obj["name"] = user.name;
        obj["credits"] = user.credits;
        obj["lastUsed"] = user.lastUsed;
        obj["isActive"] = user.isActive;
    }

    String out;
    serializeJson(doc, out);
    return out;
}