// Core Arduino and C++ includes
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ArduinoModbus.h>
#include "sys_init.h"
// Use ANALOG_INPUTS from sys_init.h instead of ADC_PINS
#include "Ezo_i2c.h"
#include <math.h>
#include <ctype.h>
#include <cstring>

// Forward declaration for sendJSON used in REST handlers
void sendJSON(WiFiClient& client, String json);

// Core Arduino and C++ includes
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ArduinoModbus.h>
#include "sys_init.h"
// Use ANALOG_INPUTS from sys_init.h instead of ADC_PINS
#include "Ezo_i2c.h"
#include <math.h>
#include <ctype.h>
#include <cstring>

// Serve available pins for a given protocol (e.g. /available-pins?protocol=I2C)
void sendAvailablePins(WiFiClient& client, String path) {
    Serial.print("[DEBUG] /available-pins called with path: ");
    Serial.println(path);
    String protocol = "";
    int idx = path.indexOf("protocol=");
    if (idx >= 0) {
        protocol = path.substring(idx + 9);
        int amp = protocol.indexOf('&');
        if (amp >= 0) protocol = protocol.substring(0, amp);
        protocol.trim();
    }
    StaticJsonDocument<512> doc;
    JsonArray pins = doc.createNestedArray("pins");
    if (protocol == "I2C") {
        JsonObject pair = pins.createNestedObject();
        pair["label"] = "SDA: GP4, SCL: GP5";
        pair["pin"] = "4,5";
    } else if (protocol == "UART") {
        int uartPins[][2] = { {0,1}, {4,5}, {8,9}, {16,17} };
        for (int i = 0; i < 4; i++) {
            JsonObject pair = pins.createNestedObject();
            pair["label"] = String("TX: GP") + uartPins[i][0] + ", RX: GP" + uartPins[i][1];
            pair["pin"] = String(uartPins[i][0]) + "," + String(uartPins[i][1]);
        }
    } else if (protocol == "Analog Voltage") {
        int analogPins[] = {26,27,28};
        for (int i = 0; i < 3; i++) {
            JsonObject pin = pins.createNestedObject();
            pin["label"] = String("GP") + analogPins[i];
            pin["pin"] = String(analogPins[i]);
        }
    } else if (protocol == "One-Wire" || protocol == "Digital Counter") {
        int digitalPins[] = {2,3,6,7,10,11,12,13,14,15,18,19,20,21,22,23,24,25};
        for (int i = 0; i < 18; i++) {
            JsonObject pin = pins.createNestedObject();
            pin["label"] = String("GP") + digitalPins[i];
            pin["pin"] = String(digitalPins[i]);
        }
    }
    String response;
    serializeJson(doc, response);
    Serial.print("[DEBUG] /available-pins response: ");
    Serial.println(response);
    sendJSON(client, response);
}

// Serve supported sensor types for each protocol
void sendSensorTypes(WiFiClient& client) {
    Serial.println("[DEBUG] /sensors/types called");
    StaticJsonDocument<1024> doc;
    JsonObject i2c = doc.createNestedObject("I2C");
    JsonArray i2cTypes = i2c.createNestedArray("types");
    i2cTypes.add("BME280");
    i2cTypes.add("SHT30");
    i2cTypes.add("SIM_I2C_TEMPERATURE");
    i2cTypes.add("SIM_I2C_HUMIDITY");
    i2cTypes.add("SIM_I2C_PRESSURE");
    i2cTypes.add("EZO_PH");
    i2cTypes.add("EZO_EC");
    i2cTypes.add("EZO_DO");
    i2cTypes.add("EZO_RTD");
    i2cTypes.add("GENERIC_I2C");
    JsonObject uart = doc.createNestedObject("UART");
    JsonArray uartTypes = uart.createNestedArray("types");
    uartTypes.add("SIM_UART_SENSOR");
    uartTypes.add("GENERIC_UART");
    uartTypes.add("RS485");
    JsonObject analog = doc.createNestedObject("Analog Voltage");
    JsonArray analogTypes = analog.createNestedArray("types");
    analogTypes.add("SIM_ANALOG_VOLTAGE");
    analogTypes.add("SIM_ANALOG_CURRENT");
    analogTypes.add("GENERIC_ANALOG");
    JsonObject onewire = doc.createNestedObject("One-Wire");
    JsonArray onewireTypes = onewire.createNestedArray("types");
    onewireTypes.add("DS18B20");
    onewireTypes.add("GENERIC_ONEWIRE");
    JsonObject digital = doc.createNestedObject("Digital Counter");
    JsonArray digitalTypes = digital.createNestedArray("types");
    digitalTypes.add("SIM_DIGITAL_SWITCH");
    digitalTypes.add("SIM_DIGITAL_COUNTER");
    digitalTypes.add("GENERIC_DIGITAL");
    String response;
    serializeJson(doc, response);
    Serial.print("[DEBUG] /sensors/types response: ");
    Serial.println(response);
    sendJSON(client, response);
}

// Formula parser functions (moved after Arduino.h to access Serial)
static double parseNumber(const char*& s) {
    double val = 0.0;
    bool neg = false;
    while (isspace(*s)) ++s;
    if (*s == '-') { neg = true; ++s; }
    while (isdigit(*s) || *s == '.') {
        char buf[16] = {0}; int i = 0;
        while ((isdigit(*s) || *s == '.') && i < 15) buf[i++] = *s++;
        val = atof(buf);
    }
    return neg ? -val : val;
}

static double parseExpression(const char*& s, double x);

static double parseFactor(const char*& s, double x) {
    while (isspace(*s)) ++s;
    
    if (*s == '(') {
        ++s;
        double result = parseExpression(s, x);
        while (isspace(*s)) ++s;
        if (*s == ')') ++s;
        return result;
    }
    
    if (*s == 'x') {
        ++s;
        return x;
    }
    
    if (strncmp(s, "sqrt", 4) == 0) {
        s += 4;
        while (isspace(*s)) ++s;
        if (*s == '(') {
            ++s;
            double arg = parseExpression(s, x);
            while (isspace(*s)) ++s;
            if (*s == ')') ++s;
            return sqrt(arg);
        }
    }
    
    if (strncmp(s, "log", 3) == 0) {
        s += 3;
        while (isspace(*s)) ++s;
        if (*s == '(') {
            ++s;
            double arg = parseExpression(s, x);
            while (isspace(*s)) ++s;
            if (*s == ')') ++s;
            return log(arg);
        }
    }
    
    if (strncmp(s, "pow", 3) == 0) {
        s += 3;
        while (isspace(*s)) ++s;
        if (*s == '(') {
            ++s;
            double base = parseExpression(s, x);
            while (isspace(*s)) ++s;
            if (*s == ',') {
                ++s;
                double exponent = parseExpression(s, x);
                while (isspace(*s)) ++s;
                if (*s == ')') ++s;
                return pow(base, exponent);
            }
        }
    }
    
    return parseNumber(s);
}

static double parseTerm(const char*& s, double x) {
    double result = parseFactor(s, x);
    
    while (true) {
        while (isspace(*s)) ++s;
        if (*s == '*') {
            ++s;
            result *= parseFactor(s, x);
        } else if (*s == '/') {
            ++s;
            double divisor = parseFactor(s, x);
            if (divisor != 0.0) {
                result /= divisor;
            } else {
                Serial.println("Warning: Division by zero in formula");
                return result;
            }
        } else {
            break;
        }
    }
    
    return result;
}

static double parseExpression(const char*& s, double x) {
    double result = parseTerm(s, x);
    
    while (true) {
        while (isspace(*s)) ++s;
        if (*s == '+') {
            ++s;
            result += parseTerm(s, x);
        } else if (*s == '-') {
            ++s;
            result -= parseTerm(s, x);
        } else {
            break;
        }
    }
    
    return result;
}

// Full formula parser supporting: +, -, *, /, sqrt(x), log(x), pow(x,y), parentheses
double applyFormula(const char* formula, double x) {
    if (strlen(formula) == 0 || strcmp(formula, "x") == 0) {
        return x; // No formula or just "x", return raw value
    }
    
    const char* s = formula;
    double result = parseExpression(s, x);
    
    // Check if we parsed the entire formula
    while (isspace(*s)) ++s;
    if (*s != '\0') {
        Serial.printf("Warning: Formula '%s' has unparsed characters, result may be incorrect\n", formula);
    }
    
    return result;
}

double applyFormulaConversion(double raw_value, const char* formula) {
    if (strlen(formula) == 0) {
        return raw_value; // No formula, return raw value
    }
    return applyFormula(formula, raw_value);
}

// SensorConfig array definition (from sys_init.h extern)
SensorConfig configuredSensors[MAX_SENSORS] = {};
int numConfiguredSensors = 0;

// Global object definitions
Config config; // Define the actual config object
IOStatus ioStatus = {};
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WiFiServer modbusServer(502); 
WiFiServer httpServer(80);    // HTTP server on port 80
WiFiClient client;
ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
int connectedClients = 0;

// Forward declarations for functions used before definition
void updateIOForClient(int clientIndex);
void handleDualHTTP();
void routeRequest(WiFiClient& client, String method, String path, String body);
void sendFile(WiFiClient& client, String filename, String contentType);
void send404(WiFiClient& client);
void sendJSONConfig(WiFiClient& client);
void sendJSONIOStatus(WiFiClient& client);
void sendJSONIOConfig(WiFiClient& client);
void sendJSONSensorConfig(WiFiClient& client);
void handlePOSTConfig(WiFiClient& client, String body);
void handlePOSTSetOutput(WiFiClient& client, String body);
void handlePOSTIOConfig(WiFiClient& client, String body);
void handlePOSTResetLatches(WiFiClient& client);
void handlePOSTResetSingleLatch(WiFiClient& client, String body);
// Sensor functions temporarily commented out
void handlePOSTSensorConfig(WiFiClient& client, String body);
void handlePOSTSensorCommand(WiFiClient& client, String body);
void setPinModes();
void setupEthernet();
void setupModbus();
void setupWebServer();
void handleAllSensors(); // Multi-protocol sensor handler
void initializeAllSensors(); // Sensor initialization

