// include/system_utils.h
#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include <ArduinoJson.h>
#include "logger.h"
#include "coffee_controller.h"
#include "user_manager.h"  // Add UserManager
#include "auth_manager.h"  // Add AuthManager

// Update function signature to include all necessary managers
void systemStatusToJson(JsonDocument &doc, 
                        Logger &logger, 
                        CoffeeController &coffee, 
                        UserManager &userManager,
                        AuthManager &authManager);

#endif