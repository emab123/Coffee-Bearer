#include "web_server.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include "system_utils.h"

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

/* -------------------- Static Routes (Corrected & Simplified) -------------------- */
void WebServerManager::setupStaticRoutes() {
    // Helper lambda to send a gzipped HTML file if it exists, otherwise the plain version
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

    // --- Explicit Page Routes ---
    server.on("/", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/login.html");
    });
    server.on("/admin/dashboard", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/admin/dashboard.html");
    });
    server.on("/admin/users", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/admin/users.html");
    });
    server.on("/admin/settings", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/admin/settings.html");
    });
    server.on("/admin/logs", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/admin/logs.html");
    });
    server.on("/admin/stats", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/admin/stats.html");
    });

    // --- User pages ---
     server.on("/user/dashboard", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/user/dashboard.html");
    });
     server.on("/user/profile", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/user/profile.html");
    });
     server.on("/user/history", HTTP_GET, [serveHtml](AsyncWebServerRequest *request) {
        serveHtml(request, "/web/user/history.html");
    });


    // --- Static Asset Handler (for CSS, JS, etc.) ---
    // This will automatically handle .gz compression if the file exists
    server.serveStatic("/css", SPIFFS, "/web/css").setCacheControl("max-age=31536000");
    server.serveStatic("/js", SPIFFS, "/web/js").setCacheControl("max-age=31536000");
    server.serveStatic("/favicon.ico", SPIFFS, "/web/favicon.ico");

    // --- Not Found Handler ---
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
        } else if (type == WS_EVT_DATA) {
            data[len] = 0;
            String msg = (char*)data;
            Serial.printf("📩 WS received: %s\n", msg.c_str());
            // TODO: parse actions like get_stats if needed
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