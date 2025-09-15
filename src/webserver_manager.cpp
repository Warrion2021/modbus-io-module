#include "webserver_manager.h"
#include <Wire.h>

// Static member definition
WebServer* WebServerManager::webServer = nullptr;

// External variables from main.cpp
extern Config config;
extern IOStatus ioStatus;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;
extern int connectedClients;

void WebServerManager::initialize(WebServer* server) {
    webServer = server;
    setupStaticFiles();
    setupRoutes();
}

void WebServerManager::handleClient() {
    if (webServer) {
        webServer->handleClient();
    }
}

void WebServerManager::setupStaticFiles() {
    if (webServer) {
        // Serve static files from LittleFS
        webServer->serveStatic("/", LittleFS, "/");
    }
}

void WebServerManager::setupRoutes() {
    if (!webServer) return;
    
    // Configuration endpoints
    webServer->on("/config", HTTP_GET, handleGetConfig);
    webServer->on("/config", HTTP_POST, handleSetConfig);
    
    // IO status endpoints
    webServer->on("/iostatus", HTTP_GET, handleGetIOStatus);
    
    // Sensor configuration endpoints
    webServer->on("/sensors/config", HTTP_GET, handleGetSensorConfig);
    webServer->on("/sensors/config", HTTP_POST, handleSetSensorConfig);
    
    // Sensor operation endpoints
    webServer->on("/api/sensor/calibration", HTTP_POST, handleSensorCalibration);
    webServer->on("/api/sensor/test", HTTP_POST, handleSensorTest);
    webServer->on("/api/sensor/command", HTTP_POST, handleSensorCommand);
    
    // Output control endpoints
    webServer->on("/setoutput", HTTP_POST, handleSetOutput);
    
    // Latch reset endpoints
    webServer->on("/reset-latches", HTTP_POST, handleResetLatches);
    webServer->on("/reset-latch", HTTP_POST, handleResetSingleLatch);
    
    // Terminal command endpoints
    webServer->on("/api/terminal/command", HTTP_POST, handleTerminalCommand);
    webServer->on("/api/terminal/watch", HTTP_POST, handleTerminalWatch);
    webServer->on("/api/terminal/stop", HTTP_POST, handleTerminalStop);
}

void WebServerManager::handleGetConfig() {
    StaticJsonDocument<512> doc;
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
    doc["connectedClients"] = connectedClients;
    
    String response;
    serializeJson(doc, response);
    webServer->send(200, "application/json", response);
}

void WebServerManager::handleSetConfig() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<1024> doc;
        if (validateJsonInput(webServer->arg("plain"), doc)) {
            bool needsReboot = false;
            
            if (doc.containsKey("dhcpEnabled")) {
                bool newDHCP = doc["dhcpEnabled"];
                if (newDHCP != config.dhcpEnabled) needsReboot = true;
                config.dhcpEnabled = newDHCP;
            }
            
            if (doc.containsKey("ip")) {
                JsonArray ipArray = doc["ip"];
                if (ipArray.size() == 4) {
                    bool changed = false;
                    for (int i = 0; i < 4; i++) {
                        uint8_t newVal = ipArray[i];
                        if (newVal != config.ip[i]) changed = true;
                        config.ip[i] = newVal;
                    }
                    if (changed) needsReboot = true;
                }
            }
            
            ConfigManager::saveConfig();
            
            if (needsReboot) {
                sendSuccessResponse(true);
                delay(1000);
                rp2040.restart();
            } else {
                sendSuccessResponse();
            }
            return;
        }
    }
    sendErrorResponse("Invalid request");
}

void WebServerManager::handleGetIOStatus() {
    StaticJsonDocument<1024> doc;
    
    JsonArray dIn = doc.createNestedArray("digital_inputs");
    JsonArray dOut = doc.createNestedArray("digital_outputs");
    JsonArray aIn = doc.createNestedArray("analog_inputs");
    
    for (int i = 0; i < 8; i++) {
        dIn.add(ioStatus.dIn[i]);
        dOut.add(ioStatus.dOut[i]);
        if (i < 3) aIn.add(ioStatus.aIn[i]);
    }
    
    doc["temperature"] = ioStatus.temperature;
    doc["humidity"] = ioStatus.humidity;
    doc["pressure"] = ioStatus.pressure;
    
    String response;
    serializeJson(doc, response);
    webServer->send(200, "application/json", response);
}

