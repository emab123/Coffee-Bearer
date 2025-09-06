/*
==================================================
SISTEMA CAFETEIRA RFID - v3.3 - VERSÃO LIMPA
Controle de Nível, Chave Mestra e Reset Semanal de Créditos
==================================================
*/

#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include "SPIFFS.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "time.h"
#include "credentials.h"

// ============== CONFIGURAÇÕES DOS PINOS ==============
#define RST_PIN 4
#define SS_PIN 5
#define BUZZER_PIN 15
#define RELAY_PIN 13

// ============== CONFIGURAÇÕES DA CHAVE MESTRA E RESET ==============
#define INTERVALO_RESET_SEMANAL (7 * 24 * 60 * 60 * 1000UL) // 7 dias em milissegundos
#define INTERVALO_CHECK_RESET 3600000UL // Verifica a cada hora

// ============== CONFIGURAÇÕES WIFI E HORA (NTP) ==============
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; // GMT -3 (São Paulo)
const int daylightOffset_sec = 0;

// ============== ESTRUTURA DE DADOS DO USUÁRIO ==============
struct Usuario {
  String uid;
  String nome;
  int creditos;
};

// ============== VARIÁVEIS GLOBAIS ==============
MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;
AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

// Novo semáforo para controle de concorrência
SemaphoreHandle_t xSemaphore = NULL;

Usuario usuarios[MAX_USUARIOS];
int total_usuarios = 0;

unsigned long ultimo_tempo_leitura = 0;
String ultimo_uid_lido = "";

// Estatísticas e Status do Sistema
int total_cafes_servidos = 0;
int cafes_restantes = MAX_CAFES;
unsigned long tempo_total_funcionamento = 0;
bool sistema_ocupado = false;
String ultimo_evento = "Sistema inicializado";
unsigned long tempo_ultimo_evento = 0;

// Controle do Reset Semanal
unsigned long ultimo_reset_semanal = 0;
unsigned long ultimo_check_reset = 0;

// ============== FUNÇÃO DE LOG ==============
void registrar_log(String evento) {
    if(!SPIFFS.exists("/datalog.txt")){
        Serial.println("Criando arquivo de log...");
        File file = SPIFFS.open("/datalog.txt", FILE_WRITE);
        if(!file){
            Serial.println("Falha ao criar arquivo de log");
            return;
        }
        file.println("======== LOG DE EVENTOS DA CAFETEIRA ========");
        file.close();
    }

    File file = SPIFFS.open("/datalog.txt", FILE_APPEND);
    if(!file){
        Serial.println("Falha ao abrir arquivo de log para escrita");
        return;
    }

    timeClient.update();
    String timestamp = timeClient.getFormattedTime();
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
        char dateString[20];
        strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo);
        String log_entry = String(dateString) + " " + timestamp + " - " + evento;
        file.println(log_entry);
        Serial.println("LOG: " + log_entry);
    }
    file.close();
}

// ============== FUNÇÕES DE PERSISTÊNCIA ==============
void salvar_dados() {
  Serial.println("Salvando dados na memória flash...");
  preferences.begin("cafeteira", false);
  preferences.putInt("total_users", total_usuarios);
  for (int i = 0; i < total_usuarios; i++) {
    String key_uid = "uid_" + String(i);
    String key_nome = "nome_" + String(i);
    String key_cred = "cred_" + String(i);
    preferences.putString(key_uid.c_str(), usuarios[i].uid);
    preferences.putString(key_nome.c_str(), usuarios[i].nome);
    preferences.putInt(key_cred.c_str(), usuarios[i].creditos);
  }
  preferences.putInt("cafes_servidos", total_cafes_servidos);
  preferences.putULong("tempo_funcionamento", tempo_total_funcionamento);
  preferences.putInt("cafes_restantes", cafes_restantes);
  preferences.putULong("ultimo_reset", ultimo_reset_semanal);
  preferences.end();
  Serial.println("Dados salvos com sucesso!");
}

