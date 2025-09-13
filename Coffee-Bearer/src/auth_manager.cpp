#include "auth_manager.h"
#include <Preferences.h>
#include <WiFi.h>
#include <mbedtls/md.h>

AuthManager::AuthManager() {
}

bool AuthManager::begin() {
    // Carregar credenciais salvas ou usar padrões
    Preferences prefs;
    prefs.begin("auth", true);
    
    // Carregar credenciais de admin
    String adminUser = prefs.getString("admin_user", DEFAULT_ADMIN_USER);
    String adminPass = prefs.getString("admin_pass", "");
    
    if (adminPass.length() == 0) {
        // Primeira inicialização - usar senha padrão
        adminPass = hashPassword(DEFAULT_ADMIN_PASS);
        prefs.end();
        prefs.begin("auth", false);
        prefs.putString("admin_user", adminUser);
        prefs.putString("admin_pass", adminPass);
        prefs.end();
        prefs.begin("auth", true);
    }
    
    adminCredentials[adminUser] = adminPass;
    
    // Carregar credenciais de usuário
    String userUser = prefs.getString("user_user", DEFAULT_USER_USER);
    String userPass = prefs.getString("user_pass", "");
    
    if (userPass.length() == 0) {
        // Primeira inicialização - usar senha padrão
        userPass = hashPassword(DEFAULT_USER_PASS);
        prefs.end();
        prefs.begin("auth", false);
        prefs.putString("user_user", userUser);
        prefs.putString("user_pass", userPass);
        prefs.end();
        prefs.begin("auth", true);
    }
    
    userCredentials[userUser] = userPass;
    prefs.end();
    
    DEBUG_PRINTLN("Auth Manager inicializado");
    DEBUG_PRINTF("Admin user: %s\n", adminUser.c_str());
    DEBUG_PRINTF("User user: %s\n", userUser.c_str());
    
    return true;
}

void AuthManager::resetToDefault() {
    Preferences prefs;
    prefs.begin("auth", false);
    prefs.clear();
    prefs.end();
    
    adminCredentials.clear();
    userCredentials.clear();
    activeSessions.clear();
    loginAttempts.clear();
    
    begin(); // Reinicializar com valores padrão
}

bool AuthManager::setAdminCredentials(const String& username, const String& password) {
    if (username.length() < 3 || password.length() < 6) {
        return false;
    }
    
    String hashedPassword = hashPassword(password);
    
    Preferences prefs;
    prefs.begin("auth", false);
    prefs.putString("admin_user", username);
    prefs.putString("admin_pass", hashedPassword);
    prefs.end();
    
    adminCredentials.clear();
    adminCredentials[username] = hashedPassword;
    
    // Terminar sessões existentes do admin
    terminateSessionsForUser(username);
    
    return true;
}

bool AuthManager::setUserCredentials(const String& username, const String& password) {
    if (username.length() < 3 || password.length() < 6) {
        return false;
    }
    
    String hashedPassword = hashPassword(password);
    
    Preferences prefs;
    prefs.begin("auth", false);
    prefs.putString("user_user", username);
    prefs.putString("user_pass", hashedPassword);
    prefs.end();
    
    userCredentials.clear();
    userCredentials[username] = hashedPassword;
    
    // Terminar sessões existentes do usuário
    terminateSessionsForUser(username);
    
    return true;
}

bool AuthManager::changePassword(const String& username, const String& oldPassword, const String& newPassword) {
    if (newPassword.length() < 6) {
        return false;
    }
    
    // Verificar senha antiga
    bool isAdmin = adminCredentials.count(username) > 0;
    bool isUser = userCredentials.count(username) > 0;
    
    if (!isAdmin && !isUser) {
        return false;
    }
    
    String storedHash = isAdmin ? adminCredentials[username] : userCredentials[username];
    if (!verifyPassword(oldPassword, storedHash)) {
        return false;
    }
    
    // Definir nova senha
    if (isAdmin) {
        return setAdminCredentials(username, newPassword);
    } else {
        return setUserCredentials(username, newPassword);
    }
}