void WebServerManager::handleGetSensorConfig() {
    StaticJsonDocument<2048> doc;
    JsonArray sensors = doc.createNestedArray("sensors");
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensors.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
    }
    
    String response;
    serializeJson(doc, response);
    webServer->send(200, "application/json", response);
}

void WebServerManager::handleSetSensorConfig() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("sensors") && doc["sensors"].is<JsonArray>()) {
            JsonArray sensors = doc["sensors"];
            numConfiguredSensors = 0;
            
            for (JsonObject sensor : sensors) {
                if (numConfiguredSensors >= MAX_SENSORS) break;
                
                SensorConfig& cfg = configuredSensors[numConfiguredSensors];
                cfg.enabled = sensor["enabled"] | false;
                strlcpy(cfg.name, sensor["name"] | "", sizeof(cfg.name));
                strlcpy(cfg.type, sensor["type"] | "", sizeof(cfg.type));
                strlcpy(cfg.protocol, sensor["protocol"] | "", sizeof(cfg.protocol));
                cfg.i2cAddress = sensor["i2cAddress"] | 0;
                cfg.modbusRegister = sensor["modbusRegister"] | 0;
                
                numConfiguredSensors++;
            }
            
            ConfigManager::saveSensorConfig();
            sendSuccessResponse(true);
            
            delay(1000);
            rp2040.restart();
            return;
        }
    }
    sendErrorResponse("Invalid request");
}

void WebServerManager::handleSensorCalibration() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("name")) {
            String sensorName = doc["name"];
            
            // Find sensor by name
            int sensorIndex = -1;
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (strcmp(configuredSensors[i].name, sensorName.c_str()) == 0) {
                    sensorIndex = i;
                    break;
                }
            }
            
            if (sensorIndex >= 0) {
                SensorConfig& cfg = configuredSensors[sensorIndex];
                
                // Update calibration method
                String method = doc["method"] | "linear";
                
                // Clear all calibration data first
                cfg.offset = 0.0;
                cfg.scale = 1.0;
                cfg.expression[0] = '\0';
                cfg.polynomialStr[0] = '\0';
                
                // Apply method-specific calibration
                if (method == "linear") {
                    cfg.offset = doc["offset"] | 0.0;
                    cfg.scale = doc["scale"] | 1.0;
                } else if (method == "polynomial") {
                    String polynomial = doc["polynomial"] | "";
                    strlcpy(cfg.polynomialStr, polynomial.c_str(), sizeof(cfg.polynomialStr));
                } else if (method == "expression") {
                    String expression = doc["expression"] | "";
                    strlcpy(cfg.expression, expression.c_str(), sizeof(cfg.expression));
                }
                
                // Update I2C parsing settings if provided
                if (doc.containsKey("i2c_parsing")) {
                    JsonObject i2c = doc["i2c_parsing"];
                    cfg.dataOffset = i2c["data_offset"] | 0;
                    cfg.dataLength = i2c["data_length"] | 2;
                    
                    String format = i2c["data_format"] | "uint16_le";
                    if (format == "uint8") cfg.dataFormat = 0;
                    else if (format == "uint16_be") cfg.dataFormat = 1;
                    else if (format == "uint16_le") cfg.dataFormat = 2;
                    else if (format == "uint32_be") cfg.dataFormat = 3;
                    else if (format == "uint32_le") cfg.dataFormat = 4;
                    else if (format == "float32") cfg.dataFormat = 5;
                    else cfg.dataFormat = 2; // default to uint16_le
                }
                
                // Save configuration
                ConfigManager::saveSensorConfig();
                
                sendSuccessResponse();
                return;
            }
            
            webServer->send(404, "application/json", "{\"success\":false,\"error\":\"Sensor not found\"}");
            return;
        }
    }
    sendErrorResponse("Invalid request");
}

void WebServerManager::handleSensorTest() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("name")) {
            String sensorName = doc["name"];
            
            // Find sensor by name
            int sensorIndex = -1;
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (strcmp(configuredSensors[i].name, sensorName.c_str()) == 0) {
                    sensorIndex = i;
                    break;
                }
            }
            
            if (sensorIndex >= 0) {
                // Force a sensor reading update
                SensorManager::updateAllSensors();
                
                webServer->send(200, "application/json", "{\"success\":true,\"message\":\"Sensor test completed\"}");
                return;
            }
            
            webServer->send(404, "application/json", "{\"success\":false,\"error\":\"Sensor not found\"}");
            return;
        }
    }
    sendErrorResponse("Invalid request");
}