// EZO sensor functionality 
Ezo_board* ezoSensors[MAX_SENSORS] = {nullptr};
bool ezoSensorsInitialized = false;



// Implementation of POST /api/sensor/calibration
void setup() {
    Serial.begin(115200);
    uint32_t timeStamp = millis();
    while (!Serial) {
        if (millis() - timeStamp > 5000) break;
    }
    Serial.println("Booting... (Firmware start)");

    pinMode(LED_BUILTIN, OUTPUT);
    analogReadResolution(12);

    // Blink status LED 3 times to confirm firmware is running
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
    Serial.println("Status LED blink complete. Firmware running.");

    // Initialize pin allocation tracking if needed
    // initializePinAllocations(); // Uncomment if using pin allocation logic

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS filesystem");
        if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully");
            if (!LittleFS.begin()) {
                Serial.println("Failed to mount LittleFS after formatting");
            }
        } else {
            Serial.println("Failed to format LittleFS");
        }
    }

    Serial.println("Loading config...");
    delay(500);
    loadConfig();

    Serial.println("Loading sensor configuration...");
    delay(500);
    loadSensorConfig();

    Serial.println("Setting pin modes...");
    setPinModes();

    Serial.println("Setup network and services...");
    setupEthernet();

    Serial.println("========================================");
    Serial.print("IP Address: ");
    Serial.println(eth.localIP());
    Serial.println("========================================");

    setupModbus();
    setupWebServer();

    // Initialize I2C bus if needed
    // Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Uncomment if using custom I2C pins

    // Start watchdog
    rp2040.wdt_begin(WDT_TIMEOUT);
    Serial.println("Setup complete.");
}

void handlePOSTSensorCalibration(WiFiClient& client, String body) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    String name = doc["name"] | "";
    String method = doc["method"] | "linear";
    int found = -1;
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (strcmp(configuredSensors[i].name, name.c_str()) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":false,\"message\":\"Sensor not found\"}");
        return;
    }
    // Store all calibration info as a JSON string in calibrationData
    String calibJson;
    serializeJson(doc, calibJson);
    strncpy(configuredSensors[found].calibrationData, calibJson.c_str(), sizeof(configuredSensors[found].calibrationData)-1);
    configuredSensors[found].calibrationData[sizeof(configuredSensors[found].calibrationData)-1] = '\0';
    saveSensorConfig();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

// Implementation of POST /terminal/command
void handlePOSTTerminalCommand(WiFiClient& client, String body) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    String protocol = doc["protocol"] | "";
    String pin = doc["pin"] | "";
    String command = doc["command"] | "";
    String i2cAddress = doc["i2cAddress"] | "";
    String response = "";
    bool success = true;
    // Basic command routing (expand as needed)
    if (protocol == "digital") {
        if (command == "read") {
            if (pin.startsWith("DI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = digitalRead(DIGITAL_INPUTS[pinNum]);
                    response = pin + " = " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else if (pin.startsWith("DO")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = ioStatus.dOut[pinNum];
                    response = pin + " = " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            }
        } else if (command.startsWith("write ")) {
            String value = command.substring(6);
            if (pin.startsWith("DO")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = (value == "1" || value.equalsIgnoreCase("HIGH"));
                    ioStatus.dOut[pinNum] = state;
                    digitalWrite(DIGITAL_OUTPUTS[pinNum], config.doInvert[pinNum] ? !state : state);
                    response = pin + " set to " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else {
                success = false;
                response = "Error: Cannot write to input pin";
            }
        } else {
            success = false;
            response = "Error: Unknown digital command";
        }
    } else if (protocol == "analog") {
        if (command == "read") {
            if (pin.startsWith("AI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 3) {
                    int value = analogRead(ANALOG_INPUTS[pinNum]);
                    response = pin + " = " + String(value) + " mV";
                } else {
                    success = false;
                    response = "Error: Invalid analog pin";
                }
            }
        } else {
            success = false;
            response = "Error: Unknown analog command";
        }
    } else if (protocol == "i2c") {
        if (command == "scan") {
            response = "I2C Device Scan:\n";
            bool foundDevices = false;
            for (int addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    response += "Found device at 0x" + String(addr, HEX) + "\n";
                    foundDevices = true;
                }
            }
            if (!foundDevices) {
                response += "No I2C devices found";
            }
        } else if (command == "probe") {
            int addr = i2cAddress.length() > 0 ? strtol(i2cAddress.c_str(), nullptr, 16) : 0;
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                response = "Device present at 0x" + String(addr, HEX);
            } else {
                response = "No device at 0x" + String(addr, HEX);
            }
        } else {
            success = false;
            response = "Error: Unknown I2C command";
        }
    } else if (protocol == "system") {
        if (command == "status") {
            response = "System OK."; // RAM info not available on RP2040
        } else if (command == "reset") {
            response = "System will reset.";
            // NVIC_SystemReset(); // Not available on RP2040
        } else {
            success = false;
            response = "Error: Unknown system command";
        }
    } else {
        success = false;
        response = "Error: Unknown protocol";
    }
    StaticJsonDocument<256> respDoc;
    respDoc["success"] = success;
    if (success) {
        respDoc["response"] = response;
    } else {
        respDoc["error"] = response;
    }
    String jsonResp;
    serializeJson(respDoc, jsonResp);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.print(jsonResp);
}

void loop() {
    // Check for new client connections on the WiFi server (actually Ethernet via W5500lwIP)
    WiFiClient newClient = modbusServer.accept();
    
    if (newClient) {
        // Find an available slot for the new client
        bool clientAdded = false;
        for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
            if (!modbusClients[i].connected) {
                Serial.print("New client connected to slot ");
                Serial.println(i);
                
                // Store the client and mark as connected
                modbusClients[i].client = newClient;
                modbusClients[i].connected = true;
                modbusClients[i].clientIP = newClient.remoteIP();
                modbusClients[i].connectionTime = millis();
                
                // Accept the connection on this server instance
                modbusClients[i].server.accept(modbusClients[i].client);
                Serial.println("Modbus server accepted client connection");
                
                // Initialize coil states for this client to match current output states
                for (int j = 0; j < 8; j++) {
                    modbusClients[i].server.coilWrite(j, ioStatus.dOut[j]);
                }
                
                connectedClients++;
                clientAdded = true;
                digitalWrite(LED_BUILTIN, HIGH);  // Turn on LED when at least one client is connected
                break;
            }
        }
        
        if (!clientAdded) {
            Serial.println("No available slots for new client");
            newClient.stop();
        }
    }
    
    // Poll all connected clients
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (modbusClients[i].connected) {
            if (modbusClients[i].client.connected()) {
                // Poll this client's Modbus server
                if (modbusClients[i].server.poll()) {
                    Serial.println("Modbus server recieved new request");
                }
                // Update IO for this specific client
                updateIOForClient(i);
            } else {
                // Client disconnected
                Serial.print("Client disconnected from slot ");
                Serial.println(i);
                modbusClients[i].connected = false;
                modbusClients[i].client.stop();
                connectedClients--;
                
                if (connectedClients == 0) {
                    digitalWrite(LED_BUILTIN, LOW);  // Turn off LED when no clients are connected
                }
            }
        }
    }
    
    updateIOpins();
    handleAllSensors(); // Generic multi-protocol sensor handling
    handleDualHTTP();  // Handle both Ethernet and USB web interfaces
    
    // Debug: Web server check (every 30 seconds)
    static unsigned long lastWebDebug = 0;
    if (millis() - lastWebDebug > 30000) {
        Serial.println("Web server status: Listening on " + eth.localIP().toString() + ":80");
        lastWebDebug = millis();
    }

    // Serial commands for debugging
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("netinfo")) {
            IPAddress ip = eth.localIP();
            Serial.println("=== NETWORK INFO ===");
            Serial.print("IP Address: ");
            Serial.println(ip);
            Serial.print("Gateway: ");
            Serial.println(eth.gatewayIP());
            Serial.print("Subnet: ");
            Serial.println(eth.subnetMask());
            uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
            Serial.print("MAC Address: ");
            for (int i = 0; i < 6; i++) {
                if (i > 0) Serial.print(":");
                if (mac[i] < 16) Serial.print("0");
                Serial.print(mac[i], HEX);
            }
            Serial.println();
            Serial.println("HTTP Server: Port 80");
            Serial.println("Modbus Server: Port 502");
            Serial.println("==================");
        } else if (cmd.equalsIgnoreCase("webtest")) {
            Serial.println("=== WEB SERVER TEST ===");
            Serial.println("Try accessing these URLs:");
            Serial.println("http://" + eth.localIP().toString() + "/test");
            Serial.println("http://" + eth.localIP().toString() + "/config");
            Serial.println("http://" + eth.localIP().toString() + "/iostatus");
            Serial.println("=====================");
        }
    }

    // Watchdog timer reset
    rp2040.wdt_reset();
}

void initializeEzoSensors() {
    if (ezoSensorsInitialized) return;
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && strncmp(configuredSensors[i].protocol, "EZO_", 4) == 0) {
            ezoSensors[i] = new Ezo_board(configuredSensors[i].i2cAddress, configuredSensors[i].name);
            configuredSensors[i].cmdPending = false;
            configuredSensors[i].lastCmdSent = 0;
            memset(configuredSensors[i].response, 0, sizeof(configuredSensors[i].response));
            Serial.printf("Initialized EZO sensor %s at I2C address 0x%02X\n", 
                configuredSensors[i].name, configuredSensors[i].i2cAddress);
        }
    }
    ezoSensorsInitialized = true;
}

