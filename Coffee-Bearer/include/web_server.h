#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class AuthManager;
class Logger;
class UserManager;
class CoffeeController;

class WebServerManager {
public:
    WebServerManager(AuthManager &auth, Logger &log, UserManager &users, CoffeeController &coffee);

    void begin();

    // Push events to all WS clients
    void pushStatus();
    void pushLog(const String &log);
    void pushUserUpdate(const String &uid);

private:
    AsyncWebServer server;
    AsyncWebSocket ws;

    AuthManager &authManager;
    Logger &logger;
    UserManager &userManager;
    CoffeeController &coffeeController;

    void setupStaticRoutes();
    void setupAuthRoutes();
    void setupApiRoutes();
    void setupWebSocket();
};

#endif
