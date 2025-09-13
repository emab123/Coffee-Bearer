#include "web_server.h"
#include "SPIFFS.h"

// <<< ALTERAÇÃO >>>
// Adicionar uma instância global para o WebSocket.
// O construtor recebe o "endpoint" onde o WebSocket irá operar.
AsyncWebSocket ws("/ws");

// Referências externas que já existiam
extern AuthManager authManager;
extern UserManager userManager;
extern CoffeeController coffeeController;
extern Logger logger;

// <<< ALTERAÇÃO >>>
// Adicionamos um novo handler de eventos para o WebSocket.
// Esta será a função central para toda a comunicação em tempo real.
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

WebServerManager::WebServerManager(AsyncWebServer& serverRef) : 
    server(serverRef),
    authManager(nullptr),
    userManager(nullptr),
    coffeeController(nullptr),
    logger(nullptr) {
}

bool WebServerManager::begin() {
    this->authManager = &::authManager;
    this->userManager = &::userManager;
    this->coffeeController = &::coffeeController;
    this->logger = &::logger;
    
    // <<< ALTERAÇÃO >>>
    // Configurar o handler de eventos para o nosso WebSocket.
    ws.onEvent(onWebSocketEvent);
    // Anexar o WebSocket ao servidor web principal.
    server.addHandler(&ws);

    // As rotas estáticas e de autenticação continuam as mesmas.
    setupStaticRoutes();
    setupAuthRoutes();
    
    // <<< ALTERAÇÃO >>>
    // As rotas de API e páginas de admin/user serão simplificadas,
    // pois a maior parte da lógica passará pelo WebSocket.
    setupAdminRoutes();
    setupUserRoutes();

    // <<< ALTERAÇÃO >>>
    // As rotas de API REST serão removidas/comentadas,
    // pois sua funcionalidade será substituída por mensagens WebSocket.
    // setupApiRoutes(); // Esta função não é mais necessária.

    DEBUG_PRINTLN("Web Server Manager inicializado com WebSocket");
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

// <<< ALTERAÇÃO >>>
// Nova função para enviar uma mensagem a todos os clientes conectados.
void WebServerManager::notifyAllClients(const String& message) {
    ws.textAll(message);
}

// <<< ALTERAÇÃO >>>
// Nova função para enviar o status atualizado para todos.
// Outros módulos (como RFID_manager) chamarão esta função.
void WebServerManager::broadcastStatusUpdate() {
    JsonDocument doc;
    doc["type"] = "system_status"; // Define o tipo de mensagem
    
    // Monta o objeto de dados (similar ao antigo handleApiStatus)
    JsonObject data = doc["data"].to<JsonObject>();
    data["system"]["version"] = SYSTEM_VERSION;
    data["system"]["uptime"] = millis();
    data["system"]["freeHeap"] = ESP.getFreeHeap();
    data["system"]["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    data["system"]["wifiIP"] = WiFi.localIP().toString();
    
    data["coffee"]["totalServed"] = coffeeController->getTotalServed();
    data["coffee"]["remaining"] = coffeeController->getRemainingCoffees();
    data["coffee"]["isBusy"] = coffeeController->isBusy();

    data["users"]["total"] = userManager->getTotalUsers();
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    notifyAllClients(jsonString); // Envia para todos os clientes
}

// <<< ALTERAÇÃO >>>
// Nova função para lidar com as mensagens recebidas via WebSocket.
// Ela funciona como um roteador.
void WebServerManager::handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);

    if (error) {
        logger->warning("Falha ao parsear JSON do WebSocket: " + String(error.c_str()));
        return;
    }

    String type = doc["type"];
    JsonObject payload = doc["data"];
    
    logger->debug("Mensagem WS recebida: " + type);

    // Roteador de mensagens
    if (type == "get_status") {
        // Quando o cliente se conecta, ele pede o status inicial
        sendFullStatus(client);
    } 
    else if (type == "get_users") {
        sendUserList(client);
    }
    else if (type == "add_user") {
        String uid = payload["uid"];
        String name = payload["name"];
        if (userManager->addUser(uid, name)) {
            // Avisa a todos que a lista de usuários mudou
            broadcastUserList(); 
        }
    }
    else if (type == "remove_user") {
        String uid = payload["uid"];
        if (userManager->removeUser(uid)) {
            broadcastUserList();
        }
    }
    else if (type == "serve_coffee") {
        if(coffeeController->serveCoffee("WEB_ADMIN", nullptr)) {
            broadcastStatusUpdate();
        }
    }
    // Adicione outros tipos de mensagem aqui (ex: "get_logs", "change_settings", etc.)
}

