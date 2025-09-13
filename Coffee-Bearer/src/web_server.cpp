#include "web_server.h"
#include "SPIFFS.h"

extern AuthManager authManager;
extern UserManager userManager;
extern CoffeeController coffeeController;
extern Logger logger;

WebServerManager::WebServerManager(AsyncWebServer& serverRef) : 
    server(serverRef),
    authManager(nullptr),
    userManager(nullptr),
    coffeeController(nullptr),
    logger(nullptr) {
}

bool WebServerManager::begin() {
    // Configurar referências para os managers
    this->authManager = &::authManager;
    this->userManager = &::userManager;
    this->coffeeController = &::coffeeController;
    this->logger = &::logger;
    
    // Configurar rotas
    setupStaticRoutes();
    setupAuthRoutes();
    setupAdminRoutes();
    setupUserRoutes();
    setupApiRoutes();
    
    DEBUG_PRINTLN("Web Server Manager inicializado");
    return true;
}

void WebServerManager::start() {
    server.begin();
    DEBUG_PRINTLN("Servidor web iniciado na porta 80");
}

void WebServerManager::stop() {
    server.end();
    DEBUG_PRINTLN("Servidor web parado");
}

void WebServerManager::setupStaticRoutes() {
    // Página inicial - redireciona para login
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String sessionId = authManager->extractSessionFromCookie(request->header("Cookie"));
        
        if (authManager->isValidSession(sessionId)) {
            UserRole role = authManager->getSessionRole(sessionId);
            if (role == ROLE_ADMIN) {
                request->redirect("/admin/dashboard");
            } else {
                request->redirect("/user/dashboard");
            }
        } else {
            request->redirect("/login");
        }
    });
    
    // Página de login
    server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/login.html", MIME_HTML);
    });
    
    // Arquivos estáticos
    server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/css/style.css", MIME_CSS);
    });
    
    server.on("/js/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/js/app.js", MIME_JS);
    });
    
    server.on("/js/admin.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/js/admin.js", MIME_JS);
    });
    
    server.on("/js/user.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/web/js/user.js", MIME_JS);
    });
    
    // Favicon
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/web/favicon.ico")) {
            request->send(SPIFFS, "/web/favicon.ico", MIME_ICO);
        } else {
            request->send(404);
        }
    });
}

void WebServerManager::setupAuthRoutes() {
    // Login
    server.addHandler(new AsyncCallbackJsonWebHandler("/auth/login", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        handleLogin(request);
    }));
    
    // Logout
    server.on("/auth/logout", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleLogout(request);
    });
    
    // Verificar autenticação
    server.on("/auth/check", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleAuthCheck(request);
    });
    
    // Alterar senha
    server.addHandler(new AsyncCallbackJsonWebHandler("/auth/change-password", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!requireAuth(request, ROLE_USER)) return;
        
        String username = json["username"];
        String oldPassword = json["oldPassword"];
        String newPassword = json["newPassword"];
        
        JsonDocument response;
        if (authManager->changePassword(username, oldPassword, newPassword)) {
            response["success"] = true;
            response["message"] = "Senha alterada com sucesso";
            logger->info("Senha alterada para usuário: " + username);
        } else {
            response["success"] = false;
            response["message"] = "Falha ao alterar senha";
        }
        
        sendJsonResponse(request, 200, response);
    }));
}

void WebServerManager::setupAdminRoutes() {
    // Dashboard Admin
    server.on("/admin/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/dashboard.html", MIME_HTML);
    });
    
    // Gerenciamento de usuários
    server.on("/admin/users", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/users.html", MIME_HTML);
    });
    
    // Configurações do sistema
    server.on("/admin/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/settings.html", MIME_HTML);
    });
    
    // Logs do sistema
    server.on("/admin/logs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/logs.html", MIME_HTML);
    });
    
    // Estatísticas
    server.on("/admin/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/stats.html", MIME_HTML);
    });
}

void WebServerManager::setupUserRoutes() {
    // Dashboard Usuário
    server.on("/user/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/dashboard.html", MIME_HTML);
    });
    
    // Perfil do usuário
    server.on("/user/profile", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/profile.html", MIME_HTML);
    });
    
    // Histórico de café
    server.on("/user/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/history.html", MIME_HTML);
    });
}