String AuthManager::login(const String& username, const String& password, const String& ipAddress) {
    // Verificar se IP está bloqueado
    if (isIpBlocked(ipAddress)) {
        return "";
    }
    
    // Verificar credenciais
    UserRole role = ROLE_GUEST;
    String storedHash = "";
    
    if (adminCredentials.count(username) > 0) {
        storedHash = adminCredentials[username];
        role = ROLE_ADMIN;
    } else if (userCredentials.count(username) > 0) {
        storedHash = userCredentials[username];
        role = ROLE_USER;
    } else {
        recordFailedLogin(ipAddress);
        return "";
    }
    
    if (!verifyPassword(password, storedHash)) {
        recordFailedLogin(ipAddress);
        return "";
    }
    
    // Login bem-sucedido - criar sessão
    String sessionId = generateSessionId();
    
    AuthSession session;
    session.sessionId = sessionId;
    session.username = username;
    session.role = role;
    session.createdAt = millis();
    session.lastAccess = millis();
    session.ipAddress = ipAddress;
    session.isActive = true;
    
    activeSessions.push_back(session);
    
    // Limpar tentativas de login para este IP
    for (auto it = loginAttempts.begin(); it != loginAttempts.end(); ++it) {
        if (it->ipAddress == ipAddress) {
            loginAttempts.erase(it);
            break;
        }
    }
    
    DEBUG_PRINTF("Login successful: %s (%s)\n", username.c_str(), roleToString(role).c_str());
    return sessionId;
}

bool AuthManager::logout(const String& sessionId) {
    for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
        if (it->sessionId == sessionId) {
            activeSessions.erase(it);
            DEBUG_PRINTLN("Logout successful");
            return true;
        }
    }
    return false;
}

bool AuthManager::isValidSession(const String& sessionId) {
    cleanupExpiredSessions();
    
    for (auto& session : activeSessions) {
        if (session.sessionId == sessionId && session.isActive) {
            // Verificar se não expirou
            if (millis() - session.lastAccess < SESSION_TIMEOUT_MS) {
                return true;
            }
        }
    }
    return false;
}

AuthSession* AuthManager::getSession(const String& sessionId) {
    for (auto& session : activeSessions) {
        if (session.sessionId == sessionId && session.isActive) {
            return &session;
        }
    }
    return nullptr;
}

UserRole AuthManager::getSessionRole(const String& sessionId) {
    AuthSession* session = getSession(sessionId);
    return session ? session->role : ROLE_GUEST;
}

bool AuthManager::isIpBlocked(const String& ipAddress) {
    cleanupOldAttempts();
    
    LoginAttempt* attempt = findLoginAttempt(ipAddress);
    if (!attempt) {
        return false;
    }
    
    return (attempt->attemptCount >= MAX_LOGIN_ATTEMPTS && 
            millis() < attempt->lockoutUntil);
}

void AuthManager::recordFailedLogin(const String& ipAddress) {
    cleanupOldAttempts();
    
    LoginAttempt* attempt = findLoginAttempt(ipAddress);
    if (!attempt) {
        LoginAttempt newAttempt;
        newAttempt.ipAddress = ipAddress;
        newAttempt.timestamp = millis();
        newAttempt.attemptCount = 1;
        newAttempt.lockoutUntil = 0;
        loginAttempts.push_back(newAttempt);
        return;
    }
    
    attempt->attemptCount++;
    attempt->timestamp = millis();
    
    if (attempt->attemptCount >= MAX_LOGIN_ATTEMPTS) {
        attempt->lockoutUntil = millis() + LOCKOUT_TIME_MS;
        DEBUG_PRINTF("IP %s bloqueado por %d minutos\n", 
                    ipAddress.c_str(), LOCKOUT_TIME_MS / 60000);
    }
}

unsigned long AuthManager::getBlockTimeRemaining(const String& ipAddress) {
    LoginAttempt* attempt = findLoginAttempt(ipAddress);
    if (!attempt || millis() >= attempt->lockoutUntil) {
        return 0;
    }
    return attempt->lockoutUntil - millis();
}

