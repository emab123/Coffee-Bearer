#include "web_server.h"
#include "auth_manager.h"
#include "logger.h"
#include "user_manager.h"
#include "coffee_controller.h"
#include "system_utils.h"

#include <FS.h>
#include <SPIFFS.h>

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
    server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("login.html");
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
            AsyncWebServerResponse *res = req->beginResponse(200, "application/json",
                "{\"success\":true,\"redirectUrl\":\"/admin/dashboard\"}");
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
        bool ok = this->authManager.isAuthenticated(req);
        String role = ok ? this->authManager.getUserRoleFromRequest(req) : "";
        String json = "{\"authenticated\":" + String(ok ? "true" : "false") +
                      ",\"role\":\"" + role + "\"}";
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

        StaticJsonDocument<512> doc;
        systemStatusToJson(doc, this->logger, this->coffeeController);
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        String json = this->userManager.listUsersJson();
        req->send(200, "application/json", json);
    });

    server.on("/api/users", HTTP_POST, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        if (!req->hasParam("uid", true) || !req->hasParam("nome", true)) {
            req->send(400, "application/json", "{\"error\":\"Missing fields\"}");
            return;
        }
        String uid = req->getParam("uid", true)->value();
        String nome = req->getParam("nome", true)->value();
        bool ok = this->userManager.addUser(uid, nome);
        req->send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + "}");
    });

    server.on("/api/users", HTTP_DELETE, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        if (!req->hasParam("uid", true)) {
            req->send(400, "application/json", "{\"error\":\"Missing uid\"}");
            return;
        }
        String uid = req->getParam("uid", true)->value();
        bool ok = this->userManager.removeUser(uid);
        req->send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + "}");
    });

    server.on("/api/serve-coffee", HTTP_POST, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_USER)) {
            req->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        this->coffeeController.serveCoffee();
        req->send(200, "application/json", "{\"success\":true}");
    });

    server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest *req) {
        if (!this->authManager.isAuthenticated(req, ROLE_ADMIN)) {
            req->send(403, "application/json", "{\"error\":\"Forbidden\"}");
            return;
        }
        int limit = req->hasParam("limit") ? req->getParam("limit")->value().toInt() : 50;
        String json = this->logger.getLogsAsJson(limit);
        req->send(200, "application/json", json);
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
    StaticJsonDocument<256> doc;
    systemStatusToJson(doc, this->logger, this->coffeeController);
    String json;
    serializeJson(doc, json);
    ws.textAll("{\"type\":\"status-update\",\"data\":" + json + "}");
}

void WebServerManager::pushLog(const String &log) {
    ws.textAll("{\"type\":\"log-entry\",\"data\":\"" + log + "\"}");
}

void WebServerManager::pushUserUpdate(const String &uid) {
    ws.textAll("{\"type\":\"user-update\",\"uid\":\"" + uid + "\"}");
}