void WebServerManager::setupApiRoutes() {
    // Status do sistema
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        handleApiStatus(request);
    });
    
    // Gerenciamento de usuários RFID
    server.on("/api/users", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiUsers(request);
    });
    
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/users", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        
        if (request->method() == HTTP_POST) {
            handleApiUserAdd(request, json);
        } else if (request->method() == HTTP_DELETE) {
            handleApiUserRemove(request, json);
        } else if (request->method() == HTTP_PUT) {
            handleApiUserUpdate(request, json);
        }
    }));
    
    // Controle de café
    server.on("/api/serve-coffee", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiServeCoffee(request);
    });
    
    server.on("/api/refill-coffee", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiRefillCoffee(request);
    });
    
    // Logs
    server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiLogs(request);
    });
    
    // Backup/Restore
    server.on("/api/backup", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiBackup(request);
    });
    
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/restore", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiRestore(request, json);
    }));
    
    // Reset do sistema
    server.on("/api/system-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        handleApiSystemReset(request);
    });
}

void WebServerManager::handleLogin(AsyncWebServerRequest *request) {
    if (!request->hasParam("username", true) || !request->hasParam("password", true)) {
        sendErrorResponse(request, 400, "Username e password são obrigatórios");
        return;
    }
    
    String username = request->getParam("username", true)->value();
    String password = request->getParam("password", true)->value();
    String clientIP = getClientIP(request);
    
    // Verificar se IP está bloqueado
    if (authManager->isIpBlocked(clientIP)) {
        unsigned long remaining = authManager->getBlockTimeRemaining(clientIP);
        JsonDocument response;
        response["success"] = false;
        response["message"] = "IP bloqueado. Tente novamente em " + String(remaining / 60000) + " minutos";
        response["blocked"] = true;
        response["remainingTime"] = remaining;
        sendJsonResponse(request, 429, response);
        return;
    }
    
    String sessionId = authManager->login(username, password, clientIP);
    
    JsonDocument response;
    if (sessionId.length() > 0) {
        UserRole role = authManager->getSessionRole(sessionId);
        response["success"] = true;
        response["sessionId"] = sessionId;
        response["role"] = authManager->roleToString(role);
        response["redirectUrl"] = (role == ROLE_ADMIN) ? "/admin/dashboard" : "/user/dashboard";
        
        // Definir cookie de sessão
        AsyncWebServerResponse *resp = request->beginResponse(200, MIME_JSON, response.as<String>());
        resp->addHeader("Set-Cookie", authManager->createSessionCookie(sessionId));
        request->send(resp);
        
        logger->info("Login realizado: " + username + " (" + clientIP + ")");
    } else {
        response["success"] = false;
        response["message"] = "Credenciais inválidas";
        sendJsonResponse(request, 401, response);
        
        logger->warning("Tentativa de login falhada: " + username + " (" + clientIP + ")");
    }
}

void WebServerManager::handleLogout(AsyncWebServerRequest *request) {
    String sessionId = authManager->extractSessionFromCookie(request->header("Cookie"));
    
    if (authManager->logout(sessionId)) {
        AsyncWebServerResponse *resp = request->beginResponse(200, MIME_JSON, "{\"success\":true}");
        resp->addHeader("Set-Cookie", "session_id=; Path=/; HttpOnly; Max-Age=0");
        request->send(resp);
        
        logger->info("Logout realizado para sessão: " + sessionId.substring(0, 8) + "...");
    } else {
        sendErrorResponse(request, 400, "Sessão inválida");
    }
}