void AuthManager::updateSessionAccess(const String& sessionId) {
    for (auto& session : activeSessions) {
        if (session.sessionId == sessionId) {
            session.lastAccess = millis();
            break;
        }
    }
}

int AuthManager::getActiveSessionCount() {
    cleanupExpiredSessions();
    return activeSessions.size();
}

std::vector<AuthSession> AuthManager::getActiveSessions() {
    cleanupExpiredSessions();
    return activeSessions;
}

void AuthManager::terminateAllSessions() {
    activeSessions.clear();
    DEBUG_PRINTLN("Todas as sessões terminadas");
}

void AuthManager::terminateSessionsForUser(const String& username) {
    activeSessions.erase(
        std::remove_if(activeSessions.begin(), activeSessions.end(),
            [&username](const AuthSession& session) {
                return session.username == username;
            }),
        activeSessions.end()
    );
}

bool AuthManager::requireAuth(const String& sessionId, UserRole minimumRole) {
    if (!isValidSession(sessionId)) {
        return false;
    }
    
    UserRole sessionRole = getSessionRole(sessionId);
    return sessionRole >= minimumRole;
}

String AuthManager::extractSessionFromCookie(const String& cookieHeader) {
    int sessionStart = cookieHeader.indexOf("session_id=");
    if (sessionStart == -1) {
        return "";
    }
    
    sessionStart += 11; // Tamanho de "session_id="
    int sessionEnd = cookieHeader.indexOf(';', sessionStart);
    if (sessionEnd == -1) {
        sessionEnd = cookieHeader.length();
    }
    
    return cookieHeader.substring(sessionStart, sessionEnd);
}

String AuthManager::createSessionCookie(const String& sessionId) {
    return "session_id=" + sessionId + "; Path=/; HttpOnly; Max-Age=" + String(SESSION_TIMEOUT_MS / 1000);
}

String AuthManager::roleToString(UserRole role) {
    switch (role) {
        case ROLE_ADMIN: return "Admin";
        case ROLE_USER: return "User";
        default: return "Guest";
    }
}

UserRole AuthManager::stringToRole(const String& roleStr) {
    if (roleStr.equalsIgnoreCase("admin")) return ROLE_ADMIN;
    if (roleStr.equalsIgnoreCase("user")) return ROLE_USER;
    return ROLE_GUEST;
}

void AuthManager::maintenance() {
    cleanupExpiredSessions();
    cleanupOldAttempts();
}

// Métodos privados

String AuthManager::generateSessionId() {
    String sessionId = "";
    const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    for (int i = 0; i < 32; i++) {
        sessionId += chars[random(0, sizeof(chars) - 1)];
    }
    
    return sessionId;
}

String AuthManager::hashPassword(const String& password) {
    // Usar SHA-256 para hash da senha
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
    
    unsigned char hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    // Converter para hex string
    String hashString = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashString += hex;
    }
    
    return hashString;
}

bool AuthManager::verifyPassword(const String& password, const String& hash) {
    return hashPassword(password) == hash;
}

void AuthManager::cleanupExpiredSessions() {
    unsigned long currentTime = millis();
    
    activeSessions.erase(
        std::remove_if(activeSessions.begin(), activeSessions.end(),
            [currentTime](const AuthSession& session) {
                return (currentTime - session.lastAccess) > SESSION_TIMEOUT_MS;
            }),
        activeSessions.end()
    );
}

void AuthManager::cleanupOldAttempts() {
    unsigned long currentTime = millis();
    
    loginAttempts.erase(
        std::remove_if(loginAttempts.begin(), loginAttempts.end(),
            [currentTime](const LoginAttempt& attempt) {
                return (currentTime - attempt.timestamp) > (LOCKOUT_TIME_MS * 2);
            }),
        loginAttempts.end()
    );
}

LoginAttempt* AuthManager::findLoginAttempt(const String& ip) {
    for (auto& attempt : loginAttempts) {
        if (attempt.ipAddress == ip) {
            return &attempt;
        }
    }
    return nullptr;
}