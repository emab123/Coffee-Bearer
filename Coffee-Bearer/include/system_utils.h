// system_utils.h
#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include <ArduinoJson.h>
#include "logger.h"
#include "coffee_controller.h"

void systemStatusToJson(JsonDocument &doc, Logger &logger, CoffeeController &coffee);

#endif
