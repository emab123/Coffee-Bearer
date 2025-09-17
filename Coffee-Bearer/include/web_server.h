#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include "system_utils.h"
#include "RFID_manager.h"

class AuthManager;
class Logger;
class UserManager;
class CoffeeController;

class WebServerManager {
public:
    WebServerManager(AuthManager &auth, Logger &log, UserManager &users, CoffeeController &coffee, FeedbackManager &feedback);

    void begin();

    // Push events to all WS clients
    void pushStatus();
    void pushLog(const String &log);
    void pushUserUpdate(const String &uid);
    void pushScannedUID(const String &uid);

private:
    AsyncWebServer server;
    AsyncWebSocket ws;

    AuthManager &authManager;
    Logger &logger;
    UserManager &userManager;
    CoffeeController &coffeeController;
    FeedbackManager &feedbackManager; // ADD THIS LINE
    
    void setupStaticRoutes();
    void sendHtmlFile(AsyncWebServerRequest* req, const String& baseDir, const String& page);
    void setupAuthRoutes();
    void setupApiRoutes();
    void setupWebSocket();
};

#endif
