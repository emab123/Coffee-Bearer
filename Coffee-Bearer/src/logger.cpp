#include "logger.h"
#include <SPIFFS.h>
#include <time.h>

Logger::Logger() :
    lastFlush(0),
    fileLogging(true),
    serialLogging(true),
    minimumLevel(DEBUG_LOG_LEVEL) {
}

bool Logger::begin() {
    logBuffer.clear();
    logBuffer.reserve(MAX_LOG_ENTRIES / 4); // Reserve espaço inicial
    
    // Verificar se SPIFFS está montado
    if (!SPIFFS.begin(false)) {
        serialLogging = true; // Garantir que pelo menos serial funcione
        DEBUG_PRINTLN("AVISO: SPIFFS não está montado - apenas logging serial");
        fileLogging = false;
    }
    
    // Limpar logs muito antigos
    cleanupOldLogs();
    
    // Log inicial do sistema
    info("Sistema de logging inicializado");
    info("Versão do sistema: " SYSTEM_VERSION);
    
    DEBUG_PRINTF("Logger inicializado - Nível: %s, Arquivo: %s, Serial: %s\n",
                levelToString(minimumLevel).c_str(),
                fileLogging ? "Sim" : "Não",
                serialLogging ? "Sim" : "Não");
    
    return true;
}

void Logger::end() {
    // Flush final antes de finalizar
    flushToFile();
    logBuffer.clear();
    DEBUG_PRINTLN("Sistema de logging finalizado");
}

void Logger::setMinimumLevel(LogLevel level) {
    minimumLevel = level;
    DEBUG_PRINTF("Nível mínimo de log definido: %s\n", levelToString(level).c_str());
}

void Logger::enableFileLogging(bool enable) {
    fileLogging = enable && SPIFFS.begin(false);
    DEBUG_PRINTF("Logging em arquivo: %s\n", fileLogging ? "Habilitado" : "Desabilitado");
}

void Logger::enableSerialLogging(bool enable) {
    serialLogging = enable;
    DEBUG_PRINTF("Logging serial: %s\n", serialLogging ? "Habilitado" : "Desabilitado");
}

void Logger::log(LogLevel level, const String& category, const String& message, const String& details) {
    // Verificar nível mínimo
    if (level < minimumLevel) {
        return;
    }
    
    // Criar entrada de log
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.details = details;
    
    // Adicionar ao buffer
    logBuffer.push_back(entry);
    
    // Remover entradas antigas se buffer estiver cheio
    if (logBuffer.size() > MAX_LOG_ENTRIES) {
        logBuffer.erase(logBuffer.begin(), logBuffer.begin() + (MAX_LOG_ENTRIES / 4));
    }
    
    // Output serial se habilitado
    if (serialLogging) {
        String formatted = formatLogEntry(entry);
        Serial.println(formatted);
    }
    
    // Flush periódico para arquivo
    if (fileLogging && (millis() - lastFlush > 30000 || level >= LOG_ERROR)) {
        flushToFile();
    }
}

void Logger::debug(const String& message, const String& details) {
    log(LOG_DEBUG, "DEBUG", message, details);
}

void Logger::info(const String& message, const String& details) {
    log(LOG_INFO, "INFO", message, details);
}

void Logger::warning(const String& message, const String& details) {
    log(LOG_WARNING, "WARNING", message, details);
}

void Logger::error(const String& message, const String& details) {
    log(LOG_ERROR, "ERROR", message, details);
}

void Logger::critical(const String& message, const String& details) {
    log(LOG_CRITICAL, "CRITICAL", message, details);
    // Flush imediatamente para logs críticos
    if (fileLogging) {
        flushToFile();
    }
}

void Logger::logRFIDEvent(const String& uid, const String& userName, const String& action, bool success) {
    String message = action + " - " + userName + " (" + uid + ")";
    String details = "Resultado: " + String(success ? "Sucesso" : "Falha");
    
    if (success) {
        info(message, details);
    } else {
        warning(message, details);
    }
}

void Logger::logCoffeeServed(const String& userName, int remainingCoffees) {
    String message = "Café servido para " + userName;
    String details = "Cafés restantes: " + String(remainingCoffees);
    info(message, details);
}

void Logger::logSystemEvent(const String& event, const String& details) {
    log(LOG_INFO, "SYSTEM", event, details);
}