void WebServerManager::handleAuthCheck(AsyncWebServerRequest *request) {
    String sessionId = authManager->extractSessionFromCookie(request->header("Cookie"));
    
    JsonDocument response;
    if (authManager->isValidSession(sessionId)) {
        authManager->updateSessionAccess(sessionId);
        AuthSession* session = authManager->getSession(sessionId);
        
        response["authenticated"] = true;
        response["username"] = session->username;
        response["role"] = authManager->roleToString(session->role);
        response["sessionTime"] = millis() - session->createdAt;
    } else {
        response["authenticated"] = false;
    }
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiStatus(AsyncWebServerRequest *request) {
    JsonDocument response;
    
    response["system"]["version"] = SYSTEM_VERSION;
    response["system"]["uptime"] = millis();
    response["system"]["freeHeap"] = ESP.getFreeHeap();
    response["system"]["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    response["system"]["wifiIP"] = WiFi.localIP().toString();
    
    response["coffee"]["totalServed"] = coffeeController->getTotalServed();
    response["coffee"]["remaining"] = coffeeController->getRemainingCoffees();
    response["coffee"]["maxCapacity"] = MAX_COFFEES;
    response["coffee"]["isBusy"] = coffeeController->isBusy();
    response["coffee"]["lastServed"] = coffeeController->getLastServedTime();
    
    response["users"]["total"] = userManager->getTotalUsers();
    response["users"]["maxUsers"] = MAX_USERS;
    response["users"]["activeToday"] = userManager->getActiveTodayCount();
    
    response["auth"]["activeSessions"] = authManager->getActiveSessionCount();
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiUsers(AsyncWebServerRequest *request) {
    JsonDocument response;
    JsonArray users = response["users"].to<JsonArray>();
    
    std::vector<UserCredits> userList = userManager->getAllUsers();
    for (const auto& user : userList) {
        JsonObject userObj = users.add<JsonObject>();
        userObj["uid"] = user.uid;
        userObj["name"] = user.name;
        userObj["credits"] = user.credits;
        userObj["lastUsed"] = user.lastUsed;
        userObj["isActive"] = user.isActive;
    }
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiUserAdd(AsyncWebServerRequest *request, JsonVariant &json) {
    String uid = json["uid"];
    String name = json["name"];
    
    JsonDocument response;
    if (userManager->addUser(uid, name)) {
        response["success"] = true;
        response["message"] = "Usuário adicionado com sucesso";
        logger->info("Usuário RFID adicionado via API: " + name + " (UID: " + uid + ")");
    } else {
        response["success"] = false;
        response["message"] = "Falha ao adicionar usuário";
    }
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiUserRemove(AsyncWebServerRequest *request, JsonVariant &json) {
    String uid = json["uid"];
    
    JsonDocument response;
    if (userManager->removeUser(uid)) {
        response["success"] = true;
        response["message"] = "Usuário removido com sucesso";
        logger->info("Usuário RFID removido via API: " + uid);
    } else {
        response["success"] = false;
        response["message"] = "Usuário não encontrado";
    }
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiServeCoffee(AsyncWebServerRequest *request) {
    JsonDocument response;
    
    if (coffeeController->serveCoffee("WEB_ADMIN", nullptr)) {
        response["success"] = true;
        response["message"] = "Café servido com sucesso";
        logger->info("Café servido via interface web");
    } else {
        response["success"] = false;
        response["message"] = "Não foi possível servir café";
    }
    
    sendJsonResponse(request, 200, response);
}

void WebServerManager::handleApiLogs(AsyncWebServerRequest *request) {
    JsonDocument response;
    JsonArray logs = response["logs"].to<JsonArray>();
    
    std::vector<String> logEntries = logger->getRecentLogs(100);
    for (const String& entry : logEntries) {
        logs.add(entry);
    }
    
    sendJsonResponse(request, 200, response);
}

// Métodos utilitários

String WebServerManager::getClientIP(AsyncWebServerRequest *request) {
    if (request->hasHeader("X-Forwarded-For")) {
        return request->header("X-Forwarded-For");
    } else if (request->hasHeader("X-Real-IP")) {
        return request->header("X-Real-IP");
    } else {
        return request->client()->remoteIP().toString();
    }
}

bool WebServerManager::requireAuth(AsyncWebServerRequest *request, UserRole minimumRole) {
    String sessionId = authManager->extractSessionFromCookie(request->header("Cookie"));
    
    if (!authManager->requireAuth(sessionId, minimumRole)) {
        sendAuthRequiredResponse(request);
        return false;
    }
    
    authManager->updateSessionAccess(sessionId);
    return true;
}

void WebServerManager::sendJsonResponse(AsyncWebServerRequest *request, int code, const JsonDocument &json) {
    String response;
    serializeJson(json, response);
    request->send(code, MIME_JSON, response);
}

void WebServerManager::sendErrorResponse(AsyncWebServerRequest *request, int code, const String &message) {
    JsonDocument response;
    response["success"] = false;
    response["message"] = message;
    sendJsonResponse(request, code, response);
}

void WebServerManager::sendAuthRequiredResponse(AsyncWebServerRequest *request) {
    JsonDocument response;
    response["success"] = false;
    response["message"] = "Autenticação necessária";
    response["requiresAuth"] = true;
    sendJsonResponse(request, 401, response);
}