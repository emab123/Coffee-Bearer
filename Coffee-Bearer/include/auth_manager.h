#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <ESPAsyncWebServer.h>
#include "config.h"

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
    // Use std::vector instead of unordered_map to avoid hash issues
    std::vector<std::pair<String, String>> adminCredentials;
    std::vector<std::pair<String, String>> userCredentials;
    std::vector<AuthSession> activeSessions;
    std::vector<LoginAttempt> loginAttempts;

    // Private methods
    String generateSessionId();
    String hashPassword(const String& password);
    bool verifyPassword(const String& password, const String& hash);
    void cleanupExpiredSessions();
    void cleanupOldAttempts();
    LoginAttempt* findLoginAttempt(const String& ip);
    
    // Helper methods to find credentials
    String findAdminPassword(const String& username);
    String findUserPassword(const String& username);
    bool hasAdminCredentials(const String& username);
    bool hasUserCredentials(const String& username);

public:
    AuthManager();
    
    bool begin();
    void resetToDefault();
    
    // Credential management
    bool setAdminCredentials(const String& username, const String& password);
    bool setUserCredentials(const String& username, const String& password);
    bool changePassword(const String& username, const String& oldPassword, const String& newPassword);
    
    // Authentication
    String login(const String& username, const String& password, const String& ipAddress);
    bool logout(const String& sessionId);
    bool isValidSession(const String& sessionId);
    AuthSession* getSession(const String& sessionId);
    UserRole getSessionRole(const String& sessionId);
    
    // Security
    bool isIpBlocked(const String& ipAddress);
    void recordFailedLogin(const String& ipAddress);
    unsigned long getBlockTimeRemaining(const String& ipAddress);
    
    // Session management
    void updateSessionAccess(const String& sessionId);
    int getActiveSessionCount();
    std::vector<AuthSession> getActiveSessions();
    void terminateAllSessions();
    void terminateSessionsForUser(const String& username);
    
    // Helper methods
    bool requireAuth(const String& sessionId, UserRole minimumRole = ROLE_USER);
    String extractSessionFromCookie(const String& cookieHeader);
    String createSessionCookie(const String& sessionId);
    
    // Utility methods
    String roleToString(UserRole role);
    UserRole stringToRole(const String& roleStr);
    
    // Web server integration
    String getSessionIdFromRequest(AsyncWebServerRequest *req);
    bool isAuthenticated(AsyncWebServerRequest *req, UserRole minimumRole = ROLE_USER);
    String getUserRoleFromRequest(AsyncWebServerRequest *req);
    
    // Maintenance
    void maintenance();
};

#endif