void carregar_dados() {
  Serial.println("Carregando dados da memória flash...");
  preferences.begin("cafeteira", true);
  total_usuarios = preferences.getInt("total_users", 0);
  for (int i = 0; i < total_usuarios; i++) {
    String key_uid = "uid_" + String(i);
    String key_nome = "nome_" + String(i);
    String key_cred = "cred_" + String(i);
    usuarios[i].uid = preferences.getString(key_uid.c_str(), "");
    usuarios[i].nome = preferences.getString(key_nome.c_str(), "");
    usuarios[i].creditos = preferences.getInt(key_cred.c_str(), 0);
  }
  total_cafes_servidos = preferences.getInt("cafes_servidos", 0);
  tempo_total_funcionamento = preferences.getULong("tempo_funcionamento", 0);
  cafes_restantes = preferences.getInt("cafes_restantes", MAX_CAFES);
  ultimo_reset_semanal = preferences.getULong("ultimo_reset", 0);
  preferences.end();
  Serial.print("Carregados ");
  Serial.print(total_usuarios);
  Serial.println(" usuários da memória!");
  Serial.print(cafes_restantes);
  Serial.println(" cafés restantes na garrafa.");
}

void limpar_dados() {
  Serial.println("Limpando todos os dados salvos...");
  preferences.begin("cafeteira", false);
  preferences.clear();
  preferences.end();
  total_usuarios = 0;
  total_cafes_servidos = 0;
  tempo_total_funcionamento = 0;
  cafes_restantes = MAX_CAFES;
  for (int i = 0; i < MAX_USUARIOS; i++) {
    usuarios[i] = {}; // Limpa a struct
  }
  Serial.println("Todos os dados foram apagados!");
  registrar_log("TODOS OS DADOS FORAM APAGADOS");
}

// ============== FUNÇÕES DE SOM ==============
void som_autorizado() { 
    tone(BUZZER_PIN, 1200, 80); 
    delay(100); 
    tone(BUZZER_PIN, 1500, 80); 
}

void som_negado() { 
    tone(BUZZER_PIN, 200, 400); 
}

void som_inicializacao() { 
    tone(BUZZER_PIN, 800, 60); 
    delay(80); 
    tone(BUZZER_PIN, 1000, 60); 
    delay(80); 
    tone(BUZZER_PIN, 1200, 60); 
}

void som_cafe_pronto() { 
    tone(BUZZER_PIN, 1300, 50); 
    delay(60); 
    tone(BUZZER_PIN, 1300, 50); 
    delay(60); 
    tone(BUZZER_PIN, 1300, 50); 
    delay(60); 
    tone(BUZZER_PIN, 1600, 100); 
}

void som_dados_salvos() { 
    tone(BUZZER_PIN, 1800, 50); 
    delay(60); 
    tone(BUZZER_PIN, 1800, 50); 
}

void som_sem_cafe() { 
    tone(BUZZER_PIN, 440, 150); 
    delay(160); 
    tone(BUZZER_PIN, 440, 150); 
    delay(160); 
    tone(BUZZER_PIN, 440, 150); 
}

void som_reabastecido() { 
    tone(BUZZER_PIN, 1500, 80); 
    delay(100); 
    tone(BUZZER_PIN, 1800, 80); 
    delay(100); 
    tone(BUZZER_PIN, 2200, 120); 
}