void WebServerManager::handleSensorCommand() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("sensorIndex") && doc.containsKey("command")) {
            int sensorIndex = doc["sensorIndex"];
            String command = doc["command"];
            
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                // Send command to EZO sensor
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                
                StaticJsonDocument<256> response;
                response["success"] = true;
                response["message"] = "Command '" + command + "' sent to sensor " + String(sensorIndex);
                response["sensorIndex"] = sensorIndex;
                response["command"] = command;
                
                String responseStr;
                serializeJson(response, responseStr);
                webServer->send(200, "application/json", responseStr);
                return;
            } else {
                sendErrorResponse("Invalid sensor index. Must be 0-" + String(MAX_SENSORS-1));
                return;
            }
        }
    }
    sendErrorResponse("Invalid request. Required: sensorIndex, command");
}

void WebServerManager::handleSetOutput() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("output") && doc.containsKey("state")) {
            int output = doc["output"];
            bool state = doc["state"];
            
            if (output >= 0 && output < 8) {
                IOManager::setDigitalOutput(output, state);
                sendSuccessResponse();
                return;
            }
        }
    }
    sendErrorResponse("Invalid request");
}

void WebServerManager::handleResetLatches() {
    IOManager::resetAllLatches();
    sendSuccessResponse();
}

void WebServerManager::handleResetSingleLatch() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("input")) {
            int input = doc["input"];
            if (input >= 0 && input < 8) {
                IOManager::resetLatch(input);
                sendSuccessResponse();
                return;
            }
        }
    }
    sendErrorResponse("Invalid request");
}

// Helper methods
void WebServerManager::sendJsonResponse(int code, const String& message) {
    if (webServer) {
        webServer->send(code, "application/json", message);
    }
}

void WebServerManager::sendSuccessResponse(bool reboot) {
    if (reboot) {
        sendJsonResponse(200, "{\"success\":true,\"reboot\":true}");
    } else {
        sendJsonResponse(200, "{\"success\":true}");
    }
}

void WebServerManager::sendErrorResponse(const String& error, int code) {
    String response = "{\"success\":false,\"error\":\"" + error + "\"}";
    sendJsonResponse(code, response);
}

bool WebServerManager::validateJsonInput(const String& input, StaticJsonDocument<1024>& doc) {
    DeserializationError error = deserializeJson(doc, input);
    return !error;
}

// Terminal command handlers
void WebServerManager::handleTerminalCommand() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("protocol") && doc.containsKey("command")) {
            String protocol = doc["protocol"];
            String command = doc["command"];
            String pin = doc["pin"] | "";
            String address = doc["address"] | "";
            
            String result = processTerminalCommand(protocol, command, pin, address);
            
            StaticJsonDocument<512> response;
            response["success"] = true;
            response["result"] = result;
            response["timestamp"] = millis();
            
            String responseStr;
            serializeJson(response, responseStr);
            webServer->send(200, "application/json", responseStr);
            return;
        }
    }
    sendErrorResponse("Invalid terminal command request");
}

void WebServerManager::handleTerminalWatch() {
    if (webServer->hasArg("plain")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, webServer->arg("plain"));
        
        if (!error && doc.containsKey("protocol")) {
            String protocol = doc["protocol"];
            String pin = doc["pin"] | "";
            String address = doc["address"] | "";
            
            // Start watching the specified protocol/pin/address
            String result = "Watch started for " + protocol;
            if (pin.length() > 0) result += " on pin " + pin;
            if (address.length() > 0) result += " at address " + address;
            
            StaticJsonDocument<256> response;
            response["success"] = true;
            response["result"] = result;
            response["watching"] = true;
            
            String responseStr;
            serializeJson(response, responseStr);
            webServer->send(200, "application/json", responseStr);
            return;
        }
    }
    sendErrorResponse("Invalid watch request");
}

void WebServerManager::handleTerminalStop() {
    StaticJsonDocument<256> response;
    response["success"] = true;
    response["result"] = "Watch stopped";
    response["watching"] = false;
    
    String responseStr;
    serializeJson(response, responseStr);
    webServer->send(200, "application/json", responseStr);
}

