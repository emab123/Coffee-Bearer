/*
==================================================
SISTEMA CAFETEIRA RFID - v3.1 - FASE 2
Gerenciamento com Créditos, Logs e Estrutura de Dados Refatorada
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

// ============== CONFIGURAÇÕES DO SISTEMA ==============
#define MAX_USUARIOS 50
#define COOLDOWN_MS 2000
#define TEMPO_CAFE_MS 5000
#define CREDITOS_INICIAIS 7 

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

// Array para usuários (nova estrutura)
Usuario usuarios[MAX_USUARIOS];
int total_usuarios = 0;

// Controle de tempo
unsigned long ultimo_tempo_leitura = 0;
String ultimo_uid_lido = "";

// Estatísticas persistentes
int total_cafes_servidos = 0;
unsigned long tempo_total_funcionamento = 0;

// Status do sistema para a interface web
bool sistema_ocupado = false;
String ultimo_evento = "Sistema inicializado";
unsigned long tempo_ultimo_evento = 0;

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


// ============== FUNÇÕES DE PERSISTÊNCIA (Refatoradas) ==============
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
  preferences.end();
  Serial.print("Carregados ");
  Serial.print(total_usuarios);
  Serial.println(" usuários da memória!");
}

void limpar_dados() {
  Serial.println("Limpando todos os dados salvos...");
  preferences.begin("cafeteira", false);
  preferences.clear();
  preferences.end();
  total_usuarios = 0;
  total_cafes_servidos = 0;
  tempo_total_funcionamento = 0;
  for (int i = 0; i < MAX_USUARIOS; i++) {
    usuarios[i] = {}; // Limpa a struct
  }
  Serial.println("Todos os dados foram apagados!");
  registrar_log("TODOS OS DADOS FORAM APAGADOS");
}

// ============== FUNÇÕES DE SOM (sem alteração) ==============
void som_autorizado() { tone(BUZZER_PIN, 1200, 80); delay(100); tone(BUZZER_PIN, 1500, 80); }
void som_negado() { tone(BUZZER_PIN, 200, 400); }
void som_inicializacao() { tone(BUZZER_PIN, 800, 60); delay(80); tone(BUZZER_PIN, 1000, 60); delay(80); tone(BUZZER_PIN, 1200, 60); }
void som_cafe_pronto() { tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1600, 100); }
void som_dados_salvos() { tone(BUZZER_PIN, 1800, 50); delay(60); tone(BUZZER_PIN, 1800, 50); }

// ============== FUNÇÕES DE CONTROLE (com sistema de crédito) ==============
void servir_cafe(int usuario_index) {
  sistema_ocupado = true;
  String nome_usuario = usuarios[usuario_index].nome;
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

  // Lógica de Crédito e Log
  if (nome_usuario != "MANUAL" && nome_usuario != "SERIAL") {
    usuarios[usuario_index].creditos--;
  }
  total_cafes_servidos++;
  registrar_log("Cafe servido para " + nome_usuario + " (UID: " + usuarios[usuario_index].uid + "). Creditos restantes: " + String(usuarios[usuario_index].creditos));
  salvar_dados(); // Salva o novo total de cafés e o crédito atualizado

  ultimo_evento = "Café servido para " + nome_usuario;
  tempo_ultimo_evento = millis();
  sistema_ocupado = false;

  Serial.println("\nAguardando próxima tag...");
}


// ============== FUNÇÕES DE USUÁRIOS (Refatoradas) ==============
bool adicionar_usuario(String uid, String nome) {
  if (total_usuarios >= MAX_USUARIOS) { Serial.println("Erro: Limite máximo de usuários atingido!"); return false; }
  for (int i = 0; i < total_usuarios; i++) {
    if (usuarios[i].uid == uid) { Serial.println("UID já cadastrado para: " + usuarios[i].nome); return false; }
  }

  usuarios[total_usuarios].uid = uid;
  usuarios[total_usuarios].nome = nome;
  usuarios[total_usuarios].creditos = CREDITOS_INICIAIS;
  total_usuarios++;

  Serial.println("Usuário adicionado: " + nome + " com " + String(CREDITOS_INICIAIS) + " créditos.");
  registrar_log("Usuario adicionado: " + nome + " (UID: " + uid + ")");
  salvar_dados();
  // som_autorizado();      <-- REMOVIDO
  // som_dados_salvos();    <-- REMOVIDO

  ultimo_evento = "Usuário adicionado: " + nome;
  tempo_ultimo_evento = millis();

  return true;
}

// Substitua esta função também
bool remover_usuario(String uid) {
  for (int i = 0; i < total_usuarios; i++) {
    if (usuarios[i].uid == uid) {
      String nome = usuarios[i].nome;
      for (int j = i; j < total_usuarios - 1; j++) {
        usuarios[j] = usuarios[j + 1];
      }
      total_usuarios--;
      usuarios[total_usuarios] = {}; // Limpa a última posição

      Serial.println("Usuário removido: " + nome);
      registrar_log("Usuario removido: " + nome + " (UID: " + uid + ")");
      salvar_dados();
      // som_dados_salvos();  <-- REMOVIDO

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


// ============== FUNÇÕES DE RFID (Refatoradas) ==============
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

  if (uid == ultimo_uid_lido && (tempo_atual - ultimo_tempo_leitura) < COOLDOWN_MS) { return; }

  ultimo_uid_lido = uid;
  ultimo_tempo_leitura = tempo_atual;

  Serial.println("\n===============================");
  Serial.println(" TAG DETECTADA: " + uid);
  Serial.println("===============================");

  int usuario_index = verificar_autorizacao(uid);

  if (usuario_index >= 0) {
    Serial.println(" ACESSO AUTORIZADO");
    som_autorizado();
    servir_cafe(usuario_index);
  } else {
    if (usuario_index == -2) { // Código para "sem créditos"
      som_negado();
      // A mensagem já foi impressa em verificar_autorizacao()
    } else { // Código para "não encontrado"
      Serial.println(" ACESSO NEGADO - UID NAO CADASTRADO");
      Serial.println("Para cadastrar via web: acesse http://" + WiFi.localIP().toString());
      ultimo_evento = "Acesso negado - UID: " + uid;
      tempo_ultimo_evento = millis();
    }
    som_negado();
  }
}

// ============== FUNÇÕES WIFI E API (com crédito no JSON) ==============
void inicializar_wifi() {
  Serial.print("Conectando a ");
  Serial.println(WIFI_SSID); // <-- ALTERADO
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // <-- ALTERADO
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

    // ---- ROTAS DE API (Atualizadas com créditos) ----
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        json["total_usuarios"] = total_usuarios;
        json["total_cafes_servidos"] = total_cafes_servidos;
        json["sistema_ocupado"] = sistema_ocupado;
        json["ultimo_evento"] = ultimo_evento;
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/usuarios", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json;
        JsonArray usuarios_json = json["usuarios"].to<JsonArray>();
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios_json.add<JsonObject>();
            usuario["uid"] = usuarios[i].uid;
            usuario["nome"] = usuarios[i].nome;
            usuario["creditos"] = usuarios[i].creditos; // Adicionado campo de créditos
        }
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });
    
    // As rotas de adicionar e remover continuam funcionando como antes
    AsyncCallbackJsonWebHandler* handler_add = new AsyncCallbackJsonWebHandler("/api/usuarios", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        String uid = obj["uid"];
        String nome = obj["nome"];
        JsonDocument response_json;
        if (adicionar_usuario(uid, nome)) {
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            response_json["message"] = "Falha ao adicionar usuário. UID já pode existir ou limite atingido.";
        }
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });
    server.addHandler(handler_add);

    AsyncCallbackJsonWebHandler* handler_delete = new AsyncCallbackJsonWebHandler("/api/usuarios", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        String uid = obj["uid"];
        JsonDocument response_json;
        if (remover_usuario(uid)) {
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            response_json["message"] = "Falha ao remover usuário. UID não encontrado.";
        }
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    }, 1024);
    handler_delete->setMethod(HTTP_DELETE);
    server.addHandler(handler_delete);

    // Servir café manualmente não consome crédito e não precisa de refatoração complexa
    server.on("/api/servir-cafe", HTTP_POST, [](AsyncWebServerRequest *request){
        JsonDocument response_json;
        if(!sistema_ocupado){
            // Criamos um usuário temporário para a função servir_cafe
            Usuario manual_user = {"N/A", "MANUAL", 1};
            usuarios[MAX_USUARIOS-1] = manual_user; // Usa uma posição segura
            servir_cafe(MAX_USUARIOS-1);
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            response_json["message"] = "Sistema está ocupado.";
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
    
    // O backup agora irá incluir os créditos
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
        // Define o cabeçalho para forçar o download
        request->send(200, "application/json", response);
    });
}

// ============== FUNÇÕES DE COMANDO SERIAL (Refatoradas) ==============
// ============== FUNÇÕES DE COMANDO SERIAL (CORRIGIDA) ==============
void processar_comando_serial() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase(); // Converte para minúsculas ANTES de usar

    if (command == "help") {
      Serial.println("\n===== COMANDOS DISPONIVEIS =====");
      Serial.println("add <uid> <nome> - Adiciona um novo usuario");
      Serial.println("rm <uid> - Remove um usuario pelo UID");
      Serial.println("list - Lista todos os usuarios cadastrados");
      Serial.println("clear - Apaga todos os dados (usuarios e estatisticas)");
      Serial.println("stats - Mostra as estatisticas do sistema");
      Serial.println("servir - Serve um cafe manualmente");
      Serial.println("logs - Exibe o log de eventos");
      Serial.println("==============================\n");
    } 
    else if (command.startsWith("add ")) {
      // Formato esperado: add a1 b2 c3 d4 nome do usuario
      // A string já está em minúsculas, mas o UID precisa ser maiúsculo para salvar
      String temp_command = Serial.readStringUntil('\n'); // Lê a linha original novamente
      temp_command.trim();

      int firstSpace = temp_command.indexOf(' ');
      int secondSpace = temp_command.indexOf(' ', firstSpace + 1);
      int thirdSpace = temp_command.indexOf(' ', secondSpace + 1);
      int fourthSpace = temp_command.indexOf(' ', thirdSpace + 1);
      
      if(fourthSpace != -1) {
          String uid = temp_command.substring(firstSpace + 1, fourthSpace);
          String nome = temp_command.substring(fourthSpace + 1);
          uid.trim();
          uid.toUpperCase(); // Garante que o UID seja salvo em maiúsculas
          nome.trim();
          adicionar_usuario(uid, nome);
      } else {
        Serial.println("Formato invalido. Use: add <UID> <NOME>");
      }
    }
    else if (command.startsWith("rm ")) {
      String uid = command.substring(3);
      uid.trim();
      uid.toUpperCase(); // O UID é sempre maiúsculo no sistema
      remover_usuario(uid);
    }
    else if (command == "list") {
      Serial.println("\n===== LISTA DE USUARIOS =====");
      if(total_usuarios == 0){
        Serial.println("Nenhum usuario cadastrado.");
      } else {
        for(int i=0; i < total_usuarios; i++){
          Serial.print(i+1);
          Serial.print(": ");
          Serial.print(usuarios[i].nome);
          Serial.print(" - UID: ");
          Serial.print(usuarios[i].uid);
          Serial.print(" - Creditos: ");
          Serial.println(usuarios[i].creditos);
        }
      }
      Serial.println("=============================\n");
    }
    else if (command == "clear") {
        Serial.println("ATENCAO: ISSO APAGARA TUDO! Digite 'CONFIRMAR' para prosseguir.");
        while(Serial.available() == 0) { delay(100); }
        String confirm = Serial.readStringUntil('\n');
        confirm.trim();
        if(confirm == "CONFIRMAR"){
            limpar_dados();
            som_dados_salvos();
        } else {
            Serial.println("Operacao cancelada.");
        }
    }
    else if (command == "stats") {
      Serial.println("\n===== ESTATISTICAS DO SISTEMA =====");
      Serial.print("Usuarios cadastrados: ");
      Serial.println(total_usuarios);
      Serial.print("Total de cafes servidos: ");
      Serial.println(total_cafes_servidos);
      Serial.println("=================================\n");
    }
    else if(command == "servir"){
      if(!sistema_ocupado){
        Usuario serial_user = {"N/A", "SERIAL", 1};
        usuarios[MAX_USUARIOS-1] = serial_user;
        servir_cafe(MAX_USUARIOS-1);
      } else {
        Serial.println("Sistema ocupado no momento.");
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
    Serial.println("Ocorreu um erro ao montar o SPIFFS");
    return;
  }

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("\n==================================================");
  Serial.println(" SISTEMA CAFETEIRA RFID - v3.1");
  Serial.println("==================================================");

  carregar_dados();
  inicializar_wifi();

  if (WiFi.status() == WL_CONNECTED) {
    configurar_rotas_api();
    server.begin();
    Serial.println("Servidor web iniciado!");
  }

  som_inicializacao();

  Serial.println("\nDigite 'HELP' para ver todos os comandos");
  Serial.println("Ou acesse a interface web: http://" + WiFi.localIP().toString());
  Serial.println("\nSistema ativo - aproxime uma tag RFID...");

  tempo_ultimo_evento = millis();
}

void loop() {
  processar_comando_serial();

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