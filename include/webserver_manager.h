#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "sys_init.h"
#include "config_manager.h"
#include "io_manager.h"
#include "sensor_manager.h"

class WebServerManager {
public:
    // Web server lifecycle
    static void initialize(WebServer* server);
    static void handleClient();
    
    // Server setup and configuration
    static void setupRoutes();
    static void setupStaticFiles();
    
    // Endpoint handlers
    static void handleGetConfig();
    static void handleSetConfig();
    static void handleGetIOStatus();
    static void handleGetSensorConfig();
    static void handleSetSensorConfig();
    static void handleSensorCalibration();
    static void handleSensorTest();
    static void handleSensorCommand();
    static void handleSetOutput();
    static void handleResetLatches();
    static void handleResetSingleLatch();
    
    // Terminal command handlers
    static void handleTerminalCommand();
    static void handleTerminalWatch();
    static void handleTerminalStop();
    
private:
    static WebServer* webServer;
    
    // Helper methods
    static void sendJsonResponse(int code, const String& message);
    static void sendSuccessResponse(bool reboot = false);
    static void sendErrorResponse(const String& error, int code = 400);
    static bool validateJsonInput(const String& input, StaticJsonDocument<1024>& doc);
    static int parseI2CAddress(const String& addrStr);
    
    // Terminal command processing
    static String processTerminalCommand(const String& protocol, const String& command, const String& pin, const String& address);
    static String executeSensorCommand(const String& command, const String& pin, const String& address);
    static String executeDigitalCommand(const String& command, const String& pin);
    static String executeAnalogCommand(const String& command, const String& pin);
    static String executeI2CCommand(const String& command, const String& address);
    static String executeSPICommand(const String& command, const String& pin);
    static String executeUARTCommand(const String& command, const String& pin);
    static String executeNetworkCommand(const String& command, const String& pin);
    static String executeSystemCommand(const String& command);
};

#endif // WEBSERVER_MANAGER_H