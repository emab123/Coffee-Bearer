/*
==================================================
CONFIGURAÇÕES CENTRALIZADAS DO SISTEMA
==================================================
*/

#pragma once

#include <Arduino.h>
// Inclui as credenciais e overrides do usuário primeiro
#include "credentials.h"

// ============== VERSÃO DO SISTEMA ==============
#define SYSTEM_VERSION "4.0.0"
#define SYSTEM_NAME "Cafeteira RFID Inteligente"

// ============== CONFIGURAÇÕES DE HARDWARE (Padrões) ==============
// Pinos podem ser sobrescritos em credentials.h
#ifndef RFID_RST_PIN
#define RFID_RST_PIN 4
#endif
#ifndef RFID_SS_PIN
#define RFID_SS_PIN 5
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN 15
#endif
#ifndef RELAY_PIN
#define RELAY_PIN 13
#endif
#ifndef NEOPIXEL_PIN
    #define NEOPIXEL_PIN 34
#endif
#ifndef NEOPIXEL_COUNT
    #define NEOPIXEL_COUNT 1
#endif


// ============== CONFIGURAÇÕES DE REDE (Padrões) ==============
// SSID e Senha devem ser definidos em credentials.h
#ifndef WIFI_SSID
    #define WIFI_SSID "SUA_REDE_AQUI"
    #define WIFI_PASSWORD "SUA_SENHA_AQUI"
    #warning "ATENCAO: Defina WIFI_SSID e WIFI_PASSWORD em seu arquivo credentials.h"
#endif

// Servidor NTP e fuso horário (podem ser sobrescritos em credentials.h)
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif
#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC -10800  // GMT-3 (Brasília)
#endif
#ifndef DAYLIGHT_OFFSET_SEC
#define DAYLIGHT_OFFSET_SEC 0
#endif


// ============== CONFIGURAÇÕES DO SISTEMA ==============
// Limites
#define MAX_USERS 50
#define MAX_COFFEES 100
#define INITIAL_CREDITS 10
#define COFFEE_SERVE_TIME_MS 8000
#define COOLDOWN_TIME_MS 3000

// Timings
#define WEEKLY_RESET_INTERVAL_MS (7UL * 24UL * 60UL * 60UL * 1000UL)  // 7 dias
#define WEEKLY_RESET_CHECK_INTERVAL (60UL * 60UL * 1000UL)            // 1 hora
#define DATA_SAVE_INTERVAL_MS (5UL * 60UL * 1000UL)                   // 5 minutos

// UID da chave mestra (deve ser definido em credentials.h)
#ifndef MASTER_UID
    #define MASTER_UID "FF FF FF FF"
    #warning "ATENCAO: Defina MASTER_UID em seu arquivo credentials.h"
#endif

// ============== CONFIGURAÇÕES DE AUTENTICAÇÃO (Padrões) ==============
// Credenciais Web (devem ser definidas em credentials.h)
#ifndef DEFAULT_ADMIN_USER
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "cafeteira123"
    #warning "SEGURANCA: Altere DEFAULT_ADMIN_USER e DEFAULT_ADMIN_PASS em credentials.h"
#endif
#ifndef DEFAULT_USER_USER
#define DEFAULT_USER_USER "usuario"
#define DEFAULT_USER_PASS "cafe123"
    #warning "SEGURANCA: Altere DEFAULT_USER_USER e DEFAULT_USER_PASS em credentials.h"
#endif

// Configuração de sessão
#define SESSION_TIMEOUT_MS (30UL * 60UL * 1000UL)  // 30 minutos
#define MAX_LOGIN_ATTEMPTS 5
#define LOCKOUT_TIME_MS (15UL * 60UL * 1000UL)     // 15 minutos


// ============== CONFIGURAÇÕES DE LOG ==============
#define MAX_LOG_ENTRIES 500
#define LOG_FILE_PATH "/system.log"
#define BACKUP_LOG_FILE_PATH "/system_backup.log"

// Níveis de log
enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARNING = 2, LOG_ERROR = 3, LOG_CRITICAL = 4 };
#ifndef DEBUG_MODE
    #define DEBUG_MODE 0
#endif
#if DEBUG_MODE
    #define DEBUG_LOG_LEVEL LOG_DEBUG
#else
    #define DEBUG_LOG_LEVEL LOG_INFO
#endif


// ============== PADRÕES VISUAIS E DE ÁUDIO ==============
// Animação
#define LED_ANIMATION_SPEED 100
#define LED_FADE_STEPS 20
#define LED_PULSE_STEPS 50

// Frequências dos tons
#define TONE_SUCCESS_FREQ1  1200
#define TONE_SUCCESS_FREQ2  1500
#define TONE_SUCCESS_DURATION 80
#define TONE_ERROR_FREQ     300
#define TONE_ERROR_DURATION 400
#define TONE_STARTUP_FREQ1  800
#define TONE_STARTUP_FREQ2  1000
#define TONE_STARTUP_FREQ3  1200
#define TONE_STARTUP_DURATION 60

#define TONE_COFFEE_FREQ1   1300
#define TONE_COFFEE_FREQ2   1600
#define TONE_COFFEE_DURATION 100

#define TONE_REFILL_FREQ1   1500
#define TONE_REFILL_FREQ2   1800
#define TONE_REFILL_FREQ3   2200

// ============== CONFIGURAÇÕES WEB ==============
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PORT 81

// Caminhos dos arquivos web
#define WEB_ROOT_PATH "/web"
#define ADMIN_PATH "/admin"
#define USER_PATH "/user"

// Tipos de conteúdo MIME
#define MIME_HTML "text/html"
#define MIME_CSS "text/css"
#define MIME_JS "text/javascript"
#define MIME_JSON "application/json"


// ============== CONFIGURAÇÕES DE BACKUP ==============
#define ENABLE_AUTO_BACKUP true
#define BACKUP_INTERVAL_MS (24UL * 60UL * 60UL * 1000UL)  // 24 horas
#define MAX_BACKUP_FILES 7


// ============== MACROS UTILITÁRIAS E VALIDAÇÃO ==============
#if DEBUG_MODE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(format, ...) Serial.printf(format, __VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(format, ...)
#endif

// ============== ESTRUTURAS DE DADOS ==============
struct SystemStatus {
    bool wifiConnected;
    bool rfidReady;
    bool systemBusy;
    int totalUsers;
    int remainingCoffees;
    int totalServed;
    unsigned long uptime;
    String lastEvent;
    unsigned long lastEventTime;
};

struct UserCredits {
    String uid;
    String name;
    int credits;
    unsigned long lastUsed;
    bool isActive;
};

// ============== CONSTANTES CALCULADAS ==============
const unsigned long MILLIS_PER_DAY = 24UL * 60UL * 60UL * 1000UL;
const unsigned long MILLIS_PER_HOUR = 60UL * 60UL * 1000UL;
const unsigned long MILLIS_PER_MINUTE = 60UL * 1000UL;