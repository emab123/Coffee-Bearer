#include "system_utils.h"
#include <Arduino.h>  // for millis(), ESP.getFreeHeap()

void systemStatusToJson(JsonDocument &doc, Logger &logger, CoffeeController &coffee) {
    doc["uptime_ms"]   = millis();
    doc["free_heap"]   = ESP.getFreeHeap();
    doc["coffee_ready"] = coffee.isReady();
    doc["log_size"]    = logger.getTotalLogCount();
    doc["log_total"]   = logger.getTotalLogCount();
    doc["log_errors"]  = logger.getLogCountByLevel(LOG_ERROR);
    doc["log_warnings"] = logger.getLogCountByLevel(LOG_WARNING);
    doc["log_info"]    = logger.getLogCountByLevel(LOG_INFO);
}