// EZO sensor-specific handler (preserves existing async protocol)
void handleEzoSensor(SensorConfig* sensor, int index) {
    if (ezoSensors[index] == nullptr) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    if (sensor->cmdPending && (currentTime - sensor->lastCmdSent > 1000)) {
        ezoSensors[index]->receive_read_cmd();
        
        if (ezoSensors[index]->get_error() == Ezo_board::SUCCESS) {
            float reading = ezoSensors[index]->get_last_received_reading();
            
            // Apply formula conversion to EZO readings too
            float convertedReading = applyFormulaConversion(reading, sensor->formula);
            sensor->lastReading = convertedReading;
            
            snprintf(sensor->response, sizeof(sensor->response), "%.2f %s", convertedReading, sensor->units);
            Serial.printf("EZO sensor %s reading: %.2f -> %.2f %s\n", 
                sensor->name, reading, convertedReading, sensor->units);
        } else {
            Serial.printf("EZO sensor %s error: %d\n", sensor->name, ezoSensors[index]->get_error());
        }
        
        sensor->cmdPending = false;
    } 
    else if (!sensor->cmdPending && (currentTime - sensor->lastCmdSent > 5000)) {
        ezoSensors[index]->send_read_cmd();
        sensor->lastCmdSent = currentTime;
        sensor->cmdPending = true;
        Serial.printf("Sent read command to EZO sensor %s\n", sensor->name);
    }
}

// Protocol-specific sensor reading functions
float readAnalogSensor(SensorConfig* sensor) {
    // Read ADC value and convert based on protocol type
    int adcValue = analogRead(sensor->pin_assignment);
    float voltage = (adcValue * 3.3) / 4096.0; // 12-bit ADC, 3.3V reference
    
    if (strcmp(sensor->protocol, "4-20mA") == 0) {
        // 4-20mA current loop sensor (using 250 ohm shunt resistor)
        float current = voltage / 0.25; // V = I * R, so I = V / R
        return current;
    } else if (strcmp(sensor->protocol, "0-10V") == 0) {
        // 0-10V sensor with voltage divider (assuming 3.3V max from divider)
        float scaledVoltage = voltage * (10.0 / 3.3);
        return scaledVoltage;
    } else if (strcmp(sensor->protocol, "Thermistor") == 0) {
        // Thermistor temperature sensor (using Steinhart-Hart approximation)
        float resistance = (voltage * 10000.0) / (3.3 - voltage); // 10k pullup
        float tempK = 1.0 / (0.001129148 + (0.000234125 * log(resistance)) + (0.0000000876741 * pow(log(resistance), 3)));
        float tempC = tempK - 273.15;
        return tempC;
    } else {
        // Default ADC - just return voltage
        return voltage;
    }
}

float readI2CSensor(SensorConfig* sensor) {
    // Generic I2C sensor reading with specific protocol support
    if (strcmp(sensor->protocol, "BME280") == 0) {
        // BME280 temperature/humidity/pressure sensor
        // Simple I2C read implementation for BME280
        Wire.beginTransmission(sensor->i2cAddress);
        Wire.write(0xFA); // Temperature register
        Wire.endTransmission();
        Wire.requestFrom(sensor->i2cAddress, 3);
        
        if (Wire.available() >= 3) {
            uint32_t rawTemp = (Wire.read() << 12) | (Wire.read() << 4) | (Wire.read() >> 4);
            // Simplified temperature calculation (would need proper calibration in real implementation)
            float temperature = (rawTemp / 16384.0) - 25.0; // Rough approximation
            return temperature;
        }
        return 0.0;
        
    } else if (strcmp(sensor->protocol, "SHT30") == 0) {
        // SHT30 temperature/humidity sensor
        Wire.beginTransmission(sensor->i2cAddress);
        Wire.write(0x2C); // High repeatability measurement
        Wire.write(0x06);
        Wire.endTransmission();
        
        delay(15); // Wait for measurement
        
        Wire.requestFrom(sensor->i2cAddress, 6);
        if (Wire.available() >= 6) {
            uint16_t rawTemp = (Wire.read() << 8) | Wire.read();
            Wire.read(); // Skip CRC
            uint16_t rawHum = (Wire.read() << 8) | Wire.read();
            Wire.read(); // Skip CRC
            
            // Convert based on what measurement type is requested
            if (strstr(sensor->name, "Temp") || strstr(sensor->name, "temp")) {
                float temperature = -45.0 + 175.0 * (rawTemp / 65535.0);
                return temperature;
            } else {
                float humidity = 100.0 * (rawHum / 65535.0);
                return humidity;
            }
        }
        return 0.0;
        
    } else if (strcmp(sensor->protocol, "SHT31") == 0) {
        // SHT31 (similar to SHT30 but might have different address/commands)
        // Use same implementation as SHT30 for now
        return readI2CSensor(sensor); // Reuse SHT30 logic
        
    } else if (strcmp(sensor->protocol, "VL53L1X") == 0) {
        // VL53L1X Time-of-Flight distance sensor
        // TODO: Implement VL53L1X reading protocol
        return 100.0 + random(-10, 10); // Placeholder distance in mm
        
    } else {
        // Generic I2C register read for unknown sensors
        Wire.beginTransmission(sensor->i2cAddress);
        Wire.write(0x00); // Read from register 0
        Wire.endTransmission();
        Wire.requestFrom(sensor->i2cAddress, 2);
        
        if (Wire.available() >= 2) {
            uint16_t value = (Wire.read() << 8) | Wire.read();
            return (float)value;
        }
    }
    return 0.0;
}

float readUARTSensor(SensorConfig* sensor) {
    // UART/RS485 sensor reading (Modbus RTU, custom protocols)
    if (strcmp(sensor->protocol, "ModbusRTU") == 0) {
        // TODO: Implement Modbus RTU client reading
        return 50.0; // Placeholder
    }
    return 0.0;
}

float readOneWireSensor(SensorConfig* sensor) {
    // OneWire sensor reading (DS18B20, etc.)
    if (strcmp(sensor->protocol, "DS18B20") == 0) {
        // TODO: Implement DS18B20 temperature reading
        return 23.7; // Placeholder
    }
    return 0.0;
}

uint32_t readDigitalCounter(SensorConfig* sensor) {
    // Read pulse counter value
    // TODO: Implement hardware pulse counting or interrupt-based counting
    static uint32_t counter = 0;
    counter += random(0, 3); // Placeholder increment
    sensor->pulse_count = counter;
    return counter;
}

bool readGPIOSensor(SensorConfig* sensor) {
    // Simple digital GPIO reading
    return digitalRead(sensor->pin_assignment);
}

// Initialize all sensors based on their types (EZO + generic)
void initializeAllSensors() {
    Serial.println("Initializing multi-protocol sensors (EZO + generic)...");
    
    // First initialize EZO sensors (if any)
    initializeEzoSensors();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) {
            continue;
        }
        
        // Initialize based on sensor type
        if (strcmp(configuredSensors[i].sensor_type, "EZO") == 0) {
            // EZO sensors already initialized above
            Serial.printf("EZO sensor %s (%s) at I2C 0x%02X\n", 
                configuredSensors[i].name, configuredSensors[i].protocol, configuredSensors[i].i2cAddress);
                
        } else if (strcmp(configuredSensors[i].sensor_type, "Analog") == 0) {
            // Set pin mode for analog reading
            pinMode(configuredSensors[i].pin_assignment, INPUT);
            Serial.printf("Initialized analog sensor %s on pin %d\n", 
                configuredSensors[i].name, configuredSensors[i].pin_assignment);
                
        } else if (strcmp(configuredSensors[i].sensor_type, "GPIO") == 0) {
            // Set pin mode for digital reading
            pinMode(configuredSensors[i].pin_assignment, INPUT_PULLUP);
            Serial.printf("Initialized GPIO sensor %s on pin %d\n", 
                configuredSensors[i].name, configuredSensors[i].pin_assignment);
                
        } else if (strcmp(configuredSensors[i].sensor_type, "I2C") == 0) {
            // Generic I2C sensors - probe the device to verify connection
            Wire.beginTransmission(configuredSensors[i].i2cAddress);
            int error = Wire.endTransmission();
            
            if (error == 0) {
                Serial.printf("✓ I2C sensor %s (%s) found at address 0x%02X\n", 
                    configuredSensors[i].name, configuredSensors[i].protocol, configuredSensors[i].i2cAddress);
            } else {
                Serial.printf("✗ I2C sensor %s (%s) NOT FOUND at address 0x%02X (error %d)\n", 
                    configuredSensors[i].name, configuredSensors[i].protocol, configuredSensors[i].i2cAddress, error);
            }
                
        } else if (strcmp(configuredSensors[i].sensor_type, "UART") == 0) {
            // TODO: Initialize UART with specified baud rate
            Serial.printf("Registered UART sensor %s (%s) at %d baud\n", 
                configuredSensors[i].name, configuredSensors[i].protocol, configuredSensors[i].baud_rate);
                
        } else if (strcmp(configuredSensors[i].sensor_type, "OneWire") == 0) {
            // TODO: Initialize OneWire bus on specified pin
            Serial.printf("Registered OneWire sensor %s (%s) on pin %d\n", 
                configuredSensors[i].name, configuredSensors[i].protocol, configuredSensors[i].pin_assignment);
                
        } else if (strcmp(configuredSensors[i].sensor_type, "DigitalCounter") == 0) {
            // TODO: Set up interrupt-based pulse counting
            pinMode(configuredSensors[i].pin_assignment, INPUT_PULLUP);
            Serial.printf("Initialized counter sensor %s on pin %d\n", 
                configuredSensors[i].name, configuredSensors[i].pin_assignment);
        }
        
        // Set default sample interval if not specified
        if (configuredSensors[i].sample_interval == 0) {
            configuredSensors[i].sample_interval = 1000; // Default 1 second
        }
    }
    
    Serial.printf("Initialized %d sensors (EZO + generic)\n", numConfiguredSensors);
}