void Logger::logUserManagement(const String& action, const String& uid, const String& userName) {
    String message = action + " - " + userName;
    String details = "UID: " + uid;
    info(message, details);
}

void Logger::logAuthEvent(const String& username, const String& action, const String& ip) {
    String message = action + " - " + username;
    String details = "IP: " + ip;
    
    if (action.indexOf("SUCCESS") >= 0 || action.indexOf("LOGIN") >= 0) {
        info(message, details);
    } else {
        warning(message, details);
    }
}

void Logger::logWebRequest(const String& method, const String& path, const String& ip, int statusCode) {
    String message = method + " " + path;
    String details = "IP: " + ip + ", Status: " + String(statusCode);
    
    if (statusCode >= 400) {
        warning(message, details);
    } else {
        debug(message, details);
    }
}

std::vector<LogEntry> Logger::getRecentLogs(int count) {
    std::vector<LogEntry> recent;
    
    int startIndex = max(0, (int)logBuffer.size() - count);
    
    for (int i = startIndex; i < (int)logBuffer.size(); i++) {
        recent.push_back(logBuffer[i]);
    }
    
    return recent;
}

std::vector<LogEntry> Logger::getLogsByLevel(LogLevel level, int count) {
    std::vector<LogEntry> filtered;
    
    for (auto it = logBuffer.rbegin(); it != logBuffer.rend() && filtered.size() < count; ++it) {
        if (it->level == level) {
            filtered.push_back(*it);
        }
    }
    
    return filtered;
}

std::vector<LogEntry> Logger::getLogsByCategory(const String& category, int count) {
    std::vector<LogEntry> filtered;
    
    for (auto it = logBuffer.rbegin(); it != logBuffer.rend() && filtered.size() < count; ++it) {
        if (it->category.equalsIgnoreCase(category)) {
            filtered.push_back(*it);
        }
    }
    
    return filtered;
}

std::vector<LogEntry> Logger::getLogsByTimeRange(unsigned long startTime, unsigned long endTime) {
    std::vector<LogEntry> filtered;
    
    for (const auto& entry : logBuffer) {
        if (entry.timestamp >= startTime && entry.timestamp <= endTime) {
            filtered.push_back(entry);
        }
    }
    
    return filtered;
}

std::vector<LogEntry> Logger::searchLogs(const String& searchTerm, int count) {
    std::vector<LogEntry> results;
    String lowerSearchTerm = searchTerm;
    lowerSearchTerm.toLowerCase();
    
    for (auto it = logBuffer.rbegin(); it != logBuffer.rend() && results.size() < count; ++it) {
        String message = it->message;
        String details = it->details;
        String category = it->category;
        
        message.toLowerCase();
        details.toLowerCase();
        category.toLowerCase();
        
        if (message.indexOf(lowerSearchTerm) >= 0 || 
            details.indexOf(lowerSearchTerm) >= 0 || 
            category.indexOf(lowerSearchTerm) >= 0) {
            results.push_back(*it);
        }
    }
    
    return results;
}

int Logger::getTotalLogCount() {
    return logBuffer.size();
}

int Logger::getLogCountByLevel(LogLevel level) {
    int count = 0;
    for (const auto& entry : logBuffer) {
        if (entry.level == level) {
            count++;
        }
    }
    return count;
}

int Logger::getLogCountByCategory(const String& category) {
    int count = 0;
    for (const auto& entry : logBuffer) {
        if (entry.category.equalsIgnoreCase(category)) {
            count++;
        }
    }
    return count;
}

unsigned long Logger::getOldestLogTime() {
    if (logBuffer.empty()) return 0;
    return logBuffer.front().timestamp;
}

unsigned long Logger::getNewestLogTime() {
    if (logBuffer.empty()) return 0;
    return logBuffer.back().timestamp;
}

void Logger::clearLogs() {
    logBuffer.clear();
    
    if (fileLogging && SPIFFS.exists(LOG_FILE_PATH)) {
        SPIFFS.remove(LOG_FILE_PATH);
    }
    
    if (fileLogging && SPIFFS.exists(BACKUP_LOG_FILE_PATH)) {
        SPIFFS.remove(BACKUP_LOG_FILE_PATH);
    }
    
    info("Logs limpos");
    DEBUG_PRINTLN("Todos os logs foram limpos");
}

