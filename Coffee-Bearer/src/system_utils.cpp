// src/system_utils.cpp
#include "system_utils.h"
#include <Arduino.h>
#include <WiFi.h>

// Updated function to build the full JSON object
void systemStatusToJson(JsonDocument &doc, 
                        Logger &logger, 
                        CoffeeController &coffee, 
                        UserManager &userManager,
                        AuthManager &authManager) {

    // System Info
    JsonObject system = doc.createNestedObject("system");
    system["uptime"] = millis();
    system["freeHeap"] = ESP.getFreeHeap();
    system["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    system["wifiIP"] = WiFi.localIP().toString();

    // Coffee Info
    JsonObject coffeeInfo = doc.createNestedObject("coffee");
    coffeeInfo["remaining"] = coffee.getRemainingCoffees();
    coffeeInfo["totalServed"] = coffee.getTotalServed();
    coffeeInfo["isBusy"] = coffee.isBusy();
    coffeeInfo["maxCapacity"] = MAX_COFFEES;

    // Users Info
    JsonObject usersInfo = doc.createNestedObject("users");
    usersInfo["total"] = userManager.getTotalUsers();
    usersInfo["activeToday"] = userManager.getActiveTodayCount();

    // Auth Info
    JsonObject authInfo = doc.createNestedObject("auth");
    authInfo["activeSessions"] = authManager.getActiveSessionCount();

    // Log Info
    JsonObject logInfo = doc.createNestedObject("logs");
    logInfo["total"] = logger.getTotalLogCount();
    logInfo["errors"] = logger.getLogCountByLevel(LOG_ERROR);
    logInfo["warnings"] = logger.getLogCountByLevel(LOG_WARNING);
}