// Multi-protocol sensor handler supporting EZO + generic sensors
void handleAllSensors() {
    static bool initialized = false;
    
    if (!initialized) {
        initializeAllSensors();
        initialized = true;
    }
    
    unsigned long currentTime = millis();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) {
            continue;
        }
        
        // Handle EZO sensors with their async protocol
        if (strcmp(configuredSensors[i].sensor_type, "EZO") == 0) {
            handleEzoSensor(&configuredSensors[i], i);
            continue;
        }
        
        // Check if it's time to sample generic sensors
        uint32_t interval = configuredSensors[i].sample_interval > 0 ? configuredSensors[i].sample_interval : 1000;
        if (currentTime - configuredSensors[i].lastSample < interval) {
            continue;
        }
        
        float rawReading = 0.0;
        bool readingValid = false;
        
        // Route to appropriate protocol handler
        if (strcmp(configuredSensors[i].sensor_type, "Analog") == 0) {
            rawReading = readAnalogSensor(&configuredSensors[i]);
            readingValid = true;
        } else if (strcmp(configuredSensors[i].sensor_type, "I2C") == 0) {
            rawReading = readI2CSensor(&configuredSensors[i]);
            readingValid = true;
        } else if (strcmp(configuredSensors[i].sensor_type, "UART") == 0) {
            rawReading = readUARTSensor(&configuredSensors[i]);
            readingValid = true;
        } else if (strcmp(configuredSensors[i].sensor_type, "OneWire") == 0) {
            rawReading = readOneWireSensor(&configuredSensors[i]);
            readingValid = true;
        } else if (strcmp(configuredSensors[i].sensor_type, "DigitalCounter") == 0) {
            rawReading = (float)readDigitalCounter(&configuredSensors[i]);
            readingValid = true;
        } else if (strcmp(configuredSensors[i].sensor_type, "GPIO") == 0) {
            rawReading = readGPIOSensor(&configuredSensors[i]) ? 1.0 : 0.0;
            readingValid = true;
        }
        
        if (readingValid) {
            // Apply formula conversion
            float convertedReading = applyFormulaConversion(rawReading, configuredSensors[i].formula);
            configuredSensors[i].lastReading = convertedReading;
            configuredSensors[i].lastSample = currentTime;
            
            // Store formatted response
            snprintf(configuredSensors[i].response, sizeof(configuredSensors[i].response), 
                    "%.2f %s", convertedReading, configuredSensors[i].units);
            
            Serial.printf("Sensor %s (%s): Raw=%.2f, Converted=%.2f %s\n", 
                configuredSensors[i].name, configuredSensors[i].sensor_type, 
                rawReading, convertedReading, configuredSensors[i].units);
        }
    }
}

void loadConfig() {
    // Read config from LittleFS
    if (LittleFS.exists(CONFIG_FILE)) {
        File configFile = LittleFS.open(CONFIG_FILE, "r");
        if (configFile) {
            // Create a JSON document to store the configuration
            StaticJsonDocument<2048> doc;
            
            // Deserialize the JSON document
            DeserializationError error = deserializeJson(doc, configFile);
            configFile.close();
            
            if (!error) {
                // Extract configuration values
                config.version = doc["version"] | CONFIG_VERSION;
                config.dhcpEnabled = doc["dhcpEnabled"] | DEFAULT_CONFIG.dhcpEnabled;
                
                // Check if the modbusPort value exists in the config and extract it
                if (doc.containsKey("modbusPort")) {
                    config.modbusPort = doc["modbusPort"].as<uint16_t>();
                    Serial.print("Found modbusPort in config file: ");
                    Serial.println(config.modbusPort);
                } else {
                    config.modbusPort = DEFAULT_CONFIG.modbusPort;
                    Serial.print("modbusPort not found in config, using default: ");
                    Serial.println(config.modbusPort);
                }
                
                Serial.print("Loaded Modbus port from config: ");
                Serial.println(config.modbusPort);
                
                // Get IP addresses
                JsonArray ipArray = doc["ip"].as<JsonArray>();
                if (ipArray) {

                    for (int i = 0; i < 4; i++) {
                        config.ip[i] = ipArray[i] | DEFAULT_CONFIG.ip[i];
                    }
                } else {
                    memcpy(config.ip, DEFAULT_CONFIG.ip, 4);
                }
                
                JsonArray gatewayArray = doc["gateway"].as<JsonArray>();
                if (gatewayArray) {
                    for (int i = 0; i < 4; i++) {
                        config.gateway[i] = gatewayArray[i] | DEFAULT_CONFIG.gateway[i];
                    }
                } else {
                    memcpy(config.gateway, DEFAULT_CONFIG.gateway, 4);
                }
                
                JsonArray subnetArray = doc["subnet"].as<JsonArray>();
                if (subnetArray) {
                    for (int i = 0; i < 4; i++) {
                        config.subnet[i] = subnetArray[i] | DEFAULT_CONFIG.subnet[i];
                    }
                } else {
                    memcpy(config.subnet, DEFAULT_CONFIG.subnet, 4);
                }
                
                // Get hostname
                const char* hostname = doc["hostname"] | DEFAULT_CONFIG.hostname;
                strncpy(config.hostname, hostname, HOSTNAME_MAX_LENGTH - 1);
                config.hostname[HOSTNAME_MAX_LENGTH - 1] = '\0';  // Ensure null termination
                
                // Get digital input configurations
                JsonArray diPullupArray = doc["diPullup"].as<JsonArray>();
                if (diPullupArray) {
                    for (int i = 0; i < 8; i++) {
                        config.diPullup[i] = diPullupArray[i] | DEFAULT_CONFIG.diPullup[i];
                    }
                } else {
                    memcpy(config.diPullup, DEFAULT_CONFIG.diPullup, 8);
                }
                
                JsonArray diInvertArray = doc["diInvert"].as<JsonArray>();
                if (diInvertArray) {
                    for (int i = 0; i < 8; i++) {
                        config.diInvert[i] = diInvertArray[i] | DEFAULT_CONFIG.diInvert[i];
                    }
                } else {
                    memcpy(config.diInvert, DEFAULT_CONFIG.diInvert, 8);
                }
                
                JsonArray diLatchArray = doc["diLatch"].as<JsonArray>();
                if (diLatchArray) {
                    for (int i = 0; i < 8; i++) {
                        config.diLatch[i] = diLatchArray[i] | DEFAULT_CONFIG.diLatch[i];
                    }
                } else {
                    memcpy(config.diLatch, DEFAULT_CONFIG.diLatch, 8);
                }
                
                // Get digital output configurations
                JsonArray doInvertArray = doc["doInvert"].as<JsonArray>();
                if (doInvertArray) {
                    for (int i = 0; i < 8; i++) {
                        config.doInvert[i] = doInvertArray[i] | DEFAULT_CONFIG.doInvert[i];
                    }
                } else {
                    memcpy(config.doInvert, DEFAULT_CONFIG.doInvert, 8);
                }
                
                JsonArray doInitialStateArray = doc["doInitialState"].as<JsonArray>();
                if (doInitialStateArray) {
                    for (int i = 0; i < 8; i++) {
                        config.doInitialState[i] = doInitialStateArray[i] | DEFAULT_CONFIG.doInitialState[i];
                    }
                } else {
                    memcpy(config.doInitialState, DEFAULT_CONFIG.doInitialState, 8);
                }
                
                Serial.println("Configuration loaded from file");
                return;
            } else {
                Serial.print("Failed to parse config file: ");
                Serial.println(error.c_str());
            }
        } else {
            Serial.println("Failed to open config file for reading");
        }
    } else {
        Serial.println("Config file does not exist, using defaults");
    }
    
    // If we get here, use default config
    config = DEFAULT_CONFIG;
    saveConfig();
}

void saveConfig() {
    Serial.println("Saving configuration to LittleFS...");
    
    // Create a JSON document to store the configuration
    StaticJsonDocument<2048> doc;
    
    // Store configuration values
    doc["version"] = config.version;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    doc["modbusPort"] = config.modbusPort;
    
    // Store IP addresses as arrays
    JsonArray ipArray = doc.createNestedArray("ip");
    JsonArray gatewayArray = doc.createNestedArray("gateway");
    JsonArray subnetArray = doc.createNestedArray("subnet");
    
    for (int i = 0; i < 4; i++) {
        ipArray.add(config.ip[i]);
        gatewayArray.add(config.gateway[i]);
        subnetArray.add(config.subnet[i]);
    }
    
    // Store hostname
    doc["hostname"] = config.hostname;
    
    // Store digital input configurations
    JsonArray diPullupArray = doc.createNestedArray("diPullup");
    JsonArray diInvertArray = doc.createNestedArray("diInvert");
    JsonArray diLatchArray = doc.createNestedArray("diLatch");
    
    for (int i = 0; i < 8; i++) {
        diPullupArray.add(config.diPullup[i]);
        diInvertArray.add(config.diInvert[i]);
        diLatchArray.add(config.diLatch[i]);
    }
    
    // Store digital output configurations
    JsonArray doInvertArray = doc.createNestedArray("doInvert");
    JsonArray doInitialStateArray = doc.createNestedArray("doInitialState");
    
    for (int i = 0; i < 8; i++) {
        doInvertArray.add(config.doInvert[i]);
        doInitialStateArray.add(config.doInitialState[i]);
    }
    
    // Open file for writing
    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    
    // Serialize JSON to file
    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Failed to write config to file");
    } else {
        Serial.println("Configuration saved successfully");
    }
    
    // Close the file
    configFile.close();
}

