/*
==================================================
SERVIDOR WEB COM AUTENTICAÇÃO
Interface separada para Admin e Usuários
==================================================
*/

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "auth_manager.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "logger.h"

class WebServerManager {
private:
    AsyncWebServer& server;
    
    // Referências para outros managers (serão definidas no begin())
    AuthManager* authManager;
    UserManager* userManager;
    CoffeeController* coffeeController;
    Logger* logger;
    
    // Métodos para servir arquivos estáticos
    void setupStaticRoutes();
    void setupAuthRoutes();
    void setupAdminRoutes();
    void setupUserRoutes();
    void setupApiRoutes();
    
    // Handlers de autenticação
    void handleLogin(AsyncWebServerRequest *request);
    void handleLogout(AsyncWebServerRequest *request);
    void handleAuthCheck(AsyncWebServerRequest *request);
    
    // Handlers Admin
    void handleAdminDashboard(AsyncWebServerRequest *request);
    void handleUserManagement(AsyncWebServerRequest *request);
    void handleSystemSettings(AsyncWebServerRequest *request);
    void handleSystemLogs(AsyncWebServerRequest *request);
    void handleSystemStats(AsyncWebServerRequest *request);
    
    // Handlers Usuário
    void handleUserDashboard(AsyncWebServerRequest *request);
    void handleUserProfile(AsyncWebServerRequest *request);
    void handleCoffeeHistory(AsyncWebServerRequest *request);
    
    // API Handlers
    void handleApiStatus(AsyncWebServerRequest *request);
    void handleApiUsers(AsyncWebServerRequest *request);
    void handleApiUserAdd(AsyncWebServerRequest *request, JsonVariant &json);
    void handleApiUserRemove(AsyncWebServerRequest *request, JsonVariant &json);
    void handleApiUserUpdate(AsyncWebServerRequest *request, JsonVariant &json);
    void handleApiServeCoffee(AsyncWebServerRequest *request);
    void handleApiRefillCoffee(AsyncWebServerRequest *request);
    void handleApiSystemReset(AsyncWebServerRequest *request);
    void handleApiLogs(AsyncWebServerRequest *request);
    void handleApiBackup(AsyncWebServerRequest *request);
    void handleApiRestore(AsyncWebServerRequest *request, JsonVariant &json);
    
    // Utilitários
    String getClientIP(AsyncWebServerRequest *request);
    bool requireAuth(AsyncWebServerRequest *request, UserRole minimumRole = ROLE_USER);
    void sendJsonResponse(AsyncWebServerRequest *request, int code, const JsonDocument &json);
    void sendErrorResponse(AsyncWebServerRequest *request, int code, const String &message);
    void sendAuthRequiredResponse(AsyncWebServerRequest *request);
    String getMimeType(const String &filename);
    bool fileExists(const String &path);
    
public:
    WebServerManager(AsyncWebServer& serverRef);
    
    // Inicialização
    bool begin();
    void setManagers(AuthManager* auth, UserManager* users, CoffeeController* coffee, Logger* log);
    
    // Controle do servidor
    void start();
    void stop();
    bool isRunning();
    
    // Estatísticas
    int getActiveConnections();
    unsigned long getTotalRequests();
};