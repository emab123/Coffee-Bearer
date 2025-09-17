#include "web_server.h"
#include "auth_manager.h"
#include "logger.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "beeps_and_bleeps.h"
#include "RFID_manager.h"
#include "system_utils.h"
#include <SPIFFS.h>
#include <AsyncJson.h>

WebServerManager::WebServerManager(AsyncWebServer &server, AuthManager &auth, Logger &log, UserManager &users, CoffeeController &coffee, FeedbackManager &feedback)
    : server(server), 
      ws("/ws"), 
      authManager(auth), 
      logger(log), 
      userManager(users), 
      coffeeController(coffee), 
      feedbackManager(feedback),
      rfidManager(nullptr) // Initialize pointer to null
{}

void WebServerManager::begin() {
    setupStaticRoutes();
    setupAuthRoutes();
    setupApiRoutes();
    setupWebSocket();
    server.begin();
    Serial.println("🌐 Web server started");
}

void WebServerManager::setRfidManager(RFIDManager* rfid) {
    this->rfidManager = rfid;
}


/* -------------------- Static Routes -------------------- */
void WebServerManager::setupStaticRoutes() {
    // Helper lambda to serve gzipped files
    auto serveHtml = [](AsyncWebServerRequest *request, const String& path) {
        String gzPath = path + ".gz";
        if (SPIFFS.exists(gzPath)) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, gzPath, "text/html");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        } else if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path, "text/html");
        } else {
            request->send(404, "text/plain", "Page Not Found");
        }
    };
    
    server.on("/", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/login.html");
    });

    // Page routes
    const char* pageRoutes[] = {
    "/admin/dashboard", "/admin/users", "/admin/settings", 
    "/admin/logs", "/admin/stats", "/user/dashboard", 
    "/user/profile", "/user/history"
    };
    for (const char* route : pageRoutes) {
        server.on(route, HTTP_GET, [serveHtml, route](AsyncWebServerRequest *request) {
            String path = String("/web") + route + ".html";
            serveHtml(request, path);
        });
    }
    // Static assets
    server.serveStatic("/css", SPIFFS, "/web/css").setCacheControl("max-age=31536000");
    server.serveStatic("/js", SPIFFS, "/web/js").setCacheControl("max-age=31536000");
    server.serveStatic("/favicon.ico", SPIFFS, "/web/favicon.ico");

    // Not Found handler
    server.onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("❗ 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not Found");
    });
}