// ============== FUNÇÕES DE CONTROLE ==============
void servir_cafe(String nome_usuario, int* creditos_ptr = nullptr) {
    
    if (xSemaphoreTake(xSemaphore, (TickType_t) 10) == pdTRUE) {
    sistema_ocupado = true;
    ultimo_evento = "Servindo café para " + nome_usuario;
    tempo_ultimo_evento = millis();

    Serial.println("\n===============================");
    Serial.println(" SERVINDO CAFÉ PARA " + nome_usuario);
    Serial.println("===============================");

    digitalWrite(RELAY_PIN, HIGH);
    for (int i = TEMPO_CAFE_MS / 1000; i > 0; i--) {
        Serial.print(" Servindo... "); 
        Serial.print(i); 
        Serial.println(" segundos");
        delay(1000);
    }
    digitalWrite(RELAY_PIN, LOW);
    Serial.println(" Café servido com sucesso!");
    som_cafe_pronto();

    // Decrementa créditos apenas se não for uso manual
    if (creditos_ptr != nullptr && nome_usuario != "MANUAL" && nome_usuario != "SERIAL") {
        (*creditos_ptr)--;
    }
    
    total_cafes_servidos++;
    cafes_restantes--;

    String log_msg = "Cafe servido para " + nome_usuario;
    if (creditos_ptr != nullptr) {
        log_msg += ". Creditos restantes: " + String(*creditos_ptr);
    }
    log_msg += ". Cafes na garrafa: " + String(cafes_restantes);
    
    registrar_log(log_msg);
    salvar_dados();

    ultimo_evento = "Café servido para " + nome_usuario;
    tempo_ultimo_evento = millis();
    sistema_ocupado = false;

    xSemaphoreGive(xSemaphore);

    Serial.println("\nAguardando próxima tag...");
    } else {
        Serial.println("ERRO DE CONCORRÊNCIA: Acesso bloqueado pelo semáforo.");}
  }

// ============== FUNÇÕES DE USUÁRIOS ==============
bool adicionar_usuario(String uid, String nome) {
    if (total_usuarios >= MAX_USUARIOS) { 
        Serial.println("Erro: Limite máximo de usuários atingido!"); 
        return false; 
    }
    
    for (int i = 0; i < total_usuarios; i++) {
        if (usuarios[i].uid == uid) { 
            Serial.println("UID já cadastrado para: " + usuarios[i].nome); 
            return false; 
        }
    }

    usuarios[total_usuarios].uid = uid;
    usuarios[total_usuarios].nome = nome;
    usuarios[total_usuarios].creditos = CREDITOS_INICIAIS;
    total_usuarios++;

    Serial.println("Usuário adicionado: " + nome + " com " + String(CREDITOS_INICIAIS) + " créditos.");
    registrar_log("Usuario adicionado: " + nome + " (UID: " + uid + ")");
    salvar_dados();

    ultimo_evento = "Usuário adicionado: " + nome;
    tempo_ultimo_evento = millis();

    return true;
}

bool remover_usuario(String uid) {
    for (int i = 0; i < total_usuarios; i++) {
        if (usuarios[i].uid == uid) {
            String nome = usuarios[i].nome;
            for (int j = i; j < total_usuarios - 1; j++) {
                usuarios[j] = usuarios[j + 1];
            }
            total_usuarios--;
            // Limpa completamente a memória da última posição do array
            memset(&usuarios[total_usuarios], 0, sizeof(Usuario));

            Serial.println("Usuário removido: " + nome);
            registrar_log("Usuario removido: " + nome + " (UID: " + uid + ")");
            salvar_dados();

            ultimo_evento = "Usuário removido: " + nome;
            tempo_ultimo_evento = millis();

            return true;
        }
    }
    Serial.println("UID não encontrado na lista");
    return false;
}

// Retorna o índice do usuário se autorizado, ou -1 se negado/não encontrado
int verificar_autorizacao(String uid) {
    for (int i = 0; i < total_usuarios; i++) {
        if (usuarios[i].uid == uid) {
            if (usuarios[i].creditos > 0) {
                return i; // Autorizado, retorna o índice
            } else {
                Serial.println("ACESSO NEGADO - SEM CREDITOS");
                ultimo_evento = "Acesso negado (sem creditos): " + usuarios[i].nome;
                tempo_ultimo_evento = millis();
                return -2; // Indica que o usuário foi encontrado mas não tem créditos
            }
        }
    }
    return -1; // Não autorizado / não encontrado
}