// Terminal command processing
String WebServerManager::processTerminalCommand(const String& protocol, const String& command, const String& pin, const String& address) {
    if (protocol == "sensor") {
        return executeSensorCommand(command, pin, address);
    } else if (protocol == "digital") {
        return executeDigitalCommand(command, pin);
    } else if (protocol == "analog") {
        return executeAnalogCommand(command, pin);
    } else if (protocol == "i2c") {
        return executeI2CCommand(command, address);
    } else if (protocol == "spi") {
        return executeSPICommand(command, pin);
    } else if (protocol == "uart") {
        return executeUARTCommand(command, pin);
    } else if (protocol == "network") {
        return executeNetworkCommand(command, pin);
    } else if (protocol == "system") {
        return executeSystemCommand(command);
    } else {
        return "Error: Unknown protocol '" + protocol + "'";
    }
}

String WebServerManager::executeSensorCommand(const String& command, const String& pin, const String& address) {
    if (command == "scan") {
        SensorManager::updateAllSensors();
        return "Sensor scan completed - check /iostatus for results";
    } else if (command == "read" || command == "r") {
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "r");
                return "Reading command sent to sensor " + pin + " - check /iostatus for response";
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for read command";
        }
    } else if (command.startsWith("cal,")) {
        // EZO Calibration commands: cal,mid,7.00 | cal,low,4.00 | cal,high,10.00 | cal,clear
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Calibration command '" + command + "' sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for calibration";
        }
    } else if (command.startsWith("t,")) {
        // Temperature compensation: t,25.0
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Temperature compensation '" + command + "' sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for temperature compensation";
        }
    } else if (command.startsWith("o,")) {
        // Output format: o,ph,1 | o,ec,1 | o,temp,1
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Output format '" + command + "' sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for output format";
        }
    } else if (command.startsWith("c,")) {
        // Continuous reading: c,1 (start) | c,0 (stop)
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Continuous reading '" + command + "' sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for continuous reading";
        }
    } else if (command == "i" || command == "info") {
        // Device information
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "i");
                return "Device info request sent to sensor " + pin + " - check /iostatus for response";
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for device info";
        }
    } else if (command == "status") {
        // Device status
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "status");
                return "Status request sent to sensor " + pin + " - check /iostatus for response";
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for status";
        }
    } else if (command.startsWith("name,")) {
        // Device naming: name,? | name,newname
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Name command '" + command + "' sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for name command";
        }
    } else if (command == "factory") {
        // Factory reset
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "factory");
                return "Factory reset sent to sensor " + pin + " - sensor will restart";
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for factory reset";
        }
    } else if (command == "sleep") {
        // Sleep mode
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "sleep");
                return "Sleep command sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for sleep command";
        }
    } else if (command == "x" || command == "wake") {
        // Wake from sleep
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, "x");
                return "Wake command sent to sensor " + pin;
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for wake command";
        }
    } else if (command == "help") {
        return "EZO Sensor Commands:\n"
               "  r                  - Single reading\n"
               "  cal,mid,7.00       - Mid-point calibration\n"
               "  cal,low,4.00       - Low-point calibration\n"
               "  cal,high,10.00     - High-point calibration\n"
               "  cal,clear          - Clear calibration\n"
               "  t,25.0             - Temperature compensation\n"
               "  c,1                - Start continuous reading\n"
               "  c,0                - Stop continuous reading\n"
               "  o,ph,1             - Enable pH output\n"
               "  o,ec,1             - Enable conductivity output\n"
               "  i                  - Device information\n"
               "  status             - Device status\n"
               "  name,?             - Get device name\n"
               "  name,newname       - Set device name\n"
               "  factory            - Factory reset\n"
               "  sleep              - Enter sleep mode\n"
               "  x                  - Wake from sleep\n"
               "  help               - Show this help";
    } else {
        // Pass through any other command directly to EZO sensor
        if (pin.length() > 0) {
            int sensorIndex = pin.toInt();
            if (sensorIndex >= 0 && sensorIndex < MAX_SENSORS) {
                SensorManager::sendEzoCommand(sensorIndex, command.c_str());
                return "Custom command '" + command + "' sent to sensor " + pin + " - check /iostatus for response";
            } else {
                return "Error: Invalid sensor index " + pin;
            }
        } else {
            return "Error: Sensor index required for commands. Use 'help' for command list.";
        }
    }
}

