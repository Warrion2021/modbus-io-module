#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
#include "../include/sys_init.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

void loadConfig();
void saveConfig();
void loadSensorConfig();
void saveSensorConfig();

#endif // CONFIG_MANAGER_H