// ============== FUNÇÃO DA CHAVE MESTRA ==============
void reabastecer_cafe() {
    Serial.println("\n===============================");
    Serial.println("      TAG MESTRA DETECTADA     ");
    Serial.println("===============================");
    cafes_restantes = MAX_CAFES;
    ultimo_evento = "Garrafa de café reabastecida!";
    tempo_ultimo_evento = millis();
    registrar_log("Garrafa de cafe reabastecida via tag mestra.");
    salvar_dados();
    som_reabastecido();
    Serial.println("Cafeteira reabastecida. Nível: " + String(cafes_restantes));
}

// ============== FUNÇÕES DE RFID ==============
String ler_uid_tag() {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        uid.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    uid.toUpperCase();
    uid.trim();
    return uid;
}

void processar_tag_detectada() {
    String uid = ler_uid_tag();
    unsigned long tempo_atual = millis();

    if (uid == ultimo_uid_lido && (tempo_atual - ultimo_tempo_leitura) < COOLDOWN_MS) { 
        return; 
    }

    ultimo_uid_lido = uid;
    ultimo_tempo_leitura = tempo_atual;

    Serial.println("\n===============================");
    Serial.println(" TAG DETECTADA: " + uid);
    Serial.println("===============================");

    // 1. Checar se é a tag mestra
    if (uid == UID_MESTRE) {
        reabastecer_cafe();
        return; // Finaliza o processamento aqui
    }

    // 2. Checar se há café na garrafa
    if (cafes_restantes <= 0) {
        Serial.println(" ACESSO NEGADO - SEM CAFÉ NA GARRAFA!");
        ultimo_evento = "Tentativa de uso sem café na garrafa.";
        tempo_ultimo_evento = millis();
        som_sem_cafe();
        return; // Finaliza o processamento aqui
    }

    // 3. Processar como usuário normal
    int usuario_index = verificar_autorizacao(uid);

    if (usuario_index >= 0) {
        Serial.println(" ACESSO AUTORIZADO");
        som_autorizado();
        servir_cafe(usuarios[usuario_index].nome, &usuarios[usuario_index].creditos);
    } else {
        Serial.println(" ACESSO NEGADO");
        if (usuario_index == -2) {
            // Usuário sem créditos - som já foi tocado na função verificar_autorizacao
        } else {
            ultimo_evento = "Acesso negado - UID não cadastrado: " + uid;
            tempo_ultimo_evento = millis();
        }
        som_negado();
    }
}