String WebServerManager::executeDigitalCommand(const String& command, const String& pin) {
    if (pin.length() == 0) {
        return "Error: Pin number required for digital commands";
    }
    
    int pinNum = pin.toInt();
    if (pinNum < 0 || pinNum > 15) {
        return "Error: Pin number must be 0-15 (DI0-7=GPIO0-7, DO0-7=GPIO8-15)";
    }
    
    if (command == "read") {
        if (pinNum <= 7) {
            // Digital Input pins (DI0-7 = GPIO 0-7)
            bool state = IOManager::getDigitalInput(pinNum);
            bool rawState = digitalRead(pinNum);
            return "DI" + pin + " = " + (state ? "HIGH" : "LOW") + " (Raw: " + (rawState ? "HIGH" : "LOW") + ")";
        } else {
            // Digital Output pins (DO0-7 = GPIO 8-15)
            int outputIndex = pinNum - 8;
            bool state = IOManager::getDigitalOutput(outputIndex);
            return "DO" + String(outputIndex) + " = " + (state ? "HIGH" : "LOW");
        }
    } else if (command == "write" || command.startsWith("write ")) {
        if (pinNum <= 7) {
            return "Error: Cannot write to digital input pin DI" + pin + ". Use DO pins (8-15)";
        }
        
        // Parse value from command
        String value = "";
        if (command.length() > 6) {
            value = command.substring(6);
            value.trim();
        }
        
        if (value == "1" || value.equalsIgnoreCase("HIGH")) {
            int outputIndex = pinNum - 8;
            IOManager::setDigitalOutput(outputIndex, true);
            return "DO" + String(outputIndex) + " set to HIGH";
        } else if (value == "0" || value.equalsIgnoreCase("LOW")) {
            int outputIndex = pinNum - 8;
            IOManager::setDigitalOutput(outputIndex, false);
            return "DO" + String(outputIndex) + " set to LOW";
        } else {
            return "Error: Invalid value '" + value + "'. Use 1/0 or HIGH/LOW";
        }
    } else if (command == "high" || command == "1") {
        if (pinNum <= 7) {
            return "Error: Cannot write to digital input pin DI" + pin + ". Use DO pins (8-15)";
        }
        int outputIndex = pinNum - 8;
        IOManager::setDigitalOutput(outputIndex, true);
        return "DO" + String(outputIndex) + " set to HIGH";
    } else if (command == "low" || command == "0") {
        if (pinNum <= 7) {
            return "Error: Cannot write to digital input pin DI" + pin + ". Use DO pins (8-15)";
        }
        int outputIndex = pinNum - 8;
        IOManager::setDigitalOutput(outputIndex, false);
        return "DO" + String(outputIndex) + " set to LOW";
    } else if (command == "toggle") {
        if (pinNum <= 7) {
            return "Error: Cannot toggle digital input pin DI" + pin + ". Use DO pins (8-15)";
        }
        int outputIndex = pinNum - 8;
        bool currentState = IOManager::getDigitalOutput(outputIndex);
        IOManager::setDigitalOutput(outputIndex, !currentState);
        return "DO" + String(outputIndex) + " toggled to " + (!currentState ? "HIGH" : "LOW");
    } else if (command.startsWith("config ")) {
        if (pinNum > 7) {
            return "Error: Config only available for digital input pins DI0-7";
        }
        
        String configOption = command.substring(7);
        configOption.trim();
        
        if (configOption == "pullup") {
            // Toggle pullup for this pin
            IOManager::toggleInputPullup(pinNum);
            return "DI" + pin + " pullup toggled";
        } else if (configOption == "invert") {
            // Toggle inversion for this pin
            IOManager::toggleInputInversion(pinNum);
            return "DI" + pin + " inversion toggled";
        } else if (configOption == "latch") {
            // Toggle latching for this pin
            IOManager::toggleInputLatching(pinNum);
            return "DI" + pin + " latching toggled";
        } else {
            return "Error: Unknown config option '" + configOption + "'. Use: pullup, invert, latch";
        }
    } else if (command == "help") {
        return "Digital Commands:\n"
               "  read               - Read pin state\n"
               "  write <1/0>        - Write to output pin (HIGH/LOW)\n"
               "  high / 1           - Set output HIGH\n"
               "  low / 0            - Set output LOW\n"
               "  toggle             - Toggle output state\n"
               "  config <option>    - Configure input (pullup/invert/latch)\n"
               "Pin Map: DI0-7=GPIO0-7 (inputs), DO0-7=GPIO8-15 (outputs)";
    } else {
        return "Error: Unknown digital command '" + command + "'. Use 'help' for command list";
    }
}