void loadSensorConfig() {
    Serial.println("Loading sensor configuration...");
    
    // Initialize sensors array
    numConfiguredSensors = 0;
    memset(configuredSensors, 0, sizeof(configuredSensors));
    
    // Check if sensors.json exists
    if (!LittleFS.exists(SENSORS_FILE)) {
        Serial.println("Sensors config file does not exist, using empty configuration");
        return;
    }
    
    // Open sensors.json for reading
    File sensorsFile = LittleFS.open(SENSORS_FILE, "r");
    if (!sensorsFile) {
        Serial.println("Failed to open sensors config file for reading");
        return;
    }
    
    // Create JSON document to parse the file (increased size for new fields)
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, sensorsFile);
    sensorsFile.close();
    
    if (error) {
        Serial.print("Failed to parse sensors config file: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Parse sensors array
    if (doc.containsKey("sensors") && doc["sensors"].is<JsonArray>()) {
        JsonArray sensorsArray = doc["sensors"].as<JsonArray>();
        int index = 0;
        
        for (JsonObject sensor : sensorsArray) {
            if (index >= MAX_SENSORS) {
                Serial.println("Warning: Maximum number of sensors exceeded, ignoring remaining sensors");
                break;
            }
            
            // Parse sensor configuration with enhanced fields
            configuredSensors[index].enabled = sensor["enabled"] | false;
            
            const char* name = sensor["name"] | "";
            strncpy(configuredSensors[index].name, name, sizeof(configuredSensors[index].name) - 1);
            configuredSensors[index].name[sizeof(configuredSensors[index].name) - 1] = '\0';
            
            // New sensor_type field for multi-protocol support
            const char* sensor_type = sensor["sensor_type"] | "Analog";
            strncpy(configuredSensors[index].sensor_type, sensor_type, sizeof(configuredSensors[index].sensor_type) - 1);
            configuredSensors[index].sensor_type[sizeof(configuredSensors[index].sensor_type) - 1] = '\0';
            
            // Protocol specification (BME280, DS18B20, etc.)
            const char* protocol = sensor["protocol"] | "ADC";
            strncpy(configuredSensors[index].protocol, protocol, sizeof(configuredSensors[index].protocol) - 1);
            configuredSensors[index].protocol[sizeof(configuredSensors[index].protocol) - 1] = '\0';
            
            // Legacy type field for backward compatibility
            const char* type = sensor["type"] | "";
            // Map legacy EZO types to new protocol system
            if (strncmp(type, "EZO_", 4) == 0) {
                strcpy(configuredSensors[index].sensor_type, "EZO");
                strcpy(configuredSensors[index].protocol, type);
            }
            
            // Formula for mathematical conversion
            const char* formula = sensor["formula"] | "x";
            strncpy(configuredSensors[index].formula, formula, sizeof(configuredSensors[index].formula) - 1);
            configuredSensors[index].formula[sizeof(configuredSensors[index].formula) - 1] = '\0';
            
            // Engineering units
            const char* units = sensor["units"] | "";
            strncpy(configuredSensors[index].units, units, sizeof(configuredSensors[index].units) - 1);
            configuredSensors[index].units[sizeof(configuredSensors[index].units) - 1] = '\0';
            
            // Pin assignment for GPIO sensors
            configuredSensors[index].pin_assignment = sensor["pin_assignment"] | 0;
            
            // Secondary pin for differential sensors
            configuredSensors[index].pin_secondary = sensor["pin_secondary"] | 0;
            
            // UART baud rate
            configuredSensors[index].baud_rate = sensor["baud_rate"] | 9600;
            
            // Device ID for Modbus or OneWire
            configuredSensors[index].device_id = sensor["device_id"] | 1;
            
            // Sample interval
            configuredSensors[index].sample_interval = sensor["sample_interval"] | 1000;
            
            configuredSensors[index].i2cAddress = sensor["i2cAddress"] | 0;
            configuredSensors[index].modbusRegister = sensor["modbusRegister"] | 0;
            configuredSensors[index].calibrationOffset = sensor["calibrationOffset"] | 0.0;
            configuredSensors[index].calibrationSlope = sensor["calibrationSlope"] | 1.0;
            
            // Initialize runtime state for both EZO and generic sensors
            configuredSensors[index].cmdPending = false;
            configuredSensors[index].lastCmdSent = 0;
            configuredSensors[index].lastSample = 0;
            configuredSensors[index].lastReading = 0.0;
            memset(configuredSensors[index].response, 0, sizeof(configuredSensors[index].response));
            memset(configuredSensors[index].calibrationData, 0, sizeof(configuredSensors[index].calibrationData));
            configuredSensors[index].pulse_count = 0;
            
            // Debug output with enhanced fields
            Serial.printf("Loaded sensor %d: %s (%s/%s) Pin=%d, Formula='%s', Units='%s', Interval=%dms, Modbus=%d, Enabled=%s\n",
                index,
                configuredSensors[index].name,
                configuredSensors[index].sensor_type,
                configuredSensors[index].protocol,
                configuredSensors[index].pin_assignment,
                configuredSensors[index].formula,
                configuredSensors[index].units,
                configuredSensors[index].sample_interval,
                configuredSensors[index].modbusRegister,
                configuredSensors[index].enabled ? "true" : "false"
            );
            
            index++;
        }
        
        numConfiguredSensors = index;
        Serial.printf("Loaded %d sensor configurations\n", numConfiguredSensors);
    } else {
        Serial.println("No sensors array found in configuration file");
    }
}

void saveSensorConfig() {
    Serial.println("Saving sensor configuration...");
    
    // Create JSON document with increased size for new fields
    StaticJsonDocument<2048> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    
    // Add each configured sensor to the array with enhanced fields
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["sensor_type"] = configuredSensors[i].sensor_type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["formula"] = configuredSensors[i].formula;
        sensor["units"] = configuredSensors[i].units;
        sensor["pin_assignment"] = configuredSensors[i].pin_assignment;
        sensor["pin_secondary"] = configuredSensors[i].pin_secondary;
        sensor["baud_rate"] = configuredSensors[i].baud_rate;
        sensor["device_id"] = configuredSensors[i].device_id;
        sensor["sample_interval"] = configuredSensors[i].sample_interval;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        sensor["calibrationOffset"] = configuredSensors[i].calibrationOffset;
        sensor["calibrationSlope"] = configuredSensors[i].calibrationSlope;
    }
    
    // Open file for writing
    File sensorsFile = LittleFS.open(SENSORS_FILE, "w");
    if (!sensorsFile) {
        Serial.println("Failed to open sensors config file for writing");
        return;
    }
    
    // Serialize JSON to file
    if (serializeJson(doc, sensorsFile) == 0) {
        Serial.println("Failed to write sensors config to file");
    } else {
        Serial.printf("Sensor configuration saved successfully (%d sensors)\n", numConfiguredSensors);
    }
    
    // Close the file
    sensorsFile.close();
}

// Reset all latched inputs
void resetLatches() {
    Serial.println("Resetting all latched inputs");
    for (int i = 0; i < 8; i++) {
        ioStatus.dInLatched[i] = false;
    }
}

void setPinModes() {
    for (int i = 0; i < sizeof(DIGITAL_INPUTS)/sizeof(DIGITAL_INPUTS[0]); i++) {
        pinMode(DIGITAL_INPUTS[i], config.diPullup[i] ? INPUT_PULLUP : INPUT);
    }
    for (int i = 0; i < sizeof(DIGITAL_OUTPUTS)/sizeof(DIGITAL_OUTPUTS[0]); i++) {
        pinMode(DIGITAL_OUTPUTS[i], OUTPUT);
        
        // Set the digital output to its initial state from config
        ioStatus.dOut[i] = config.doInitialState[i];
        
        // Apply any inversion logic
        bool physicalState = config.doInvert[i] ? !ioStatus.dOut[i] : ioStatus.dOut[i];
        digitalWrite(DIGITAL_OUTPUTS[i], physicalState);
    }
}

