#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declarations to keep this header clean
class AuthManager;
class Logger;
class UserManager;
class CoffeeController;
class FeedbackManager;

class WebServerManager {
public:
    // The constructor is now simpler and doesn't need the RFIDManager
    WebServerManager(AsyncWebServer &server, AuthManager &auth, Logger &log, UserManager &users, CoffeeController &coffee, FeedbackManager &feedback);

    void begin();
    void pushScannedUID(const String &uid);

private:
    AsyncWebServer &server;
    AsyncWebSocket ws;
    AuthManager &authManager;
    Logger &logger;
    UserManager &userManager;
    CoffeeController &coffeeController;
    FeedbackManager &feedbackManager;

    void setupStaticRoutes();
    void setupAuthRoutes();
    void setupApiRoutes();
    void setupWebSocket();
};

#endif