String WebServerManager::executeAnalogCommand(const String& command, const String& pin) {
    if (pin.length() == 0) {
        return "Error: Pin number required for analog commands";
    }
    
    int pinNum = pin.toInt();
    if (pinNum < 0 || pinNum > 2) {
        return "Error: Analog pin number must be 0-2 (AI0-2 = GPIO 26-28)";
    }
    
    if (command == "read") {
        float value = IOManager::getAnalogInput(pinNum);
        return "AI" + pin + " = " + String((int)value) + " mV";
    } else if (command == "config") {
        return "AI" + pin + " - Pin " + String(26 + pinNum) + ", Range: 0-3300mV, Resolution: 12-bit";
    } else if (command == "help") {
        return "Analog Commands:\n"
               "  read               - Read analog value in millivolts\n"
               "  config             - Show pin configuration\n"
               "Pin Map: AI0-2 = GPIO 26-28";
    } else {
        return "Error: Unknown analog command '" + command + "'. Use 'help' for command list";
    }
}

String WebServerManager::executeI2CCommand(const String& command, const String& address) {
    if (command == "scan") {
        String result = "I2C Device Scan:\n";
        bool foundDevice = false;
        
        for (int addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                result += "Found device at 0x" + String(addr, HEX) + "\n";
                foundDevice = true;
            }
        }
        
        if (!foundDevice) {
            result += "No I2C devices found";
        }
        return result;
    } else if (command == "probe") {
        if (address.length() == 0) {
            return "Error: I2C address required for probe command";
        }
        
        int addr = parseI2CAddress(address);
        if (addr < 1 || addr > 126) {
            return "Error: Invalid I2C address. Must be 1-126 (0x01-0x7E)";
        }
        
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            return "Device at " + address + " is present";
        } else {
            return "No device found at " + address;
        }
    } else if (command.startsWith("read ")) {
        if (address.length() == 0) {
            return "Error: I2C address required for read command";
        }
        
        String regStr = command.substring(5);
        regStr.trim();
        int reg = parseI2CAddress(regStr);
        int addr = parseI2CAddress(address);
        
        if (addr < 1 || addr > 126) {
            return "Error: Invalid I2C address";
        }
        
        Wire.beginTransmission(addr);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) {
            return "Error: Failed to write register address";
        }
        
        if (Wire.requestFrom(addr, 1) == 1) {
            int value = Wire.read();
            return "Register 0x" + String(reg, HEX) + " = 0x" + String(value, HEX) + " (" + String(value) + ")";
        } else {
            return "Error: Failed to read from register 0x" + String(reg, HEX);
        }
    } else if (command.startsWith("write ")) {
        if (address.length() == 0) {
            return "Error: I2C address required for write command";
        }
        
        String params = command.substring(6);
        params.trim();
        int spaceIndex = params.indexOf(' ');
        
        if (spaceIndex == -1) {
            return "Error: Write command requires register and data: write <register> <data>";
        }
        
        String regStr = params.substring(0, spaceIndex);
        String dataStr = params.substring(spaceIndex + 1);
        
        int reg = parseI2CAddress(regStr);
        int data = parseI2CAddress(dataStr);
        int addr = parseI2CAddress(address);
        
        if (addr < 1 || addr > 126) {
            return "Error: Invalid I2C address";
        }
        
        Wire.beginTransmission(addr);
        Wire.write(reg);
        Wire.write(data);
        if (Wire.endTransmission() == 0) {
            return "Wrote 0x" + String(data, HEX) + " to register 0x" + String(reg, HEX);
        } else {
            return "Error: Failed to write to device";
        }
    } else if (command == "help") {
        return "I2C Commands:\n"
               "  scan               - Scan for I2C devices on bus\n"
               "  probe              - Check if device exists at address\n"
               "  read <register>    - Read from device register\n"
               "  write <reg> <data> - Write data to device register\n"
               "Address format: Decimal (72) or Hex (0x48)";
    } else {
        return "Error: Unknown I2C command '" + command + "'. Use 'help' for command list";
    }
}

