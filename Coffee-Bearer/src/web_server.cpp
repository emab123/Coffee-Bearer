#include "web_server.h"
#include "auth_manager.h"
#include "logger.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "system_utils.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>

// Constructor
WebServerManager::WebServerManager(AuthManager &auth, Logger &log, UserManager &users, CoffeeController &coffee)
    : server(80), ws("/ws"), authManager(auth), logger(log), userManager(users), coffeeController(coffee) {}

void WebServerManager::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("⚠️ SPIFFS mount failed");
    }

    setupStaticRoutes();
    setupAuthRoutes();
    setupApiRoutes();
    setupWebSocket();

    server.begin();
    Serial.println("🌐 Web server started");
}

/* -------------------- Static Routes -------------------- */
void WebServerManager::setupStaticRoutes() {
    server.serveStatic("/", SPIFFS, "/web/")
        .setDefaultFile("login.html")
        .setCacheControl("max-age=600")
        .setFilter([](AsyncWebServerRequest *request) {
            String path = request->url();
            if (path.endsWith("/")) {
                path += "login.html";
            } else if (!path.endsWith(".html") && !path.endsWith(".css") && !path.endsWith(".js") && !path.endsWith(".ico") && !path.endsWith(".png")) {
                path += ".html";
            }

            String gzipped_path = path + ".gz";
            if (SPIFFS.exists(gzipped_path)) {
                String contentType = "text/plain";
                if (path.endsWith(".html")) contentType = "text/html";
                else if (path.endsWith(".css")) contentType = "text/css";
                else if (path.endsWith(".js")) contentType = "text/javascript";
                
                AsyncWebServerResponse *response = request->beginResponse(SPIFFS, gzipped_path, contentType);
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
                return false; 
            }
            return true;
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
            String redirectUrl = (role == ROLE_ADMIN) ? "/admin/dashboard.html" : "/user/dashboard.html";
            
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

    // --- User Management Endpoint ---
    
    // GET /api/users - List all users
    server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        String json = this->userManager.listUsersJson();
        req->send(200, "application/json", json);
    });

    // This single handler will process POST (add) and DELETE (remove) with a JSON body
    AsyncCallbackJsonWebHandler* userHandler = new AsyncCallbackJsonWebHandler("/api/users", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!this->authManager.isAuthenticated(request, ROLE_ADMIN)) {
            request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }

        JsonObject jsonObj = json.as<JsonObject>();
        
        // ADD USER (POST)
        if (request->method() == HTTP_POST) {
            String uid = jsonObj["uid"];
            String name = jsonObj["name"];
            if (this->userManager.addUser(uid, name)) {
                request->send(200, "application/json", "{\"success\":true}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"message\":\"Failed to add user\"}");
            }
        }
        // REMOVE USER (DELETE)
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
}


/* -------------------- WebSocket -------------------- */
void WebServerManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("🔌 WS client %u connected\n", client->id());
            this->pushStatus();
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("❌ WS client %u disconnected\n", client->id());
        }
    });

    server.addHandler(&ws);
}

void WebServerManager::pushStatus() {
    StaticJsonDocument<1024> doc;
    systemStatusToJson(doc, this->logger, this->coffeeController, this->userManager, this->authManager);
    String json;
    serializeJson(doc, json);
    ws.textAll("{\"type\":\"system_status\",\"data\":" + json + "}");
}

void WebServerManager::pushLog(const String &log) {
    ws.textAll("{\"type\":\"log_entry\",\"data\":" + log + "}");
}

void WebServerManager::pushUserUpdate(const String &uid) {
    UserCredits* user = this->userManager.getUserByUID(uid);
    if(user){
        String userJson = this->userManager.userToJson(*user);
        ws.textAll("{\"type\":\"user_activity\",\"data\":" + userJson + "}");
    }
}