// ============== FUNÇÕES WIFI E API ==============
void inicializar_wifi() {
    Serial.print("Conectando a ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());

    // Inicializa o cliente de tempo
    timeClient.begin();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    registrar_log("Sistema inicializado e conectado ao WiFi.");
}

void configurar_rotas_api() {
    // ---- ROTAS PARA SERVIR OS ARQUIVOS DA PÁGINA WEB ----
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/script.js", "text/javascript");
    });

    // ---- ROTAS DE API ----
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        json["total_usuarios"] = total_usuarios;
        json["total_cafes_servidos"] = total_cafes_servidos;
        json["sistema_ocupado"] = sistema_ocupado;
        json["ultimo_evento"] = ultimo_evento;
        json["cafes_restantes"] = cafes_restantes;
        json["max_cafes"] = MAX_CAFES;             
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    // ROTA CORRETA PARA LISTAR TODOS OS USUÁRIOS
    server.on("/api/usuarios", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        JsonArray usuarios_json = json["usuarios"].to<JsonArray>();
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios_json.add<JsonObject>();
            usuario["uid"] = usuarios[i].uid;
            usuario["nome"] = usuarios[i].nome;
            usuario["creditos"] = usuarios[i].creditos;
        }
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    // ROTA PARA BUSCAR UM ÚNICO USUÁRIO
    server.on("/api/usuario", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("uid")) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"UID não fornecido\"}");
            return;
        }
        
        String uid = request->getParam("uid")->value();
        uid.toUpperCase();
        
        JsonDocument json;
        bool encontrado = false;
        
        for (int i = 0; i < total_usuarios; i++) {
            if (usuarios[i].uid == uid) {
                json["success"] = true;
                json["uid"] = usuarios[i].uid;
                json["nome"] = usuarios[i].nome;
                json["creditos"] = usuarios[i].creditos;
                encontrado = true;
                break;
            }
        }
        
        if (!encontrado) {
            json["success"] = false;
            json["message"] = "Usuário não encontrado";
        }
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    // Endpoint para obter logs do sistema
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        JsonArray logs_array = json["logs"].to<JsonArray>();

        File file = SPIFFS.open("/datalog.txt", FILE_READ);
        if(file && file.size() > 0) {
            String line;
            while(file.available()) {
                line = file.readStringUntil('\n');
                if(line.length() > 0) {
                    if(request->hasParam("uid")) {
                        String uid_filtro = request->getParam("uid")->value();
                        uid_filtro.toUpperCase();
                        if(line.indexOf(uid_filtro) != -1 || line.indexOf("Reset") != -1 || line.indexOf("inicializado") != -1) {
                            logs_array.add(line);
                        }
                    } else {
                        logs_array.add(line);
                    }
                }
            }
            file.close();
        }

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });
    
    // ROTA UNIFICADA PARA GERENCIAR USUÁRIOS (POST, DELETE)
    server.on("/api/usuarios", HTTP_ANY, 
        [](AsyncWebServerRequest *request){
            // Esta primeira parte é chamada quando os headers são recebidos, mas antes do corpo.
            // Não fazemos nada aqui, mas a função é necessária.
        },
        NULL, // Nenhuma função para upload de arquivo
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            // Esta função é chamada quando o corpo (body) da requisição é recebido.
            JsonDocument json;
            if (deserializeJson(json, (const char*)data) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
                return;
            }

            JsonObject obj = json.as<JsonObject>();
            JsonDocument response_json;

            // Verifica o método HTTP da requisição
            if (request->method() == HTTP_POST) {
                String uid = obj["uid"];
                String nome = obj["nome"];
                if (adicionar_usuario(uid, nome)) {
                    response_json["success"] = true;
                } else {
                    response_json["success"] = false;
                    response_json["message"] = "Falha ao adicionar. UID já existe ou limite atingido.";
                }
            } else if (request->method() == HTTP_DELETE) {
                String uid = obj["uid"];
                if (remover_usuario(uid)) {
                    response_json["success"] = true;
                } else {
                    response_json["success"] = false;
                    response_json["message"] = "Falha ao remover. UID não encontrado.";
                }
            } else {
                // Informa que o método não é permitido se não for POST ou DELETE
                request->send(405, "application/json", "{\"success\":false,\"message\":\"Método não permitido\"}");
                return;
            }

            String response;
            serializeJson(response_json, response);
            request->send(200, "application/json", response);
        }
    );

    server.on("/api/servir-cafe", HTTP_POST, [](AsyncWebServerRequest *request){
        JsonDocument response_json;
        if(!sistema_ocupado && cafes_restantes > 0){
            servir_cafe("MANUAL");
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            if(sistema_ocupado) {
                response_json["message"] = "Sistema está ocupado.";
            } else {
                response_json["message"] = "Sem café na garrafa.";
            }
        }
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/limpar-dados", HTTP_DELETE, [](AsyncWebServerRequest *request){
        limpar_dados();
        JsonDocument response_json;
        response_json["success"] = true;
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        JsonArray usuarios_json = json["usuarios"].to<JsonArray>();
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios_json.add<JsonObject>();
            usuario["uid"] = usuarios[i].uid;
            usuario["nome"] = usuarios[i].nome;
            usuario["creditos"] = usuarios[i].creditos;
        }
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });
}

