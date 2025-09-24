#include "config_manager.h"
#include "../include/sys_init.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

extern Config config;
extern IOStatus ioStatus;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;

void loadConfig() {
    if (LittleFS.exists(CONFIG_FILE)) {
        File configFile = LittleFS.open(CONFIG_FILE, "r");
        if (configFile) {
            StaticJsonDocument<2048> doc;
            DeserializationError error = deserializeJson(doc, configFile);
            configFile.close();
            if (!error) {
                config.version = doc["version"] | CONFIG_VERSION;
                config.dhcpEnabled = doc["dhcpEnabled"] | true;
                if (doc.containsKey("modbusPort")) {
                    config.modbusPort = doc["modbusPort"].as<uint16_t>();
                } else {
                    config.modbusPort = 502;
                }
                JsonArray ipArray = doc["ip"].as<JsonArray>();
                if (ipArray) {
                    for (int i = 0; i < 4; i++) config.ip[i] = ipArray[i] | 192;
                }
                JsonArray gatewayArray = doc["gateway"].as<JsonArray>();
                if (gatewayArray) {
                    for (int i = 0; i < 4; i++) config.gateway[i] = gatewayArray[i] | 192;
                }
                JsonArray subnetArray = doc["subnet"].as<JsonArray>();
                if (subnetArray) {
                    for (int i = 0; i < 4; i++) config.subnet[i] = subnetArray[i] | 255;
                }
                const char* hostname = doc["hostname"] | "modbus-io-module";
                strncpy(config.hostname, hostname, HOSTNAME_MAX_LENGTH - 1);
                config.hostname[HOSTNAME_MAX_LENGTH - 1] = '\0';
                JsonArray diPullupArray = doc["diPullup"].as<JsonArray>();
                if (diPullupArray) {
                    for (int i = 0; i < 8; i++) config.diPullup[i] = diPullupArray[i] | true;
                }
                JsonArray diInvertArray = doc["diInvert"].as<JsonArray>();
                if (diInvertArray) {
                    for (int i = 0; i < 8; i++) config.diInvert[i] = diInvertArray[i] | false;
                }
                JsonArray diLatchArray = doc["diLatch"].as<JsonArray>();
                if (diLatchArray) {
                    for (int i = 0; i < 8; i++) config.diLatch[i] = diLatchArray[i] | false;
                }
                JsonArray doInvertArray = doc["doInvert"].as<JsonArray>();
                if (doInvertArray) {
                    for (int i = 0; i < 8; i++) config.doInvert[i] = doInvertArray[i] | false;
                }
                JsonArray doInitialStateArray = doc["doInitialState"].as<JsonArray>();
                if (doInitialStateArray) {
                    for (int i = 0; i < 8; i++) config.doInitialState[i] = doInitialStateArray[i] | false;
                }
            }
        }
    }
}

void saveConfig() {
    StaticJsonDocument<2048> doc;
    doc["version"] = config.version;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    doc["modbusPort"] = config.modbusPort;
    JsonArray ipArray = doc.createNestedArray("ip");
    JsonArray gatewayArray = doc.createNestedArray("gateway");
    JsonArray subnetArray = doc.createNestedArray("subnet");
    for (int i = 0; i < 4; i++) {
        ipArray.add(config.ip[i]);
        gatewayArray.add(config.gateway[i]);
        subnetArray.add(config.subnet[i]);
    }
    doc["hostname"] = config.hostname;
    JsonArray diPullupArray = doc.createNestedArray("diPullup");
    JsonArray diInvertArray = doc.createNestedArray("diInvert");
    JsonArray diLatchArray = doc.createNestedArray("diLatch");
    for (int i = 0; i < 8; i++) {
        diPullupArray.add(config.diPullup[i]);
        diInvertArray.add(config.diInvert[i]);
        diLatchArray.add(config.diLatch[i]);
    }
    JsonArray doInvertArray = doc.createNestedArray("doInvert");
    JsonArray doInitialStateArray = doc.createNestedArray("doInitialState");
    for (int i = 0; i < 8; i++) {
        doInvertArray.add(config.doInvert[i]);
        doInitialStateArray.add(config.doInitialState[i]);
    }
    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
    }
}