void Logger::clearOldLogs(unsigned long olderThan) {
    unsigned long cutoffTime = millis() - olderThan;
    
    auto it = logBuffer.begin();
    while (it != logBuffer.end()) {
        if (it->timestamp < cutoffTime) {
            it = logBuffer.erase(it);
        } else {
            ++it;
        }
    }
    
    DEBUG_PRINTF("Logs antigos removidos (mais antigos que %lu ms)\n", olderThan);
}

bool Logger::exportLogs(const String& filename) {
    if (!fileLogging) return false;
    
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        error("Falha ao criar arquivo de exportação: " + filename);
        return false;
    }
    
    // Cabeçalho JSON
    file.println("{\"logs\":[");
    
    for (size_t i = 0; i < logBuffer.size(); i++) {
        if (i > 0) file.print(",");
        
        file.print("{");
        file.print("\"timestamp\":" + String(logBuffer[i].timestamp) + ",");
        file.print("\"level\":\"" + levelToString(logBuffer[i].level) + "\",");
        file.print("\"category\":\"" + logBuffer[i].category + "\",");
        file.print("\"message\":\"" + logBuffer[i].message + "\",");
        file.print("\"details\":\"" + logBuffer[i].details + "\"");
        file.println("}");
    }
    
    file.println("]}");
    file.close();
    
    info("Logs exportados para: " + filename);
    return true;
}

size_t Logger::getLogFileSize() {
    if (!fileLogging || !SPIFFS.exists(LOG_FILE_PATH)) {
        return 0;
    }
    
    File file = SPIFFS.open(LOG_FILE_PATH, "r");
    if (!file) return 0;
    
    size_t size = file.size();
    file.close();
    return size;
}

void Logger::printLogs(int count) {
    std::vector<LogEntry> recent = getRecentLogs(count);
    
    Serial.printf("\n=== ÚLTIMOS %d LOGS ===\n", recent.size());
    
    for (const auto& entry : recent) {
        Serial.println(formatLogEntry(entry));
    }
    
    Serial.println("========================\n");
}

void Logger::printLogStats() {
    Serial.println("\n=== ESTATÍSTICAS DE LOGS ===");
    Serial.printf("Total de logs: %d\n", getTotalLogCount());
    Serial.printf("Debug: %d\n", getLogCountByLevel(LOG_DEBUG));
    Serial.printf("Info: %d\n", getLogCountByLevel(LOG_INFO));
    Serial.printf("Warning: %d\n", getLogCountByLevel(LOG_WARNING));
    Serial.printf("Error: %d\n", getLogCountByLevel(LOG_ERROR));
    Serial.printf("Critical: %d\n", getLogCountByLevel(LOG_CRITICAL));
    
    if (fileLogging) {
        Serial.printf("Tamanho do arquivo: %d bytes\n", getLogFileSize());
    }
    
    Serial.printf("Logging em arquivo: %s\n", fileLogging ? "Sim" : "Não");
    Serial.printf("Logging serial: %s\n", serialLogging ? "Sim" : "Não");
    Serial.printf("Nível mínimo: %s\n", levelToString(minimumLevel).c_str());
    
    Serial.println("============================\n");
}

String Logger::getLogsAsJson(int count) {
    std::vector<LogEntry> recent = getRecentLogs(count);
    String json = "[";
    
    for (size_t i = 0; i < recent.size(); i++) {
        if (i > 0) json += ",";
        
        json += "{";
        json += "\"timestamp\":" + String(recent[i].timestamp) + ",";
        json += "\"level\":\"" + levelToString(recent[i].level) + "\",";
        json += "\"category\":\"" + recent[i].category + "\",";
        json += "\"message\":\"" + recent[i].message + "\",";
        json += "\"details\":\"" + recent[i].details + "\"";
        json += "}";
    }
    
    json += "]";
    return json;
}

String Logger::getLogSummary() {
    String summary = "Logs: " + String(getTotalLogCount());
    summary += " (E:" + String(getLogCountByLevel(LOG_ERROR));
    summary += ", W:" + String(getLogCountByLevel(LOG_WARNING));
    summary += ", I:" + String(getLogCountByLevel(LOG_INFO)) + ")";
    return summary;
}