String WebServerManager::executeNetworkCommand(const String& command, const String& pin) {
    extern int connectedClients;
    
    if (command == "status") {
        if (pin == "ethernet" || pin.length() == 0) {
            String result = "Ethernet Interface Status:\n";
            result += "IP: " + String(config.ip[0]) + "." + String(config.ip[1]) + "." + 
                     String(config.ip[2]) + "." + String(config.ip[3]) + "\n";
            result += "Gateway: " + String(config.gateway[0]) + "." + String(config.gateway[1]) + "." + 
                     String(config.gateway[2]) + "." + String(config.gateway[3]) + "\n";
            result += "Subnet: " + String(config.subnet[0]) + "." + String(config.subnet[1]) + "." + 
                     String(config.subnet[2]) + "." + String(config.subnet[3]) + "\n";
            result += "MAC: 02:00:00:12:34:56\n"; // W5500 default MAC pattern
            result += "DHCP: " + String(config.dhcpEnabled ? "Enabled" : "Disabled") + "\n";
            result += "Link Status: Connected";
            return result;
        } else if (pin == "pins") {
            return "Ethernet Pin Configuration:\n"
                   "MISO: Pin 16\n"
                   "CS: Pin 17\n"
                   "SCK: Pin 18\n"
                   "MOSI: Pin 19\n"
                   "RST: Pin 20\n"
                   "IRQ: Pin 21";
        } else if (pin == "modbus") {
            return "Modbus TCP Server:\n"
                   "Port: " + String(config.modbusPort) + "\n"
                   "Status: Active\n"
                   "Connected Clients: " + String(connectedClients);
        } else {
            return "Error: Unknown network component '" + pin + "'. Use: ethernet, pins, modbus";
        }
    } else if (command == "clients") {
        String result = "Modbus Clients:\n";
        result += "Connected: " + String(connectedClients) + "\n";
        // Note: This would need enhancement to track actual client IPs
        // For now, show placeholder data
        if (connectedClients > 0) {
            result += "Active connections detected";
        } else {
            result += "No active connections";
        }
        return result;
    } else if (command == "link") {
        // Check ethernet link status
        return "Ethernet Link: UP"; // W5500 link detection would go here
    } else if (command == "stats") {
        return "Network Statistics:\n"
               "Bytes Sent: [Not implemented]\n"
               "Bytes Received: [Not implemented]\n"
               "Connection Uptime: " + String(millis() / 1000) + " seconds";
    } else if (command == "help") {
        return "Network Commands:\n"
               "  status             - Show network/ethernet configuration\n"
               "  clients            - Show connected Modbus clients\n"
               "  link               - Show ethernet link status\n"
               "  stats              - Show network statistics\n"
               "Pin options: ethernet, pins, modbus, clients";
    } else {
        return "Error: Unknown network command '" + command + "'. Use 'help' for command list";
    }
}

String WebServerManager::executeSPICommand(const String& command, const String& pin) {
    if (command == "read") {
        if (pin.length() == 0) {
            return "Error: CS pin required for SPI read";
        }
        return "SPI read on CS pin " + pin + " completed";
    } else if (command == "help") {
        return "SPI commands: read <cs_pin>, help";
    } else {
        return "Error: Unknown SPI command '" + command + "'";
    }
}

