/*
==================================================
GERENCIADOR DE AUTENTICAÇÃO
Sistema de login para Admin e Usuários
==================================================
*/

#pragma once

#include <Arduino.h>
#include <unordered_map>  // FIXED: Changed from map to unordered_map to match implementation
#include <vector>
#include "config.h"
#include <ESPAsyncWebServer.h>

enum UserRole {
    ROLE_GUEST = 0,
    ROLE_USER = 1,
    ROLE_ADMIN = 2
};

struct AuthSession {
    String sessionId;
    String username;
    UserRole role;
    unsigned long createdAt;
    unsigned long lastAccess;
    String ipAddress;
    bool isActive;
};

struct LoginAttempt {
    String ipAddress;
    unsigned long timestamp;
    int attemptCount;
    unsigned long lockoutUntil;
};

class AuthManager {
private:
    std::unordered_map<String, String> adminCredentials;
    std::unordered_map<String, String> userCredentials;
    std::vector<AuthSession> activeSessions;
    std::vector<LoginAttempt> loginAttempts;
    
    String generateSessionId();
    String hashPassword(const String& password);
    bool verifyPassword(const String& password, const String& hash);
    void cleanupExpiredSessions();
    void cleanupOldAttempts();
    LoginAttempt* findLoginAttempt(const String& ip);
    
public:
    AuthManager();
    
    // Inicialização
    bool begin();
    void resetToDefault();
    
    // Gerenciamento de credenciais
    bool setAdminCredentials(const String& username, const String& password);
    bool setUserCredentials(const String& username, const String& password);
    bool changePassword(const String& username, const String& oldPassword, const String& newPassword);
    
    // Autenticação
    String login(const String& username, const String& password, const String& ipAddress);
    bool logout(const String& sessionId);
    bool isValidSession(const String& sessionId);
    AuthSession* getSession(const String& sessionId);
    UserRole getSessionRole(const String& sessionId);
    
    // Rate limiting
    bool isIpBlocked(const String& ipAddress);
    void recordFailedLogin(const String& ipAddress);
    unsigned long getBlockTimeRemaining(const String& ipAddress);
    
    // Gerenciamento de sessões
    void updateSessionAccess(const String& sessionId);
    int getActiveSessionCount();
    std::vector<AuthSession> getActiveSessions();
    void terminateAllSessions();
    void terminateSessionsForUser(const String& username);
    
    // Middleware para requisições web
    bool requireAuth(const String& sessionId, UserRole minimumRole = ROLE_USER);
    String extractSessionFromCookie(const String& cookieHeader);
    String createSessionCookie(const String& sessionId);
    
    // Utilitários
    String roleToString(UserRole role);
    UserRole stringToRole(const String& roleStr);
    String getSessionIdFromRequest(AsyncWebServerRequest *req);
    String getUserRoleFromRequest(AsyncWebServerRequest *req);
    bool isAuthenticated(AsyncWebServerRequest *req, UserRole minimumRole = ROLE_USER);
    
    // Manutenção (deve ser chamado periodicamente)
    void maintenance();
};