// ============== FUNÇÃO DE RESET DE CRÉDITOS ==============
void resetar_creditos_semanal() {
    if (ultimo_reset_semanal == 0) { // Se nunca rodou, define o tempo inicial
        ultimo_reset_semanal = millis();
        salvar_dados();
        return;
    }

    if (millis() - ultimo_reset_semanal > INTERVALO_RESET_SEMANAL) {
        Serial.println("\n===============================");
        Serial.println("     RESET SEMANAL DE CRÉDITOS     ");
        Serial.println("===============================");

        for (int i = 0; i < total_usuarios; i++) {
            usuarios[i].creditos = CREDITOS_INICIAIS;
        }

        ultimo_reset_semanal = millis();
        registrar_log("Reset semanal de creditos executado para todos os usuarios.");
        salvar_dados();
        som_dados_salvos();

        Serial.println("Créditos de todos os usuários foram resetados para " + String(CREDITOS_INICIAIS));
    }
}

// ============== FUNÇÕES DE COMANDO SERIAL ==============
void processar_comando_serial() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        String original_command = command; // Mantém o comando original
        command.toLowerCase(); // Converte para minúsculas para comparação

        if (command == "help") {
            Serial.println("\n===== COMANDOS DISPONÍVEIS =====");
            Serial.println("add <uid> <nome> - Adiciona um novo usuário");
            Serial.println("rm <uid> - Remove um usuário pelo UID");
            Serial.println("list - Lista todos os usuários cadastrados");
            Serial.println("clear - Apaga todos os dados (usuários e estatísticas)");
            Serial.println("stats - Mostra as estatísticas do sistema");
            Serial.println("servir - Serve um café manualmente");
            Serial.println("logs - Exibe o log de eventos");
            Serial.println("reset - Força reset dos créditos");
            Serial.println("==============================\n");
        } 
        else if (command.startsWith("add ")) {
            int firstSpace = original_command.indexOf(' ');
            if (firstSpace != -1) {
                // O UID é a primeira parte
                String uid = original_command.substring(firstSpace + 1);
                int secondSpace = uid.indexOf(' ');

                if (secondSpace != -1) {
                    // O nome é tudo após o UID
                    String nome = uid.substring(secondSpace + 1);
                    uid = uid.substring(0, secondSpace);
                    
                    uid.trim();
                    uid.toUpperCase();
                    nome.trim();
                    
                    if (uid.length() > 0 && nome.length() > 0) {
                        adicionar_usuario(uid, nome);
                    } else {
                        Serial.println("Formato inválido. Use: add <UID> <NOME COMPLETO>");
                    }
                } else {
                    Serial.println("Formato inválido. Falta o nome. Use: add <UID> <NOME COMPLETO>");
                }
            }
        }
        else if (command.startsWith("rm ")) {
            String uid = original_command.substring(3);
            uid.trim();
            uid.toUpperCase(); // O UID é sempre maiúsculo no sistema
            remover_usuario(uid);
        }
        else if (command == "list") {
            Serial.println("\n===== LISTA DE USUÁRIOS =====");
            if(total_usuarios == 0){
                Serial.println("Nenhum usuário cadastrado.");
            } else {
                for(int i = 0; i < total_usuarios; i++){
                    Serial.print(i+1);
                    Serial.print(": ");
                    Serial.print(usuarios[i].nome);
                    Serial.print(" - UID: ");
                    Serial.print(usuarios[i].uid);
                    Serial.print(" - Créditos: ");
                    Serial.println(usuarios[i].creditos);
                }
            }
            Serial.println("=============================\n");
        }
        else if (command == "clear") {
            Serial.println("ATENÇÃO: ISSO APAGARÁ TUDO! Digite 'CONFIRMAR' para prosseguir.");
            while(Serial.available() == 0) { delay(100); }
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            if(confirm == "CONFIRMAR"){
                limpar_dados();
                som_dados_salvos();
            } else {
                Serial.println("Operação cancelada.");
            }
        }
        else if (command == "stats") {
            Serial.println("\n===== ESTATÍSTICAS DO SISTEMA =====");
            Serial.print("Usuários cadastrados: ");
            Serial.println(total_usuarios);
            Serial.print("Total de cafés servidos: ");
            Serial.println(total_cafes_servidos);
            Serial.print("Cafés restantes: ");
            Serial.print(cafes_restantes);
            Serial.print(" / ");
            Serial.println(MAX_CAFES);
            Serial.println("=================================\n");
        }
        else if(command == "servir"){
            if(!sistema_ocupado && cafes_restantes > 0){
                servir_cafe("SERIAL");
            } else if(sistema_ocupado) {
                Serial.println("Sistema ocupado no momento.");
            } else {
                Serial.println("Sem café na garrafa.");
            }
        }
        else if(command == "logs") {
            File file = SPIFFS.open("/datalog.txt", FILE_READ);
            if(!file || file.size() == 0){
                Serial.println("Nenhum log encontrado.");
                return;
            }
            Serial.println("\n===== LOG DE EVENTOS =====");
            while(file.available()){
                Serial.write(file.read());
            }
            file.close();
            Serial.println("\n========================\n");
        }
        else if(command == "reset") {
            Serial.println("Forçando reset de créditos...");
            for (int i = 0; i < total_usuarios; i++) {
                usuarios[i].creditos = CREDITOS_INICIAIS;
            }
            ultimo_reset_semanal = millis();
            registrar_log("Reset manual de creditos executado.");
            salvar_dados();
            som_dados_salvos();
            Serial.println("Créditos resetados para todos os usuários!");
        }
        else {
            if(command.length() > 0)
                Serial.println("Comando desconhecido. Digite 'help' para ver a lista de comandos.");
        }
    }
}