void setupEthernet() {
    Serial.println("Initializing W5500 Ethernet...");
    Serial.print("  Configuring SPI pins - CS:"); Serial.print(PIN_ETH_CS);
    Serial.print(", MISO:"); Serial.print(PIN_ETH_MISO);
    Serial.print(", SCK:"); Serial.print(PIN_ETH_SCK);
    Serial.print(", MOSI:"); Serial.println(PIN_ETH_MOSI);

    // Initialize SPI for Ethernet
    SPI.setRX(PIN_ETH_MISO);
    SPI.setCS(PIN_ETH_CS);
    SPI.setSCK(PIN_ETH_SCK);
    SPI.setTX(PIN_ETH_MOSI);
    SPI.begin();
    Serial.println("  SPI initialized successfully");
    
    // Set hostname first
    eth.hostname(config.hostname);
    eth.setSPISpeed(30000000);
    lwipPollingPeriod(3);
    
    bool connected = false;
    
    // Print config for debugging
    Serial.print("  DHCP Enabled: "); Serial.println(config.dhcpEnabled ? "Yes" : "No");
    Serial.print("  Static IP: "); Serial.print(config.ip[0]); Serial.print("."); Serial.print(config.ip[1]); Serial.print("."); Serial.print(config.ip[2]); Serial.print("."); Serial.println(config.ip[3]);
    Serial.print("  Gateway: "); Serial.print(config.gateway[0]); Serial.print("."); Serial.print(config.gateway[1]); Serial.print("."); Serial.print(config.gateway[2]); Serial.print("."); Serial.println(config.gateway[3]);
    Serial.print("  Subnet: "); Serial.print(config.subnet[0]); Serial.print("."); Serial.print(config.subnet[1]); Serial.print("."); Serial.print(config.subnet[2]); Serial.print("."); Serial.println(config.subnet[3]);

    if (config.dhcpEnabled) {
        // Try DHCP first
        Serial.println("Attempting to use DHCP...");
        if (eth.begin()) {
            // Wait for DHCP to complete
            Serial.println("DHCP process started, waiting for IP assignment...");
            int dhcpTimeout = 0;
            while (dhcpTimeout < 15) {  // Wait up to 15 seconds for DHCP
                IPAddress ip = eth.localIP();
                if (ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0) {
                    connected = true;
                    Serial.println("DHCP configuration successful");
                    break;
                }
                delay(1000);
                Serial.print(".");
                dhcpTimeout++;
            }
            
            if (!connected) {
                Serial.println("\nDHCP timeout, falling back to static IP");
            }
        } else {
            Serial.println("Failed to start DHCP process, falling back to static IP");
        }
    }
    
    // If DHCP failed or not enabled, use static IP
    if (!connected) {
        Serial.println("Using static IP configuration");
        IPAddress ip(config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
        IPAddress gateway(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]);
        IPAddress subnet(config.subnet[0], config.subnet[1], config.subnet[2], config.subnet[3]);
        
        // Stop current connection attempt if any
        eth.end();
        delay(500);  // Short delay to ensure clean restart
        
        eth.config(ip, gateway, subnet);
        if (eth.begin()) {
            delay(1000);  // Give it time to initialize with static IP
            IPAddress currentIP = eth.localIP();
            if (currentIP[0] != 0 || currentIP[1] != 0 || currentIP[2] != 0 || currentIP[3] != 0) {
                connected = true;
                Serial.println("Static IP configuration successful");
            } else {
                Serial.println("Static IP configuration failed - IP not assigned");
            }
        } else {
            Serial.println("Failed to start Ethernet with static IP");
        }
    }

    Serial.print("Hostname: "); Serial.println(config.hostname);
    Serial.print("IP Address: "); Serial.println(eth.localIP());

    if (!connected) {
        Serial.println("WARNING: Network connection not established. Please check cable, router, and IP settings.");
    } else {
        Serial.println("Network connection established successfully.");
    }
}

// USB RNDIS/ECM network is auto-configured by RP2040 Arduino core (Earle Philhower) using board build flags.
// No manual instantiation or library required. USB network appears as 192.168.7.1 when connected.
void setupUSBNetwork() {
    Serial.println("USB RNDIS/ECM Network Configuration:");
    Serial.println("  RP2040 Pico USB network is enabled via board build flags.");
    Serial.println("  USB IP: 192.168.7.1 (auto-configured)");
    Serial.println("  Web interface will be available on USB when HTTP server is bound to USB network.");
}

void setupModbus() {
    // Begin the modbus server with the configured port
    modbusServer.begin(config.modbusPort);
    
    Serial.print("Starting Modbus server on port: ");
    Serial.println(config.modbusPort);
    
    // Initialize all ModbusTCPServer instances
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        modbusClients[i].connected = false;
        
        // Initialize each ModbusTCPServer with the same unit ID (1)
        if (!modbusClients[i].server.begin(1)) {
            Serial.print("Failed to start Modbus TCP Server for client ");
            Serial.println(i);
            continue;
        }
        
        // Configure Modbus registers for each client server
        modbusClients[i].server.configureHoldingRegisters(0x00, 16);  // 16 holding registers
        modbusClients[i].server.configureInputRegisters(0x00, 32);    // 32 input registers
        modbusClients[i].server.configureCoils(0x00, 128);           // 128 coils (0-127)
        modbusClients[i].server.configureDiscreteInputs(0x00, 16);   // 16 discrete inputs
    }
    
    Serial.println("Modbus TCP Servers started");
}

void setupWebServer() {
    // Start HTTP server on Ethernet interface
    Serial.println("=== STARTING WEB SERVER ===");
    httpServer.begin();
    Serial.println("HTTP Server started on port 80");
    Serial.print("Server listening at: http://");
    Serial.println(eth.localIP());
    Serial.println("Web server ready for connections");
    Serial.println("================================");
}

void handleSimpleHTTP() {
    WiFiClient client = httpServer.accept();
    if (client) {
        Serial.println("=== WEB CLIENT CONNECTED ===");
        Serial.print("Client IP: ");
        Serial.println(client.remoteIP());
        
        String request = "";
        String method = "";
        String path = "";
        String body = "";
        bool inBody = false;
        int contentLength = 0;

        // Read the HTTP request headers
        while (client.connected() && client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();

            if (line.length() == 0) {
                inBody = true;
                break; // End of headers
            }
            if (request.length() == 0) {
                // First line: method and path
                int firstSpace = line.indexOf(' ');
                int secondSpace = line.indexOf(' ', firstSpace + 1);
                if (firstSpace > 0 && secondSpace > firstSpace) {
                    method = line.substring(0, firstSpace);
                    path = line.substring(firstSpace + 1, secondSpace);
                }
                request = line;
                Serial.println("HTTP Request: " + method + " " + path);
            }
            if (line.startsWith("Content-Length:")) {
                contentLength = line.substring(15).toInt();
            }
        }

        // Read body if present
        if (inBody && contentLength > 0) {
            char bodyBuffer[contentLength + 1];
            int bytesRead = client.readBytes(bodyBuffer, contentLength);
            bodyBuffer[bytesRead] = '\0';
            body = String(bodyBuffer);
        }

        // Route the request to existing handlers
        Serial.println("Routing request...");
        routeRequest(client, method, path, body);
        client.stop();
        Serial.println("=== WEB CLIENT DISCONNECTED ===");
    }
}

void routeRequest(WiFiClient& client, String method, String path, String body) {
    Serial.println("=== ROUTING REQUEST ===");
    Serial.println("Method: " + method);
    Serial.println("Path: " + path);
    
    // Simple routing to existing handler functions
    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            Serial.println("Serving index.html");
            // Try to serve file first, if it fails, serve basic page
            if (LittleFS.exists("/index.html")) {
                sendFile(client, "/index.html", "text/html");
            } else {
                // Fallback basic HTML page
                Serial.println("Filesystem index.html not found, serving basic page");
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println("Connection: close");
                client.println();
                client.println("<!DOCTYPE html><html><head>");
                client.println("<title>Modbus IO Module</title>");
                client.println("<style>body{font-family:Arial;margin:40px;} h1{color:#333;} .status{background:#f0f0f0;padding:20px;border-radius:5px;}</style>");
                client.println("</head><body>");
                client.println("<h1>Modbus IO Module</h1>");
                client.println("<div class='status'>");
                client.println("<h2>Device Status</h2>");
                client.println("<p><strong>IP Address:</strong> " + eth.localIP().toString() + "</p>");
                client.println("<p><strong>Uptime:</strong> " + String(millis()/1000) + " seconds</p>");
                client.println("<p><strong>Web Server:</strong> Active</p>");
                client.println("</div>");
                client.println("<h2>Available Endpoints:</h2>");
                client.println("<ul>");
                client.println("<li><a href='/test'>Test Page</a></li>");
                client.println("<li><a href='/config'>Configuration (JSON)</a></li>");
                client.println("<li><a href='/iostatus'>IO Status (JSON)</a></li>");
                client.println("</ul>");
                client.println("</body></html>");
            }
        } else if (path == "/test") {
            // Simple test page
            Serial.println("Serving test page");
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.println("<html><body>");
            client.println("<h1>Modbus IO Module - Test Page</h1>");
            client.println("<p>Web server is working!</p>");
            client.println("<p>Device IP: " + eth.localIP().toString() + "</p>");
            client.println("<p>Uptime: " + String(millis()/1000) + " seconds</p>");
            client.println("</body></html>");
        } else if (path == "/styles.css") {
            sendFile(client, "/styles.css", "text/css");
        } else if (path == "/script.js") {
            sendFile(client, "/script.js", "application/javascript");
        } else if (path == "/favicon.ico") {
            sendFile(client, "/favicon.ico", "image/x-icon");
        } else if (path == "/logo.png") {
            sendFile(client, "/logo.png", "image/png");
        } else if (path == "/config") {
            sendJSONConfig(client);
        } else if (path == "/iostatus") {
            sendJSONIOStatus(client);
        } else if (path == "/ioconfig") {
            sendJSONIOConfig(client);
        } else if (path == "/sensors/config") {
            sendJSONSensorConfig(client);
        } else if (path.startsWith("/available-pins")) {
            sendAvailablePins(client, path);
        } else if (path == "/sensors/types") {
            sendSensorTypes(client);
        } else {
            send404(client);
        }

// --- Place these at file scope, not inside any function ---
    } else if (method == "POST") {
        if (path == "/config") {
            handlePOSTConfig(client, body);
        } else if (path == "/setoutput") {
            handlePOSTSetOutput(client, body);
        } else if (path == "/ioconfig") {
            handlePOSTIOConfig(client, body);
        } else if (path == "/reset-latches") {
            handlePOSTResetLatches(client);
        } else if (path == "/reset-latch") {
            handlePOSTResetSingleLatch(client, body);
        } else if (path == "/sensors/config") {
            handlePOSTSensorConfig(client, body);
        } else if (path == "/api/sensor/command") {
            handlePOSTSensorCommand(client, body);
        } else if (path == "/api/sensor/calibration") {
            handlePOSTSensorCalibration(client, body);
        } else if (path == "/terminal/command") {
            handlePOSTTerminalCommand(client, body);
        } else {
            send404(client);
        }
    } else {
        send404(client);
    }
}

void sendFile(WiFiClient& client, String filename, String contentType) {
    if (LittleFS.exists(filename)) {
        File file = LittleFS.open(filename, "r");
        if (file) {
            client.println("HTTP/1.1 200 OK");
            client.print("Content-Type: ");
            client.println(contentType);
            client.println("Connection: close");
            client.print("Content-Length: ");
            client.println(file.size());
            client.println();
            
            while (file.available()) {
                client.write(file.read());
            }
            file.close();
            return;
        }
    }
    send404(client);
}