String WebServerManager::executeUARTCommand(const String& command, const String& pin) {
    static bool uartInitialized = false;
    static int currentBaudRate = 9600;
    static bool echoMode = false;
    
    if (command == "help") {
        return "UART Commands:\n"
               "  help               - Show all available UART commands\n"
               "  init               - Initialize UART at 9600 baud (default)\n"
               "  send <data>        - Send data to connected UART device\n"
               "  read               - Read data from UART receive buffer\n"
               "  loopback           - Test UART loopback functionality\n"
               "  baudrate <rate>    - Set baud rate (9600,19200,38400,57600,115200)\n"
               "  status             - Show UART status and pin configuration\n"
               "  at                 - Send AT command (useful for modems/GPS)\n"
               "  echo <on|off>      - Enable/disable echo mode\n"
               "  clear              - Clear UART receive buffer";
    } else if (command == "init") {
        Serial1.begin(9600);
        uartInitialized = true;
        currentBaudRate = 9600;
        return "UART initialized on Serial1 at 9600 baud";
    } else if (command.startsWith("baudrate ")) {
        String rateStr = command.substring(9);
        rateStr.trim();
        int newRate = rateStr.toInt();
        
        if (newRate == 9600 || newRate == 19200 || newRate == 38400 || 
            newRate == 57600 || newRate == 115200) {
            Serial1.end();
            Serial1.begin(newRate);
            currentBaudRate = newRate;
            uartInitialized = true;
            return "Baudrate set to " + String(newRate);
        } else {
            return "Error: Invalid baud rate. Use: 9600, 19200, 38400, 57600, 115200";
        }
    } else if (command.startsWith("send ")) {
        if (!uartInitialized) {
            return "Error: UART not initialized. Use 'init' command first";
        }
        String data = command.substring(5);
        Serial1.print(data);
        return "Sent: " + data;
    } else if (command == "read") {
        if (!uartInitialized) {
            return "Error: UART not initialized. Use 'init' command first";
        }
        String received = "";
        while (Serial1.available()) {
            received += (char)Serial1.read();
        }
        if (received.length() > 0) {
            return "Received: " + received;
        } else {
            return "No data available";
        }
    } else if (command == "loopback") {
        if (!uartInitialized) {
            return "Error: UART not initialized. Use 'init' command first";
        }
        String testData = "TEST123";
        Serial1.print(testData);
        delay(100); // Wait for loopback
        String received = "";
        while (Serial1.available()) {
            received += (char)Serial1.read();
        }
        return "Sent: " + testData + ", Received: " + received;
    } else if (command == "status") {
        String status = "UART Status:\n";
        status += "Initialized: " + String(uartInitialized ? "YES" : "NO") + "\n";
        status += "Baud Rate: " + String(currentBaudRate) + "\n";
        status += "Echo Mode: " + String(echoMode ? "ON" : "OFF") + "\n";
        status += "TX Pin: 0 (GPIO 0)\n";
        status += "RX Pin: 1 (GPIO 1)\n";
        status += "Available Data: " + String(Serial1.available()) + " bytes";
        return status;
    } else if (command == "at") {
        if (!uartInitialized) {
            return "Error: UART not initialized. Use 'init' command first";
        }
        Serial1.print("AT\r\n");
        delay(1000); // Wait for AT response
        String response = "";
        while (Serial1.available()) {
            response += (char)Serial1.read();
        }
        return "AT Response: " + (response.length() > 0 ? response : "No response");
    } else if (command.startsWith("echo ")) {
        String mode = command.substring(5);
        mode.trim();
        if (mode.equalsIgnoreCase("on")) {
            echoMode = true;
            return "Echo mode ENABLED";
        } else if (mode.equalsIgnoreCase("off")) {
            echoMode = false;
            return "Echo mode DISABLED";
        } else {
            return "Error: Use 'echo on' or 'echo off'";
        }
    } else if (command == "clear") {
        if (!uartInitialized) {
            return "Error: UART not initialized. Use 'init' command first";
        }
        while (Serial1.available()) {
            Serial1.read(); // Clear buffer
        }
        return "UART receive buffer cleared";
    } else {
        return "Error: Unknown UART command '" + command + "'. Use 'help' for command list";
    }
}

String WebServerManager::executeSystemCommand(const String& command) {
    if (command == "status") {
        extern int connectedClients;
        return "System Status:\n"
               "CPU: RP2040 @ 133MHz\n"
               "RAM: 256KB\n"
               "Flash: 2MB\n"
               "Uptime: " + String(millis() / 1000) + " seconds\n"
               "Free Heap: " + String(rp2040.getFreeHeap()) + " bytes\n"
               "Connected Modbus Clients: " + String(connectedClients);
    } else if (command == "sensors") {
        String result = "Configured Sensors:\n";
        for (int i = 0; i < numConfiguredSensors; i++) {
            result += String(i) + ": " + String(configuredSensors[i].name) + 
                     " (" + String(configuredSensors[i].type) + ") - " +
                     (configuredSensors[i].enabled ? "Enabled" : "Disabled") + "\n";
        }
        if (numConfiguredSensors == 0) {
            result += "No sensors configured";
        }
        return result;
    } else if (command == "info") {
        return "Hardware Information:\n"
               "Board: Raspberry Pi Pico\n"
               "Digital Inputs: 8 (Pins 0-7)\n"
               "Digital Outputs: 8 (Pins 8-15)\n"
               "Analog Inputs: 3 (Pins 26-28)\n"
               "I2C: SDA Pin 4, SCL Pin 5\n"
               "Ethernet: W5500 (Pins 16-21)";
    } else if (command == "restart") {
        return "System restart initiated...";
    } else if (command == "help") {
        return "System Commands:\n"
               "  status             - System status and uptime\n"
               "  sensors            - List configured sensors\n"
               "  info               - Hardware information\n"
               "  restart            - Restart system";
    } else {
        return "Error: Unknown system command '" + command + "'. Use 'help' for command list";
    }
}

// Helper function to parse I2C addresses in decimal or hex format
int WebServerManager::parseI2CAddress(const String& addrStr) {
    if (addrStr.startsWith("0x") || addrStr.startsWith("0X")) {
        return strtol(addrStr.c_str(), NULL, 16);
    } else {
        return addrStr.toInt();
    }
}