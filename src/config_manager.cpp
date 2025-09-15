#include "config_manager.h"

// External variables from main.cpp
extern Config config;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;

void ConfigManager::loadConfig() {
    if (LittleFS.exists(CONFIG_FILE)) {
        File file = LittleFS.open(CONFIG_FILE, "r");
        if (file) {
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error && doc["version"] == CONFIG_VERSION) {
                // Load network settings
                if (doc.containsKey("dhcpEnabled")) config.dhcpEnabled = doc["dhcpEnabled"];
                if (doc.containsKey("ip")) {
                    JsonArray ipArray = doc["ip"];
                    for (int i = 0; i < 4 && i < ipArray.size(); i++) {
                        config.ip[i] = ipArray[i];
                    }
                }
                if (doc.containsKey("gateway")) {
                    JsonArray gwArray = doc["gateway"];
                    for (int i = 0; i < 4 && i < gwArray.size(); i++) {
                        config.gateway[i] = gwArray[i];
                    }
                }
                if (doc.containsKey("subnet")) {
                    JsonArray subArray = doc["subnet"];
                    for (int i = 0; i < 4 && i < subArray.size(); i++) {
                        config.subnet[i] = subArray[i];
                    }
                }
                
                Serial.println("Configuration loaded successfully");
                return;
            }
        }
    }
    
    Serial.println("Using default configuration");
}

void ConfigManager::saveConfig() {
    StaticJsonDocument<1024> doc;
    doc["version"] = CONFIG_VERSION;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    
    JsonArray ipArray = doc.createNestedArray("ip");
    JsonArray gwArray = doc.createNestedArray("gateway");
    JsonArray subArray = doc.createNestedArray("subnet");
    
    for (int i = 0; i < 4; i++) {
        ipArray.add(config.ip[i]);
        gwArray.add(config.gateway[i]);
        subArray.add(config.subnet[i]);
    }
    
    doc["modbusPort"] = config.modbusPort;
    
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("Configuration saved");
    }
}

void ConfigManager::loadSensorConfig() {
    numConfiguredSensors = 0;
    
    if (LittleFS.exists(SENSORS_FILE)) {
        File file = LittleFS.open(SENSORS_FILE, "r");
        if (file) {
            StaticJsonDocument<2048> doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            
            if (!error && doc.is<JsonArray>()) {
                JsonArray sensors = doc.as<JsonArray>();
                
                for (JsonObject sensor : sensors) {
                    if (numConfiguredSensors >= MAX_SENSORS) break;
                    
                    SensorConfig& cfg = configuredSensors[numConfiguredSensors];
                    cfg.enabled = sensor["enabled"] | false;
                    strlcpy(cfg.name, sensor["name"] | "", sizeof(cfg.name));
                    strlcpy(cfg.type, sensor["type"] | "", sizeof(cfg.type));
                    strlcpy(cfg.protocol, sensor["protocol"] | "", sizeof(cfg.protocol));
                    cfg.i2cAddress = sensor["i2cAddress"] | 0;
                    cfg.modbusRegister = sensor["modbusRegister"] | 0;
                    
                    // Load calibration data
                    cfg.offset = sensor["offset"] | 0.0;
                    cfg.scale = sensor["scale"] | 1.0;
                    strlcpy(cfg.expression, sensor["expression"] | "", sizeof(cfg.expression));
                    strlcpy(cfg.polynomialStr, sensor["polynomial"] | "", sizeof(cfg.polynomialStr));
                    
                    // Load I2C parsing settings
                    cfg.dataOffset = sensor["dataOffset"] | 0;
                    cfg.dataLength = sensor["dataLength"] | 2;
                    cfg.dataFormat = sensor["dataFormat"] | 2; // default to uint16_le
                    
                    numConfiguredSensors++;
                }
                
                Serial.printf("Loaded %d sensor configurations\n", numConfiguredSensors);
                return;
            }
        }
    }
    
    Serial.println("No sensor configuration found");
}

void ConfigManager::saveSensorConfig() {
    StaticJsonDocument<2048> doc;
    JsonArray sensors = doc.to<JsonArray>();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensors.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        
        // Save calibration data
        sensor["offset"] = configuredSensors[i].offset;
        sensor["scale"] = configuredSensors[i].scale;
        sensor["expression"] = configuredSensors[i].expression;
        sensor["polynomial"] = configuredSensors[i].polynomialStr;
        
        // Save I2C parsing settings
        sensor["dataOffset"] = configuredSensors[i].dataOffset;
        sensor["dataLength"] = configuredSensors[i].dataLength;
        sensor["dataFormat"] = configuredSensors[i].dataFormat;
    }
    
    File file = LittleFS.open(SENSORS_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("Sensor configuration saved");
    }
}

bool ConfigManager::configExists() {
    return LittleFS.exists(CONFIG_FILE);
}

bool ConfigManager::sensorConfigExists() {
    return LittleFS.exists(SENSORS_FILE);
}

void ConfigManager::resetToDefaults() {
    // Remove existing config files to force defaults
    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }
    if (LittleFS.exists(SENSORS_FILE)) {
        LittleFS.remove(SENSORS_FILE);
    }
    
    // Reset in-memory configuration to defaults
    config.dhcpEnabled = true;
    config.ip[0] = 192; config.ip[1] = 168; config.ip[2] = 1; config.ip[3] = 100;
    config.gateway[0] = 192; config.gateway[1] = 168; config.gateway[2] = 1; config.gateway[3] = 1;
    config.subnet[0] = 255; config.subnet[1] = 255; config.subnet[2] = 255; config.subnet[3] = 0;
    config.modbusPort = 502;
    
    numConfiguredSensors = 0;
    
    Serial.println("Configuration reset to defaults");
}