void loadSensorConfig() {
    numConfiguredSensors = 0;
    memset(configuredSensors, 0, sizeof(configuredSensors));
    if (!LittleFS.exists(SENSORS_FILE)) return;
    File sensorsFile = LittleFS.open(SENSORS_FILE, "r");
    if (!sensorsFile) return;
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, sensorsFile);
    sensorsFile.close();
    if (error) return;
    if (doc.containsKey("sensors") && doc["sensors"].is<JsonArray>()) {
        JsonArray sensorsArray = doc["sensors"].as<JsonArray>();
        int index = 0;
        for (JsonObject sensor : sensorsArray) {
            if (index >= MAX_SENSORS) break;
            configuredSensors[index].enabled = sensor["enabled"] | false;
            const char* name = sensor["name"] | "";
            strncpy(configuredSensors[index].name, name, sizeof(configuredSensors[index].name) - 1);
            configuredSensors[index].name[sizeof(configuredSensors[index].name) - 1] = '\0';
            const char* sensor_type = sensor["sensor_type"] | "";
            strncpy(configuredSensors[index].sensor_type, sensor_type, sizeof(configuredSensors[index].sensor_type) - 1);
            configuredSensors[index].sensor_type[sizeof(configuredSensors[index].sensor_type) - 1] = '\0';
            const char* formula = sensor["formula"] | "";
            strncpy(configuredSensors[index].formula, formula, sizeof(configuredSensors[index].formula) - 1);
            configuredSensors[index].formula[sizeof(configuredSensors[index].formula) - 1] = '\0';
            const char* units = sensor["units"] | "";
            strncpy(configuredSensors[index].units, units, sizeof(configuredSensors[index].units) - 1);
            configuredSensors[index].units[sizeof(configuredSensors[index].units) - 1] = '\0';
            const char* type = sensor["type"] | "";
            strncpy(configuredSensors[index].type, type, sizeof(configuredSensors[index].type) - 1);
            configuredSensors[index].type[sizeof(configuredSensors[index].type) - 1] = '\0';
            configuredSensors[index].i2cAddress = sensor["i2cAddress"] | 0;
            configuredSensors[index].modbusRegister = sensor["modbusRegister"] | 0;
            configuredSensors[index].updateInterval = sensor["updateInterval"] | 1000;
            const char* calibrationData = sensor["calibrationData"] | "";
            strncpy(configuredSensors[index].calibrationData, calibrationData, sizeof(configuredSensors[index].calibrationData) - 1);
            configuredSensors[index].calibrationData[sizeof(configuredSensors[index].calibrationData) - 1] = '\0';
            const char* response = sensor["response"] | "";
            strncpy(configuredSensors[index].response, response, sizeof(configuredSensors[index].response) - 1);
            configuredSensors[index].response[sizeof(configuredSensors[index].response) - 1] = '\0';
            configuredSensors[index].cmdPending = sensor["cmdPending"] | false;
            configuredSensors[index].lastCmdSent = sensor["lastCmdSent"] | 0;
            index++;
        }
        numConfiguredSensors = index;
    }
}

void saveSensorConfig() {
    StaticJsonDocument<1024> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["sensor_type"] = configuredSensors[i].sensor_type;
        sensor["formula"] = configuredSensors[i].formula;
        sensor["units"] = configuredSensors[i].units;
        sensor["type"] = configuredSensors[i].type;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        sensor["updateInterval"] = configuredSensors[i].updateInterval;
        sensor["calibrationData"] = configuredSensors[i].calibrationData;
        sensor["response"] = configuredSensors[i].response;
        sensor["cmdPending"] = configuredSensors[i].cmdPending;
        sensor["lastCmdSent"] = configuredSensors[i].lastCmdSent;
    }
    File sensorsFile = LittleFS.open(SENSORS_FILE, "w");
    if (sensorsFile) {
        serializeJson(doc, sensorsFile);
        sensorsFile.close();
    }
}