// <<< ALTERAÇÃO >>>
// Esta é a implementação do handler de eventos que declaramos anteriormente.
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WebServerManager* self = (WebServerManager*) &webServer; // Acessa a instância global
    
    switch (type) {
        case WS_EVT_CONNECT:
            self->logger->info("Cliente WebSocket conectado: " + String(client->id()));
            // Quando um cliente se conecta, enviamos o status completo para ele.
            self->sendFullStatus(client);
            break;
        case WS_EVT_DISCONNECT:
            self->logger->info("Cliente WebSocket desconectado: " + String(client->id()));
            break;
        case WS_EVT_DATA:
            // Quando recebemos dados, passamos para o nosso roteador de mensagens.
            self->handleWebSocketMessage(client, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}


// O resto do arquivo permanece similar, mas as rotas de API são removidas.

void WebServerManager::setupStaticRoutes() {
    // Esta parte não muda
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->redirect("/login");
    });
    server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("login.html");
}

void WebServerManager::setupAuthRoutes() {
    // Esta parte não muda
    server.on("/auth/login", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleLogin(request);
    });
    server.on("/auth/logout", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleLogout(request);
    });
    server.on("/auth/check", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleAuthCheck(request);
    });
}

// As rotas de páginas específicas (admin/user) continuam, pois precisam servir os arquivos HTML.
void WebServerManager::setupAdminRoutes() {
    server.on("/admin/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/dashboard.html", MIME_HTML);
    });
    server.on("/admin/users", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/users.html", MIME_HTML);
    });
     server.on("/admin/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/settings.html", MIME_HTML);
    });
    server.on("/admin/logs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/logs.html", MIME_HTML);
    });
    server.on("/admin/stats", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_ADMIN)) return;
        request->send(SPIFFS, "/web/admin/stats.html", MIME_HTML);
    });
}

void WebServerManager::setupUserRoutes() {
    server.on("/user/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/dashboard.html", MIME_HTML);
    });
    server.on("/user/profile", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/profile.html", MIME_HTML);
    });
    server.on("/user/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!requireAuth(request, ROLE_USER)) return;
        request->send(SPIFFS, "/web/user/history.html", MIME_HTML);
    });
}

// As funções de handler de login/logout/check não mudam, pois são o ponto de entrada.
void WebServerManager::handleLogin(AsyncWebServerRequest *request) {
    // ... (código original sem alterações)
}

void WebServerManager::handleLogout(AsyncWebServerRequest *request) {
    // ... (código original sem alterações)
}

void WebServerManager::handleAuthCheck(AsyncWebServerRequest *request) {
    // ... (código original sem alterações)
}

// Funções utilitárias como getClientIP, requireAuth, sendJsonResponse, etc. não mudam.
String WebServerManager::getClientIP(AsyncWebServerRequest *request) {
    // ... (código original sem alterações)
    return request->client()->remoteIP().toString();
}

bool WebServerManager::requireAuth(AsyncWebServerRequest *request, UserRole minimumRole) {
    // ... (código original sem alterações)
    return true; // Placeholder
}

void WebServerManager::sendJsonResponse(AsyncWebServerRequest *request, int code, const JsonDocument &json) {
    // ... (código original sem alterações)
}

// <<< ALTERAÇÃO >>>
// Funções auxiliares para enviar dados específicos via WebSocket
void WebServerManager::sendFullStatus(AsyncWebSocketClient *client) {
    JsonDocument doc;
    doc["type"] = "full_status"; // Mensagem inicial com todos os dados
    
    JsonObject data = doc["data"].to<JsonObject>();
    // ... (monta o JSON completo com status do sistema, café, usuários, etc.) ...

    String jsonString;
    serializeJson(doc, jsonString);
    client->text(jsonString); // Envia apenas para o cliente que solicitou
}

void WebServerManager::sendUserList(AsyncWebSocketClient *client) {
    JsonDocument doc;
    doc["type"] = "user_list";
    JsonArray userArray = doc["data"].to<JsonArray>();
    
    std::vector<UserCredits> userList = userManager->getAllUsers();
    for (const auto& user : userList) {
        JsonObject userObj = userArray.add<JsonObject>();
        userObj["uid"] = user.uid;
        userObj["name"] = user.name;
        userObj["credits"] = user.credits;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    client->text(jsonString);
}

void WebServerManager::broadcastUserList() {
    JsonDocument doc;
    doc["type"] = "user_list"; // O frontend usará esse tipo para atualizar a tabela
    JsonArray userArray = doc["data"].to<JsonArray>();
    
    std::vector<UserCredits> userList = userManager->getAllUsers();
    for (const auto& user : userList) {
        JsonObject userObj = userArray.add<JsonObject>();
        userObj["uid"] = user.uid;
        userObj["name"] = user.name;
        userObj["credits"] = user.credits;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    notifyAllClients(jsonString);
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