/* -------------------- Auth Routes -------------------- */
void WebServerManager::setupAuthRoutes() {
    server.on("/auth/login", HTTP_POST, [this](AsyncWebServerRequest *req) {
        if (!req->hasParam("username", true) || !req->hasParam("password", true)) {
            req->send(400, "application/json", "{\"success\":false,\"message\":\"Missing credentials\"}");
            return;
        }

        String username = req->getParam("username", true)->value();
        String password = req->getParam("password", true)->value();
        String ip = req->client()->remoteIP().toString();

        String sessionId = this->authManager.login(username, password, ip);
        if (sessionId.length() > 0) {
            UserRole role = this->authManager.getSessionRole(sessionId);
            String redirectUrl = (role == ROLE_ADMIN) ? "/admin/dashboard" : "/user/dashboard";
            
            AsyncWebServerResponse *res = req->beginResponse(200, "application/json",
                "{\"success\":true,\"redirectUrl\":\"" + redirectUrl + "\"}");
            res->addHeader("Set-Cookie", this->authManager.createSessionCookie(sessionId));
            req->send(res);
        } else {
            req->send(401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        }
    });

    server.on("/auth/logout", HTTP_POST, [this](AsyncWebServerRequest *req) {
        String sessionId = this->authManager.getSessionIdFromRequest(req);
        if (!sessionId.isEmpty()) {
            this->authManager.logout(sessionId);
        }
        req->send(200, "application/json", "{\"success\":true}");
    });

    server.on("/auth/check", HTTP_GET, [this](AsyncWebServerRequest *req) {
        String sessionId = this->authManager.getSessionIdFromRequest(req);
        bool ok = this->authManager.isValidSession(sessionId);
        String role = ok ? this->authManager.roleToString(this->authManager.getSessionRole(sessionId)) : "";
        String username = ok ? this->authManager.getSession(sessionId)->username : "";
        
        String json = "{\"authenticated\":" + String(ok ? "true" : "false") +
                      ",\"role\":\"" + role + "\"" +
                      ",\"username\":\"" + username + "\"}";
        req->send(200, "application/json", json);
    });
}

/* -------------------- API Routes -------------------- */
void WebServerManager::setupApiRoutes() {
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req)) {
            req->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        StaticJsonDocument<1024> doc;
        systemStatusToJson(doc, this->logger, this->coffeeController, this->userManager, this->authManager);
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });
    
    // --- LED Brightness Endpoint ---
    AsyncCallbackJsonWebHandler* ledHandler = new AsyncCallbackJsonWebHandler("/api/led/brightness", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!this->authManager.isAuthenticated(request, ROLE_ADMIN)) {
            request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }

        JsonObject jsonObj = json.as<JsonObject>();
        if (jsonObj.containsKey("brightness")) {
            uint8_t brightness = jsonObj["brightness"];
            this->feedbackManager.setBrightness(brightness);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"success\":false, \"message\":\"Missing brightness value\"}");
        }
    });
    server.addHandler(ledHandler);
    
    AsyncCallbackJsonWebHandler* systemSettingsHandler = new AsyncCallbackJsonWebHandler("/api/system/settings", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!this->authManager.isAuthenticated(request, ROLE_ADMIN)) {
            request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }

        JsonObject jsonObj = json.as<JsonObject>();
        
        if (jsonObj.containsKey("logLevel")) {
            int logLevel = jsonObj["logLevel"];
            Serial.printf("Received new log level: %d\n", logLevel);
        }
        if (jsonObj.containsKey("timezone")) {
            int timezone = jsonObj["timezone"];
            Serial.printf("Received new timezone offset: %d\n", timezone);
        }
        
        request->send(200, "application/json", "{\"success\":true, \"message\":\"Settings received\"}");
    });
    server.addHandler(systemSettingsHandler);

    // --- User Management Endpoint ---
    server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        String json = this->userManager.listUsersJson();
        req->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* userHandler = new AsyncCallbackJsonWebHandler("/api/users", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!this->authManager.isAuthenticated(request, ROLE_ADMIN)) {
            request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }

        JsonObject jsonObj = json.as<JsonObject>();
        
        if (request->method() == HTTP_POST) {
            String uid = jsonObj["uid"];
            String name = jsonObj["name"];
            if (this->userManager.addUser(uid, name)) {
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"message\":\"Failed to add user\"}");
            }
        }
        else if (request->method() == HTTP_DELETE) {
            String uid = jsonObj["uid"];
            if (this->userManager.removeUser(uid)) {
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"message\":\"User not found\"}");
            }
        }
        else {
            request->send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
        }
    });
    server.addHandler(userHandler);

    server.on("/api/serve-coffee", HTTP_POST, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_USER)) {
            req->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        bool success = this->coffeeController.serveCoffee("WEB_MANUAL", nullptr);
        req->send(200, "application/json", String("{\"success\":") + (success ? "true" : "false") + "}");
    });

    server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        int limit = req->hasParam("limit") ? req->getParam("limit")->value().toInt() : 50;
        String json = this->logger.getLogsAsJson(limit);
        req->send(200, "application/json", "{\"logs\":" + json + "}");
    });

    server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!this->authManager.isAuthenticated(request, ROLE_ADMIN)) {
            request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        StaticJsonDocument<2048> doc;
        JsonObject kpis = doc.createNestedObject("kpis");
        kpis["totalServed"] = this->coffeeController.getTotalServed();
        kpis["dailyAverage"] = String(this->logger.getDailyAverage(7), 1);
        kpis["peakDay"] = this->logger.getPeakDayOfWeek(7);
        std::vector<UserCredits> topUsersList = this->userManager.getTopUsersByConsumption(1);
        if (!topUsersList.empty()) {
            kpis["topUser"] = topUsersList[0].name;
        } else {
            kpis["topUser"] = "N/A";
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}


/* -------------------- WebSocket -------------------- */
void WebServerManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("🔌 WS client %u connected\n", client->id());
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("❌ WS client %u disconnected\n", client->id());
        } else if (type == WS_EVT_DATA) {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0;
                String msg = (char*)data;
                
                StaticJsonDocument<256> doc;
                deserializeJson(doc, msg);
                
                String msgType = doc["type"];

                if (msgType == "start_scan_for_add") {
                    if (this->rfidManager) {
                        this->rfidManager->setScanMode(SCAN_FOR_ADD);
                    }
                }
            }
        }
    });
    server.addHandler(&ws);
}

void WebServerManager::pushScannedUID(const String &uid) {
    StaticJsonDocument<128> doc;
    doc["type"] = "new_rfid_uid";
    JsonObject data = doc.createNestedObject("data");
    data["uid"] = uid;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}