void send404(WiFiClient& client) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("404 Not Found");
}

void sendJSON(WiFiClient& client, String json) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(json.length());
    client.println();
    client.print(json);
}

// These functions extract the JSON creation logic from the original handlers
void sendJSONConfig(WiFiClient& client) {
    StaticJsonDocument<2048> doc;
    doc["version"] = config.version;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    doc["modbusPort"] = config.modbusPort;
    
    JsonArray ipArray = doc.createNestedArray("ip");
    for (int i = 0; i < 4; i++) {
        ipArray.add(config.ip[i]);
    }
    
    JsonArray gatewayArray = doc.createNestedArray("gateway");
    for (int i = 0; i < 4; i++) {
        gatewayArray.add(config.gateway[i]);
    }
    
    JsonArray subnetArray = doc.createNestedArray("subnet");
    for (int i = 0; i < 4; i++) {
        subnetArray.add(config.subnet[i]);
    }
    
    doc["hostname"] = config.hostname;
    
    JsonArray diPullupArray = doc.createNestedArray("diPullup");
    for (int i = 0; i < 8; i++) {
        diPullupArray.add(config.diPullup[i]);
    }
    
    JsonArray diInvertArray = doc.createNestedArray("diInvert");
    for (int i = 0; i < 8; i++) {
        diInvertArray.add(config.diInvert[i]);
    }
    
    JsonArray diLatchArray = doc.createNestedArray("diLatch");
    for (int i = 0; i < 8; i++) {
        diLatchArray.add(config.diLatch[i]);
    }
    
    JsonArray doInvertArray = doc.createNestedArray("doInvert");
    for (int i = 0; i < 8; i++) {
        doInvertArray.add(config.doInvert[i]);
    }
    
    JsonArray doInitialStateArray = doc.createNestedArray("doInitialState");
    for (int i = 0; i < 8; i++) {
        doInitialStateArray.add(config.doInitialState[i]);
    }
    
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    sendJSON(client, jsonBuffer);
}

void sendJSONIOStatus(WiFiClient& client) {
    StaticJsonDocument<1024> doc;
    
    JsonArray dInArray = doc.createNestedArray("dIn");
    for (int i = 0; i < 8; i++) {
        dInArray.add(ioStatus.dIn[i]);
    }
    
    JsonArray dOutArray = doc.createNestedArray("dOut");
    for (int i = 0; i < 8; i++) {
        dOutArray.add(ioStatus.dOut[i]);
    }
    
    JsonArray aInArray = doc.createNestedArray("aIn");
    for (int i = 0; i < 3; i++) {
        aInArray.add(ioStatus.aIn[i]);
    }
    
    JsonArray dInLatchedArray = doc.createNestedArray("dInLatched");
    for (int i = 0; i < 8; i++) {
        dInLatchedArray.add(ioStatus.dInLatched[i]);
    }
    
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    sendJSON(client, jsonBuffer);
}

void sendJSONIOConfig(WiFiClient& client) {
    StaticJsonDocument<1024> doc;
    
    JsonArray diPullupArray = doc.createNestedArray("diPullup");
    for (int i = 0; i < 8; i++) {
        diPullupArray.add(config.diPullup[i]);
    }
    
    JsonArray diInvertArray = doc.createNestedArray("diInvert");
    for (int i = 0; i < 8; i++) {
        diInvertArray.add(config.diInvert[i]);
    }
    
    JsonArray diLatchArray = doc.createNestedArray("diLatch");
    for (int i = 0; i < 8; i++) {
        diLatchArray.add(config.diLatch[i]);
    }
    
    JsonArray doInvertArray = doc.createNestedArray("doInvert");
    for (int i = 0; i < 8; i++) {
        doInvertArray.add(config.doInvert[i]);
    }
    
    JsonArray doInitialStateArray = doc.createNestedArray("doInitialState");
    for (int i = 0; i < 8; i++) {
        doInitialStateArray.add(config.doInitialState[i]);
    }
    
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

void sendJSONSensorConfig(WiFiClient& client) {
    StaticJsonDocument<2048> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["sensor_type"] = configuredSensors[i].sensor_type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["type"] = configuredSensors[i].protocol; // Legacy compatibility
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["response"] = configuredSensors[i].response;
    }
    
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

// POST handler functions
void handlePOSTConfig(WiFiClient& client, String body) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Connection: close");
        client.println();
        return;
    }
    
    // Update configuration (same logic as original handleSetConfig)
    config.dhcpEnabled = doc["dhcpEnabled"] | config.dhcpEnabled;
    config.modbusPort = doc["modbusPort"] | config.modbusPort;
    
    if (doc.containsKey("ip")) {
        JsonArray ipArray = doc["ip"];
        for (int i = 0; i < 4 && i < ipArray.size(); i++) {
            config.ip[i] = ipArray[i];
        }
    }
    
    saveConfig();
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

void handlePOSTSetOutput(WiFiClient& client, String body) {
    // Simple query parameter parsing for output=X&state=Y
    int outputIndex = -1;
    int state = -1;
    
    int outputPos = body.indexOf("output=");
    int statePos = body.indexOf("state=");
    
    if (outputPos >= 0) {
        outputIndex = body.substring(outputPos + 7, body.indexOf('&', outputPos)).toInt();
    }
    if (statePos >= 0) {
        state = body.substring(statePos + 6).toInt();
    }
    
    if (outputIndex >= 0 && outputIndex < 8 && (state == 0 || state == 1)) {
        ioStatus.dOut[outputIndex] = state;
        digitalWrite(DIGITAL_OUTPUTS[outputIndex], config.doInvert[outputIndex] ? !state : state);
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":true}");
    } else {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Connection: close");
        client.println();
    }
}