// ============== SETUP e LOOP ==============
void setup() {
    Serial.begin(115200);

    if(!SPIFFS.begin(true)){
        Serial.println("ERRO FATAL: Ocorreu um erro ao montar o SPIFFS. O sistema será paralisado.");
        // Pisca um LED ou emite um som de erro contínuo para indicar falha
        while(1){
            digitalWrite(BUZZER_PIN, HIGH); delay(100);
            digitalWrite(BUZZER_PIN, LOW); delay(100);
        }
    }

    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    SPI.begin();
    mfrc522.PCD_Init();

    Serial.println("\n==================================================");
    Serial.println(" SISTEMA CAFETEIRA RFID - v3.3");
    Serial.println("==================================================");

    carregar_dados();
    inicializar_wifi();

    if (WiFi.status() == WL_CONNECTED) {
        configurar_rotas_api();
        server.begin();
        Serial.println("Servidor web iniciado!");
    }

    if(ultimo_reset_semanal == 0) { // Garante que a data do primeiro reset seja salva
        ultimo_reset_semanal = millis();
        salvar_dados();
    }

     xSemaphore = xSemaphoreCreateMutex();
      if (xSemaphore == NULL) {
          Serial.println("ERRO: Não foi possível criar o semáforo!");
          // Trava a execução se não conseguir criar o semáforo, pois é crítico
          while(1); 
      }

    som_inicializacao();

    Serial.println("\nDigite 'HELP' para ver todos os comandos");
    Serial.println("Ou acesse a interface web: http://" + WiFi.localIP().toString());
    Serial.println("\nSistema ativo - aproxime uma tag RFID...");

    tempo_ultimo_evento = millis();
}

void loop() {
    processar_comando_serial();

    if (millis() - ultimo_check_reset > INTERVALO_CHECK_RESET) {
        resetar_creditos_semanal();
        ultimo_check_reset = millis();
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectado. Tentando reconectar...");
        WiFi.reconnect();
        delay(2000);
    }

    if (sistema_ocupado || !mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    processar_tag_detectada();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}