#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "sys_init.h"

class ConfigManager {
public:
    // Network configuration management
    static void loadConfig();
    static void saveConfig();
    
    // Sensor configuration management
    static void loadSensorConfig();
    static void saveSensorConfig();
    
    // Helper methods
    static bool configExists();
    static bool sensorConfigExists();
    static void resetToDefaults();
    
private:
    // File paths and version are defined in sys_init.h as macros
    // CONFIG_FILE, SENSORS_FILE, CONFIG_VERSION
};

#endif // CONFIG_MANAGER_H