void handlePOSTIOConfig(WiFiClient& client, String body) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (!error) {
        // Update IO configuration
        if (doc.containsKey("diPullup")) {
            JsonArray array = doc["diPullup"];
            for (int i = 0; i < 8 && i < array.size(); i++) {
                config.diPullup[i] = array[i];
            }
        }
        saveConfig();
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

void handlePOSTResetLatches(WiFiClient& client) {
    for (int i = 0; i < 8; i++) {
        if (config.diLatch[i]) {
            ioStatus.dInLatched[i] = false;
        }
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

void handlePOSTResetSingleLatch(WiFiClient& client, String body) {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, body) && doc.containsKey("input")) {
        int input = doc["input"];
        if (input >= 0 && input < 8 && config.diLatch[input]) {
            ioStatus.dInLatched[input] = false;
        }
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

void handlePOSTSensorConfig(WiFiClient& client, String body) {
    StaticJsonDocument<2048> doc;
    if (!deserializeJson(doc, body)) {
        // Save sensor configuration logic here
        saveSensorConfig();
    }
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"success\":true}");
}

void handlePOSTSensorCommand(WiFiClient& client, String body) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    String protocol = doc["protocol"] | "";
    String pin = doc["pin"] | "";
    String command = doc["command"] | "";
    String i2cAddress = doc["i2cAddress"] | "";
    
    String response = "Command executed";
    bool success = true;
    
    // Route command based on protocol
    if (protocol == "digital") {
        // Handle digital I/O commands
        if (command == "read") {
            if (pin.startsWith("DI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = digitalRead(DIGITAL_INPUTS[pinNum]);
                    response = pin + " = " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else if (pin.startsWith("DO")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = ioStatus.dOut[pinNum];
                    response = pin + " = " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            }
        } else if (command.startsWith("write ")) {
            String value = command.substring(6);
            if (pin.startsWith("DO")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = (value == "1" || value.equalsIgnoreCase("HIGH"));
                    ioStatus.dOut[pinNum] = state;
                    digitalWrite(DIGITAL_OUTPUTS[pinNum], config.doInvert[pinNum] ? !state : state);
                    response = pin + " set to " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else {
                success = false;
                response = "Error: Cannot write to input pin";
            }
        } else {
            success = false;
            response = "Error: Unknown digital command";
        }
    } else if (protocol == "analog") {
        // Handle analog commands
        if (command == "read") {
            if (pin.startsWith("AI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 3) {
                    uint32_t rawValue = analogRead(ANALOG_INPUTS[pinNum]);
                    uint16_t millivolts = (rawValue * 3300UL) / 4095UL;
                    response = pin + " = " + String(millivolts) + " mV";
                } else {
                    success = false;
                    response = "Error: Invalid analog pin number";
                }
            }
        } else {
            success = false;
            response = "Error: Unknown analog command";
        }
    } else if (protocol == "i2c") {
        // Handle I2C commands
        if (command == "scan") {
            response = "I2C Device Scan:\\n";
            bool foundDevices = false;
            for (int addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    response += "Found device at 0x" + String(addr, HEX) + "\\n";
                    foundDevices = true;
                }
            }
            if (!foundDevices) {
                response += "No I2C devices found";
            }
        } else if (command == "probe") {
            int addr = i2cAddress.startsWith("0x") ? strtol(i2cAddress.c_str(), NULL, 16) : i2cAddress.toInt();
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                response = "Device at " + i2cAddress + " is present";
            } else {
                response = "No device found at " + i2cAddress;
            }
        } else {
            success = false;
            response = "Error: I2C command not implemented";
        }
    } else if (protocol == "system") {
        // Handle system commands
        if (command == "status") {
            response = "System Status:\\n";
            response += "CPU: RP2040 @ 133MHz\\n";
            response += "RAM: 256KB\\n";
            response += "Flash: 2MB\\n";
            response += "Uptime: " + String(millis() / 1000) + " seconds\\n";
            response += "Free Heap: " + String(rp2040.getFreeHeap()) + " bytes";
        } else if (command == "sensors") {
            response = "Configured Sensors:\\n";
            for (int i = 0; i < numConfiguredSensors; i++) {
                response += String(i) + ": " + String(configuredSensors[i].name) + 
                           " (" + String(configuredSensors[i].protocol) + ") - " + 
                           (configuredSensors[i].enabled ? "Enabled" : "Disabled") + "\\n";
            }
            if (numConfiguredSensors == 0) {
                response += "No sensors configured";
            }
        } else {
            success = false;
            response = "Error: Unknown system command";
        }
    } else if (protocol == "network") {
        // Handle network commands
        if (command == "status") {
            response = "Network Status:\\n";
            IPAddress ip = eth.localIP();
            response += "IP: " + ip.toString() + "\\n";
            response += "DHCP: " + String(config.dhcpEnabled ? "Enabled" : "Disabled") + "\\n";
            response += "Modbus Port: " + String(config.modbusPort) + "\\n";
            response += "Connected Clients: " + String(connectedClients);
        } else if (command == "clients") {
            response = "Modbus Clients:\\n";
            response += "Connected: " + String(connectedClients) + "\\n";
            for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
                if (modbusClients[i].connected) {
                    response += "Slot " + String(i) + ": " + modbusClients[i].clientIP.toString() + "\\n";
                }
            }
        } else {
            success = false;
            response = "Error: Unknown network command";
        }
    } else {
        success = false;
        response = "Error: Unknown protocol";
    }
    
    // Send JSON response
    StaticJsonDocument<1024> responseDoc;
    responseDoc["success"] = success;
    responseDoc["message"] = response;
    
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(jsonResponse.length());
    client.println();
    client.print(jsonResponse);
}

void updateIOpins() {
    // Update Modbus registers with current IO state
    
    // Update digital inputs - account for invert configuration and latching behavior
    for (int i = 0; i < 8; i++) {
        uint16_t rawValue = digitalRead(DIGITAL_INPUTS[i]);
        
        // Apply inversion if configured
        if (config.diInvert[i]) {
            rawValue = !rawValue;
        }
        
        // Store the raw input state
        ioStatus.dInRaw[i] = rawValue;
        
        // Check if latching is enabled for this input
        if (config.diLatch[i]) {
            // If input is active (HIGH) and not already latched, set the latch
            if (rawValue && !ioStatus.dInLatched[i]) {
                ioStatus.dInLatched[i] = true;
                ioStatus.dIn[i] = true; // Set the input state to ON
            }
            // If input is latched, keep it ON regardless of the current physical state
            else if (ioStatus.dInLatched[i]) {
                ioStatus.dIn[i] = true;
            }
            // Otherwise, use the raw value
            else {
                ioStatus.dIn[i] = rawValue;
            }
        }
    }
    
    // Update digital outputs - account for inversion
    for (int i = 0; i < 8; i++) {
        // Check the coil state for each client and update if any client changed an output
        bool logicalState = ioStatus.dOut[i];
        bool stateChanged = false;
        
        // First detect if any client has changed the state
        for (int j = 0; j < MAX_MODBUS_CLIENTS; j++) {
            if (modbusClients[j].connected) {
                bool clientCoilState = modbusClients[j].server.coilRead(i);
                if (clientCoilState != logicalState) {
                    // A client has changed this output's state, update our logical state
                    logicalState = clientCoilState;
                    ioStatus.dOut[i] = logicalState;
                    stateChanged = true;
                    break;  // We found a change, no need to check other clients
                }
            }
        }
        
        // If state changed, synchronize all clients to the new state
        if (stateChanged) {
            Serial.printf("Output %d state changed to %d, synchronizing all clients\n", i, logicalState);
            for (int j = 0; j < MAX_MODBUS_CLIENTS; j++) {
                if (modbusClients[j].connected) {
                    modbusClients[j].server.coilWrite(i, logicalState);
                }
            }
        }
        
        // Apply inversion only to the physical pin, not to the logical state
        bool physicalState = logicalState;
        if (config.doInvert[i]) {
            physicalState = !logicalState;
        }
        
        // Set the physical pin state
        digitalWrite(DIGITAL_OUTPUTS[i], physicalState);
    }
    
    // Update analog inputs, using millivolts format
    for (int i = 0; i < 3; i++) {
        uint32_t rawValue = analogRead(ANALOG_INPUTS[i]);
        uint16_t valueToWrite = (rawValue * 3300UL) / 4095UL;
        ioStatus.aIn[i] = valueToWrite;
    }
    
    // I2C Sensor Reading - Dynamic sensor configuration
    static uint32_t sensorReadTime = 0;
    if (millis() - sensorReadTime > 1000) { // Update every 1 second
        // Initialize sensor values - only temperature is used by EZO_RTD if configured
        ioStatus.temperature = 0.0;  // Only used by EZO_RTD sensors for pH compensation
        
        // Read from configured sensors
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled) {
                Serial.printf("Reading sensor %s (%s) at I2C address 0x%02X\n", 
                    configuredSensors[i].name,
                    configuredSensors[i].protocol,
                    configuredSensors[i].i2cAddress
                );
                
                // Add actual sensor reading logic here based on sensor type
                if (strncmp(configuredSensors[i].protocol, "EZO_", 4) == 0) {
                    // Parse EZO sensor response from the response field
                    if (strlen(configuredSensors[i].response) > 0 && strcmp(configuredSensors[i].response, "ERROR") != 0) {
                        float value = atof(configuredSensors[i].response);
                        
                        // Map EZO sensor types to appropriate ioStatus fields
                        if (strcmp(configuredSensors[i].protocol, "EZO_PH") == 0) {
                            // For pH sensors, we can store in temperature field temporarily or add a pH field later
                            Serial.printf("EZO_PH reading: pH=%.2f\n", value);
                        } else if (strcmp(configuredSensors[i].protocol, "EZO_DO") == 0) {
                            Serial.printf("EZO_DO reading: DO=%.2f mg/L\n", value);
                        } else if (strcmp(configuredSensors[i].protocol, "EZO_EC") == 0) {
                            Serial.printf("EZO_EC reading: EC=%.2f μS/cm\n", value);
                        } else if (strcmp(configuredSensors[i].protocol, "EZO_RTD") == 0) {
                            ioStatus.temperature = value;
                            Serial.printf("EZO_RTD reading: Temperature=%.2f°C\n", value);
                        } else {
                            Serial.printf("EZO sensor %s reading: %.2f\n", configuredSensors[i].protocol, value);
                        }
                    } else if (strcmp(configuredSensors[i].response, "ERROR") == 0) {
                        Serial.printf("EZO sensor %s has error response\n", configuredSensors[i].protocol);
                    }
                }
                // TODO: Add support for other sensor types here
                // Example for BME280:
                // else if (strcmp(configuredSensors[i].type, "BME280") == 0) {
                //     // Add BME280 library initialization and reading code
                // }
            }
        }
        
        // No built-in sensors - all sensor data comes from configured I2C sensors
        if (numConfiguredSensors == 0) {
            // Only print this message once every 60 seconds to avoid spam
            static unsigned long lastSensorMessage = 0;
            if (millis() - lastSensorMessage > 60000) {
                Serial.println("No I2C sensors configured");
                lastSensorMessage = millis();
            }
        }
        
        // Print IP address every 30 seconds for easy reference
        static uint32_t ipPrintTime = 0;
        if (millis() - ipPrintTime > 30000) {
            Serial.println("========================================");
            Serial.print("Device IP Address: ");
            Serial.println(eth.localIP());
            Serial.println("========================================");
            ipPrintTime = millis();
        }
        
        sensorReadTime = millis();
    }
}

void updateIOForClient(int clientIndex) {
    // Update Modbus registers with current IO state, actual pin states measured in updateIOpins()
    
    // Update digital inputs
    for (int i = 0; i < 8; i++) {
        modbusClients[clientIndex].server.discreteInputWrite(i, ioStatus.dIn[i]);
    }
        
    // Update analog inputs
    for (int i = 0; i < 3; i++) {
        modbusClients[clientIndex].server.inputRegisterWrite(i, ioStatus.aIn[i]);
    }
    
    // I2C Sensor Data Modbus Mapping - Convert float values to 16-bit integers
    // PLACEHOLDER SENSORS - COMMENTED OUT (only enable when simulation is on or real sensors added)
    // uint16_t temp_x_100 = (uint16_t)(ioStatus.temperature * 100);
    // uint16_t hum_x_100 = (uint16_t)(ioStatus.humidity * 100);

    // modbusClients[clientIndex].server.inputRegisterWrite(3, temp_x_100); // Temperature
    // modbusClients[clientIndex].server.inputRegisterWrite(4, hum_x_100); // Humidity
    
    // Check coils 100-107 for latch reset commands
    for (int i = 0; i < 8; i++) {
        if (modbusClients[clientIndex].server.coilRead(100 + i)) {
            // If coil is set to 1, reset the corresponding latch
            if (config.diLatch[i] && ioStatus.dInLatched[i]) {
                ioStatus.dInLatched[i] = false;
                // Update the input state based on the raw input state
                ioStatus.dIn[i] = ioStatus.dInRaw[i];
                Serial.printf("Reset latch for digital input %d via Modbus coil %d\n", i, 100 + i);
            }
            // Reset the coil back to 0 after processing
            modbusClients[clientIndex].server.coilWrite(100 + i, false);
        }
    }
}

// HTTP Handler - Currently Ethernet only, USB interface planned
void handleDualHTTP() {
    // Handle Ethernet HTTP requests
    handleSimpleHTTP();
    // USB RNDIS HTTP implementation is auto-configured by RP2040 core; no manual handling required here.
    // To support USB HTTP, bind server to USB network interface if/when supported by Arduino core.
}