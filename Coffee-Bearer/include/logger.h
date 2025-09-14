/*
==================================================
SISTEMA DE LOGGING
Registro detalhado de eventos do sistema
==================================================
*/

#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"

struct LogEntry {
    unsigned long timestamp;
    LogLevel level;
    String category;
    String message;
    String details;
};

class Logger {
private:
    std::vector<LogEntry> logBuffer;
    unsigned long lastFlush;
    bool fileLogging;
    bool serialLogging;
    LogLevel minimumLevel;
    
    void flushToFile();
    String formatLogEntry(const LogEntry& entry);
    String levelToString(LogLevel level);
    String getTimestamp();
    void rotateLogFile();
    void cleanupOldLogs();
    
public:
    Logger();
    
    // Inicialização
    bool begin();
    void end();
    
    // Configuração
    void setMinimumLevel(LogLevel level);
    void enableFileLogging(bool enable = true);
    void enableSerialLogging(bool enable = true);
    LogLevel getMinimumLevel() { return minimumLevel; }
    
    // Métodos de log principais
    void log(LogLevel level, const String& category, const String& message, const String& details = "");
    void debug(const String& message, const String& details = "");
    void info(const String& message, const String& details = "");
    void warning(const String& message, const String& details = "");
    void error(const String& message, const String& details = "");
    void critical(const String& message, const String& details = "");
    
    // Logs específicos do sistema
    void logRFIDEvent(const String& uid, const String& userName, const String& action, bool success);
    void logCoffeeServed(const String& userName, int remainingCoffees);
    void logSystemEvent(const String& event, const String& details = "");
    void logUserManagement(const String& action, const String& uid, const String& userName);
    void logAuthEvent(const String& username, const String& action, const String& ip);
    void logWebRequest(const String& method, const String& path, const String& ip, int statusCode);
    
    // Consulta de logs
    std::vector<LogEntry> getRecentLogs(int count = 50);
    std::vector<LogEntry> getLogsByLevel(LogLevel level, int count = 50);
    std::vector<LogEntry> getLogsByCategory(const String& category, int count = 50);
    std::vector<LogEntry> getLogsByTimeRange(unsigned long startTime, unsigned long endTime);
    std::vector<LogEntry> searchLogs(const String& searchTerm, int count = 50);
    
    // Estatísticas
    int getTotalLogCount();
    int getLogCountByLevel(LogLevel level);
    int getLogCountByCategory(const String& category);
    unsigned long getOldestLogTime();
    unsigned long getNewestLogTime();
    
    // Gestão de arquivo
    void clearLogs();
    void clearOldLogs(unsigned long olderThan);
    bool exportLogs(const String& filename);
    size_t getLogFileSize();
    bool isFileLoggingEnabled() { return fileLogging; }
    
    // Utilitários
    void printLogs(int count = 20);
    void printLogStats();
    String getLogsAsJson(int count = 50);
    String getLogSummary();
    
    // Manutenção (deve ser chamado periodicamente)
    void maintenance();
};