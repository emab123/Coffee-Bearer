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

// ============== CONFIGURAÇÕES DOS PINOS ==============
#define RST_PIN 4
#define SS_PIN 5
#define BUZZER_PIN 15
#define RELAY_PIN 13 // Alterado do pino 2 para 13 para evitar conflitos
#define LED_PIN 16

// ============== CONFIGURAÇÕES DO SISTEMA ==============
#define MAX_USUARIOS 50
#define COOLDOWN_MS 2000
#define TEMPO_CAFE_MS 5000

// ============== CONFIGURAÇÕES WIFI ==============
const char* ssid = "Gergelim"; // Altere para sua rede
const char* password = "32414451"; // Altere para sua senha

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

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RELAY_PIN, HIGH);

  for (int i = TEMPO_CAFE_MS / 1000; i > 0; i--) {
    Serial.print(" Servindo... ");
    Serial.print(i);
    Serial.println(" segundos");
    delay(1000);
  }

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

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

// ============== FUNÇÕES WEB SERVER ==============
String obter_pagina_principal() {
  // CORREÇÃO: Usando R"=====( ... )=====" para evitar conflito com )" dentro do HTML/JS
  return R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cafeteira RFID v3.0</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.1);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #4a90e2, #357abd);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3); }
        .header p { font-size: 1.1em; opacity: 0.9; }
        .main-content { padding: 30px; display: grid; grid-template-columns: 1fr 1fr; gap: 30px; }
        .card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 8px 25px rgba(0, 0, 0, 0.08); border: 1px solid #e1e5e9; transition: transform 0.3s ease, box-shadow 0.3s ease; }
        .card:hover { transform: translateY(-5px); box-shadow: 0 15px 40px rgba(0, 0, 0, 0.15); }
        .card h2 { color: #333; margin-bottom: 20px; font-size: 1.5em; border-bottom: 2px solid #4a90e2; padding-bottom: 10px; }
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 20px; }
        .stat-item { background: #f8f9fa; padding: 15px; border-radius: 10px; text-align: center; border-left: 4px solid #4a90e2; }
        .stat-number { font-size: 2em; font-weight: bold; color: #4a90e2; display: block; }
        .stat-label { color: #666; font-size: 0.9em; margin-top: 5px; }
        .form-group { margin-bottom: 20px; }
        .form-group label { display: block; margin-bottom: 8px; color: #333; font-weight: 600; }
        .form-group input { width: 100%; padding: 12px 15px; border: 2px solid #e1e5e9; border-radius: 10px; font-size: 1em; transition: border-color 0.3s ease; }
        .form-group input:focus { outline: none; border-color: #4a90e2; box-shadow: 0 0 0 3px rgba(74, 144, 226, 0.1); }
        .btn { background: linear-gradient(135deg, #4a90e2, #357abd); color: white; border: none; padding: 12px 25px; border-radius: 10px; cursor: pointer; font-size: 1em; font-weight: 600; transition: all 0.3s ease; margin-right: 10px; margin-bottom: 10px; }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(74, 144, 226, 0.3); }
        .btn-danger { background: linear-gradient(135deg, #e74c3c, #c0392b); }
        .btn-danger:hover { box-shadow: 0 8px 25px rgba(231, 76, 60, 0.3); }
        .btn-success { background: linear-gradient(135deg, #27ae60, #229954); }
        .btn-success:hover { box-shadow: 0 8px 25px rgba(39, 174, 96, 0.3); }
        .usuarios-lista { max-height: 300px; overflow-y: auto; border: 1px solid #e1e5e9; border-radius: 10px; padding: 15px; }
        .usuario-item { display: flex; justify-content: space-between; align-items: center; padding: 12px; border-bottom: 1px solid #f1f1f1; background: #f8f9fa; margin-bottom: 8px; border-radius: 8px; }
        .usuario-info { flex-grow: 1; }
        .usuario-nome { font-weight: 600; color: #333; }
        .usuario-uid { font-size: 0.8em; color: #666; font-family: monospace; }
        .status-sistema { background: #e8f5e8; border: 1px solid #d4e6d4; border-radius: 10px; padding: 15px; margin-bottom: 20px; }
        .status-ocupado { background: #fff3cd; border-color: #ffeaa7; }
        .status-erro { background: #f8d7da; border-color: #f5c6cb; }
        .ultimo-evento { font-size: 0.9em; color: #666; margin-top: 10px; }
        @media (max-width: 768px) {
            .main-content { grid-template-columns: 1fr; }
            .stats-grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Sistema Cafeteira RFID</h1>
            <p>Gerenciamento Inteligente de Usuários - Versão 3.0</p>
        </div>
        
        <div class="main-content">
            <div class="card">
                <h2>Status do Sistema</h2>
                <div id="status-sistema" class="status-sistema">
                    <strong>Sistema Ativo</strong>
                    <div id="ultimo-evento" class="ultimo-evento">Aguardando eventos...</div>
                </div>
                
                <div class="stats-grid">
                    <div class="stat-item">
                        <span id="total-usuarios" class="stat-number">0</span>
                        <span class="stat-label">Usuários Cadastrados</span>
                    </div>
                    <div class="stat-item">
                        <span id="total-cafes" class="stat-number">0</span>
                        <span class="stat-label">Cafés Servidos</span>
                    </div>
                </div>
                
                <button class="btn btn-success" onclick="servirCafeManual()">Servir Café Manual</button>
                <button class="btn" onclick="atualizarStatus()">Atualizar Status</button>
            </div>
            
            <div class="card">
                <h2>Gerenciar Usuários</h2>
                
                <form onsubmit="adicionarUsuario(event)">
                    <div class="form-group">
                        <label for="uid">UID da Tag RFID:</label>
                        <input type="text" id="uid" name="uid" placeholder="Ex: A1 B2 C3 D4" required>
                    </div>
                    <div class="form-group">
                        <label for="nome">Nome do Usuário:</label>
                        <input type="text" id="nome" name="nome" placeholder="Ex: João Silva" required>
                    </div>
                    <button type="submit" class="btn">Adicionar Usuário</button>
                    <button type="button" class="btn" onclick="listarUsuarios()">Listar Usuários</button>
                </form>
                
                <div style="margin-top: 20px;">
                    <button class="btn btn-danger" onclick="confirmarLimpeza()">Limpar Todos os Dados</button>
                    <button class="btn" onclick="fazerBackup()">Fazer Backup</button>
                </div>
            </div>
            
            <div class="card" style="grid-column: 1 / -1;">
                <h2>Usuários Cadastrados</h2>
                <div id="usuarios-lista" class="usuarios-lista">
                    <div style="text-align: center; color: #666; padding: 20px;">Carregando usuários...</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        async function atualizarStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                
                document.getElementById('total-usuarios').textContent = data.total_usuarios;
                document.getElementById('total-cafes').textContent = data.total_cafes_servidos;
                
                const statusDiv = document.getElementById('status-sistema');
                const eventoDiv = document.getElementById('ultimo-evento');
                
                if (data.sistema_ocupado) {
                    statusDiv.className = 'status-sistema status-ocupado';
                    statusDiv.querySelector('strong').textContent = 'Sistema Ocupado';
                } else {
                    statusDiv.className = 'status-sistema';
                    statusDiv.querySelector('strong').textContent = 'Sistema Ativo';
                }
                eventoDiv.textContent = data.ultimo_evento || 'Aguardando eventos...';
            } catch (error) { console.error('Erro ao atualizar status:', error); }
        }
        
        async function listarUsuarios() {
            try {
                const response = await fetch('/api/usuarios');
                const data = await response.json();
                const listaDiv = document.getElementById('usuarios-lista');
                
                if (data.usuarios.length === 0) {
                    listaDiv.innerHTML = '<div style="text-align: center; color: #666; padding: 20px;">Nenhum usuário cadastrado</div>';
                } else {
                    listaDiv.innerHTML = "";
                    data.usuarios.forEach(usuario => {
                        const userDiv = document.createElement('div');
                        userDiv.className = 'usuario-item';
                        userDiv.innerHTML = `
                            <div class="usuario-info">
                                <div class="usuario-nome">${usuario.nome}</div>
                                <div class="usuario-uid">${usuario.uid}</div>
                            </div>
                            <button class="btn btn-danger" onclick="removerUsuario('${usuario.uid}')">Remover</button>`;
                        listaDiv.appendChild(userDiv);
                    });
                }
            } catch (error) { console.error('Erro ao listar usuários:', error); alert('Erro ao carregar lista de usuários'); }
        }
        
        async function adicionarUsuario(event) {
            event.preventDefault();
            const uid = document.getElementById('uid').value.trim().toUpperCase();
            const nome = document.getElementById('nome').value.trim();
            if (!uid || !nome) { alert('Por favor, preencha todos os campos'); return; }
            try {
                const response = await fetch('/api/usuarios', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ uid: uid, nome: nome })
                });
                const result = await response.json();
                if (result.success) {
                    alert('Usuário adicionado com sucesso!');
                    document.getElementById('uid').value = "";
                    document.getElementById('nome').value = "";
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao adicionar usuário:', error); alert('Erro ao adicionar usuário'); }
        }
        
        async function removerUsuario(uid) {
            if (!confirm('Tem certeza que deseja remover este usuário?')) { return; }
            try {
                const response = await fetch('/api/usuarios', {
                    method: 'DELETE',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ uid: uid })
                });
                const result = await response.json();
                if (result.success) {
                    alert('Usuário removido com sucesso!');
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao remover usuário:', error); alert('Erro ao remover usuário'); }
        }
        
        async function servirCafeManual() {
            try {
                const response = await fetch('/api/servir-cafe', { method: 'POST' });
                const result = await response.json();
                if (result.success) {
                    alert('Café servido manualmente!');
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao servir café:', error); alert('Erro ao servir café'); }
        }
        
        async function confirmarLimpeza() {
            const confirmacao = prompt('ATENÇÃO: Isso apagará TODOS os dados!\\n\\nPara confirmar, digite: LIMPAR TUDO');
            if (confirmacao !== 'LIMPAR TUDO') { alert('Operação cancelada'); return; }
            try {
                const response = await fetch('/api/limpar-dados', { method: 'DELETE' });
                const result = await response.json();
                if (result.success) {
                    alert('Todos os dados foram apagados!');
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao limpar dados:', error); alert('Erro ao limpar dados'); }
        }
        
        async function fazerBackup() {
            try {
                const response = await fetch('/api/backup');
                const data = await response.json();
                if (data.usuarios.length === 0) { alert('Não há dados para fazer backup'); return; }
                let backupText = '// BACKUP DOS DADOS - ' + new Date().toLocaleString() + '\\n\\n';
                data.usuarios.forEach(usuario => { backupText += `ADD ${usuario.uid} ${usuario.nome}\\n`; });
                const blob = new Blob([backupText.replace(/\\n/g, '\\r\\n')], { type: 'text/plain' });
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'backup_cafeteira_' + new Date().toISOString().slice(0, 10) + '.txt';
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                alert('Backup realizado com sucesso!');
            } catch (error) { console.error('Erro ao fazer backup:', error); alert('Erro ao fazer backup'); }
        }
        
        setInterval(atualizarStatus, 5000);
        document.addEventListener('DOMContentLoaded', function() {
            atualizarStatus();
            listarUsuarios();
        });
    </script>
</body>
</html>
)=====";
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

void configurar_rotas_api() {
    // Rota principal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", obter_pagina_principal());
    });

    // API para obter status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument json(1024);
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
        DynamicJsonDocument json(2048);
        JsonArray usuarios = json.createNestedArray("usuarios");
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios.createNestedObject();
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
        DynamicJsonDocument response_json(256);
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


    // API para remover usuário
     AsyncCallbackJsonWebHandler* handler_delete = new AsyncCallbackJsonWebHandler("/api/usuarios", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        String uid = obj["uid"];
        DynamicJsonDocument response_json(256);
        if (remover_usuario(uid)) {
            response_json["success"] = true;
        } else {
            response_json["success"] = false;
            response_json["message"] = "Falha ao remover usuário. UID não encontrado.";
        }
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    }, 1024, HTTP_DELETE); // Especifica o método HTTP
    server.addHandler(handler_delete);

    // API para servir café manualmente
    server.on("/api/servir-cafe", HTTP_POST, [](AsyncWebServerRequest *request){
        DynamicJsonDocument response_json(256);
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
        DynamicJsonDocument response_json(256);
        response_json["success"] = true;
        String response;
        serializeJson(response_json, response);
        request->send(200, "application/json", response);
    });

    // API para backup
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
        // Reutiliza a mesma lógica de listar usuários
        DynamicJsonDocument json(2048);
        JsonArray usuarios = json.createNestedArray("usuarios");
        for (int i = 0; i < total_usuarios; i++) {
            JsonObject usuario = usuarios.createNestedObject();
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

  // Configurar pinos
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

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