void Logger::maintenance() {
    // Flush periódico
    if (fileLogging && (millis() - lastFlush > 60000)) {
        flushToFile();
    }
    
    // Limpeza de logs antigos
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 3600000) { // A cada hora
        cleanupOldLogs();
        lastCleanup = millis();
    }
    
    // Rotação de arquivo se muito grande
    if (fileLogging && getLogFileSize() > 1024 * 1024) { // 1MB
        rotateLogFile();
    }
}

// Métodos privados

void Logger::flushToFile() {
    if (!fileLogging || logBuffer.empty()) {
        return;
    }
    
    // Encontrar índice da última entrada que foi salva
    static size_t lastFlushedIndex = 0;
    
    if (lastFlushedIndex >= logBuffer.size()) {
        lastFlushedIndex = 0;
    }
    
    File file = SPIFFS.open(LOG_FILE_PATH, "a");
    if (!file) {
        DEBUG_PRINTLN("ERRO: Não foi possível abrir arquivo de log para escrita");
        return;
    }
    
    // Escrever novas entradas
    size_t entriesWritten = 0;
    for (size_t i = lastFlushedIndex; i < logBuffer.size(); i++) {
        String formatted = formatLogEntry(logBuffer[i]);
        file.println(formatted);
        entriesWritten++;
    }
    
    file.close();
    lastFlushedIndex = logBuffer.size();
    lastFlush = millis();
    
    if (entriesWritten > 0) {
        DEBUG_PRINTF("Flush: %d entradas salvas no arquivo\n", entriesWritten);
    }
}

String Logger::formatLogEntry(const LogEntry& entry) {
    String formatted = "[";
    
    // Timestamp em formato legível
    unsigned long seconds = entry.timestamp / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    formatted += String(hours % 24) + ":";
    formatted += (minutes % 60 < 10 ? "0" : "") + String(minutes % 60) + ":";
    formatted += (seconds % 60 < 10 ? "0" : "") + String(seconds % 60);
    
    formatted += "] ";
    formatted += levelToString(entry.level);
    formatted += " [" + entry.category + "] ";
    formatted += entry.message;
    
    if (entry.details.length() > 0) {
        formatted += " | " + entry.details;
    }
    
    return formatted;
}

String Logger::levelToString(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:    return "DEBUG";
        case LOG_INFO:     return "INFO";
        case LOG_WARNING:  return "WARN";
        case LOG_ERROR:    return "ERROR";
        case LOG_CRITICAL: return "CRIT";
        default:           return "UNKNOWN";
    }
}

String Logger::getTimestamp() {
    unsigned long now = millis();
    unsigned long seconds = now / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    String timestamp = "";
    timestamp += (hours % 24 < 10 ? "0" : "") + String(hours % 24) + ":";
    timestamp += (minutes % 60 < 10 ? "0" : "") + String(minutes % 60) + ":";
    timestamp += (seconds % 60 < 10 ? "0" : "") + String(seconds % 60);
    
    return timestamp;
}

void Logger::rotateLogFile() {
    if (!fileLogging) return;
    
    DEBUG_PRINTLN("Executando rotação do arquivo de log");
    
    // Remover backup antigo se existir
    if (SPIFFS.exists(BACKUP_LOG_FILE_PATH)) {
        SPIFFS.remove(BACKUP_LOG_FILE_PATH);
    }
    
    // Mover arquivo atual para backup
    if (SPIFFS.exists(LOG_FILE_PATH)) {
        SPIFFS.rename(LOG_FILE_PATH, BACKUP_LOG_FILE_PATH);
    }
    
    // Resetar índice de flush
    lastFlush = 0;
    
    info("Rotação de log executada");
}

void Logger::cleanupOldLogs() {
    // Remover logs mais antigos que 7 dias do buffer
    unsigned long weekAgo = millis() - (7UL * 24UL * 60UL * 60UL * 1000UL);
    clearOldLogs(weekAgo);
    
    // Se o arquivo de backup for muito antigo, removê-lo
    if (fileLogging && SPIFFS.exists(BACKUP_LOG_FILE_PATH)) {
        File backupFile = SPIFFS.open(BACKUP_LOG_FILE_PATH, "r");
        if (backupFile) {
            // Verificar se o arquivo é muito antigo (implementação simplificada)
            // Em uma implementação real, você verificaria o timestamp do arquivo
            backupFile.close();
        }
    }
}