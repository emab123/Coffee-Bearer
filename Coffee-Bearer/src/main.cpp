/*
==================================================
SISTEMA CAFETEIRA RFID - v3.0 INTERFACE WEB
Gerenciamento completo via navegador web
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

// ============== CONFIGURAÇÕES DOS PINOS ==============
#define RST_PIN 4
#define SS_PIN 5
#define BUZZER_PIN 15
#define RELAY_PIN 13 

// ============== CONFIGURAÇÕES DO SISTEMA ==============
#define MAX_USUARIOS 50
#define COOLDOWN_MS 2000
#define TEMPO_CAFE_MS 5000

// ============== CONFIGURAÇÕES WIFI ==============
const char* ssid = "SUAREDE"; // Altere para sua rede
const char* password = "SUASENHA"; // Altere para sua senha

// ============== VARIÁVEIS GLOBAIS ==============
MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences preferences;
AsyncWebServer server(80);

// Arrays para usuários
String usuarios_autorizados[MAX_USUARIOS];
String nomes_usuarios[MAX_USUARIOS];
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

// ============== FUNÇÕES DE PERSISTÊNCIA ==============
void salvar_dados() {
  Serial.println("Salvando dados na memória flash...");
  preferences.begin("cafeteira", false);
  preferences.putInt("total_users", total_usuarios);
  for (int i = 0; i < total_usuarios; i++) {
    String key_uid = "uid_" + String(i);
    String key_nome = "nome_" + String(i);
    preferences.putString(key_uid.c_str(), usuarios_autorizados[i]);
    preferences.putString(key_nome.c_str(), nomes_usuarios[i]);
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
    usuarios_autorizados[i] = preferences.getString(key_uid.c_str(), "");
    nomes_usuarios[i] = preferences.getString(key_nome.c_str(), "");
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
    usuarios_autorizados[i] = "";
    nomes_usuarios[i] = "";
  }
  Serial.println("Todos os dados foram apagados!");
}

// ============== FUNÇÕES DE SOM ==============
void som_autorizado() { tone(BUZZER_PIN, 1200, 80); delay(100); tone(BUZZER_PIN, 1500, 80); }
void som_negado() { tone(BUZZER_PIN, 200, 400); }
void som_inicializacao() { tone(BUZZER_PIN, 800, 60); delay(80); tone(BUZZER_PIN, 1000, 60); delay(80); tone(BUZZER_PIN, 1200, 60); }
void som_cafe_pronto() { tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1300, 50); delay(60); tone(BUZZER_PIN, 1600, 100); }
void som_dados_salvos() { tone(BUZZER_PIN, 1800, 50); delay(60); tone(BUZZER_PIN, 1800, 50); }

// ============== FUNÇÕES DE CONTROLE ==============
void servir_cafe(String nome_usuario) {
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

  total_cafes_servidos++;
  salvar_dados();

  ultimo_evento = "Café servido para " + nome_usuario;
  tempo_ultimo_evento = millis();
  sistema_ocupado = false;

  Serial.println("\nAguardando próxima tag...");
}

// ============== FUNÇÕES DE USUÁRIOS ==============
bool adicionar_usuario(String uid, String nome) {
  if (total_usuarios >= MAX_USUARIOS) { Serial.println("Erro: Limite máximo de usuários atingido!"); return false; }
  for (int i = 0; i < total_usuarios; i++) {
    if (usuarios_autorizados[i] == uid) { Serial.println("UID já cadastrado para: " + nomes_usuarios[i]); return false; }
  }

  usuarios_autorizados[total_usuarios] = uid;
  nomes_usuarios[total_usuarios] = nome;
  total_usuarios++;

  Serial.println("Usuário adicionado: " + nome);
  salvar_dados();
  som_autorizado();
  som_dados_salvos();

  ultimo_evento = "Usuário adicionado: " + nome;
  tempo_ultimo_evento = millis();

  return true;
}

bool remover_usuario(String uid) {
  for (int i = 0; i < total_usuarios; i++) {
    if (usuarios_autorizados[i] == uid) {
      String nome = nomes_usuarios[i];
      for (int j = i; j < total_usuarios - 1; j++) {
        usuarios_autorizados[j] = usuarios_autorizados[j + 1];
        nomes_usuarios[j] = nomes_usuarios[j + 1];
      }
      total_usuarios--;
      usuarios_autorizados[total_usuarios] = "";
      nomes_usuarios[total_usuarios] = "";

      Serial.println("Usuário removido: " + nome);
      salvar_dados();
      som_dados_salvos();

      ultimo_evento = "Usuário removido: " + nome;
      tempo_ultimo_evento = millis();

      return true;
    }
  }
  Serial.println("UID não encontrado na lista");
  return false;
}

String verificar_autorizacao(String uid) {
  for (int i = 0; i < total_usuarios; i++) {
    if (usuarios_autorizados[i] == uid) {
      return nomes_usuarios[i];
    }
  }
  return "";
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

  if (uid == ultimo_uid_lido && (tempo_atual - ultimo_tempo_leitura) < COOLDOWN_MS) { return; }

  ultimo_uid_lido = uid;
  ultimo_tempo_leitura = tempo_atual;

  Serial.println("\n===============================");
  Serial.println(" TAG DETECTADA: " + uid);
  Serial.println("===============================");

  String nome_usuario = verificar_autorizacao(uid);

  if (nome_usuario != "") {
    Serial.println(" ACESSO AUTORIZADO");
    som_autorizado();
    servir_cafe(nome_usuario);
  } else {
    Serial.println(" ACESSO NEGADO");
    Serial.println("Para cadastrar via web: acesse http://" + WiFi.localIP().toString());
    som_negado();

    ultimo_evento = "Acesso negado - UID: " + uid;
    tempo_ultimo_evento = millis();
  }
}

// ============== FUNÇÕES ADICIONADAS (PLACEHOLDERS) ==============
// Você precisa adicionar a lógica correta para estas funções

void inicializar_wifi() {
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}
// ... (código anterior sem alterações) ...


void configurar_rotas_api() {
    // ---- NOVAS ROTAS PARA SERVIR OS ARQUIVOS DA PÁGINA WEB ----
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/script.js", "text/javascript");
    });

    // ---- SUAS ROTAS DE API EXISTENTES (COM SINTAXE JSON MODERNIZADA) ----
    // API para obter status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json; // MUDANÇA: Usando a nova sintaxe
        json["total_usuarios"] = total_usuarios;
        json["total_cafes_servidos"] = total_cafes_servidos;
        json["sistema_ocupado"] = sistema_ocupado;
        json["ultimo_evento"] = ultimo_evento;
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    // API para listar usuários
    server.on("/api/usuarios", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json; // MUDANÇA: Usando a nova sintaxe
        JsonArray usuarios = json["usuarios"].to<JsonArray>(); // MUDANÇA: Nova forma de criar um array
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios.add<JsonObject>(); // MUDANÇA: Nova forma de adicionar um objeto ao array
            usuario["uid"] = usuarios_autorizados[i];
            usuario["nome"] = nomes_usuarios[i];
        }
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });

    // API para adicionar usuário
    AsyncCallbackJsonWebHandler* handler_add = new AsyncCallbackJsonWebHandler("/api/usuarios", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        String uid = obj["uid"];
        String nome = obj["nome"];
        JsonDocument response_json; // MUDANÇA: Usando a nova sintaxe
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

    // API para remover usuário (corrigido)
    AsyncCallbackJsonWebHandler* handler_delete = new AsyncCallbackJsonWebHandler("/api/usuarios", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        String uid = obj["uid"];
        JsonDocument response_json; // MUDANÇA: Usando a nova sintaxe
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
    handler_delete->setMethod(HTTP_DELETE); // Define o método HTTP correto
    server.addHandler(handler_delete);

    // API para servir café manualmente
    server.on("/api/servir-cafe", HTTP_POST, [](AsyncWebServerRequest *request){
        JsonDocument response_json; // MUDANÇA: Usando a nova sintaxe
        if(!sistema_ocupado){
            servir_cafe("MANUAL");
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            response_json["message"] = "Sistema está ocupado.";
        }
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });

    // API para limpar todos os dados
    server.on("/api/limpar-dados", HTTP_DELETE, [](AsyncWebServerRequest *request){
        limpar_dados();
        JsonDocument response_json; // MUDANÇA: Usando a nova sintaxe
        response_json["success"] = true;
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });

    // API para backup
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
        JsonDocument json; // MUDANÇA: Usando a nova sintaxe
        JsonArray usuarios = json["usuarios"].to<JsonArray>(); // MUDANÇA: Nova forma de criar um array
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios.add<JsonObject>(); // MUDANÇA: Nova forma de adicionar um objeto ao array
            usuario["uid"] = usuarios_autorizados[i];
            usuario["nome"] = nomes_usuarios[i];
        }
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    });
}

void processar_comando_serial() {
  // Se houver algum dado disponível na porta serial
  if (Serial.available() > 0) {
    // Lê a string até encontrar uma nova linha
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove espaços em branco extras
    command.toUpperCase(); // Converte para maiúsculas para facilitar a comparação

    if (command == "HELP") {
      Serial.println("\n===== COMANDOS DISPONIVEIS =====");
      Serial.println("ADD <UID> <NOME> - Adiciona um novo usuario");
      Serial.println("RM <UID> - Remove um usuario pelo UID");
      Serial.println("LIST - Lista todos os usuarios cadastrados");
      Serial.println("CLEAR - Apaga todos os dados (usuarios e estatisticas)");
      Serial.println("STATS - Mostra as estatisticas do sistema");
      Serial.println("SERVIR - Serve um cafe manualmente");
      Serial.println("==============================\n");
    } 
    else if (command.startsWith("ADD ")) {
      // Formato esperado: ADD 1A 2B 3C 4D NOME DO USUARIO
      int firstSpace = command.indexOf(' ');
      int secondSpace = command.indexOf(' ', firstSpace + 1);
      int thirdSpace = command.indexOf(' ', secondSpace + 1);
      int fourthSpace = command.indexOf(' ', thirdSpace + 1);
      
      if(fourthSpace != -1) {
          String uid = command.substring(firstSpace + 1, fourthSpace);
          String nome = command.substring(fourthSpace + 1);
          uid.trim();
          nome.trim();
          adicionar_usuario(uid, nome);
      } else {
        Serial.println("Formato invalido. Use: ADD <UID> <NOME>");
      }
    }
    else if (command.startsWith("RM ")) {
      String uid = command.substring(3);
      uid.trim();
      remover_usuario(uid);
    }
    else if (command == "LIST") {
      Serial.println("\n===== LISTA DE USUARIOS =====");
      if(total_usuarios == 0){
        Serial.println("Nenhum usuario cadastrado.");
      } else {
        for(int i=0; i < total_usuarios; i++){
          Serial.print(i+1);
          Serial.print(": ");
          Serial.print(nomes_usuarios[i]);
          Serial.print(" - UID: ");
          Serial.println(usuarios_autorizados[i]);
        }
      }
      Serial.println("=============================\n");
    }
    else if (command == "CLEAR") {
        Serial.println("ATENCAO: ISSO APAGARA TUDO! Digite 'CONFIRMAR' para prosseguir.");
        while(Serial.available() == 0) { delay(100); } // espera por confirmação
        String confirm = Serial.readStringUntil('\n');
        confirm.trim();
        if(confirm == "CONFIRMAR"){
            limpar_dados();
            som_dados_salvos();
        } else {
            Serial.println("Operacao cancelada.");
        }
    }
    else if (command == "STATS") {
      Serial.println("\n===== ESTATISTICAS DO SISTEMA =====");
      Serial.print("Usuarios cadastrados: ");
      Serial.println(total_usuarios);
      Serial.print("Total de cafes servidos: ");
      Serial.println(total_cafes_servidos);
      Serial.println("=================================\n");
    }
    else if(command == "SERVIR"){
      if(!sistema_ocupado){
        servir_cafe("SERIAL");
      } else {
        Serial.println("Sistema ocupado no momento.");
      }
    }
    else {
      if(command.length() > 0)
        Serial.println("Comando desconhecido. Digite 'HELP' para ver a lista de comandos.");
    }
  }
}

// ============== SETUP e LOOP ==============
void setup() {
  Serial.begin(115200);

  // Inicializar o sistema de arquivos SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("Ocorreu um erro ao montar o SPIFFS");
    return;
  }

  // Configurar pinos
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Inicializar SPI e RFID
  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("\n==================================================");
  Serial.println(" SISTEMA CAFETEIRA RFID - v3.0 INTERFACE WEB");
  Serial.println("==================================================");

  // Carregar dados da memória
  carregar_dados();

  // Conectar ao WiFi
  inicializar_wifi();

  // Configurar servidor web
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
  // Processar comandos seriais (compatibilidade)
  processar_comando_serial();

  // Verificar WiFi e reconectar se necessário
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Tentando reconectar...");
    WiFi.reconnect();
    delay(2000);
  }

  // Processar RFID
  if (sistema_ocupado || !mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  processar_tag_detectada();
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}