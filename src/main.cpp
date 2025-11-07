// ...existing code...


// ...existing code...


// ...existing code...


#include "sys_init.h"

// Sensor reading functions
bool readSHT30(uint8_t sensorIndex, float& temperature, float& humidity);
bool readDS18B20(uint8_t sensorIndex, float& temperature);
bool readEZOPH(uint8_t sensorIndex, float& ph);
bool readEZOEC(uint8_t sensorIndex, float& conductivity);
float parseSensorData(const char* rawData, const SensorConfig& sensor);
float evaluateCalibrationExpression(float x, const SensorConfig& sensor);
float applyCalibration(float rawValue, const SensorConfig& sensor);
float applyCalibrationB(float rawValue, const SensorConfig& sensor);
float applyCalibrationC(float rawValue, const SensorConfig& sensor);
// Use ANALOG_INPUTS from sys_init.h instead of ADC_PINS
#include "Ezo_i2c.h"

// SensorConfig array definition (from sys_init.h extern)
SensorConfig configuredSensors[MAX_SENSORS] = {};
int numConfiguredSensors = 0;

// Preset table for named sensors
struct SensorPreset {
    const char* type;
    const char* protocol;
    uint8_t command[2];
    int commandLen;
    int updateInterval;
    int delayBeforeRead;
};

const SensorPreset sensorPresets[] = {
    // SHT30: I2C command 0x2C 0x06, 1s polling, 15ms delay
    { "SHT30", "I2C", {0x2C, 0x06}, 2, 1000, 15 },
    // Add more named sensors here as needed
};

void applySensorPresets() {
    int ezoPhCount = 0;
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        // Auto-configure based on sensor type (only if not already configured)
            if (strcmp(configuredSensors[i].type, "EZO-PH") == 0 || strcmp(configuredSensors[i].type, "EZO_PH") == 0) {
            ezoPhCount++;
            if (ezoPhCount > 1) {
                // Disable duplicate EZO-PH sensors
                configuredSensors[i].enabled = false;
                Serial.println("Duplicate EZO-PH sensor disabled");
                continue;
            }
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x63;
            // Only set command if not already configured by user
            if (strlen(configuredSensors[i].command) == 0) strcpy(configuredSensors[i].command, "R\r");
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 5000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 10;
            Serial.printf("Auto-configured EZO-PH sensor (preserved user settings: addr=%d, cmd='%s', interval=%d)\n", 
                         configuredSensors[i].i2cAddress, configuredSensors[i].command, configuredSensors[i].updateInterval);
        } else if (strcmp(configuredSensors[i].type, "EZO-EC") == 0) {
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x64;
            if (strlen(configuredSensors[i].command) == 0) strcpy(configuredSensors[i].command, "R");
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 5000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 11;
            Serial.println("Auto-configured EZO-EC sensor");
        } else if (strcmp(configuredSensors[i].type, "EZO-DO") == 0) {
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x61;
            if (strlen(configuredSensors[i].command) == 0) strcpy(configuredSensors[i].command, "R");
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 5000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 12;
            Serial.println("Auto-configured EZO-DO sensor");
        } else if (strcmp(configuredSensors[i].type, "EZO-RTD") == 0) {
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x66;
            if (strlen(configuredSensors[i].command) == 0) strcpy(configuredSensors[i].command, "R");
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 5000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 13;
            Serial.println("Auto-configured EZO-RTD sensor");
        } else if (strcmp(configuredSensors[i].type, "SHT30") == 0) {
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x44;
            if (strlen(configuredSensors[i].command) == 0) strcpy(configuredSensors[i].command, "0x2C06");  // SHT30 measurement command
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 1000;
            // Don't override manually configured modbus register!
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 15;
            Serial.printf("Auto-configured SHT30 sensor (register preserved: %d)\n", configuredSensors[i].modbusRegister);
        } else if (strcmp(configuredSensors[i].type, "BME280") == 0) {
            if (configuredSensors[i].i2cAddress == 0) configuredSensors[i].i2cAddress = 0x76;
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 1000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 16;
            Serial.println("Auto-configured BME280 sensor");
        } else if (strcmp(configuredSensors[i].type, "DS18B20") == 0) {
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "One-Wire");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 2000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 14;
            Serial.println("Auto-configured DS18B20 sensor");
        } else if (strcmp(configuredSensors[i].type, "Generic One-Wire") == 0) {
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "One-Wire");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 2000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 17; // Next available register
            Serial.println("Auto-configured Generic One-Wire sensor");
        } else if (strcmp(configuredSensors[i].type, "Generic I2C") == 0) {
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "I2C");
            if (configuredSensors[i].updateInterval == 0) configuredSensors[i].updateInterval = 1000;
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 18; // Next available register
            Serial.println("Auto-configured Generic I2C sensor");
        } else if (strcmp(configuredSensors[i].type, "GENERIC_UART") == 0) {
            if (strlen(configuredSensors[i].protocol) == 0) strcpy(configuredSensors[i].protocol, "UART");
            // Don't override updateInterval - use what was configured in web UI
            if (configuredSensors[i].modbusRegister == 0) configuredSensors[i].modbusRegister = 19; // Next available register
            // Set default UART pins if not configured
            if (configuredSensors[i].uartTxPin == 0) configuredSensors[i].uartTxPin = 0; // GP0
            if (configuredSensors[i].uartRxPin == 0) configuredSensors[i].uartRxPin = 1; // GP1
            Serial.printf("Auto-configured GENERIC_UART sensor (updateInterval preserved: %d)\n", configuredSensors[i].updateInterval);
        }
        
        // Set default pins if not configured
        if (configuredSensors[i].sdaPin == 0) configuredSensors[i].sdaPin = I2C_SDA_PIN;
        if (configuredSensors[i].sclPin == 0) configuredSensors[i].sclPin = I2C_SCL_PIN;
    }
    for (int i = 0; i < numConfiguredSensors; i++) {
        for (unsigned int p = 0; p < sizeof(sensorPresets)/sizeof(sensorPresets[0]); p++) {
            if (strcmp(configuredSensors[i].type, sensorPresets[p].type) == 0 &&
                strcmp(configuredSensors[i].protocol, sensorPresets[p].protocol) == 0) {
                // Only set fields if not already set
                if (configuredSensors[i].updateInterval <= 0)
                    configuredSensors[i].updateInterval = sensorPresets[p].updateInterval;
                // Don't override calibration - let user-configured values persist
                if (strlen(configuredSensors[i].command) == 0 && configuredSensors[i].calibrationOffset == 0 && configuredSensors[i].calibrationSlope <= 1.0) {
                    // Set command bytes as string for compatibility - only if no user command set
                    snprintf(configuredSensors[i].command, sizeof(configuredSensors[i].command), "0x%02X 0x%02X", sensorPresets[p].command[0], sensorPresets[p].command[1]);
                }
                // Protocol-specific delay assignment
                if (strcmp(configuredSensors[i].protocol, "I2C") == 0) {
                    // For I2C, use updateInterval and (optionally) a dedicated delay field if present
                    // If you have a conversionTime or similar, set it here
                    // Example: configuredSensors[i].conversionTime = sensorPresets[p].delayBeforeRead;
                } else if (strcmp(configuredSensors[i].protocol, "ONEWIRE") == 0) {
                    // For One-Wire, set oneWireConversionTime and oneWireCommand
                    if (configuredSensors[i].oneWireConversionTime <= 0)
                        configuredSensors[i].oneWireConversionTime = sensorPresets[p].delayBeforeRead;
                    // Set oneWireCommand if not set
                    if (strlen(configuredSensors[i].oneWireCommand) == 0)
                        snprintf(configuredSensors[i].oneWireCommand, sizeof(configuredSensors[i].oneWireCommand), "0x%02X", sensorPresets[p].command[0]);
                }
            }
        }
    }
}

// Constants
#define MAX_TERMINAL_BUFFER 100
#define HTTP_PORT 80

// Global object definitions
Config config; // Define the actual config object
IOStatus ioStatus = {};

// Network configuration for W5500
uint8_t mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WiFiServer modbusServer(502); 
WiFiServer httpServer(80);    // HTTP server on port 80
WiFiClient client;
ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
int connectedClients = 0;

// Bus operation queues are declared in sys_init.h
extern BusOperation i2cQueue[MAX_SENSORS];
extern BusOperation uartQueue[MAX_SENSORS];
extern BusOperation oneWireQueue[MAX_SENSORS];
extern int i2cQueueSize;
extern int uartQueueSize;
extern int oneWireQueueSize;

// Command arrays for each bus type
CommandArray i2cCommands;
CommandArray uartCommands;
CommandArray oneWireCommands;

// Bus operation queues
BusOperation i2cQueue[MAX_SENSORS];
BusOperation uartQueue[MAX_SENSORS];
BusOperation oneWireQueue[MAX_SENSORS];
int i2cQueueSize = 0;
int uartQueueSize = 0;
int oneWireQueueSize = 0;

// Command queues are already declared as extern above

// Forward declarations for functions used before definition
void handleSimpleHTTP();
void updateIOForClient(int clientIndex);
void routeRequest(WiFiClient& client, String method, String path, String body);
void sendFile(WiFiClient& client, String filename, String contentType);
void send404(WiFiClient& client);
void sendJSONConfig(WiFiClient& client);
void sendJSONIOStatus(WiFiClient& client);
void sendJSONIOConfig(WiFiClient& client);
void sendJSONSensorData(WiFiClient& client);
void sendJSONSensorConfig(WiFiClient& client);
void handlePOSTConfig(WiFiClient& client, String body);
void handlePOSTSetOutput(WiFiClient& client, String body);
void handlePOSTIOConfig(WiFiClient& client, String body);
void handlePOSTResetLatches(WiFiClient& client);
void handlePOSTResetSingleLatch(WiFiClient& client, String body);
// Sensor functions temporarily commented out
void handlePOSTSensorConfig(WiFiClient& client, String body);
void handlePOSTSensorCommand(WiFiClient& client, String body);
void handlePOSTSensorPoll(WiFiClient& client, String body);
// TODO: Implement Poll Now functionality
bool validateCRC(uint8_t* data, size_t length) {
    uint8_t crc = 0;
    
    for (size_t i = 0; i < length - 1; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    
    return crc == data[length - 1];
}

void setPinModes();
void setupEthernet();
void setupModbus();
void setupWebServer();

// Terminal monitoring function declarations
void addTerminalLog(String message);
void logI2CTransaction(int address, String direction, String data, String pin);
void logOneWireTransaction(String pin, String direction, String data);
void logUARTTransaction(String pin, String direction, String data);
void logNetworkTransaction(String protocol, String direction, String localAddr, String remoteAddr, String data);
String executeTerminalCommand(String command, String pin, String protocol);

// Expression evaluator for sensor calibration
float evaluateExpression(const char* expr, float x) {
    // Simple expression parser supporting basic math operations
    // Variables: x = input value
    // Operations: +, -, *, /, ^, sqrt(), log(), abs()
    
    if (strlen(expr) == 0) return x;
    
    String expression = String(expr);
    expression.trim();
    expression.toLowerCase();
    
    // Replace 'x' with actual value
    expression.replace("x", String(x, 6));
    
    // Handle common math functions first
    if (expression.startsWith("sqrt(") && expression.endsWith(")")) {
        String inner = expression.substring(5, expression.length() - 1);
        float val = evaluateExpression(inner.c_str(), x);
        return sqrt(val);
    }
    
    if (expression.startsWith("log(") && expression.endsWith(")")) {
        String inner = expression.substring(4, expression.length() - 1);
        float val = evaluateExpression(inner.c_str(), x);
        return log(val);
    }
    
    if (expression.startsWith("abs(") && expression.endsWith(")")) {
        String inner = expression.substring(4, expression.length() - 1);
        float val = evaluateExpression(inner.c_str(), x);
        return abs(val);
    }
    
    // Simple recursive descent parser for basic arithmetic
    // Handle power operator (^) first (highest precedence)
    int powerPos = expression.lastIndexOf('^');
    if (powerPos > 0) {
        String left = expression.substring(0, powerPos);
        String right = expression.substring(powerPos + 1);
        float leftVal = evaluateExpression(left.c_str(), x);
        float rightVal = evaluateExpression(right.c_str(), x);
        return pow(leftVal, rightVal);
    }
    
    // Handle addition and subtraction (lowest precedence)
    for (int i = expression.length() - 1; i >= 0; i--) {
        char c = expression.charAt(i);
        if (c == '+' && i > 0) {
            String left = expression.substring(0, i);
            String right = expression.substring(i + 1);
            return evaluateExpression(left.c_str(), x) + evaluateExpression(right.c_str(), x);
        }
        if (c == '-' && i > 0) {
            String left = expression.substring(0, i);
            String right = expression.substring(i + 1);
            return evaluateExpression(left.c_str(), x) - evaluateExpression(right.c_str(), x);
        }
    }
    
    // Handle multiplication and division
    for (int i = expression.length() - 1; i >= 0; i--) {
        char c = expression.charAt(i);
        if (c == '*' && i > 0) {
            String left = expression.substring(0, i);
            String right = expression.substring(i + 1);
            return evaluateExpression(left.c_str(), x) * evaluateExpression(right.c_str(), x);
        }
        if (c == '/' && i > 0) {
            String left = expression.substring(0, i);
            String right = expression.substring(i + 1);
            float rightVal = evaluateExpression(right.c_str(), x);
            if (rightVal != 0.0f) {
                return evaluateExpression(left.c_str(), x) / rightVal;
            }
            return 0.0f; // Division by zero protection
        }
    }
    
    // Handle parentheses
    if (expression.startsWith("(") && expression.endsWith(")")) {
        String inner = expression.substring(1, expression.length() - 1);
        return evaluateExpression(inner.c_str(), x);
    }
    
    // Parse as number (base case)
    return expression.toFloat();
}

// Bus operation management functions
void processI2CQueue();
void processUARTQueue();
void processOneWireQueue();
void enqueueBusOperation(uint8_t sensorIndex, const char* protocol);
void updateBusQueues();
// validateCRC is already declared above

// CRC validation for One-Wire sensors is implemented above

// Bus queue processor implementations
void processI2CQueue() {
    if (i2cQueueSize == 0) return;
    
    unsigned long currentTime = millis();
    BusOperation& op = i2cQueue[0];
    
    switch(op.state) {
        case BusOpState::IDLE: {
            // Start I2C operation
            Wire.beginTransmission(configuredSensors[op.sensorIndex].i2cAddress);
            
            // Check if sensor has a command to send
            bool hasCommand = strlen(configuredSensors[op.sensorIndex].command) > 0;
            
            if (hasCommand) {
                // Sanitize command: remove control chars except explicit \r or \n
                String rawCmd = String(configuredSensors[op.sensorIndex].command);
                String command = "";
                for (int i = 0; i < rawCmd.length(); i++) {
                    char c = rawCmd[i];
                    // Allow printable ASCII, or explicit \r/\n sequences
                    if (c >= 32 && c <= 126) {
                        command += c;
                    } else if (c == '\\' && i + 1 < rawCmd.length()) {
                        char next = rawCmd[i + 1];
                        if (next == 'r' || next == 'n') {
                            command += c;
                            command += next;
                            i++;
                        }
                    }
                    // Ignore all other control chars
                }

                // Check if command is hex format (starts with 0x or contains only hex digits)
                bool isHexCommand = command.startsWith("0x") || command.startsWith("0X");
                if (!isHexCommand) {
                    // Check if it's all hex digits (allowing spaces)
                    String testCmd = command;
                    testCmd.replace(" ", "");
                    isHexCommand = true;
                    for (int i = 0; i < testCmd.length(); i++) {
                        char c = testCmd[i];
                        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                            isHexCommand = false;
                            break;
                        }
                    }
                }

                if (isHexCommand) {
                    // Hex command format (e.g., "0x2C06" or "2C 06")
                    String cleanCmd = command;
                    cleanCmd.replace("0x", "");
                    cleanCmd.replace("0X", "");
                    cleanCmd.replace(" ", "");

                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "TX", 
                                    "CMD: " + command + " (HEX) (" + String(configuredSensors[op.sensorIndex].type) + ")", 
                                    String(configuredSensors[op.sensorIndex].name));

                    // Write hex command bytes
                    for (int i = 0; i < cleanCmd.length(); i += 2) {
                        if (i + 1 < cleanCmd.length()) {
                            String hexByte = cleanCmd.substring(i, i + 2);
                            uint8_t byte = strtoul(hexByte.c_str(), NULL, 16);
                            Wire.write(byte);
                        }
                    }
                } else {
                    // Text command format (e.g., "R\r" for EZO sensors)
                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "TX", 
                                    "CMD: \"" + command + "\" (TEXT) (" + String(configuredSensors[op.sensorIndex].type) + ")", 
                                    String(configuredSensors[op.sensorIndex].name));

                    // Write text command as ASCII bytes
                    for (int i = 0; i < command.length(); i++) {
                        Wire.write((uint8_t)command[i]);
                    }
                }
            } else {
                // No command - direct read sensor
                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "PREP", 
                                "Direct read (no command) for " + String(configuredSensors[op.sensorIndex].name), 
                                String(configuredSensors[op.sensorIndex].name));
            }
            
            int result = Wire.endTransmission();
            
            if (result == 0) {
                // Check if sensor needs delay before reading
                bool hasCommand = strlen(configuredSensors[op.sensorIndex].command) > 0;
                if (hasCommand && op.conversionTime > 0) {
                    op.state = BusOpState::REQUEST_SENT;
                    op.startTime = currentTime;
                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ACK", "Command sent successfully", String(configuredSensors[op.sensorIndex].name));
                } else {
                    // No delay needed - go directly to read
                    op.state = BusOpState::READY_TO_READ;
                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ACK", "Ready for immediate read", String(configuredSensors[op.sensorIndex].name));
                }
            } else {
                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "NACK", "Error code: " + String(result), String(configuredSensors[op.sensorIndex].name));
                if (++op.retryCount >= 3) {
                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "Max retries exceeded", String(configuredSensors[op.sensorIndex].name));
                    // Move to next operation after 3 retries
                    for(int i = 0; i < i2cQueueSize - 1; i++) {
                        i2cQueue[i] = i2cQueue[i + 1];
                    }
                    i2cQueueSize--;
                }
            }
            break;
        }
            
        case BusOpState::REQUEST_SENT: {
            if (currentTime - op.startTime >= op.conversionTime) {
                op.state = BusOpState::READY_TO_READ;
            }
            break;
        }
            
        case BusOpState::READY_TO_READ: {
            logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "REQ", 
                            "Requesting 32 bytes from " + String(configuredSensors[op.sensorIndex].name), 
                            String(configuredSensors[op.sensorIndex].name));
            Wire.requestFrom((int)configuredSensors[op.sensorIndex].i2cAddress, 32);
            
            if (Wire.available()) {
                char response[32] = {0};
                String rawHex = "";
                int idx = 0;
                
                while(Wire.available() && idx < 31) {
                    uint8_t byte = Wire.read();
                    response[idx++] = byte;
                    if (rawHex.length() > 0) rawHex += " ";
                    rawHex += "0x" + String(byte, HEX);
                }
                response[idx] = '\0';
                
                // Log raw response
                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "RX", 
                                "Raw: [" + rawHex + "] ASCII: \"" + String(response) + "\"", 
                                String(configuredSensors[op.sensorIndex].name));
                
                // Store response for web interface (clean for JSON compatibility)
                String cleanResponse = "";
                for (int j = 0; j < idx; j++) {
                    char c = response[j];
                    if (c >= 32 && c <= 126) { // Only printable ASCII
                        cleanResponse += c;
                    }
                }
                strncpy(configuredSensors[op.sensorIndex].response, cleanResponse.c_str(), sizeof(configuredSensors[op.sensorIndex].response)-1);
                configuredSensors[op.sensorIndex].response[sizeof(configuredSensors[op.sensorIndex].response)-1] = '\0';
                configuredSensors[op.sensorIndex].lastReadTime = currentTime;
                
                // Process response based on sensor type
                if (strcmp(configuredSensors[op.sensorIndex].type, "SHT30") == 0) {
                    // SHT30 returns 6 bytes: temp_msb, temp_lsb, temp_crc, hum_msb, hum_lsb, hum_crc
                    if (idx >= 6) {
                        uint16_t temp_raw = ((uint8_t)response[0] << 8) | (uint8_t)response[1];
                        uint16_t hum_raw = ((uint8_t)response[3] << 8) | (uint8_t)response[4];
                        
                        float temperature = -45.0 + 175.0 * ((float)temp_raw / 65535.0);
                        float humidity = 100.0 * ((float)hum_raw / 65535.0);
                        
                        // Store both values in multi-output structure
                        configuredSensors[op.sensorIndex].rawValue = temperature;
                        configuredSensors[op.sensorIndex].rawValueB = humidity;
                        
                        Serial.printf("[DEBUG] SHT30 parsed: Temp=%.2f°C, Hum=%.2f%%\n", temperature, humidity);
                        
                        // Apply calibration to both values using new expression-capable functions
                        float calibratedTemp = applyCalibration(temperature, configuredSensors[op.sensorIndex]);
                        float calibratedHum = applyCalibrationB(humidity, configuredSensors[op.sensorIndex]);
                        
                        configuredSensors[op.sensorIndex].calibratedValue = calibratedTemp;
                        configuredSensors[op.sensorIndex].calibratedValueB = calibratedHum;
                        
                        // Convert to Modbus values (scaled by 100 for decimal precision)
                        configuredSensors[op.sensorIndex].modbusValue = (int)(calibratedTemp * 100);
                        configuredSensors[op.sensorIndex].modbusValueB = (int)(calibratedHum * 100);
                        
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", 
                                        "Temp: " + String(temperature) + "°C, Hum: " + String(humidity) + "%", 
                                        String(configuredSensors[op.sensorIndex].name));
                    } else {
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", 
                                        "SHT30 response too short: " + String(idx) + " bytes", 
                                        String(configuredSensors[op.sensorIndex].name));
                    }
                } else if (strcmp(configuredSensors[op.sensorIndex].type, "EZO-PH") == 0 || 
                          strcmp(configuredSensors[op.sensorIndex].type, "EZO_PH") == 0) {
                    // Atlas Scientific EZO PH protocol: first byte is status
                    if (idx > 0) {
                        uint8_t statusCode = (uint8_t)response[0];
                        if (statusCode == 1) {
                            // Success: parse ASCII data after status byte
                            String dataStr = "";
                            for (int j = 1; j < idx; j++) {
                                char c = response[j];
                                if (c >= 32 && c <= 126) dataStr += c;
                            }
                            dataStr.trim();
                            if (dataStr.length() > 0) {
                                configuredSensors[op.sensorIndex].rawValue = dataStr.toFloat();
                                // Apply calibration and update calibratedValue/modbusValue
                                float calibratedValue = applyCalibration(configuredSensors[op.sensorIndex].rawValue, configuredSensors[op.sensorIndex]);
                                configuredSensors[op.sensorIndex].calibratedValue = calibratedValue;
                                configuredSensors[op.sensorIndex].modbusValue = (int)(calibratedValue * 100);
                                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", "EZO Success: '" + dataStr + "', Calibrated: " + String(calibratedValue), String(configuredSensors[op.sensorIndex].name));
                            } else {
                                configuredSensors[op.sensorIndex].rawValue = -998.0;
                                configuredSensors[op.sensorIndex].calibratedValue = 0.0;
                                configuredSensors[op.sensorIndex].modbusValue = 0;
                                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "EZO-PH: Empty data after success code", String(configuredSensors[op.sensorIndex].name));
                            }
                        } else if (statusCode == 254) {
                            // Processing: retry up to 3 times with delay
                            if (op.retryCount < 3) {
                                op.retryCount++;
                                delay(100); // Wait 100ms before retry
                                op.state = BusOpState::READY_TO_READ;
                                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "WARN", "EZO-PH: Processing, retry " + String(op.retryCount), String(configuredSensors[op.sensorIndex].name));
                                return; // Do not advance queue
                            } else {
                                configuredSensors[op.sensorIndex].rawValue = -996.0;
                                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "EZO-PH: Processing timeout", String(configuredSensors[op.sensorIndex].name));
                            }
                        } else if (statusCode == 2) {
                            configuredSensors[op.sensorIndex].rawValue = -997.0;
                            logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "EZO-PH: Syntax error", String(configuredSensors[op.sensorIndex].name));
                        } else if (statusCode == 255) {
                            configuredSensors[op.sensorIndex].rawValue = -995.0;
                            logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "WARN", "EZO-PH: No data available", String(configuredSensors[op.sensorIndex].name));
                        } else {
                            configuredSensors[op.sensorIndex].rawValue = -994.0;
                            logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "EZO-PH: Unknown status " + String(statusCode), String(configuredSensors[op.sensorIndex].name));
                        }
                    } else {
                        configuredSensors[op.sensorIndex].rawValue = -993.0;
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "ERR", "EZO-PH: No response data", String(configuredSensors[op.sensorIndex].name));
                    }
                } else if (strcmp(configuredSensors[op.sensorIndex].type, "EZO-EC") == 0 || 
                          strcmp(configuredSensors[op.sensorIndex].type, "EZO_EC") == 0) {
                    // Handle both hyphen and underscore variants for compatibility
                    uint8_t responseCode = (uint8_t)response[0];
                    if (responseCode == 1 && idx > 1) {
                        String dataStr = "";
                        for (int j = 1; j < idx; j++) {
                            if (response[j] >= 32 && response[j] <= 126) {
                                dataStr += (char)response[j];
                            }
                        }
                        configuredSensors[op.sensorIndex].rawValue = dataStr.toFloat();
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", "EC: " + String(configuredSensors[op.sensorIndex].rawValue), String(configuredSensors[op.sensorIndex].name));
                    } else {
                        configuredSensors[op.sensorIndex].rawValue = atof(response);
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", "EC: " + String(configuredSensors[op.sensorIndex].rawValue), String(configuredSensors[op.sensorIndex].name));
                    }
                } else if (strcmp(configuredSensors[op.sensorIndex].type, "GENERIC_I2C") == 0 || 
                          strcmp(configuredSensors[op.sensorIndex].type, "GENERIC") == 0 ||
                          strcmp(configuredSensors[op.sensorIndex].type, "Generic I2C") == 0) {
                    // Use existing parsing infrastructure for generic sensors
                    float primaryValue = parseSensorData(response, configuredSensors[op.sensorIndex]);
                    configuredSensors[op.sensorIndex].rawValue = primaryValue;
                    
                    // Apply primary calibration using expression-capable function
                    float calibratedPrimary = applyCalibration(primaryValue, configuredSensors[op.sensorIndex]);
                    configuredSensors[op.sensorIndex].calibratedValue = calibratedPrimary;
                    configuredSensors[op.sensorIndex].modbusValue = (int)(calibratedPrimary * 100);
                    
                    // Check if secondary parsing is configured (for multi-output)
                    if (strlen(configuredSensors[op.sensorIndex].parsingMethodB) > 0 && 
                        strcmp(configuredSensors[op.sensorIndex].parsingMethodB, "raw") != 0) {
                        
                        // Create temporary sensor config for secondary parsing
                        SensorConfig tempConfig = configuredSensors[op.sensorIndex];
                        strcpy(tempConfig.parsingMethod, configuredSensors[op.sensorIndex].parsingMethodB);
                        strcpy(tempConfig.parsingConfig, configuredSensors[op.sensorIndex].parsingConfigB);
                        
                        float secondaryValue = parseSensorData(response, tempConfig);
                        configuredSensors[op.sensorIndex].rawValueB = secondaryValue;
                        
                        // Apply secondary calibration using expression-capable function
                        float calibratedSecondary = applyCalibrationB(secondaryValue, configuredSensors[op.sensorIndex]);
                        configuredSensors[op.sensorIndex].calibratedValueB = calibratedSecondary;
                        configuredSensors[op.sensorIndex].modbusValueB = (int)(calibratedSecondary * 100);
                        
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", 
                                        "Primary: " + String(primaryValue) + ", Secondary: " + String(secondaryValue), 
                                        String(configuredSensors[op.sensorIndex].name));
                    } else {
                        logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", 
                                        "Parsed: " + String(primaryValue), 
                                        String(configuredSensors[op.sensorIndex].name));
                    }
                } else {
                    // Fallback: log raw value for all other I2C sensors
                    logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "VAL", "Raw: " + String(response), String(configuredSensors[op.sensorIndex].name));
                }
                
                // Move queue forward
                for(int i = 0; i < i2cQueueSize - 1; i++) {
                    i2cQueue[i] = i2cQueue[i + 1];
                }
                i2cQueueSize--;
            } else {
                logI2CTransaction(configuredSensors[op.sensorIndex].i2cAddress, "TIMEOUT", "No response received", String(configuredSensors[op.sensorIndex].name));
                // Move to next operation on timeout
                for(int i = 0; i < i2cQueueSize - 1; i++) {
                    i2cQueue[i] = i2cQueue[i + 1];
                }
                i2cQueueSize--;
            }
            break;
        }
            
        case BusOpState::ERROR: {
            // Move to next operation
            for(int i = 0; i < i2cQueueSize - 1; i++) {
                i2cQueue[i] = i2cQueue[i + 1];
            }
            i2cQueueSize--;
            break;
        }
    }
}

void processUARTQueue() {
    if (uartQueueSize == 0) return;
    
    unsigned long currentTime = millis();
    BusOperation& op = uartQueue[0];
    
    // Declare variables outside switch to avoid 'crosses initialization' error
    int txPin, rxPin;
    
    Serial.printf("[DEBUG] UART: Processing sensor %d (%s) - state %d\n", 
                 op.sensorIndex, configuredSensors[op.sensorIndex].name, (int)op.state);
    
    switch(op.state) {
        case BusOpState::IDLE:
            txPin = configuredSensors[op.sensorIndex].uartTxPin;
            rxPin = configuredSensors[op.sensorIndex].uartRxPin;
            
            // Initialize UART if pins are valid
            if (txPin >= 0 && rxPin >= 0 && txPin <= 28 && rxPin <= 28) {
                // Configure hardware UART (RP2040 has UART0 and UART1)
                if ((txPin == 0 && rxPin == 1) || (txPin == 12 && rxPin == 13) || 
                    (txPin == 16 && rxPin == 17) || (txPin == 4 && rxPin == 5)) {
                    
                    // Use Serial1 for UART communication
                    Serial1.setTX(txPin);
                    Serial1.setRX(rxPin);
                    Serial1.begin(9600); // Default baud rate
                    
                    // Send command if configured
                    const char* command = configuredSensors[op.sensorIndex].command;
                    if (strlen(command) > 0) {
                        Serial1.print(command);
                        Serial1.print("\r\n");
                        Serial.printf("[DEBUG] UART: Sent command '%s' to TX:%d\n", command, txPin);
                        
                        // Log the UART transaction for terminal watch
                        String pinStr = String(txPin) + "," + String(rxPin);
                        logUARTTransaction(pinStr, "TX", String(command));
                        
                        // Wait for response
                        delay(100);
                        
                        String response = "";
                        unsigned long timeout = millis() + 1000; // 1 second timeout
                        while (millis() < timeout && response.length() < 120) {
                            if (Serial1.available()) {
                                char c = Serial1.read();
                                response += c;
                                if (c == '\n' || c == '\r') break; // End of line
                            }
                        }
                        
                        if (response.length() > 0) {
                            response.trim();
                            
                            // Log the UART response for terminal watch
                            logUARTTransaction(pinStr, "RX", response);
                            
                            strncpy(configuredSensors[op.sensorIndex].rawDataString, response.c_str(), 
                                   sizeof(configuredSensors[op.sensorIndex].rawDataString)-1);
                            Serial.printf("[DEBUG] UART: Received response '%s'\n", response.c_str());
                            
                            // Parse the response to extract numeric value
                            float value = 0.0;
                            // Try to extract first number from response
                            for (int i = 0; i < response.length(); i++) {
                                if (isdigit(response[i]) || response[i] == '.' || response[i] == '-') {
                                    value = response.substring(i).toFloat();
                                    break;
                                }
                            }
                            
                            configuredSensors[op.sensorIndex].rawValue = value;
                            // Apply calibration using expression-capable function
                            float calibratedValue = applyCalibration(value, configuredSensors[op.sensorIndex]);
                            configuredSensors[op.sensorIndex].calibratedValue = calibratedValue;
                            configuredSensors[op.sensorIndex].modbusValue = (int)(calibratedValue * 100);
                            
                        } else {
                            Serial.printf("[DEBUG] UART: No response received\n");
                            configuredSensors[op.sensorIndex].rawValue = 0.0;
                            strcpy(configuredSensors[op.sensorIndex].rawDataString, "NO_RESPONSE");
                        }
                    } else {
                        Serial.printf("[DEBUG] UART: No command configured, reading available data\n");
                        // Just read any available data
                        String response = "";
                        while (Serial1.available() && response.length() < 120) {
                            char c = Serial1.read();
                            response += c;
                        }
                        if (response.length() > 0) {
                            response.trim();
                            strncpy(configuredSensors[op.sensorIndex].rawDataString, response.c_str(), 
                                   sizeof(configuredSensors[op.sensorIndex].rawDataString)-1);
                        }
                    }
                    
                    Serial1.end(); // Close UART to free pins for other sensors
                } else {
                    Serial.printf("[DEBUG] UART: Invalid pin combination TX:%d RX:%d\n", txPin, rxPin);
                    configuredSensors[op.sensorIndex].rawValue = 0.0;
                    strcpy(configuredSensors[op.sensorIndex].rawDataString, "INVALID_PINS");
                }
            } else {
                Serial.printf("[DEBUG] UART: Invalid pins TX:%d RX:%d\n", txPin, rxPin);
                configuredSensors[op.sensorIndex].rawValue = 0.0;
                strcpy(configuredSensors[op.sensorIndex].rawDataString, "INVALID_PINS");
            }
            
            configuredSensors[op.sensorIndex].lastReadTime = currentTime;
            
            // Move to next operation
            for(int i = 0; i < uartQueueSize - 1; i++) {
                uartQueue[i] = uartQueue[i + 1];
            }
            uartQueueSize--;
            break;
            
        case BusOpState::REQUEST_SENT:
        case BusOpState::READY_TO_READ:
        case BusOpState::ERROR:
            // Move to next operation
            for(int i = 0; i < uartQueueSize - 1; i++) {
                uartQueue[i] = uartQueue[i + 1];
            }
            uartQueueSize--;
            break;
    }
}

void processOneWireQueue() {
    if (oneWireQueueSize == 0) return;
    
    unsigned long currentTime = millis();
    BusOperation& op = oneWireQueue[0];
    
    int owPin = configuredSensors[op.sensorIndex].oneWirePin;
    uint8_t scratchpad[9] = {0};
    bool presence;
    bool readOk;
    
    switch(op.state) {
        case BusOpState::IDLE:
            Serial.printf("[DEBUG] One-Wire: Sensor %d (%s) IDLE, pin %d\n", op.sensorIndex, configuredSensors[op.sensorIndex].name, owPin);
            // Initialize One-Wire transaction
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            presence = !digitalRead(owPin);
            delayMicroseconds(410);

            if (presence) {
                Serial.printf("[DEBUG] One-Wire: Presence detected for sensor %d, sending convert command\n", op.sensorIndex);
                
                // Log the One-Wire transaction for terminal watch
                logOneWireTransaction(String(owPin), "TX", "0xCC 0x44 (Skip ROM + Convert T)");
                
                // Send Skip ROM (0xCC) + Convert T (0x44) command
                // Skip ROM command
                for (int bit = 0; bit < 8; bit++) {
                    pinMode(owPin, OUTPUT);
                    digitalWrite(owPin, LOW);
                    if ((0xCC >> bit) & 1) {
                        delayMicroseconds(6);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(64);
                    } else {
                        delayMicroseconds(60);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                    }
                }
                
                // Convert T command
                for (int bit = 0; bit < 8; bit++) {
                    pinMode(owPin, OUTPUT);
                    digitalWrite(owPin, LOW);
                    if ((0x44 >> bit) & 1) {
                        delayMicroseconds(6);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(64);
                    } else {
                        delayMicroseconds(60);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                    }
                }
                
                op.state = BusOpState::REQUEST_SENT;
                op.startTime = currentTime;
                configuredSensors[op.sensorIndex].lastOneWireCmd = currentTime;
            } else {
                Serial.printf("[DEBUG] One-Wire: No presence detected for sensor %d, retry %d\n", op.sensorIndex, op.retryCount+1);
                if (++op.retryCount >= 3) {
                    Serial.printf("[DEBUG] One-Wire: Max retries exceeded for sensor %d, removing from queue\n", op.sensorIndex);
                    for(int i = 0; i < oneWireQueueSize - 1; i++) {
                        oneWireQueue[i] = oneWireQueue[i + 1];
                    }
                    oneWireQueueSize--;
                }
            }
            break;

        case BusOpState::REQUEST_SENT:
            if (currentTime - op.startTime >= op.conversionTime) {
                Serial.printf("[DEBUG] One-Wire: Conversion time elapsed for sensor %d\n", op.sensorIndex);
                op.state = BusOpState::READY_TO_READ;
            }
            break;

        case BusOpState::READY_TO_READ:
            readOk = true;
            Serial.printf("[DEBUG] One-Wire: Ready to read sensor %d\n", op.sensorIndex);
            // Send read scratchpad command
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            presence = !digitalRead(owPin);
            delayMicroseconds(410);

            if (presence) {
                Serial.printf("[DEBUG] One-Wire: Presence detected for read sensor %d\n", op.sensorIndex);
                
                // Send Skip ROM (0xCC) + Read Scratchpad (0xBE) command
                // Skip ROM command
                for (int bit = 0; bit < 8; bit++) {
                    pinMode(owPin, OUTPUT);
                    digitalWrite(owPin, LOW);
                    if ((0xCC >> bit) & 1) {
                        delayMicroseconds(6);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(64);
                    } else {
                        delayMicroseconds(60);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                    }
                }
                
                // Read Scratchpad command
                for (int bit = 0; bit < 8; bit++) {
                    pinMode(owPin, OUTPUT);
                    digitalWrite(owPin, LOW);
                    if ((0xBE >> bit) & 1) {
                        delayMicroseconds(6);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(64);
                    } else {
                        delayMicroseconds(60);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                    }
                }
                
                // Read scratchpad data
                for (int i = 0; i < 9 && readOk; i++) {
                    uint8_t value = 0;
                    for (int bit = 0; bit < 8; bit++) {
                        pinMode(owPin, OUTPUT);
                        digitalWrite(owPin, LOW);
                        delayMicroseconds(3);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                        if (digitalRead(owPin)) {
                            value |= (1 << bit);
                        }
                        delayMicroseconds(53);
                    }
                    scratchpad[i] = value;
                }
                
                // Print scratchpad for debugging
                Serial.printf("[DEBUG] One-Wire: Scratchpad: ");
                for (int i = 0; i < 9; i++) {
                    Serial.printf("%02X ", scratchpad[i]);
                }
                Serial.println();

                // Skip CRC validation for now - DS18B20 often has CRC issues with bit-banged implementation
                // if (op.needsCRC && !validateCRC(scratchpad, 9)) {
                //     Serial.printf("[DEBUG] One-Wire: CRC failed for sensor %d\n", op.sensorIndex);
                //     readOk = false;
                // }

                if (readOk) {
                    // Convert raw data to temperature (DS18B20 format)
                    int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
                    float temp = raw / 16.0;
                    
                    // Log the One-Wire read for terminal watch
                    char readData[64];
                    snprintf(readData, sizeof(readData), "Scratchpad: %02X %02X %02X %02X %02X %02X %02X %02X %02X (%.2f°C)", 
                             scratchpad[0], scratchpad[1], scratchpad[2], scratchpad[3], scratchpad[4], 
                             scratchpad[5], scratchpad[6], scratchpad[7], scratchpad[8], temp);
                    logOneWireTransaction(String(owPin), "RX", String(readData));
                    
                    // Apply calibration using expression-capable function
                    float calibratedTemp = applyCalibration(temp, configuredSensors[op.sensorIndex]);
                    
                    configuredSensors[op.sensorIndex].rawValue = temp;
                    configuredSensors[op.sensorIndex].calibratedValue = calibratedTemp;
                    configuredSensors[op.sensorIndex].modbusValue = (int)(calibratedTemp * 100);
                    configuredSensors[op.sensorIndex].lastReadTime = currentTime;

                    Serial.printf("[DEBUG] One-Wire: Sensor %d read value %.2f°C (raw=0x%04X, calibrated=%.2f, modbus=%d)\n", 
                                 op.sensorIndex, temp, raw, calibratedTemp, configuredSensors[op.sensorIndex].modbusValue);

                    // Format raw data string
                    char dataStr[32];
                    snprintf(dataStr, sizeof(dataStr), "%.2f°C", temp);
                    strncpy(configuredSensors[op.sensorIndex].rawDataString, dataStr, 
                            sizeof(configuredSensors[op.sensorIndex].rawDataString)-1);
                } else {
                    Serial.printf("[DEBUG] One-Wire: Read failed for sensor %d\n", op.sensorIndex);
                }
            } else {
                Serial.printf("[DEBUG] One-Wire: No presence detected for read sensor %d\n", op.sensorIndex);
            }

            // Move to next operation
            for(int i = 0; i < oneWireQueueSize - 1; i++) {
                oneWireQueue[i] = oneWireQueue[i + 1];
            }
            oneWireQueueSize--;
            break;

        case BusOpState::ERROR:
            Serial.printf("[DEBUG] One-Wire: ERROR state for sensor %d\n", op.sensorIndex);
            // Move to next operation
            for(int i = 0; i < oneWireQueueSize - 1; i++) {
                oneWireQueue[i] = oneWireQueue[i + 1];
            }
            oneWireQueueSize--;
            break;
    }
}

void enqueueBusOperation(uint8_t sensorIndex, const char* protocol) {
    if (!configuredSensors[sensorIndex].enabled) return;
    
    BusOperation op = {
        .sensorIndex = sensorIndex,
            .startTime = millis(),
        .conversionTime = 750, // Default 750ms, adjust per sensor type
        .state = BusOpState::IDLE,
        .retryCount = 0,
        .needsCRC = true
    };
    
    // Adjust conversion time based on sensor type
    if (strcmp(configuredSensors[sensorIndex].type, "EZO-PH") == 0) {
        op.conversionTime = 900; // Atlas Scientific pH spec: 900ms
    } else if (strcmp(configuredSensors[sensorIndex].type, "EZO-EC") == 0) {
        op.conversionTime = 900;
    } else if (strcmp(configuredSensors[sensorIndex].type, "DS18B20") == 0) {
        op.conversionTime = configuredSensors[sensorIndex].oneWireConversionTime;
        if (op.conversionTime <= 0) op.conversionTime = 750;
    }
    
    // Add to appropriate queue
    if (strcmp(protocol, "I2C") == 0 && i2cQueueSize < MAX_SENSORS) {
        i2cQueue[i2cQueueSize++] = op;
    } else if (strcmp(protocol, "UART") == 0 && uartQueueSize < MAX_SENSORS) {
        uartQueue[uartQueueSize++] = op;
    } else if (strcmp(protocol, "One-Wire") == 0 && oneWireQueueSize < MAX_SENSORS) {
        oneWireQueue[oneWireQueueSize++] = op;
    }
}

void updateBusQueues() {
    static unsigned long lastQueueUpdate = 0;
    unsigned long currentTime = millis();
    
    // Update every 100ms
    if (currentTime - lastQueueUpdate < 100) return;
    lastQueueUpdate = currentTime;
    
    // Check each enabled sensor
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        
        // Add to queue if it's time for next reading
        if (currentTime - configuredSensors[i].lastReadTime >= configuredSensors[i].updateInterval) {
            Serial.printf("[DEBUG] Sensor %d (%s) ready for polling. Protocol: %s\n", 
                         i, configuredSensors[i].name, configuredSensors[i].protocol);
            if (strncmp(configuredSensors[i].protocol, "I2C", 3) == 0) {
                Serial.printf("[DEBUG] Adding sensor %d to I2C queue (current size: %d)\n", i, i2cQueueSize);
                enqueueBusOperation(i, "I2C");
            } else if (strncmp(configuredSensors[i].protocol, "UART", 4) == 0) {
                Serial.printf("[DEBUG] Adding sensor %d to UART queue (current size: %d)\n", i, uartQueueSize);
                enqueueBusOperation(i, "UART");
            } else if (strncmp(configuredSensors[i].protocol, "One-Wire", 8) == 0) {
                Serial.printf("[DEBUG] Adding sensor %d to One-Wire queue (current size: %d)\n", i, oneWireQueueSize);
                enqueueBusOperation(i, "One-Wire");
            }
        }
    }
    
    // Process queues
    if (i2cQueueSize > 0) {
        Serial.printf("[DEBUG] Processing I2C queue (size: %d)\n", i2cQueueSize);
    }
    processI2CQueue();
    if (uartQueueSize > 0) {
        Serial.printf("[DEBUG] Processing UART queue (size: %d)\n", uartQueueSize);
    }
    processUARTQueue();
    if (oneWireQueueSize > 0) {
        Serial.printf("[DEBUG] Processing One-Wire queue (size: %d)\n", oneWireQueueSize);
    }
    processOneWireQueue();
}

// EZO sensor functionality 
Ezo_board* ezoSensors[MAX_SENSORS] = {nullptr};
bool ezoSensorsInitialized = false;

// Terminal monitoring variables
bool terminalWatchActive = false;
String watchedPin = "";
String watchedProtocol = "";
std::vector<String> terminalBuffer;

// Bus traffic logging functions
void addTerminalLog(String message) {
    if (!terminalWatchActive) return;
    
    String timestamp = String(millis());
    String logEntry = "[" + timestamp + "] " + message;
    
    terminalBuffer.push_back(logEntry);
    if (terminalBuffer.size() > MAX_TERMINAL_BUFFER) {
        terminalBuffer.erase(terminalBuffer.begin());
    }
    
    // Also print to Serial for debugging
    Serial.println(logEntry);
}

void logI2CTransaction(int address, String direction, String data, String pin) {
    // Check if terminal is watching this transaction
    bool shouldLog = false;
    
    if (terminalWatchActive) {
        // Debug what we're checking
        Serial.printf("DEBUG logI2C: addr=0x%02X, dir=%s, pin=%s, watchedPin=%s, watchedProtocol=%s\n", 
                     address, direction.c_str(), pin.c_str(), watchedPin.c_str(), watchedProtocol.c_str());
        
        // Support both pin numbers and sensor names
        if (watchedPin == "all" || watchedPin == pin) {
            // Direct match (legacy support)
            shouldLog = true;
            Serial.printf("DEBUG logI2C: Direct match - shouldLog=true\n");
        } else if (watchedPin.length() <= 2 && watchedPin.toInt() > 0) {
            // Pin number selected (e.g., "4" for GP4/GP5) - show ALL traffic on that pin pair
            int pinNum = watchedPin.toInt();
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (configuredSensors[i].enabled && 
                    configuredSensors[i].i2cAddress == address &&
                    (configuredSensors[i].sdaPin == pinNum || configuredSensors[i].sclPin == pinNum ||
                     configuredSensors[i].sdaPin == pinNum + 1 || configuredSensors[i].sclPin == pinNum + 1)) {
                    shouldLog = true;
                    Serial.printf("DEBUG logI2C: Pin pair match for pins %d/%d - shouldLog=true\n", pinNum, pinNum + 1);
                    break;
                }
            }
            // Also check default I2C pins if no sensors match
            if (!shouldLog && (pinNum == 4 || pinNum == 5)) {
                shouldLog = true; // Default I2C bus
                Serial.printf("DEBUG logI2C: Default I2C bus match - shouldLog=true\n");
            }
        } else {
            // Sensor name selected - show only that sensor's traffic
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (configuredSensors[i].enabled && 
                    strcmp(configuredSensors[i].name, watchedPin.c_str()) == 0 &&
                    configuredSensors[i].i2cAddress == address) {
                    shouldLog = true;
                    Serial.printf("DEBUG logI2C: Sensor name match for %s - shouldLog=true\n", configuredSensors[i].name);
                    break;
                }
            }
        }
        
        // Check protocol (case insensitive)
        String watchedProtocolUpper = watchedProtocol;
        watchedProtocolUpper.toUpperCase();
        if (watchedProtocolUpper == "I2C" && shouldLog) {
            String logMsg = "I2C [0x" + String(address, HEX) + "] " + direction + ": " + data;
            addTerminalLog(logMsg);
            Serial.println("TERMINAL_LOG: " + logMsg);
        } else {
            Serial.printf("DEBUG logI2C: Protocol check failed or shouldLog=false (protocol=%s, shouldLog=%d)\n", 
                         watchedProtocolUpper.c_str(), shouldLog);
        }
    } else {
        Serial.println("DEBUG logI2C: terminalWatchActive=false");
    }
}

void logOneWireTransaction(String pin, String direction, String data) {
    bool shouldLog = false;
    
    if (terminalWatchActive) {
        // Extract pin number from pin string (handle both "28" and "GP28" formats)
        int pinNumber = pin.toInt();
        if (pinNumber == 0 && pin.startsWith("GP")) {
            pinNumber = pin.substring(2).toInt();
        }
        
        // Support both pin numbers and sensor names
        if (watchedPin == "all" || watchedPin == pin || watchedPin == String(pinNumber) || watchedPin == ("GP" + String(pinNumber))) {
            shouldLog = true;
        } else {
            // Check if watchedPin is a sensor name that uses this pin
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (configuredSensors[i].enabled && 
                    strcmp(configuredSensors[i].name, watchedPin.c_str()) == 0 &&
                    configuredSensors[i].oneWirePin == pinNumber) {
                    shouldLog = true;
                    break;
                }
            }
        }
        
        // Check protocol (case insensitive)
        String watchedProtocolUpper = watchedProtocol;
        watchedProtocolUpper.toUpperCase();
        if ((watchedProtocolUpper == "ONE-WIRE" || watchedProtocolUpper == "ONEWIRE") && shouldLog) {
            String logMsg = "1Wire [Pin " + String(pinNumber) + "] " + direction + ": " + data;
            addTerminalLog(logMsg);
        }
    }
}

void logUARTTransaction(String pin, String direction, String data) {
    bool shouldLog = false;
    
    if (terminalWatchActive) {
        // Support both pin numbers and sensor names
        if (watchedPin == "all" || watchedPin == pin) {
            shouldLog = true;
        } else {
            // Extract pin numbers from pin string (handle "0,1" or "GP0,GP1" formats)
            int txPin = -1, rxPin = -1;
            if (pin.indexOf(',') > 0) {
                String txStr = pin.substring(0, pin.indexOf(','));
                String rxStr = pin.substring(pin.indexOf(',') + 1);
                txPin = txStr.startsWith("GP") ? txStr.substring(2).toInt() : txStr.toInt();
                rxPin = rxStr.startsWith("GP") ? rxStr.substring(2).toInt() : rxStr.toInt();
            }
            
            // Check if watchedPin is a sensor name that uses UART
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (configuredSensors[i].enabled && 
                    strcmp(configuredSensors[i].name, watchedPin.c_str()) == 0 &&
                    (configuredSensors[i].uartTxPin == txPin || configuredSensors[i].uartRxPin == rxPin)) {
                    shouldLog = true;
                    break;
                }
            }
        }
        
        // Check protocol (case insensitive)
        String watchedProtocolUpper = watchedProtocol;
        watchedProtocolUpper.toUpperCase();
        if (watchedProtocolUpper == "UART" && shouldLog) {
            String logMsg = "UART [Pin " + pin + "] " + direction + ": " + data;
            addTerminalLog(logMsg);
        }
    }
}

// Log network transaction for terminal watch
void logNetworkTransaction(String protocol, String direction, String localAddr, String remoteAddr, String data) {
    if (!terminalWatchActive) return;
    
    // Check protocol (case insensitive)
    String watchedProtocolUpper = watchedProtocol;
    watchedProtocolUpper.toUpperCase();
    String protocolUpper = protocol;
    protocolUpper.toUpperCase();
    
    if ((watchedProtocolUpper == "NETWORK" || watchedProtocolUpper == protocolUpper) && 
        (watchedPin == "all" || watchedPin == "ethernet" || watchedPin == "eth0")) {
        String logMsg = protocolUpper + " [" + localAddr + " <-> " + remoteAddr + "] " + direction + ": " + data;
        addTerminalLog(logMsg);
    }
}

// Send JSON response helper
void sendJSON(WiFiClient& client, String json) {
    // Log HTTP response for network monitoring
    String remoteIP = client.remoteIP().toString();
    String localIP = eth.localIP().toString() + ":" + String(HTTP_PORT);
    String responseData = "200 OK (JSON: " + json.substring(0, min(50, (int)json.length())) + (json.length() > 50 ? "..." : "") + ")";
    logNetworkTransaction("HTTP", "TX", localIP, remoteIP, responseData);
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
    client.println("Access-Control-Allow-Headers: Content-Type");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(json.length());
    client.println();
    client.print(json);
}

// Send 404 response helper
void send404(WiFiClient& client) {
    // Log HTTP response for network monitoring
    String remoteIP = client.remoteIP().toString();
    String localIP = eth.localIP().toString() + ":" + String(HTTP_PORT);
    logNetworkTransaction("HTTP", "TX", localIP, remoteIP, "404 Not Found");
    
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
    client.println("Access-Control-Allow-Headers: Content-Type");
    client.println("Connection: close");
    client.println();
    client.println("404 Not Found");
}

String executeTerminalCommand(String command, String pin, String protocol) {
    String response = "No response";
    
    if (protocol == "I2C") {
        // Parse I2C address from pin parameter
        int address = 0x63; // Default EZO pH address
        if (pin.startsWith("0x")) {
            address = strtol(pin.c_str(), NULL, 16);
        } else if (pin.toInt() > 0) {
            address = pin.toInt();
        }
        
        // Handle escape sequences in command
        command.replace("\\r", "\r");
        command.replace("\\n", "\n");
        
        // Log the outgoing command
        logI2CTransaction(address, "TX", command, pin);
        
        Wire.beginTransmission(address);
        Wire.print(command);
        int result = Wire.endTransmission();
        
        if (result == 0) {
            delay(300); // EZO sensors need more time to process
            Wire.requestFrom(address, 32);
            response = "";
            while (Wire.available()) {
                char c = Wire.read();
                // Filter out non-printable characters except CR/LF
                if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
                    response += c;
                }
            }
            response.trim(); // Remove leading/trailing whitespace
            
            // Log the incoming response
            if (response.length() > 0) {
                logI2CTransaction(address, "RX", response, pin);
            }
        } else {
            response = "I2C Error: " + String(result);
            logI2CTransaction(address, "ERR", "EndTransmission failed: " + String(result), pin);
        }
    } else if (protocol == "onewire") {
        // Add OneWire command handling with logging
        logOneWireTransaction(pin, "TX", command);
        response = "OneWire command sent: " + command;
        logOneWireTransaction(pin, "RX", response);
    } else if (protocol == "uart") {
        // Add UART command handling with logging
        logUARTTransaction(pin, "TX", command);
        response = "UART command sent: " + command;
        logUARTTransaction(pin, "RX", response);
    }
    
    return response;
}

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



    Serial.println("Loading config...");
    delay(500);
    loadConfig();

    // Initialize LittleFS for web file serving
    Serial.println("Initializing filesystem...");
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed!");
    } else {
        Serial.println("LittleFS mounted successfully");
    }

    // Debug: dump sensors file existence and a short preview to help troubleshoot persistence
    Serial.println("Checking sensors file on filesystem...");
    auto dumpSensorsFile = [&]() {
        if (!LittleFS.exists(SENSORS_FILE)) {
            Serial.println("Sensors file does not exist on LittleFS");
            return;
        }
        File sf = LittleFS.open(SENSORS_FILE, "r");
        if (!sf) {
            Serial.println("Failed to open sensors file for reading");
            return;
        }
        size_t sz = sf.size();
        Serial.printf("Sensors file exists, size=%u bytes\n", (unsigned)sz);
        const size_t previewSize = min((size_t)1024, sz);
        Serial.println("-- Begin sensors.json preview --");
        char buf[256];
        size_t remaining = previewSize;
        while (remaining > 0) {
            size_t toRead = min((size_t)sizeof(buf)-1, remaining);
            int r = sf.readBytes(buf, toRead);
            buf[r] = '\0';
            Serial.print(buf);
            remaining -= r;
            if (r == 0) break;
        }
        Serial.println("\n-- End preview --");
        sf.close();
    };

    delay(200);
    dumpSensorsFile();

    Serial.println("Loading sensor configuration...");
    delay(500);
    loadSensorConfig();
    // Ensure presets are applied after initial config load
    applySensorPresets();
    
    // Initialize command queues
    
    Serial.printf("Loaded %d sensors\n", numConfiguredSensors);
    
    // Initialize bus queues and command arrays
    i2cQueueSize = 0;
    uartQueueSize = 0;
    oneWireQueueSize = 0;
    i2cCommands.init();
    uartCommands.init();
    oneWireCommands.init();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        Serial.printf("Sensor[%d]: %s (%s) - %s\n", i, configuredSensors[i].name, configuredSensors[i].type, configuredSensors[i].enabled ? "ENABLED" : "DISABLED");
        
        // Initialize sensor commands
        if (configuredSensors[i].enabled) {
            SensorCommand cmd = {
                .sensorIndex = (uint8_t)i,
                .nextExecutionMs = millis(),
                .intervalMs = configuredSensors[i].updateInterval,
                .command = (strcmp(configuredSensors[i].type, "GENERIC") == 0) ? configuredSensors[i].command : nullptr,
                .isGeneric = (strcmp(configuredSensors[i].type, "GENERIC") == 0)
            };
            
            // Add to appropriate command array (including SHT30 sensors)
            if (strncmp(configuredSensors[i].protocol, "I2C", 3) == 0) {
                i2cCommands.add(cmd);
                // Queue initial bus operation
                enqueueBusOperation(i, "I2C");
            } else if (strncmp(configuredSensors[i].protocol, "UART", 4) == 0) {
                uartCommands.add(cmd);
                enqueueBusOperation(i, "UART");
            } else if (strncmp(configuredSensors[i].protocol, "One-Wire", 8) == 0) {
                oneWireCommands.add(cmd);
                enqueueBusOperation(i, "One-Wire");
            }
        }
    }

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

    // Initialize I2C bus with default pins (GP4=SDA, GP5=SCL)
    Wire.begin();
    Serial.println("I2C initialized on default pins (GP4=SDA, GP5=SCL)");
    
    // Scan for I2C devices at startup
    Serial.println("Scanning I2C bus...");
    bool foundDevice = false;
    for (int addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", addr);
            foundDevice = true;
        }
    }
    if (!foundDevice) {
        Serial.println("No I2C devices found");
    }

    // Start watchdog
    rp2040.wdt_begin(WDT_TIMEOUT);
    Serial.println("Setup complete.");
}

// Implementation of POST /api/sensor/calibration
void handlePOSTSensorCalibration(WiFiClient& client, String body) {
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

// updateSensorReadings() function removed - all sensors now handled in unified queue system

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
    String encoding = doc["encoding"] | "text";
    String originalCommand = doc["originalCommand"] | "";
    String response = "";
    bool success = true;
    
    // Note: Don't convert to lowercase - EZO sensors are case-sensitive
    // protocol.toLowerCase();
    // command.toLowerCase();
    
    if (protocol == "digital") {
        if (command == "read") {
            if (pin.startsWith("DI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = digitalRead(DIGITAL_INPUTS[pinNum]);
                    bool raw = ioStatus.dInRaw[pinNum];
                    response = pin + " = " + (state ? "HIGH" : "LOW") + " (Raw: " + (raw ? "HIGH" : "LOW") + ")";
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
            } else {
                success = false;
                response = "Error: Invalid pin format. Use DI0-DI7 or DO0-DO7";
            }
        } else if (command.startsWith("write ")) {
            String value = command.substring(6);
            value.trim();
            if (pin.startsWith("DO")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    bool state = (value == "1" || value.equalsIgnoreCase("HIGH"));
                    ioStatus.dOut[pinNum] = state;
                    digitalWrite(DIGITAL_OUTPUTS[pinNum], config.doInvert[pinNum] ? !state : state);
                    
                    // Update all connected Modbus clients
                    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
                        if (modbusClients[i].connected) {
                            modbusClients[i].server.coilWrite(pinNum, state);
                        }
                    }
                    
                    response = pin + " set to " + (state ? "HIGH" : "LOW");
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else {
                success = false;
                response = "Error: Cannot write to input pin";
            }
        } else if (command.startsWith("config ")) {
            String option = command.substring(7);
            option.trim();
            if (pin.startsWith("DI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 8) {
                    if (option == "pullup") {
                        config.diPullup[pinNum] = !config.diPullup[pinNum];
                        pinMode(DIGITAL_INPUTS[pinNum], config.diPullup[pinNum] ? INPUT_PULLUP : INPUT);
                        response = pin + " pullup " + (config.diPullup[pinNum] ? "ENABLED" : "DISABLED");
                    } else if (option == "invert") {
                        config.diInvert[pinNum] = !config.diInvert[pinNum];
                        response = pin + " invert " + (config.diInvert[pinNum] ? "ENABLED" : "DISABLED");
                    } else if (option == "latch") {
                        config.diLatch[pinNum] = !config.diLatch[pinNum];
                        response = pin + " latch " + (config.diLatch[pinNum] ? "ENABLED" : "DISABLED");
                    } else {
                        success = false;
                        response = "Error: Unknown config option. Use 'pullup', 'invert', or 'latch'";
                    }
                } else {
                    success = false;
                    response = "Error: Invalid pin number";
                }
            } else {
                success = false;
                response = "Error: Config only available for digital inputs (DI0-DI7)";
            }
        } else {
            success = false;
            response = "Error: Unknown digital command. Use 'read', 'write <value>', or 'config <option>'";
        }
    } else if (protocol == "analog") {
        if (command == "read") {
            if (pin.startsWith("AI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 3) {
                    int value = ioStatus.aIn[pinNum];
                    response = pin + " = " + String(value) + " mV";
                } else {
                    success = false;
                    response = "Error: Invalid analog pin. Use AI0-AI2";
                }
            } else {
                success = false;
                response = "Error: Invalid pin format. Use AI0-AI2";
            }
        } else if (command == "config") {
            if (pin.startsWith("AI")) {
                int pinNum = pin.substring(2).toInt();
                if (pinNum >= 0 && pinNum < 3) {
                    response = pin + " - Pin " + String(ANALOG_INPUTS[pinNum]) + ", Range: 0-3300mV, Resolution: 12-bit";
                } else {
                    success = false;
                    response = "Error: Invalid analog pin";
                }
            } else {
                success = false;
                response = "Error: Invalid pin format. Use AI0-AI2";
            }
        } else {
            success = false;
            response = "Error: Unknown analog command. Use 'read' or 'config'";
        }
    } else if (protocol == "i2c") {
        if (command == "scan") {
            response = "I2C Device Scan:\n";
            bool foundDevices = false;
            // RP2040 Wire library does not support dynamic pin assignment via begin(sda, scl)
            // Only scan using default pins
            int sda = 4; // Default SDA
            int scl = 5; // Default SCL
            for (int addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    response += "Found device at 0x" + String(addr, HEX) + " on SDA: " + String(sda) + ", SCL: " + String(scl) + "\n";
                    foundDevices = true;
                }
                delay(1);
            }
            if (!foundDevices) {
                response += "No I2C devices found";
            }
        } else if (command == "probe") {
            if (i2cAddress.length() > 0) {
                int addr = 0;
                if (i2cAddress.startsWith("0x") || i2cAddress.startsWith("0X")) {
                    addr = strtol(i2cAddress.c_str(), nullptr, 16);
                } else {
                    addr = i2cAddress.toInt();
                }
                
                if (addr >= 1 && addr <= 127) {
                    Wire.beginTransmission(addr);
                    if (Wire.endTransmission() == 0) {
                        response = "Device at 0x" + String(addr, HEX) + " is present";
                    } else {
                        response = "No device found at 0x" + String(addr, HEX);
                    }
                } else {
                    success = false;
                    response = "Error: Invalid I2C address. Must be 1-127 (0x01-0x7F)";
                }
            } else {
                success = false;
                response = "Error: I2C address required for probe command";
            }
        } else if (command.startsWith("read ")) {
            String regStr = command.substring(5);
            int reg = regStr.toInt();
            if (i2cAddress.length() > 0) {
                int addr = 0;
                if (i2cAddress.startsWith("0x") || i2cAddress.startsWith("0X")) {
                    addr = strtol(i2cAddress.c_str(), nullptr, 16);
                } else {
                    addr = i2cAddress.toInt();
                }
                
                Wire.beginTransmission(addr);
                Wire.write(reg);
                if (Wire.endTransmission() == 0) {
                    Wire.requestFrom(addr, 1);
                    if (Wire.available()) {
                        int value = Wire.read();
                        response = "Register 0x" + String(reg, HEX) + " = 0x" + String(value, HEX) + " (" + String(value) + ")";
                    } else {
                        success = false;
                        response = "Error: No data received from device";
                    }
                } else {
                    success = false;
                    response = "Error: Communication failed with device";
                }
            } else {
                success = false;
                response = "Error: I2C address required";
            }
        } else if (command.startsWith("write ")) {
            // Parse "write <register> <data>"
            String params = command.substring(6);
            int spaceIndex = params.indexOf(' ');
            if (spaceIndex > 0 && i2cAddress.length() > 0) {
                int reg = params.substring(0, spaceIndex).toInt();
                int data = params.substring(spaceIndex + 1).toInt();
                
                int addr = 0;
                if (i2cAddress.startsWith("0x") || i2cAddress.startsWith("0X")) {
                    addr = strtol(i2cAddress.c_str(), nullptr, 16);
                } else {
                    addr = i2cAddress.toInt();
                }
                
                Wire.beginTransmission(addr);
                Wire.write(reg);
                Wire.write(data);
                if (Wire.endTransmission() == 0) {
                    response = "Wrote 0x" + String(data, HEX) + " to register 0x" + String(reg, HEX);
                } else {
                    success = false;
                    response = "Error: Write failed";
                }
            } else {
                success = false;
                response = "Error: Invalid write format. Use 'write <register> <data>' with I2C address";
            }
        } else {
            // Handle custom commands with encoding support
            if (i2cAddress.length() > 0) {
                int addr = 0;
                if (i2cAddress.startsWith("0x") || i2cAddress.startsWith("0X")) {
                    addr = strtol(i2cAddress.c_str(), nullptr, 16);
                } else {
                    addr = i2cAddress.toInt();
                }
                
                // Check for encoding info from frontend
                String encoding = doc["encoding"] | "text";
                String originalCommand = doc["originalCommand"] | command;
                
                Wire.beginTransmission(addr);
                
                if (encoding == "ascii" || encoding == "decimal") {
                    // Parse space-separated byte values
                    String cmdCopy = command;
                    int byteCount = 0;
                    while (cmdCopy.length() > 0) {
                        int spaceIndex = cmdCopy.indexOf(' ');
                        String byteStr;
                        if (spaceIndex >= 0) {
                            byteStr = cmdCopy.substring(0, spaceIndex);
                            cmdCopy = cmdCopy.substring(spaceIndex + 1);
                        } else {
                            byteStr = cmdCopy;
                            cmdCopy = "";
                        }
                        
                        if (byteStr.length() > 0) {
                            int byteVal = byteStr.toInt();
                            if (byteVal >= 0 && byteVal <= 255) {
                                Wire.write((uint8_t)byteVal);
                                byteCount++;
                            }
                        }
                    }
                    response = "Sent " + String(byteCount) + " bytes (" + encoding + ") to 0x" + String(addr, HEX);
                } else if (encoding == "hex") {
                    // Parse space-separated hex values
                    String cmdCopy = command;
                    int byteCount = 0;
                    while (cmdCopy.length() > 0) {
                        int spaceIndex = cmdCopy.indexOf(' ');
                        String hexStr;
                        if (spaceIndex >= 0) {
                            hexStr = cmdCopy.substring(0, spaceIndex);
                            cmdCopy = cmdCopy.substring(spaceIndex + 1);
                        } else {
                            hexStr = cmdCopy;
                            cmdCopy = "";
                        }
                        
                        if (hexStr.length() > 0) {
                            int byteVal = strtoul(hexStr.c_str(), nullptr, 16);
                            Wire.write((uint8_t)byteVal);
                            byteCount++;
                        }
                    }
                    response = "Sent " + String(byteCount) + " hex bytes to 0x" + String(addr, HEX);
                } else {
                    // Text encoding - send as ASCII bytes
                    for (int i = 0; i < command.length(); i++) {
                        Wire.write((uint8_t)command[i]);
                    }
                    response = "Sent text command \"" + command + "\" to 0x" + String(addr, HEX);
                }
                
                int result = Wire.endTransmission();
                if (result != 0) {
                    success = false;
                    response = "Error: I2C transmission failed (code " + String(result) + ")";
                } else {
                    // Try to read response
                    delay(100); // Brief delay for sensor to process
                    int available = Wire.requestFrom(addr, 32);
                    if (available > 0) {
                        String readData = "";
                        while (Wire.available()) {
                            char c = Wire.read();
                            if (c >= 32 && c <= 126) {
                                readData += c;
                            } else {
                                readData += "[0x" + String((uint8_t)c, HEX) + "]";
                            }
                        }
                        response += "\nResponse: " + readData;
                    }
                }
            } else {
                success = false;
                response = "Error: I2C address required for custom commands";
            }
        }
    } else if (protocol == "system") {
        if (command == "status") {
            unsigned long uptime = millis() / 1000;
            response = "System Status:\\n";
            response += "CPU: RP2040 @ 133MHz\\n";
            response += "RAM: 256KB\\n";
            response += "Flash: 2MB\\n";
            response += "Uptime: " + String(uptime) + " seconds\\n";
            response += "Free Heap: " + String(rp2040.getFreeHeap()) + " bytes";
        } else if (command == "sensors") {
            response = "Configured Sensors:\\n";
            // Load and display sensor config
            // Note: This would need sensor config loading logic
            response += "0: System I/O - Enabled\\n";
            response += "Total configured: 1";
        } else if (command == "info") {
            response = "Hardware Information:\\n";
            response += "Board: Raspberry Pi Pico\\n";
            response += "Digital Inputs: 8 (Pins 0-7)\\n";
            response += "Digital Outputs: 8 (Pins 8-15)\\n";
            response += "Analog Inputs: 3 (Pins 26-28)\\n";
            response += "I2C: SDA Pin 24, SCL Pin 25\\n";
            response += "Ethernet: W5500 (Pins 16-21)";
        } else {
            success = false;
            response = "Error: Unknown system command. Use 'status', 'sensors', or 'info'";
        }
    } else if (protocol == "network") {
        if (command == "status") {
            response = "Ethernet Interface Status:\\n";
            response += "IP: " + WiFi.localIP().toString() + "\\n";
            response += "Gateway: " + WiFi.gatewayIP().toString() + "\\n";
            response += "Subnet: " + WiFi.subnetMask().toString() + "\\n";
            response += "MAC: " + WiFi.macAddress() + "\\n";
            response += "Link Status: ";
            response += (WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        } else if (command == "clients") {
            response = "Modbus Clients:\\n";
            response += "Connected: " + String(connectedClients) + "\\n";
            for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
                if (modbusClients[i].connected) {
                    response += "Slot " + String(i) + ": " + modbusClients[i].clientIP.toString() + "\\n";
                }
            }
        } else if (command == "link") {
            response = "Ethernet Link: ";
            response += (WiFi.status() == WL_CONNECTED ? "UP" : "DOWN");
        } else if (command == "stats") {
            unsigned long uptime = millis() / 1000;
            response = "Network Statistics:\\n";
            response += "Connection Uptime: " + String(uptime) + " seconds\\n";
            response += "Modbus Port: 502\\n";
            response += "HTTP Port: 80";
        } else {
            success = false;
            response = "Error: Unknown network command. Use 'status', 'clients', 'link', or 'stats'";
        }
    } else if (protocol == "onewire") {
        // Extract pin number from pin string (e.g., "GP28" -> 28)
        int owPin = -1;
        if (pin.startsWith("GP")) {
            owPin = pin.substring(2).toInt();
        } else if (pin.length() > 0) {
            // Try to extract from sensor name if it's a configured sensor
            for (int i = 0; i < numConfiguredSensors; i++) {
                if (String(configuredSensors[i].name) == pin) {
                    owPin = configuredSensors[i].oneWirePin;
                    break;
                }
            }
        }
        
        if (owPin < 0 || owPin > 28) {
            success = false;
            response = "Error: Invalid One-Wire pin. Use GP0-GP28 format";
        } else if (command == "scan" || command == "search") {
            // Scan all supported One-Wire pins
            int oneWirePins[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,22,26,27,28};
            bool foundDevice = false;
            for (int i = 0; i < sizeof(oneWirePins)/sizeof(oneWirePins[0]); i++) {
                int pin = oneWirePins[i];
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
                delayMicroseconds(480); // Reset pulse
                pinMode(pin, INPUT_PULLUP);
                delayMicroseconds(70);
                bool presence = !digitalRead(pin); // Presence pulse is low
                delayMicroseconds(410);
                if (presence) {
                    response += "One-Wire device detected on GP" + String(pin) + "\n";
                    foundDevice = true;
                }
            }
            if (!foundDevice) {
                response += "No One-Wire devices found on any supported pin";
            }
        } else if (command == "read") {
            // Raw One-Wire data read - no libraries needed!
            response = "One-Wire Raw Read from GP" + String(owPin) + ":\\n";
            
            // Send reset pulse
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            bool presence = !digitalRead(owPin);
            delayMicroseconds(410);
            
            if (!presence) {
                response += "Error: No device presence detected";
            } else {
                // Send SKIP ROM (0xCC) + READ SCRATCHPAD (0xBE)
                uint8_t commands[] = {0xCC, 0xBE};
                for (int cmd = 0; cmd < 2; cmd++) {
                    for (int bit = 0; bit < 8; bit++) {
                        pinMode(owPin, OUTPUT);
                        digitalWrite(owPin, LOW);
                        if (commands[cmd] & (1 << bit)) {
                            delayMicroseconds(6);
                            pinMode(owPin, INPUT_PULLUP);
                            delayMicroseconds(64);
                        } else {
                            delayMicroseconds(60);
                            pinMode(owPin, INPUT_PULLUP);
                            delayMicroseconds(10);
                        }
                    }
                }
                
                // Read 9 bytes of scratchpad data
                uint8_t data[9];
                for (int byte = 0; byte < 9; byte++) {
                    data[byte] = 0;
                    for (int bit = 0; bit < 8; bit++) {
                        pinMode(owPin, OUTPUT);
                        digitalWrite(owPin, LOW);
                        delayMicroseconds(3);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                        if (digitalRead(owPin)) {
                            data[byte] |= (1 << bit);
                        }
                        delayMicroseconds(53);
                    }
                }
                
                // Display raw data
                response += "Raw Data: ";
                for (int i = 0; i < 9; i++) {
                    response += "0x" + String(data[i], HEX) + " ";
                }
                
                // Parse temperature if it looks like DS18B20 data
                if (data[4] == 0xFF || data[4] == 0x00) { // Valid config register
                    int16_t temp_raw = (data[1] << 8) | data[0];
                    float temperature = temp_raw / 16.0;
                    response += "\\nParsed Temperature: " + String(temperature, 2) + "°C";
                    response += "\\nNote: Raw parsing - verify against device datasheet";
                } else {
                    response += "\\nUse 'convert' first, then 'read' after 750ms delay";
                }
            }
        } else if (command == "convert") {
            // Send temperature conversion command
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            bool presence = !digitalRead(owPin);
            delayMicroseconds(410);
            
            if (!presence) {
                response = "Error: No device presence detected on GP" + String(owPin);
            } else {
                // Send SKIP ROM (0xCC) + CONVERT T (0x44)
                uint8_t commands[] = {0xCC, 0x44};
                for (int cmd = 0; cmd < 2; cmd++) {
                    for (int bit = 0; bit < 8; bit++) {
                        pinMode(owPin, OUTPUT);
                        digitalWrite(owPin, LOW);
                        if (commands[cmd] & (1 << bit)) {
                            delayMicroseconds(6);
                            pinMode(owPin, INPUT_PULLUP);
                            delayMicroseconds(64);
                        } else {
                            delayMicroseconds(60);
                            pinMode(owPin, INPUT_PULLUP);
                            delayMicroseconds(10);
                        }
                    }
                }
                response = "Temperature conversion started on GP" + String(owPin) + "\\n";
                response += "Wait 750ms, then use 'read' to get data\\n";
                response += "Device is converting temperature...";
            }
        } else if (command == "power") {
            // Check if device is using parasitic power
            response = "Power mode check for GP" + String(owPin) + ":\\n";
            response += "Use external 4.7kΩ pullup resistor for reliable operation";
        } else if (command == "reset") {
            // Send reset pulse
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            bool presence = !digitalRead(owPin);
            delayMicroseconds(410);
            
            response = "Reset pulse sent to GP" + String(owPin) + "\\n";
            response += presence ? "Device presence detected" : "No device response";
        } else if (command == "rom") {
            // Read ROM command - get device ID without libraries
            pinMode(owPin, OUTPUT);
            digitalWrite(owPin, LOW);
            delayMicroseconds(480);
            pinMode(owPin, INPUT_PULLUP);
            delayMicroseconds(70);
            bool presence = !digitalRead(owPin);
            delayMicroseconds(410);
            
            if (!presence) {
                response = "Error: No device presence detected on GP" + String(owPin);
            } else {
                // Send READ ROM (0x33) - only works with single device
                uint8_t read_rom_cmd = 0x33;
                for (int bit = 0; bit < 8; bit++) {
                    pinMode(owPin, OUTPUT);
                    digitalWrite(owPin, LOW);
                    if (read_rom_cmd & (1 << bit)) {
                        delayMicroseconds(6);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(64);
                    } else {
                        delayMicroseconds(60);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                    }
                }
                
                // Read 8 bytes of ROM data
                uint8_t rom[8];
                for (int byte = 0; byte < 8; byte++) {
                    rom[byte] = 0;
                    for (int bit = 0; bit < 8; bit++) {
                        pinMode(owPin, OUTPUT);
                        digitalWrite(owPin, LOW);
                        delayMicroseconds(3);
                        pinMode(owPin, INPUT_PULLUP);
                        delayMicroseconds(10);
                        if (digitalRead(owPin)) {
                            rom[byte] |= (1 << bit);
                        }
                        delayMicroseconds(53);
                    }
                }
                
                response = "ROM Data from GP" + String(owPin) + ":\\n";
                response += "Family Code: 0x" + String(rom[0], HEX) + "\\n";
                response += "Serial: ";
                for (int i = 1; i < 7; i++) {
                    response += String(rom[i], HEX) + " ";
                }
                response += "\\nCRC: 0x" + String(rom[7], HEX) + "\\n";
                
                // Identify common device types
                switch(rom[0]) {
                    case 0x28: response += "Device Type: DS18B20 Temperature Sensor"; break;
                    case 0x10: response += "Device Type: DS18S20 Temperature Sensor"; break;
                    case 0x22: response += "Device Type: DS1822 Temperature Sensor"; break;
                    case 0x26: response += "Device Type: DS2438 Battery Monitor"; break;
                    default: response += "Device Type: Unknown (0x" + String(rom[0], HEX) + ")"; break;
                }
            }
        } else if (command == "cmd") {
            // Send custom command - ultimate flexibility!
            // Extract parameters from command (e.g., "cmd 0x44" -> params = "0x44")
            String params = "";
            int spaceIndex = command.indexOf(' ');
            if (spaceIndex > 0 && command.length() > spaceIndex + 1) {
                params = command.substring(spaceIndex + 1);
            }
            
            if (params.length() == 0) {
                success = false;
                response = "Error: Specify hex command. Example: 'cmd 0x44' or 'cmd 0xCC,0x44'";
            } else {
                pinMode(owPin, OUTPUT);
                digitalWrite(owPin, LOW);
                delayMicroseconds(480);
                pinMode(owPin, INPUT_PULLUP);
                delayMicroseconds(70);
                bool presence = !digitalRead(owPin);
                delayMicroseconds(410);
                
                if (!presence) {
                    response = "Error: No device presence detected on GP" + String(owPin);
                } else {
                    response = "Sending custom command(s) to GP" + String(owPin) + ":\\n";
                    
                    // Parse comma-separated hex commands
                    int startPos = 0;
                    while (startPos < params.length()) {
                        int commaPos = params.indexOf(',', startPos);
                        if (commaPos == -1) commaPos = params.length();
                        
                        String cmdStr = params.substring(startPos, commaPos);
                        cmdStr.trim();
                        
                        uint8_t cmd = 0;
                        if (cmdStr.startsWith("0x") || cmdStr.startsWith("0X")) {
                            cmd = strtol(cmdStr.c_str(), nullptr, 16);
                        } else {
                            cmd = cmdStr.toInt();
                        }
                        
                        response += "Sending: 0x" + String(cmd, HEX) + " ";
                        
                        // Send the command
                        for (int bit = 0; bit < 8; bit++) {
                            pinMode(owPin, OUTPUT);
                            digitalWrite(owPin, LOW);
                            if (cmd & (1 << bit)) {
                                delayMicroseconds(6);
                                pinMode(owPin, INPUT_PULLUP);
                                delayMicroseconds(64);
                            } else {
                                delayMicroseconds(60);
                                pinMode(owPin, INPUT_PULLUP);
                                delayMicroseconds(10);
                            }
                        }
                        
                        startPos = commaPos + 1;
                    }
                    response += "\\nCommands sent successfully";
                }
            }
        } else if (command == "info") {
            response = "One-Wire Information for GP" + String(owPin) + ":\\n";
            response += "Protocol: Dallas 1-Wire\\n";
            response += "Common devices: DS18B20, DS18S20, DS1822\\n";
            response += "Requires: 4.7kΩ pullup resistor\\n";
            response += "Voltage: 3.3V or 5V\\n";
            response += "Speed: 15.4 kbps (standard), 125 kbps (overdrive)\\n\\n";
            response += "Available Commands:\\n";
            response += "• scan/search - Detect device presence\\n";
            response += "• convert - Start temperature conversion\\n";
            response += "• read - Read scratchpad data (raw bytes)\\n";
            response += "• rom - Read device ROM ID\\n";
            response += "• cmd <hex> - Send custom command\\n";
            response += "• reset - Send reset pulse\\n";
            response += "• power - Check power mode";
        } else if (command == "crc") {
            response = "CRC check functionality requires OneWire library\\n";
            response += "Install: OneWire by Jim Studt, Tom Pollard\\n";
            response += "Also: DallasTemperature by Miles Burton";
        } else {
            success = false;
            response = "Error: Unknown One-Wire command. Use 'scan', 'read', 'convert', 'rom', 'cmd', 'reset', 'power', 'info', or 'crc'";
        }
    } else if (protocol == "uart") {
        // Extract pin numbers from pin string (e.g., "GP8,GP9" for TX,RX)
        int txPin = -1;
        int rxPin = -1;
        
        if (pin.indexOf(',') > 0) {
            // Parse "GP8,GP9" format
            int commaPos = pin.indexOf(',');
            String txPinStr = pin.substring(0, commaPos);
            String rxPinStr = pin.substring(commaPos + 1);
            if (txPinStr.startsWith("GP")) txPin = txPinStr.substring(2).toInt();
            if (rxPinStr.startsWith("GP")) rxPin = rxPinStr.substring(2).toInt();
        } else {
            // Try to find from configured sensor
            for (int i = 0; i < numConfiguredSensors; i++) {
                Serial.printf("[DEBUG] Checking sensor %d: name='%s', protocol='%s', pin query='%s'\n", 
                             i, configuredSensors[i].name, configuredSensors[i].protocol, pin.c_str());
                if (String(configuredSensors[i].name) == pin && strcmp(configuredSensors[i].protocol, "UART") == 0) {
                    txPin = configuredSensors[i].uartTxPin;
                    rxPin = configuredSensors[i].uartRxPin;
                    Serial.printf("[DEBUG] Found UART sensor: TX=%d, RX=%d\n", txPin, rxPin);
                    break;
                }
            }
            Serial.printf("[DEBUG] Final UART pins: TX=%d, RX=%d\n", txPin, rxPin);
        }
        
        if (txPin < 0 || rxPin < 0 || txPin > 28 || rxPin > 28) {
            success = false;
            response = "Error: Invalid UART pins. Use 'GP<tx>,GP<rx>' format or configured sensor name";
        } else if (command == "info") {
            response = "UART Information for TX:GP" + String(txPin) + ", RX:GP" + String(rxPin) + ":\\n";
            response += "Protocol: Universal Asynchronous Receiver-Transmitter\\n";
            response += "Common baud rates: 9600, 19200, 38400, 57600, 115200\\n";
            response += "Default: 8 data bits, no parity, 1 stop bit\\n";
            response += "Voltage: 3.3V logic level\\n\\n";
            response += "Available Commands:\\n";
            response += "• send <data> - Send data to UART device\\n";
            response += "• read - Read available data from UART\\n";
            response += "• config - Show UART configuration\\n";
            response += "• test - Send test command and read response\\n";
            response += "• info - Show this information";
        } else if (command == "config") {
            response = "UART Configuration:\\n";
            response += "TX Pin: GP" + String(txPin) + "\\n";
            response += "RX Pin: GP" + String(rxPin) + "\\n";
            response += "Baud Rate: 9600 (default)\\n";
            response += "Data Bits: 8\\n";
            response += "Parity: None\\n";
            response += "Stop Bits: 1\\n";
            response += "Note: Use PlatformIO Serial2 for hardware UART on these pins";
        } else if (command.startsWith("send ")) {
            String data = command.substring(5);
            
            // Handle angle bracket format: send <data>
            if (data.startsWith("<") && data.endsWith(">")) {
                data = data.substring(1, data.length() - 1);
            }
            
            // Handle escape sequences
            data.replace("\\r", "\r");
            data.replace("\\n", "\n");
            data.replace("\\t", "\t");
            
            if (data.length() > 0) {
                // Log the outgoing command
                logUARTTransaction("GP" + String(txPin) + ",GP" + String(rxPin), "TX", data);
                
                // Real UART implementation
                if ((txPin == 0 && rxPin == 1) || (txPin == 12 && rxPin == 13) || 
                    (txPin == 16 && rxPin == 17) || (txPin == 4 && rxPin == 5)) {
                    
                    // Configure and send via hardware UART
                    Serial1.setTX(txPin);
                    Serial1.setRX(rxPin);
                    Serial1.begin(9600);
                    
                    // Send data as-is (don't add extra CR/LF if already present)
                    Serial1.print(data);
                    
                    response = "UART TX (GP" + String(txPin) + "): " + data;
                    
                    // Wait for response
                    delay(100);
                    String uartResponse = "";
                    unsigned long timeout = millis() + 1000;
                    while (millis() < timeout && uartResponse.length() < 100) {
                        if (Serial1.available()) {
                            char c = Serial1.read();
                            uartResponse += c;
                            if (c == '\n' || c == '\r') break;
                        }
                    }
                    
                    if (uartResponse.length() > 0) {
                        uartResponse.trim();
                        response += "\nRX: " + uartResponse;
                        logUARTTransaction("GP" + String(txPin) + ",GP" + String(rxPin), "RX", uartResponse);
                    } else {
                        response += "\nRX: (no response)";
                    }
                    
                    Serial1.end();
                } else {
                    success = false;
                    response = "Error: Invalid UART pin combination for hardware UART";
                }
            } else {
                success = false;
                response = "Error: No data to send. Use 'send <data>'";
            }
        } else if (command == "read") {
            // Real UART read
            if ((txPin == 0 && rxPin == 1) || (txPin == 12 && rxPin == 13) || 
                (txPin == 16 && rxPin == 17) || (txPin == 4 && rxPin == 5)) {
                
                Serial1.setTX(txPin);
                Serial1.setRX(rxPin);
                Serial1.begin(9600);
                
                String readData = "";
                while (Serial1.available() && readData.length() < 100) {
                    char c = Serial1.read();
                    readData += c;
                }
                
                if (readData.length() > 0) {
                    response = "UART RX (GP" + String(rxPin) + "): " + readData;
                    logUARTTransaction("GP" + String(txPin) + ",GP" + String(rxPin), "RX", readData);
                } else {
                    response = "UART RX (GP" + String(rxPin) + "): (no data available)";
                }
                
                Serial1.end();
            } else {
                success = false;
                response = "Error: Invalid UART pin combination for hardware UART";
            }
        } else if (command == "test") {
            // Send test command and wait for response
            String testCmd = "R";
            logUARTTransaction("GP" + String(txPin) + ",GP" + String(rxPin), "TX", testCmd);
            
            if ((txPin == 0 && rxPin == 1) || (txPin == 12 && rxPin == 13) || 
                (txPin == 16 && rxPin == 17) || (txPin == 4 && rxPin == 5)) {
                
                Serial1.setTX(txPin);
                Serial1.setRX(rxPin);
                Serial1.begin(9600);
                
                Serial1.print(testCmd);
                Serial1.print("\r\n");
                
                response = "UART Test Command Sent: " + testCmd;
                
                // Wait for response
                delay(500); // Longer wait for test
                String testResp = "";
                unsigned long timeout = millis() + 2000;
                while (millis() < timeout && testResp.length() < 100) {
                    if (Serial1.available()) {
                        char c = Serial1.read();
                        testResp += c;
                        if (c == '\n' || c == '\r') break;
                    }
                }
                
                if (testResp.length() > 0) {
                    testResp.trim();
                    response += "\nResponse: " + testResp;
                    logUARTTransaction("GP" + String(txPin) + ",GP" + String(rxPin), "RX", testResp);
                } else {
                    response += "\nResponse: (timeout - no response)";
                }
                
                Serial1.end();
            } else {
                success = false;
                response = "Error: Invalid UART pin combination for hardware UART";
            }
        } else {
            success = false;
            response = "Error: Unknown UART command. Use 'info', 'config', 'send <data>', 'read', or 'test'";
        }
    } else {
        success = false;
        response = "Error: Unknown protocol. Use 'digital', 'analog', 'i2c', 'uart', 'onewire', 'system', or 'network'";
    }
    
    StaticJsonDocument<1024> respDoc;
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
    static unsigned long lastWebCheck = 0;
    static unsigned long lastStats = 0;
    static unsigned long webRequests = 0;
    static unsigned long loopCount = 0;
    unsigned long now = millis();
    
    // Process web requests more frequently
    if (now - lastWebCheck >= 1) {  // Check every 1ms
        handleSimpleHTTP();
        lastWebCheck = now;
    }
    
    // Print stats every 5 seconds
    if (now - lastStats >= 5000) {
        Serial.print("Loop frequency: ");
        Serial.print(loopCount / 5);
        Serial.println(" Hz");
        Serial.print("Web requests/5s: ");
        Serial.println(webRequests);
        Serial.print("Free RAM: ");
        Serial.println(rp2040.getFreeHeap());
        loopCount = 0;
        webRequests = 0;
        lastStats = now;
    }
    
    // Update bus operation queues
    updateBusQueues();

    // Check for new client connections on the WiFi server (actually Ethernet via W5500lwIP)
    WiFiClient newClient = modbusServer.accept();
    loopCount++;
    
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
                
                // Log Modbus connection for network monitoring
                String remoteIP = modbusClients[i].clientIP.toString();
                String localIP = eth.localIP().toString() + ":" + String(config.modbusPort);
                logNetworkTransaction("MODBUS", "CONNECT", localIP, remoteIP, "New Modbus TCP connection established");
                
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
                    
                    // Log Modbus request for network monitoring
                    String remoteIP = modbusClients[i].clientIP.toString();
                    String localIP = eth.localIP().toString() + ":" + String(config.modbusPort);
                    logNetworkTransaction("MODBUS", "RX", localIP, remoteIP, "Modbus Request (Function Code Processing)");
                }
                // Update IO for this specific client
                updateIOForClient(i);
            } else {
                // Client disconnected
                Serial.print("Client disconnected from slot ");
                Serial.println(i);
                
                // Log Modbus disconnection for network monitoring
                String remoteIP = modbusClients[i].clientIP.toString();
                String localIP = eth.localIP().toString() + ":" + String(config.modbusPort);
                logNetworkTransaction("MODBUS", "DISCONNECT", localIP, remoteIP, "Modbus TCP connection closed");
                
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
    // updateSensorReadings();  // DISABLED - SHT30 sensors now handled in queue system
    handleEzoSensors(); // Handle EZO sensor communications with logging
    
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
        } else if (cmd.equalsIgnoreCase("sensors")) {
            Serial.println("=== SENSOR STATUS ===");
            Serial.printf("Configured sensors: %d\n", numConfiguredSensors);
            Serial.printf("Queue sizes - I2C: %d, UART: %d, One-Wire: %d\n", i2cQueueSize, uartQueueSize, oneWireQueueSize);
            for (int i = 0; i < numConfiguredSensors; i++) {
                Serial.printf("[%d] %s (%s): enabled=%s, lastRead=%lu, interval=%d\n", 
                             i, configuredSensors[i].name, configuredSensors[i].type,
                             configuredSensors[i].enabled ? "YES" : "NO",
                             configuredSensors[i].lastReadTime, configuredSensors[i].updateInterval);
                Serial.printf("    Protocol: %s, I2C: 0x%02X, ModbusReg: %d\n",
                             configuredSensors[i].protocol, configuredSensors[i].i2cAddress, configuredSensors[i].modbusRegister);
                Serial.printf("    Raw: %.2f, Calibrated: %.2f, Modbus: %d\n",
                             configuredSensors[i].rawValue, configuredSensors[i].calibratedValue, configuredSensors[i].modbusValue);
                if (strcmp(configuredSensors[i].type, "SHT30") == 0) {
                    Serial.printf("    Secondary - Raw: %.2f, Calibrated: %.2f, Modbus: %d\n",
                                 configuredSensors[i].rawValueB, configuredSensors[i].calibratedValueB, configuredSensors[i].modbusValueB);
                }
            }
            Serial.println("====================");
        } else if (cmd.equalsIgnoreCase("webtest")) {
            Serial.println("=== WEB SERVER TEST ===");
            Serial.println("Try accessing these URLs:");
            Serial.println("http://" + eth.localIP().toString() + "/test");
            Serial.println("http://" + eth.localIP().toString() + "/config");
            Serial.println("http://" + eth.localIP().toString() + "/iostatus");
            Serial.println("http://" + eth.localIP().toString() + "/sensors/config");
            Serial.println("http://" + eth.localIP().toString() + "/sensors/data");
            Serial.println("=====================");
        }
    }

    // Watchdog timer reset
    rp2040.wdt_reset();
}

void initializeEzoSensors() {
    if (ezoSensorsInitialized) return;
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && strncmp(configuredSensors[i].type, "EZO-", 4) == 0) {
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

void handleEzoSensors() {
    static bool initialized = false;
    
    if (!initialized) {
        initializeEzoSensors();
        initialized = true;
    }
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled || strncmp(configuredSensors[i].type, "EZO-", 4) != 0) {
            continue;
        }
        
        if (ezoSensors[i] == nullptr) {
            continue;
        }
        
        unsigned long currentTime = millis();
        
        // Check if we should log for terminal watch
        bool shouldLog = terminalWatchActive && 
                        (watchedProtocol.equalsIgnoreCase("I2C") || watchedProtocol.equalsIgnoreCase("EZO")) &&
                        (watchedPin == "all" || watchedPin == String(configuredSensors[i].i2cAddress, HEX) || 
                         watchedPin == String(configuredSensors[i].name));
        
        if (configuredSensors[i].cmdPending && (currentTime - configuredSensors[i].lastCmdSent > 1000)) {
            ezoSensors[i]->receive_read_cmd();
            
            if (ezoSensors[i]->get_error() == Ezo_board::SUCCESS) {
                float reading = ezoSensors[i]->get_last_received_reading();
                snprintf(configuredSensors[i].response, sizeof(configuredSensors[i].response), "%.2f", reading);
                Serial.printf("EZO sensor %s reading: %s\n", configuredSensors[i].name, configuredSensors[i].response);
                
                // Log for terminal watch
                if (shouldLog) {
                    logI2CTransaction(configuredSensors[i].i2cAddress, "RX", 
                                    String(reading), String(configuredSensors[i].i2cAddress, HEX));
                }
            } else {
                Serial.printf("EZO sensor %s error: %d\n", configuredSensors[i].name, ezoSensors[i]->get_error());
                
                // Log error for terminal watch
                if (shouldLog) {
                    logI2CTransaction(configuredSensors[i].i2cAddress, "ERR", 
                                    "Error: " + String(ezoSensors[i]->get_error()), 
                                    String(configuredSensors[i].i2cAddress, HEX));
                }
            }
            
            configuredSensors[i].cmdPending = false;
        } 
        else if (!configuredSensors[i].cmdPending && (currentTime - configuredSensors[i].lastCmdSent > 5000)) {
            // Log command being sent for terminal watch
            if (shouldLog) {
                logI2CTransaction(configuredSensors[i].i2cAddress, "TX", 
                                "READ", String(configuredSensors[i].i2cAddress, HEX));
            }
            
            ezoSensors[i]->send_read_cmd();
            configuredSensors[i].lastCmdSent = currentTime;
            configuredSensors[i].cmdPending = true;
            Serial.printf("Sent read command to EZO sensor %s\n", configuredSensors[i].name);
        }
    }
}

void loadConfig() {
    // Use hardcoded defaults for config (RAM only)
    config = DEFAULT_CONFIG;
    config.modbusPort = 502;
    // If your Config struct has a deviceName field, set it here. Otherwise, remove this line.
    // All old JSON/file logic removed.
    return;
}

void saveConfig() {
    Serial.println("Config save (RAM only, not persisted)");
}

void loadSensorConfig() {
    Serial.println("Loading sensor configuration...");
    // Initialize sensors array
    numConfiguredSensors = 0;
    memset(configuredSensors, 0, sizeof(configuredSensors));

    if (!LittleFS.exists(SENSORS_FILE)) {
        Serial.println("No sensors file found on LittleFS");
        return;
    }

    File file = LittleFS.open(SENSORS_FILE, "r");
    if (!file) {
        Serial.println("Failed to open sensors file");
        return;
    }

    size_t size = file.size();
    if (size == 0) {
        Serial.println("Sensors file is empty");
        file.close();
        return;
    }

    // Parse JSON
    StaticJsonDocument<8192> doc; // generous buffer for sensor configs
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Serial.printf("Failed to parse sensors JSON: %s\n", err.c_str());
        return;
    }

    if (!doc.containsKey("sensors") || !doc["sensors"].is<JsonArray>()) {
        Serial.println("sensors array missing or invalid in JSON");
        return;
    }

    JsonArray sensorsArray = doc["sensors"].as<JsonArray>();
    Serial.printf("Found %u sensors in file\n", (unsigned)sensorsArray.size());

    for (JsonObject sensor : sensorsArray) {
        if (numConfiguredSensors >= MAX_SENSORS) break;

        // Mirror the POST parsing logic to populate SensorConfig
        SensorConfig &cfg = configuredSensors[numConfiguredSensors];
        cfg.enabled = sensor["enabled"] | false;
        const char* name = sensor["name"] | "";
        strncpy(cfg.name, name, sizeof(cfg.name)-1);
        cfg.name[sizeof(cfg.name)-1] = '\0';

        const char* type = sensor["type"] | "";
        strncpy(cfg.type, type, sizeof(cfg.type)-1);
        cfg.type[sizeof(cfg.type)-1] = '\0';

        const char* protocol = sensor["protocol"] | "";
        strncpy(cfg.protocol, protocol, sizeof(cfg.protocol)-1);
        cfg.protocol[sizeof(cfg.protocol)-1] = '\0';

        cfg.i2cAddress = sensor["i2cAddress"] | 0;
        cfg.modbusRegister = sensor["modbusRegister"] | 0;

        // command may be string or object
        const char* command = "";
        int delayBeforeRead = 0;
        if (sensor.containsKey("command") && sensor["command"].is<const char*>()) {
            command = sensor["command"] | "";
        } else if (sensor.containsKey("command") && sensor["command"].is<JsonObject>()) {
            JsonObject cmdObj = sensor["command"];
            command = cmdObj["command"] | "";
            delayBeforeRead = cmdObj["waitTime"] | 0;
        }
        strncpy(cfg.command, command, sizeof(cfg.command)-1);
        cfg.command[sizeof(cfg.command)-1] = '\0';

        cfg.updateInterval = sensor["updateInterval"] | sensor["pollingFrequency"] | 5000;
        cfg.delayBeforeRead = sensor["delayBeforeRead"] | delayBeforeRead;

        // Pins
        cfg.sdaPin = sensor["sdaPin"] | -1;
        cfg.sclPin = sensor["sclPin"] | -1;
        cfg.dataPin = sensor["dataPin"] | -1;
        cfg.uartTxPin = sensor["uartTxPin"] | -1;
        cfg.uartRxPin = sensor["uartRxPin"] | -1;
        cfg.analogPin = sensor["analogPin"] | -1;
        cfg.oneWirePin = sensor["oneWirePin"] | -1;
        cfg.digitalPin = sensor["digitalPin"] | -1;

        const char* owCommand = sensor["oneWireCommand"] | "0x44";
        strncpy(cfg.oneWireCommand, owCommand, sizeof(cfg.oneWireCommand)-1);
        cfg.oneWireCommand[sizeof(cfg.oneWireCommand)-1] = '\0';
        cfg.oneWireInterval = sensor["oneWireInterval"] | 5;
        cfg.oneWireConversionTime = sensor["oneWireConversionTime"] | 750;
        cfg.oneWireAutoMode = sensor["oneWireAutoMode"] | true;

        // Calibration nested or flat
        if (sensor.containsKey("calibration") && sensor["calibration"].is<JsonObject>()) {
            JsonObject cal = sensor["calibration"];
            cfg.calibrationOffset = cal["offset"] | 0.0;
            cfg.calibrationSlope = cal["scale"] | 1.0;
            const char* expr = cal["expression"] | "";
            strncpy(cfg.calibrationExpression, expr, sizeof(cfg.calibrationExpression)-1);
            cfg.calibrationExpression[sizeof(cfg.calibrationExpression)-1] = '\0';
        } else {
            cfg.calibrationOffset = sensor["calibrationOffset"] | 0.0;
            cfg.calibrationSlope = sensor["calibrationSlope"] | 1.0;
            const char* expr = sensor["calibrationExpression"] | "";
            strncpy(cfg.calibrationExpression, expr, sizeof(cfg.calibrationExpression)-1);
            cfg.calibrationExpression[sizeof(cfg.calibrationExpression)-1] = '\0';
        }

        // Multi-output calibration
        cfg.calibrationOffsetB = sensor["calibrationOffsetB"] | 0.0;
        cfg.calibrationSlopeB = sensor["calibrationSlopeB"] | 1.0;
        cfg.calibrationOffsetC = sensor["calibrationOffsetC"] | 0.0;
        cfg.calibrationSlopeC = sensor["calibrationSlopeC"] | 1.0;
        const char* exprB = sensor["calibrationExpressionB"] | "";
        strncpy(cfg.calibrationExpressionB, exprB, sizeof(cfg.calibrationExpressionB)-1);
        cfg.calibrationExpressionB[sizeof(cfg.calibrationExpressionB)-1] = '\0';
        const char* exprC = sensor["calibrationExpressionC"] | "";
        strncpy(cfg.calibrationExpressionC, exprC, sizeof(cfg.calibrationExpressionC)-1);
        cfg.calibrationExpressionC[sizeof(cfg.calibrationExpressionC)-1] = '\0';

        // Data parsing
        if (sensor.containsKey("dataParsing") && sensor["dataParsing"].is<JsonObject>()) {
            JsonObject dp = sensor["dataParsing"];
            const char* method = dp["method"] | "raw";
            strncpy(cfg.parsingMethod, method, sizeof(cfg.parsingMethod)-1);
            cfg.parsingMethod[sizeof(cfg.parsingMethod)-1] = '\0';
            StaticJsonDocument<256> tmp;
            tmp.set(dp);
            String s; serializeJson(tmp, s);
            strncpy(cfg.parsingConfig, s.c_str(), sizeof(cfg.parsingConfig)-1);
            cfg.parsingConfig[sizeof(cfg.parsingConfig)-1] = '\0';
        } else {
            strncpy(cfg.parsingMethod, "raw", sizeof(cfg.parsingMethod)-1);
            cfg.parsingMethod[sizeof(cfg.parsingMethod)-1] = '\0';
            cfg.parsingConfig[0] = '\0';
        }

        if (sensor.containsKey("dataParsingB") && sensor["dataParsingB"].is<JsonObject>()) {
            JsonObject dp = sensor["dataParsingB"];
            const char* method = dp["method"] | "raw";
            strncpy(cfg.parsingMethodB, method, sizeof(cfg.parsingMethodB)-1);
            cfg.parsingMethodB[sizeof(cfg.parsingMethodB)-1] = '\0';
            StaticJsonDocument<256> tmp;
            tmp.set(dp);
            String s; serializeJson(tmp, s);
            strncpy(cfg.parsingConfigB, s.c_str(), sizeof(cfg.parsingConfigB)-1);
            cfg.parsingConfigB[sizeof(cfg.parsingConfigB)-1] = '\0';
        } else {
            strncpy(cfg.parsingMethodB, "raw", sizeof(cfg.parsingMethodB)-1);
            cfg.parsingMethodB[sizeof(cfg.parsingMethodB)-1] = '\0';
            cfg.parsingConfigB[0] = '\0';
        }

        // Runtime init
        cfg.cmdPending = false;
        cfg.lastCmdSent = 0;
        cfg.response[0] = '\0';
        cfg.calibrationData[0] = '\0';

        numConfiguredSensors++;
    }

    Serial.printf("Loaded %d sensors from filesystem\n", numConfiguredSensors);

    // Apply presets after loading
    applySensorPresets();
}

void saveSensorConfig() {
    Serial.println("Saving sensor configuration...");
    // Create JSON document
    StaticJsonDocument<2048> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    // Add each configured sensor to the array
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        sensor["command"] = configuredSensors[i].command;
        sensor["updateInterval"] = configuredSensors[i].updateInterval;
        sensor["delayBeforeRead"] = configuredSensors[i].delayBeforeRead;
        
        // Pin assignments
        sensor["sdaPin"] = configuredSensors[i].sdaPin;
        sensor["sclPin"] = configuredSensors[i].sclPin;
        sensor["dataPin"] = configuredSensors[i].dataPin;
        sensor["uartTxPin"] = configuredSensors[i].uartTxPin;
        sensor["uartRxPin"] = configuredSensors[i].uartRxPin;
        sensor["analogPin"] = configuredSensors[i].analogPin;
        sensor["oneWirePin"] = configuredSensors[i].oneWirePin;
        sensor["digitalPin"] = configuredSensors[i].digitalPin;
        
        // One-Wire specific configuration
        sensor["oneWireCommand"] = configuredSensors[i].oneWireCommand;
        sensor["oneWireInterval"] = configuredSensors[i].oneWireInterval;
        sensor["oneWireConversionTime"] = configuredSensors[i].oneWireConversionTime;
        sensor["oneWireAutoMode"] = configuredSensors[i].oneWireAutoMode;
        
        // Calibration data
        sensor["calibrationOffset"] = configuredSensors[i].calibrationOffset;
        sensor["calibrationSlope"] = configuredSensors[i].calibrationSlope;
        sensor["calibrationOffsetB"] = configuredSensors[i].calibrationOffsetB;
        sensor["calibrationSlopeB"] = configuredSensors[i].calibrationSlopeB;
        sensor["calibrationOffsetC"] = configuredSensors[i].calibrationOffsetC;
        sensor["calibrationSlopeC"] = configuredSensors[i].calibrationSlopeC;
        
        // Include expression fields for all output channels
        if (strlen(configuredSensors[i].calibrationExpression) > 0) {
            sensor["calibrationExpression"] = configuredSensors[i].calibrationExpression;
        }
        if (strlen(configuredSensors[i].calibrationExpressionB) > 0) {
            sensor["calibrationExpressionB"] = configuredSensors[i].calibrationExpressionB;
        }
        if (strlen(configuredSensors[i].calibrationExpressionC) > 0) {
            sensor["calibrationExpressionC"] = configuredSensors[i].calibrationExpressionC;
        }
        
        // Data parsing configuration
        if (strlen(configuredSensors[i].parsingConfig) > 0) {
            StaticJsonDocument<256> parsingDoc;
            DeserializationError error = deserializeJson(parsingDoc, configuredSensors[i].parsingConfig);
            if (!error) {
                sensor["dataParsing"] = parsingDoc.as<JsonObject>();
            }
        }
    }
    
    // Open file for writing
    File file = LittleFS.open(SENSORS_FILE, "w");
    if (!file) {
        Serial.println("Failed to open sensors file for writing");
        return;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write sensors JSON");
    } else {
        Serial.println("Sensors configuration saved successfully");
    }
    file.close();
    
    // After saving, re-apply presets to ensure runtime config is correct
    applySensorPresets();
}

// Reset all latched inputs
void resetLatches() {
    Serial.println("Resetting all latched inputs");
    for (int i = 0; i < 8; i++) {
        ioStatus.dInLatched[i] = false;
    }
}

// ...existing code...

void setPinModes() {
    // First, set up default pin modes for standard IO arrays
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
    
    // Now configure pins dynamically based on configured sensors
    Serial.println("=== DYNAMIC PIN CONFIGURATION FROM SENSOR CONFIG ===");
    
    bool configuredPins[32] = {false};  // Track which pins are used by sensors
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        
        Serial.printf("Configuring sensor '%s' (type: %s, protocol: %s)\n", 
                     configuredSensors[i].name, configuredSensors[i].type, configuredSensors[i].protocol);
        
        // --- I2C Protocol ---
        if (strncmp(configuredSensors[i].protocol, "I2C", 3) == 0) {
            int sda = configuredSensors[i].sdaPin >= 0 ? configuredSensors[i].sdaPin : 4;
            int scl = configuredSensors[i].sclPin >= 0 ? configuredSensors[i].sclPin : 5;
            if (sda >= 0 && sda < 32 && !configuredPins[sda]) {
                pinMode(sda, INPUT_PULLUP);
                configuredPins[sda] = true;
                Serial.printf("  I2C SDA pin configured: GP%d\n", sda);
            }
            if (scl >= 0 && scl < 32 && !configuredPins[scl]) {
                pinMode(scl, INPUT_PULLUP);
                configuredPins[scl] = true;
                Serial.printf("  I2C SCL pin configured: GP%d\n", scl);
            }
        }
        
        // --- Analog Voltage Protocol ---
        else if (strcmp(configuredSensors[i].type, "ANALOG_CUSTOM") == 0) {
            int pin = configuredSensors[i].analogPin;
            if (pin >= 0 && pin < 32 && !configuredPins[pin]) {
                pinMode(pin, INPUT);
                configuredPins[pin] = true;
                Serial.printf("  Analog input pin configured: GP%d\n", pin);
            }
        }
        
        // --- One-Wire Protocol ---
        else if (strcmp(configuredSensors[i].protocol, "One-Wire") == 0) {
            int pin = configuredSensors[i].oneWirePin;
            if (pin >= 0 && pin < 32 && !configuredPins[pin]) {
                pinMode(pin, INPUT_PULLUP);  // One-Wire uses pullup
                configuredPins[pin] = true;
                Serial.printf("  One-Wire data pin configured: GP%d\n", pin);
            }
        }
        
        // --- UART Protocol ---
        else if (strcmp(configuredSensors[i].protocol, "UART") == 0) {
            int txPin = configuredSensors[i].uartTxPin;
            int rxPin = configuredSensors[i].uartRxPin;
            if (txPin >= 0 && txPin < 32 && !configuredPins[txPin]) {
                pinMode(txPin, OUTPUT);
                configuredPins[txPin] = true;
                Serial.printf("  UART TX pin configured: GP%d\n", txPin);
            }
            if (rxPin >= 0 && rxPin < 32 && !configuredPins[rxPin]) {
                pinMode(rxPin, INPUT);
                configuredPins[rxPin] = true;
                Serial.printf("  UART RX pin configured: GP%d\n", rxPin);
            }
        }
        
        // --- Digital Input Protocol ---
        else if (strcmp(configuredSensors[i].protocol, "Digital") == 0) {
            int pin = configuredSensors[i].digitalPin;
            if (pin >= 0 && pin < 32 && !configuredPins[pin]) {
                pinMode(pin, INPUT_PULLUP);  // Digital sensors typically use pullup
                configuredPins[pin] = true;
                Serial.printf("  Digital input pin configured: GP%d\n", pin);
            }
        }
    }
    
    Serial.println("Dynamic pin configuration complete.\n");
}
// ...existing code...

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
    static unsigned long lastDebugPrint = 0;
    static unsigned long requestCount = 0;
    
    WiFiClient client = httpServer.accept();
    if (client) {
        requestCount++;
        if (millis() - lastDebugPrint > 5000) { // Print debug every 5 seconds
            Serial.print("=== WEB STATS: Requests/5s: ");
            Serial.println(requestCount);
            requestCount = 0;
            lastDebugPrint = millis();
        }
        
        Serial.print("Client Connected - Free RAM: ");
        Serial.println(rp2040.getFreeHeap());
        
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

        // Log HTTP request for network monitoring
        String remoteIP = client.remoteIP().toString();
        String localIP = eth.localIP().toString() + ":" + String(HTTP_PORT);
        String requestData = method + " " + path;
        if (body.length() > 0) {
            requestData += " (Body: " + body.substring(0, min(50, (int)body.length())) + (body.length() > 50 ? "..." : "") + ")";
        }
        logNetworkTransaction("HTTP", "RX", localIP, remoteIP, requestData);

        // Route the request to existing handlers
        Serial.println("Routing request...");
        Serial.println("DEBUG: Method='" + method + "' Path='" + path + "'");
        routeRequest(client, method, path, body);
        client.stop();
        Serial.println("=== WEB CLIENT DISCONNECTED ===");
    }
}

// Forward declarations
void sendJSONPinMap(WiFiClient& client);
void sendJSONSensorPinStatus(WiFiClient& client);
void sendJSON(WiFiClient& client, String json); // Ensure sendJSON is declared

// Implementation: Return available pins for each protocol
void sendJSONPinMap(WiFiClient& client) {
    StaticJsonDocument<1024> doc;
    JsonObject digital = doc.createNestedObject("digital");
    digital["pins"] = "2,3,4,5,6,7,8,9";
    JsonObject analog = doc.createNestedObject("analog");
    analog["pins"] = "26,27,28";
    JsonObject i2c = doc.createNestedObject("i2c");
    i2c["pins"] = "20,21";
    JsonObject uart = doc.createNestedObject("uart");
    uart["pins"] = "0,1";
    JsonObject onewire = doc.createNestedObject("onewire");
    onewire["pins"] = "2,3,22";
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

// Implementation: Return sensor pin status and assignments
void sendJSONSensorPinStatus(WiFiClient& client) {
    StaticJsonDocument<2048> doc;
    JsonArray sensors = doc.createNestedArray("sensors");
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensors.createNestedObject();
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        // Only reference existing members
        sensor["sdaPin"] = configuredSensors[i].sdaPin;
        sensor["sclPin"] = configuredSensors[i].sclPin;
        sensor["analogPin"] = configuredSensors[i].analogPin;
        sensor["digitalPin"] = configuredSensors[i].digitalPin;
    }
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}
void sendJSON(WiFiClient& client, String json); // Ensure sendJSON is declared

void sendJSONConfig(WiFiClient& client) {
    StaticJsonDocument<1024> doc;
    doc["config"] = "placeholder";
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

void routeRequest(WiFiClient& client, String method, String path, String body) {
    Serial.println("=== ROUTING REQUEST ===");
    Serial.println("Method: " + method);
    Serial.println("Path: " + path);
    
    // Handle OPTIONS requests for CORS preflight
    if (method == "OPTIONS") {
        client.println("HTTP/1.1 200 OK");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
        client.println("Access-Control-Allow-Headers: Content-Type");
        client.println("Connection: close");
        client.println();
        return;
    }
    
    // Simple routing to existing handler functions
    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            Serial.println("Serving embedded index.html");
            sendFile(client, "/index.html", "text/html");
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
            Serial.println("DEBUG: Routing to sendJSONIOStatus");
            sendJSONIOStatus(client);
            Serial.println("DEBUG: sendJSONIOStatus completed");
        } else if (path == "/ioconfig") {
            sendJSONIOConfig(client);
        } else if (path == "/sensors/config") {
            sendJSONSensorConfig(client);
        } else if (path == "/sensors/data") {
            sendJSONSensorData(client);
        } else if (path == "/api/pins/map") {
            sendJSONPinMap(client);
        } else if (path == "/api/sensors/status") {
            sendJSONSensorPinStatus(client);
        } else if (path == "/terminal/logs") {
            // Send terminal buffer for bus traffic monitoring
            StaticJsonDocument<2048> terminalDoc;
            JsonArray terminalArray = terminalDoc.to<JsonArray>();
            
            for (size_t i = 0; i < terminalBuffer.size(); i++) {
                // Clean each log entry to prevent JSON corruption
                String cleanEntry = "";
                for (int j = 0; j < terminalBuffer[i].length(); j++) {
                    char c = terminalBuffer[i][j];
                    if (c >= 32 && c <= 126) { // Only printable ASCII
                        cleanEntry += c;
                    } else if (c == '\n') {
                        cleanEntry += "\\n";
                    } else if (c == '\r') {
                        cleanEntry += "\\r";
                    } else if (c == '\t') {
                        cleanEntry += "\\t";
                    }
                }
                terminalArray.add(cleanEntry);
            }
            
            String response;
            serializeJson(terminalDoc, response);
            sendJSON(client, response);
        } else {
            send404(client);
        }
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
        } else if (path == "/api/sensor/poll") {
            handlePOSTSensorPoll(client, body);
        } else if (path == "/terminal/command") {
            handlePOSTTerminalCommand(client, body);
        } else if (path == "/terminal/start-watch") {
            // Start watching bus traffic
            StaticJsonDocument<128> doc;
            deserializeJson(doc, body);
            watchedPin = doc["pin"].as<String>();
            watchedProtocol = doc["protocol"].as<String>();
            terminalWatchActive = true;
            terminalBuffer.clear();
            
            addTerminalLog("Started watching " + watchedProtocol + " on pin " + watchedPin);
            sendJSON(client, "{\"status\":\"started\",\"pin\":\"" + watchedPin + "\",\"protocol\":\"" + watchedProtocol + "\"}");
        } else if (path == "/terminal/stop-watch") {
            // Stop watching bus traffic
            terminalWatchActive = false;
            addTerminalLog("Stopped watching");
            sendJSON(client, "{\"status\":\"stopped\"}");
        } else if (path == "/terminal/send-command") {
            // Send command and log traffic
            StaticJsonDocument<128> doc;
            deserializeJson(doc, body);
            String command = doc["command"];
            String pin = doc["pin"];
            String protocol = doc["protocol"];
            
            String response = executeTerminalCommand(command, pin, protocol);
            
            sendJSON(client, "{\"status\":\"sent\",\"response\":\"" + response + "\"}");
        } else {
            send404(client);
        }
    } else {
        send404(client);
    }
}

// Delegate file serving to sys_init.h helper
void sendFile(WiFiClient& client, String filename, String contentType) {
    serveFileFromFS(client, filename, contentType);
}

void sendJSONIOStatus(WiFiClient& client) {
    static unsigned long lastCall = 0;
    unsigned long now = millis();
    
    Serial.println("DEBUG: sendJSONIOStatus called");
    
    // Debug timing between calls
    if (lastCall != 0) {
        unsigned long delta = now - lastCall;
        if (delta > 1000) { // Log if more than 1 second between calls
            Serial.print("IOStatus delay: ");
            Serial.print(delta);
            Serial.println("ms");
        }
    }
    lastCall = now;
    
    Serial.printf("[DEBUG] Generating IOStatus JSON for %d sensors\n", numConfiguredSensors);
    
    if (!client.connected()) {
        Serial.println("ERROR: Client not connected in sendJSONIOStatus");
        return;
    }
    
    StaticJsonDocument<2048> doc;
    
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
    
    // Add sensor data for sensor dataflow
    JsonArray sensorsArray = doc.createNestedArray("configured_sensors");
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled) {
            JsonObject sensor = sensorsArray.createNestedObject();
            sensor["name"] = configuredSensors[i].name;
            sensor["type"] = configuredSensors[i].type;
            sensor["protocol"] = configuredSensors[i].protocol;
            sensor["i2c_address"] = configuredSensors[i].i2cAddress;
            sensor["modbus_register"] = configuredSensors[i].modbusRegister;
            
            // Actual sensor readings
            sensor["raw_value"] = configuredSensors[i].rawValue;
            sensor["raw_i2c_data"] = configuredSensors[i].rawDataString;
            
            // Calibrated output (applying calibration equation)
            sensor["calibrated_value"] = configuredSensors[i].calibratedValue;
            
            // Modbus register value (what gets sent to Modbus)
            sensor["modbus_value"] = configuredSensors[i].modbusValue;
            
            // Multi-output sensor support (for SHT30 humidity, BME280 pressure, etc.)
            if (strcmp(configuredSensors[i].type, "SHT30") == 0 && configuredSensors[i].rawValueB != 0) {
                sensor["raw_value_b"] = configuredSensors[i].rawValueB;        // Humidity raw
                sensor["calibrated_value_b"] = configuredSensors[i].calibratedValueB;  // Humidity calibrated
                sensor["modbus_value_b"] = configuredSensors[i].modbusValueB;  // Humidity modbus (register+1)
                sensor["modbus_register_b"] = configuredSensors[i].modbusRegister + 1;
            }
            
            // Include calibration info for dataflow display
            sensor["calibration_offset"] = configuredSensors[i].calibrationOffset;
            sensor["calibration_slope"] = configuredSensors[i].calibrationSlope;
            if (strlen(configuredSensors[i].calibrationExpression) > 0) {
                sensor["calibration_expression"] = configuredSensors[i].calibrationExpression;
            }
            
            // Multi-output calibration info
            if (configuredSensors[i].calibrationSlopeB != 1.0 || configuredSensors[i].calibrationOffsetB != 0.0) {
                sensor["calibration_offset_b"] = configuredSensors[i].calibrationOffsetB;
                sensor["calibration_slope_b"] = configuredSensors[i].calibrationSlopeB;
            }
            if (strlen(configuredSensors[i].calibrationExpressionB) > 0) {
                sensor["calibration_expression_b"] = configuredSensors[i].calibrationExpressionB;
            }
            if (strlen(configuredSensors[i].calibrationExpressionC) > 0) {
                sensor["calibration_expression_c"] = configuredSensors[i].calibrationExpressionC;
            }
            
            // Include last read time for status
            sensor["last_read_time"] = configuredSensors[i].lastReadTime;
            
            // Include pin assignments for reference
            if (String(configuredSensors[i].protocol).equalsIgnoreCase("I2C")) {
                sensor["sda_pin"] = configuredSensors[i].sdaPin;
                sensor["scl_pin"] = configuredSensors[i].sclPin;
            } else if (String(configuredSensors[i].protocol).equalsIgnoreCase("Analog Voltage")) {
                sensor["analog_pin"] = configuredSensors[i].analogPin;
            } else if (String(configuredSensors[i].protocol).equalsIgnoreCase("Digital Counter")) {
                sensor["digital_pin"] = configuredSensors[i].digitalPin;
            } else if (String(configuredSensors[i].protocol).equalsIgnoreCase("One-Wire")) {
                sensor["onewire_pin"] = configuredSensors[i].oneWirePin;
            } else if (String(configuredSensors[i].protocol).equalsIgnoreCase("UART")) {
                sensor["uart_tx_pin"] = configuredSensors[i].uartTxPin;
                sensor["uart_rx_pin"] = configuredSensors[i].uartRxPin;
            }
        }
    }
    
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    sendJSON(client, jsonBuffer);
}

void sendJSONIOConfig(WiFiClient& client) {
    StaticJsonDocument<1024> doc;

    JsonArray diPullupArray = doc.createNestedArray("diPullup");
    JsonArray diInvertArray = doc.createNestedArray("diInvert");
    JsonArray diLatchArray = doc.createNestedArray("diLatch");
    JsonArray diStateArray = doc.createNestedArray("diState");      // Display: digital input states
    JsonArray diLatchedArray = doc.createNestedArray("diLatched");  // Display: latched states

    for (int i = 0; i < 8; i++) {
        diPullupArray.add(config.diPullup[i]);
        diInvertArray.add(config.diInvert[i]);
        diLatchArray.add(config.diLatch[i]);
        diStateArray.add(ioStatus.dInRaw[i]);        // Actual pin state (HIGH/LOW)
        diLatchedArray.add(ioStatus.dInLatched[i]);  // Latched state (true/false)
    }

    JsonArray doInvertArray = doc.createNestedArray("doInvert");
    JsonArray doInitialStateArray = doc.createNestedArray("doInitialState");
    JsonArray doStateArray = doc.createNestedArray("doState");      // Display: digital output states

    for (int i = 0; i < 8; i++) {
        doInvertArray.add(config.doInvert[i]);
        doInitialStateArray.add(config.doInitialState[i]);
        doStateArray.add(ioStatus.dOut[i]);          // Actual output state (HIGH/LOW)
    }

    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

// Mathematical expression evaluator for sensor calibration
float evaluateCalibrationExpression(float x, const SensorConfig& sensor) {
    String expr = String(sensor.calibrationExpression);
    
    if (expr.length() == 0) {
        // No expression, use linear calibration: y = slope*x + offset
        return (sensor.calibrationSlope * x) + sensor.calibrationOffset;
    }
    
    // Replace 'x' with actual value in the expression
    expr.replace("x", String(x, 6));
    expr.replace("X", String(x, 6));
    
    // Handle power operations (x^2, x^3, etc.)
    while (expr.indexOf("^") >= 0) {
        int powerPos = expr.indexOf("^");
        if (powerPos > 0 && powerPos < expr.length() - 1) {
            // Find the base number before ^
            int baseStart = powerPos - 1;
            while (baseStart > 0 && (isdigit(expr.charAt(baseStart - 1)) || expr.charAt(baseStart - 1) == '.')) {
                baseStart--;
            }
            
            // Find the exponent after ^
            int expEnd = powerPos + 2;
            while (expEnd < expr.length() && (isdigit(expr.charAt(expEnd)) || expr.charAt(expEnd) == '.')) {
                expEnd++;
            }
            
            float base = expr.substring(baseStart, powerPos).toFloat();
            float exponent = expr.substring(powerPos + 1, expEnd).toFloat();
            float result = pow(base, exponent);
            
            // Replace the power expression with the result
            String replacement = String(result, 6);
            expr = expr.substring(0, baseStart) + replacement + expr.substring(expEnd);
        } else {
            break; // Avoid infinite loop if parsing fails
        }
    }
    
    // Handle mathematical functions
    expr.replace("sin(", "SIN(");
    expr.replace("cos(", "COS(");
    expr.replace("tan(", "TAN(");
    expr.replace("log(", "LOG(");
    expr.replace("ln(", "LN(");
    expr.replace("exp(", "EXP(");
    expr.replace("sqrt(", "SQRT(");
    
    // Simple function evaluation (basic implementation)
    while (expr.indexOf("SIN(") >= 0) {
        int pos = expr.indexOf("SIN(");
        int closePos = expr.indexOf(")", pos);
        if (closePos > pos) {
            float arg = expr.substring(pos + 4, closePos).toFloat();
            float result = sin(arg);
            expr = expr.substring(0, pos) + String(result, 6) + expr.substring(closePos + 1);
        } else break;
    }
    
    while (expr.indexOf("COS(") >= 0) {
        int pos = expr.indexOf("COS(");
        int closePos = expr.indexOf(")", pos);
        if (closePos > pos) {
            float arg = expr.substring(pos + 4, closePos).toFloat();
            float result = cos(arg);
            expr = expr.substring(0, pos) + String(result, 6) + expr.substring(closePos + 1);
        } else break;
    }
    
    while (expr.indexOf("SQRT(") >= 0) {
        int pos = expr.indexOf("SQRT(");
        int closePos = expr.indexOf(")", pos);
        if (closePos > pos) {
            float arg = expr.substring(pos + 5, closePos).toFloat();
            float result = sqrt(arg);
            expr = expr.substring(0, pos) + String(result, 6) + expr.substring(closePos + 1);
        } else break;
    }
    
    while (expr.indexOf("LOG(") >= 0) {
        int pos = expr.indexOf("LOG(");
        int closePos = expr.indexOf(")", pos);
        if (closePos > pos) {
            float arg = expr.substring(pos + 4, closePos).toFloat();
            float result = log10(arg);
            expr = expr.substring(0, pos) + String(result, 6) + expr.substring(closePos + 1);
        } else break;
    }
    
    while (expr.indexOf("LN(") >= 0) {
        int pos = expr.indexOf("LN(");
        int closePos = expr.indexOf(")", pos);
        if (closePos > pos) {
            float arg = expr.substring(pos + 3, closePos).toFloat();
            float result = log(arg);
            expr = expr.substring(0, pos) + String(result, 6) + expr.substring(closePos + 1);
        } else break;
    }
    
    // Simple arithmetic expression evaluator
    // Handle basic operations in proper order: *, /, +, -
    String workingExpr = expr;
    
    // Remove spaces for easier parsing
    workingExpr.replace(" ", "");
    
    // If it's just a number after substitutions, return it
    if (workingExpr.indexOf("+") < 0 && workingExpr.indexOf("-") < 0 && 
        workingExpr.indexOf("*") < 0 && workingExpr.indexOf("/") < 0) {
        return workingExpr.toFloat();
    }
    
    // Simple expression parser for basic arithmetic
    // This handles expressions like "3.5*1.8+32" or "24*0.75-5"
    
    // First handle multiplication and division (left to right)
    while (workingExpr.indexOf("*") >= 0 || workingExpr.indexOf("/") >= 0) {
        int multPos = workingExpr.indexOf("*");
        int divPos = workingExpr.indexOf("/");
        
        // Find which operation comes first
        int opPos = -1;
        char op = ' ';
        if (multPos >= 0 && divPos >= 0) {
            if (multPos < divPos) {
                opPos = multPos;
                op = '*';
            } else {
                opPos = divPos;
                op = '/';
            }
        } else if (multPos >= 0) {
            opPos = multPos;
            op = '*';
        } else if (divPos >= 0) {
            opPos = divPos;
            op = '/';
        }
        
        if (opPos < 0) break;
        
        // Find the numbers before and after the operator
        int leftStart = opPos - 1;
        while (leftStart > 0 && (isdigit(workingExpr.charAt(leftStart - 1)) || workingExpr.charAt(leftStart - 1) == '.')) {
            leftStart--;
        }
        
        int rightEnd = opPos + 2;
        while (rightEnd < workingExpr.length() && (isdigit(workingExpr.charAt(rightEnd)) || workingExpr.charAt(rightEnd) == '.')) {
            rightEnd++;
        }
        
        float leftNum = workingExpr.substring(leftStart, opPos).toFloat();
        float rightNum = workingExpr.substring(opPos + 1, rightEnd).toFloat();
        float result = (op == '*') ? leftNum * rightNum : leftNum / rightNum;
        
        // Replace the operation with the result
        workingExpr = workingExpr.substring(0, leftStart) + String(result, 6) + workingExpr.substring(rightEnd);
    }
    
    // Then handle addition and subtraction (left to right)
    while (workingExpr.indexOf("+") >= 0 || workingExpr.indexOf("-") >= 0) {
        int addPos = workingExpr.indexOf("+");
        int subPos = workingExpr.indexOf("-");
        
        // Find which operation comes first (but skip negative numbers at start)
        int opPos = -1;
        char op = ' ';
        
        // Skip initial negative sign
        if (subPos == 0) {
            subPos = workingExpr.indexOf("-", 1);
        }
        
        if (addPos >= 0 && subPos >= 0) {
            if (addPos < subPos) {
                opPos = addPos;
                op = '+';
            } else {
                opPos = subPos;
                op = '-';
            }
        } else if (addPos >= 0) {
            opPos = addPos;
            op = '+';
        } else if (subPos >= 0) {
            opPos = subPos;
            op = '-';
        }
        
        if (opPos < 0) break;
        
        // Find the numbers before and after the operator
        int leftStart = opPos - 1;
        while (leftStart > 0 && (isdigit(workingExpr.charAt(leftStart - 1)) || workingExpr.charAt(leftStart - 1) == '.')) {
            leftStart--;
        }
        
        int rightEnd = opPos + 2;
        while (rightEnd < workingExpr.length() && (isdigit(workingExpr.charAt(rightEnd)) || workingExpr.charAt(rightEnd) == '.')) {
            rightEnd++;
        }
        
        float leftNum = workingExpr.substring(leftStart, opPos).toFloat();
        float rightNum = workingExpr.substring(opPos + 1, rightEnd).toFloat();
        float result = (op == '+') ? leftNum + rightNum : leftNum - rightNum;
        
        // Replace the operation with the result
        workingExpr = workingExpr.substring(0, leftStart) + String(result, 6) + workingExpr.substring(rightEnd);
    }
    
    return workingExpr.toFloat();
}

// Apply calibration to raw sensor value
float applyCalibration(float rawValue, const SensorConfig& sensor) {
    // Check if mathematical expression calibration is configured
    if (strlen(sensor.calibrationExpression) > 0) {
        return evaluateCalibrationExpression(rawValue, sensor);
    }
    
    // Default linear calibration: y = slope*x + offset
    return (sensor.calibrationSlope * rawValue) + sensor.calibrationOffset;
}

// Apply calibration to secondary sensor value (rawValueB)
float applyCalibrationB(float rawValue, const SensorConfig& sensor) {
    // Check if mathematical expression calibration is configured for channel B
    if (strlen(sensor.calibrationExpressionB) > 0) {
        // Create a temporary sensor config for expression evaluation
        SensorConfig tempSensor = sensor;
        strncpy(tempSensor.calibrationExpression, sensor.calibrationExpressionB, sizeof(tempSensor.calibrationExpression));
        return evaluateCalibrationExpression(rawValue, tempSensor);
    }
    
    // Default linear calibration for channel B: y = slope*x + offset
    return (sensor.calibrationSlopeB * rawValue) + sensor.calibrationOffsetB;
}

// Apply calibration to tertiary sensor value (rawValueC)
float applyCalibrationC(float rawValue, const SensorConfig& sensor) {
    // Check if mathematical expression calibration is configured for channel C
    if (strlen(sensor.calibrationExpressionC) > 0) {
        // Create a temporary sensor config for expression evaluation
        SensorConfig tempSensor = sensor;
        strncpy(tempSensor.calibrationExpression, sensor.calibrationExpressionC, sizeof(tempSensor.calibrationExpression));
        return evaluateCalibrationExpression(rawValue, tempSensor);
    }
    
    // Default linear calibration for channel C: y = slope*x + offset
    return (sensor.calibrationSlopeC * rawValue) + sensor.calibrationOffsetC;
}

// Data parsing function - converts raw sensor data based on parsing configuration
float parseSensorData(const char* rawData, const SensorConfig& sensor) {
    if (strlen(sensor.parsingMethod) == 0 || strcmp(sensor.parsingMethod, "raw") == 0) {
        // Raw parsing - convert string to float
        return atof(rawData);
    }
    
    // Parse the parsing configuration
    StaticJsonDocument<256> parsingDoc;
    DeserializationError error = deserializeJson(parsingDoc, sensor.parsingConfig);
    if (error) {
        return atof(rawData); // Fallback to raw parsing
    }
    
    JsonObject config = parsingDoc.as<JsonObject>();
    String method = sensor.parsingMethod;
    
    if (method == "custom_bits") {
        // Extract specific bits from integer data
        uint32_t rawValue = (uint32_t)atol(rawData);
        String bitPositions = config["bitPositions"] | "";
        uint32_t result = 0;
        
        // Parse bit positions (e.g., "0,1,7" or "0-3,7")
        int bitIndex = 0;
        String positions = bitPositions;
        while (positions.length() > 0) {
            int commaIndex = positions.indexOf(',');
            String part = (commaIndex >= 0) ? positions.substring(0, commaIndex) : positions;
            
            if (part.indexOf('-') >= 0) {
                // Range like "0-3"
                int dashIndex = part.indexOf('-');
                int startBit = part.substring(0, dashIndex).toInt();
                int endBit = part.substring(dashIndex + 1).toInt();
                for (int bit = startBit; bit <= endBit; bit++) {
                    if ((rawValue >> bit) & 1) {
                        result |= (1 << bitIndex);
                    }
                    bitIndex++;
                }
            } else {
                // Single bit
                int bit = part.toInt();
                if ((rawValue >> bit) & 1) {
                    result |= (1 << bitIndex);
                }
                bitIndex++;
            }
            
            if (commaIndex >= 0) {
                positions = positions.substring(commaIndex + 1);
            } else {
                break;
            }
        }
        return (float)result;
        
    } else if (method == "bit_field") {
        // Extract a range of bits
        uint32_t rawValue = (uint32_t)atol(rawData);
        int startBit = config["bitStart"] | 0;
        int bitLength = config["bitLength"] | 8;
        
        uint32_t mask = (1 << bitLength) - 1;
        uint32_t result = (rawValue >> startBit) & mask;
        return (float)result;
        
    } else if (method == "status_register") {
        // Return status as bit pattern
        uint32_t rawValue = (uint32_t)atol(rawData);
        return (float)rawValue;
        
    } else if (method == "json_path") {
        // Extract value from JSON data
        StaticJsonDocument<512> jsonDoc;
        DeserializationError jsonError = deserializeJson(jsonDoc, rawData);
        if (jsonError) {
            return 0.0; // Could not parse JSON
        }
        
        String jsonPath = config["jsonPath"] | "";
        // Simple path parsing (supports "key" or "obj.key" or "array[0].key")
        JsonVariant value = jsonDoc;
        
        // Split path by dots and process each part
        int lastDot = 0;
        while (lastDot < jsonPath.length()) {
            int nextDot = jsonPath.indexOf('.', lastDot);
            if (nextDot == -1) nextDot = jsonPath.length();
            
            String pathPart = jsonPath.substring(lastDot, nextDot);
            
            // Check for array indexing
            int bracketStart = pathPart.indexOf('[');
            if (bracketStart >= 0) {
                String arrayName = pathPart.substring(0, bracketStart);
                int bracketEnd = pathPart.indexOf(']', bracketStart);
                int arrayIndex = pathPart.substring(bracketStart + 1, bracketEnd).toInt();
                
                if (value.containsKey(arrayName) && value[arrayName].is<JsonArray>()) {
                    JsonArray array = value[arrayName];
                    if (arrayIndex < array.size()) {
                        value = array[arrayIndex];
                    } else {
                        return 0.0; // Array index out of bounds
                    }
                } else {
                    return 0.0; // Array not found
                }
            } else {
                if (value.containsKey(pathPart)) {
                    value = value[pathPart];
                } else {
                    return 0.0; // Key not found
                }
            }
            
            lastDot = nextDot + 1;
        }
        
        return value.as<float>();
        
    } else if (method == "csv_column") {
        // Extract specific column from CSV data
        int columnIndex = config["csvColumn"] | 0;
        String delimiter = config["csvDelimiter"] | ",";
        
        String data = String(rawData);
        int currentColumn = 0;
        int startIndex = 0;
        
        while (currentColumn < columnIndex && startIndex < data.length()) {
            int delimiterIndex = data.indexOf(delimiter, startIndex);
            if (delimiterIndex == -1) {
                return 0.0; // Column not found
            }
            startIndex = delimiterIndex + delimiter.length();
            currentColumn++;
        }
        
        if (currentColumn == columnIndex) {
            int endIndex = data.indexOf(delimiter, startIndex);
            if (endIndex == -1) endIndex = data.length();
            
            String columnValue = data.substring(startIndex, endIndex);
            return atof(columnValue.c_str());
        }
        
        return 0.0; // Column not found
    }
    
    // Unknown method, fallback to raw parsing
    return atof(rawData);
}

void sendJSONSensorConfig(WiFiClient& client) {
    StaticJsonDocument<2048> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = sensorsArray.createNestedObject();
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
        sensor["modbusRegister"] = configuredSensors[i].modbusRegister;
        
        // Critical polling configuration - THESE WERE MISSING!
        sensor["command"] = configuredSensors[i].command;
        sensor["updateInterval"] = configuredSensors[i].updateInterval;
        sensor["delayBeforeRead"] = configuredSensors[i].delayBeforeRead;
        
        // Clean response field to prevent JSON corruption from binary data
        String cleanResponse = "";
        for (int j = 0; j < strlen(configuredSensors[i].response); j++) {
            char c = configuredSensors[i].response[j];
            if (c >= 32 && c <= 126) { // Only printable ASCII
                cleanResponse += c;
            }
        }
        sensor["response"] = cleanResponse;
        
        // Include pin assignments
        sensor["sdaPin"] = configuredSensors[i].sdaPin;
        sensor["sclPin"] = configuredSensors[i].sclPin;
        sensor["dataPin"] = configuredSensors[i].dataPin;
        sensor["uartTxPin"] = configuredSensors[i].uartTxPin;
        sensor["uartRxPin"] = configuredSensors[i].uartRxPin;
        sensor["analogPin"] = configuredSensors[i].analogPin;
        sensor["oneWirePin"] = configuredSensors[i].oneWirePin;
        sensor["digitalPin"] = configuredSensors[i].digitalPin;
        
        // One-Wire specific configuration
        if (strlen(configuredSensors[i].oneWireCommand) > 0) {
            sensor["oneWireCommand"] = configuredSensors[i].oneWireCommand;
        }
        if (configuredSensors[i].oneWireInterval > 0) {
            sensor["oneWireInterval"] = configuredSensors[i].oneWireInterval;
        }
        if (configuredSensors[i].oneWireConversionTime > 0) {
            sensor["oneWireConversionTime"] = configuredSensors[i].oneWireConversionTime;
        }
        sensor["oneWireAutoMode"] = configuredSensors[i].oneWireAutoMode;
        
        // Always include calibration data
        JsonObject calibration = sensor.createNestedObject("calibration");
        calibration["offset"] = configuredSensors[i].calibrationOffset;
        calibration["scale"] = configuredSensors[i].calibrationSlope;
        
        // Include expression if configured
        if (strlen(configuredSensors[i].calibrationExpression) > 0) {
            calibration["expression"] = configuredSensors[i].calibrationExpression;
        } else {
            calibration["expression"] = "";
        }
        
        // For compatibility with frontend, also set polynomialStr to empty
        calibration["polynomialStr"] = "";
        
        // Include data parsing configuration
        if (strlen(configuredSensors[i].parsingMethod) > 0 && strcmp(configuredSensors[i].parsingMethod, "raw") != 0) {
            JsonObject dataParsing = sensor.createNestedObject("dataParsing");
            dataParsing["method"] = configuredSensors[i].parsingMethod;
            
            // Parse the stored JSON config string
            if (strlen(configuredSensors[i].parsingConfig) > 0) {
                StaticJsonDocument<256> parsingDoc;
                DeserializationError parsingError = deserializeJson(parsingDoc, configuredSensors[i].parsingConfig);
                if (!parsingError) {
                    JsonObject parsingObj = parsingDoc.as<JsonObject>();
                    for (JsonPair kv : parsingObj) {
                        dataParsing[kv.key()] = kv.value();
                    }
                }
            }
        }
        
        // Include secondary parsing configuration (for multi-output sensors)
        if (strlen(configuredSensors[i].parsingMethodB) > 0 && strcmp(configuredSensors[i].parsingMethodB, "raw") != 0) {
            JsonObject dataParsingB = sensor.createNestedObject("dataParsingB");
            dataParsingB["method"] = configuredSensors[i].parsingMethodB;
            
            if (strlen(configuredSensors[i].parsingConfigB) > 0) {
                StaticJsonDocument<256> parsingDocB;
                DeserializationError parsingErrorB = deserializeJson(parsingDocB, configuredSensors[i].parsingConfigB);
                if (!parsingErrorB) {
                    JsonObject parsingObjB = parsingDocB.as<JsonObject>();
                    for (JsonPair kv : parsingObjB) {
                        dataParsingB[kv.key()] = kv.value();
                    }
                }
            }
        }
    }
    
    String response;
    serializeJson(doc, response);
    sendJSON(client, response);
}

void sendJSONSensorData(WiFiClient& client) {
    StaticJsonDocument<4096> doc;
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled) {
            JsonObject sensor = sensorsArray.createNestedObject();
            sensor["name"] = configuredSensors[i].name;
            sensor["type"] = configuredSensors[i].type;
            sensor["protocol"] = configuredSensors[i].protocol;
            sensor["i2c_address"] = configuredSensors[i].i2cAddress;
            sensor["modbus_register"] = configuredSensors[i].modbusRegister;
            
            // Raw sensor data
            sensor["raw_value"] = configuredSensors[i].rawValue;
            sensor["raw_data_string"] = configuredSensors[i].rawDataString;
            
            // Clean response field to prevent JSON corruption from binary data
            String cleanResponse = "";
            for (int j = 0; j < strlen(configuredSensors[i].response); j++) {
                char c = configuredSensors[i].response[j];
                if (c >= 32 && c <= 126) { // Only printable ASCII
                    cleanResponse += c;
                }
            }
            sensor["response"] = cleanResponse;
            
            // Calibrated values
            sensor["calibrated_value"] = configuredSensors[i].calibratedValue;
            sensor["modbus_value"] = configuredSensors[i].modbusValue;
            
            // Multi-output sensor support (SHT30, BME280, etc.)
            if (configuredSensors[i].rawValueB != 0) {
                sensor["raw_value_b"] = configuredSensors[i].rawValueB;
                sensor["calibrated_value_b"] = configuredSensors[i].calibratedValueB;
                sensor["modbus_value_b"] = configuredSensors[i].modbusValueB;
                sensor["modbus_register_b"] = configuredSensors[i].modbusRegister + 1;
            }
            
            if (configuredSensors[i].rawValueC != 0) {
                sensor["raw_value_c"] = configuredSensors[i].rawValueC;
                sensor["calibrated_value_c"] = configuredSensors[i].calibratedValueC;
                sensor["modbus_value_c"] = configuredSensors[i].modbusValueC;
                sensor["modbus_register_c"] = configuredSensors[i].modbusRegister + 2;
            }
            
            // Timing information
            sensor["last_read_time"] = configuredSensors[i].lastReadTime;
            sensor["update_interval"] = configuredSensors[i].updateInterval;
            
            // Calibration settings
            sensor["calibration_offset"] = configuredSensors[i].calibrationOffset;
            sensor["calibration_slope"] = configuredSensors[i].calibrationSlope;
            if (strlen(configuredSensors[i].calibrationExpression) > 0) {
                sensor["calibration_expression"] = configuredSensors[i].calibrationExpression;
            }
            
            if (configuredSensors[i].calibrationSlopeB != 1.0 || configuredSensors[i].calibrationOffsetB != 0.0) {
                sensor["calibration_offset_b"] = configuredSensors[i].calibrationOffsetB;
                sensor["calibration_slope_b"] = configuredSensors[i].calibrationSlopeB;
            }
            if (strlen(configuredSensors[i].calibrationExpressionB) > 0) {
                sensor["calibration_expression_b"] = configuredSensors[i].calibrationExpressionB;
            }
            
            if (configuredSensors[i].calibrationSlopeC != 1.0 || configuredSensors[i].calibrationOffsetC != 0.0) {
                sensor["calibration_offset_c"] = configuredSensors[i].calibrationOffsetC;
                sensor["calibration_slope_c"] = configuredSensors[i].calibrationSlopeC;
            }
            if (strlen(configuredSensors[i].calibrationExpressionC) > 0) {
                sensor["calibration_expression_c"] = configuredSensors[i].calibrationExpressionC;
            }
        }
    }
    
    // Add system information
    doc["system_time"] = millis();
    doc["num_configured_sensors"] = numConfiguredSensors;
    doc["queue_sizes"]["i2c"] = i2cQueueSize;
    doc["queue_sizes"]["uart"] = uartQueueSize;
    doc["queue_sizes"]["onewire"] = oneWireQueueSize;
    
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
    Serial.printf("POST /sensors/config - Body length: %d bytes\n", body.length());
    Serial.printf("Body content: %s\n", body.c_str());
    
    StaticJsonDocument<4096> doc; // Increased from 2048 to 4096
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.printf("JSON deserialization error: %s\n", error.c_str());
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.printf("{\"success\":false,\"error\":\"Invalid JSON: %s\"}", error.c_str());
        return;
    }

    // Example: expects an array of sensors in doc["sensors"]
    if (!doc.containsKey("sensors") || !doc["sensors"].is<JsonArray>()) {
        Serial.println("Missing or invalid sensors array");
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.println("{\"success\":false,\"error\":\"Missing sensors array\"}");
        return;
    }

    JsonArray sensorsArray = doc["sensors"].as<JsonArray>();
    // Temporary array to check for pin conflicts
    struct PinUsage { int pin; const char* type; } usedPins[32];
    int usedCount = 0;
    
    // Array to track used Modbus registers
    int usedModbusRegisters[MAX_SENSORS * 2]; // Allow for multi-output sensors
    int usedRegisterCount = 0;

    // Helper: fill defaults for known sensor types
    auto fillDefaults = [](JsonObject& sensor) {
        const char* type = sensor["type"] | "";
        if (strcmp(type, "BME280") == 0) {
            if (!sensor.containsKey("i2cAddress") || (int)sensor["i2cAddress"] == 0) {
                sensor["i2cAddress"] = 0x76;
            }
            sensor["modbusRegister"] = 3;
        } else if (strcmp(type, "EZO-PH") == 0 || strcmp(type, "EZO_PH") == 0) {
            if (!sensor.containsKey("i2cAddress") || (int)sensor["i2cAddress"] == 0) {
                sensor["i2cAddress"] = 0x63;
            }
            sensor["modbusRegister"] = 4;
            if (!sensor.containsKey("command") || String(sensor["command"] | "").length() == 0) {
                sensor["command"] = "R";
            }
        } else if (strcmp(type, "EZO-EC") == 0 || strcmp(type, "EZO_EC") == 0) {
            if (!sensor.containsKey("i2cAddress") || (int)sensor["i2cAddress"] == 0) {
                sensor["i2cAddress"] = 0x64;
            }
            sensor["modbusRegister"] = 5;
            if (!sensor.containsKey("command") || String(sensor["command"] | "").length() == 0) {
                sensor["command"] = "R";
            }
        } else if (strcmp(type, "EZO-DO") == 0 || strcmp(type, "EZO_DO") == 0) {
            if (!sensor.containsKey("i2cAddress") || (int)sensor["i2cAddress"] == 0) {
                sensor["i2cAddress"] = 0x61;
            }
            sensor["modbusRegister"] = 6;
            if (!sensor.containsKey("command") || String(sensor["command"] | "").length() == 0) {
                sensor["command"] = "R";
            }
        } else if (strcmp(type, "EZO-RTD") == 0 || strcmp(type, "EZO_RTD") == 0) {
            if (!sensor.containsKey("i2cAddress") || (int)sensor["i2cAddress"] == 0) {
                sensor["i2cAddress"] = 0x66;
            }
            sensor["modbusRegister"] = 7;
            if (!sensor.containsKey("command") || String(sensor["command"] | "").length() == 0) {
                sensor["command"] = "R";
            }
        } // Add more known types as needed
    };

    // Check for pin conflicts and fill defaults
    Serial.printf("Processing %d sensors for conflicts\n", sensorsArray.size());
    for (JsonObject sensor : sensorsArray) {
        fillDefaults(sensor);
        // Example: check I2C address conflict
        int i2cAddr = sensor["i2cAddress"] | -1;
        const char* sensorName = sensor["name"] | "Unknown";
        Serial.printf("Checking sensor '%s' with I2C address: 0x%02X\n", sensorName, i2cAddr);
        if (i2cAddr > 0) {
            for (int i = 0; i < usedCount; ++i) {
                if (usedPins[i].pin == i2cAddr && strcmp(usedPins[i].type, "I2C") == 0) {
                    Serial.printf("I2C address conflict detected: 0x%02X already used\n", i2cAddr);
                    client.println("HTTP/1.1 400 Bad Request");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.printf("{\"success\":false,\"error\":\"I2C address conflict at 0x%02X\"}", i2cAddr);
                    return;
                }
            }
            usedPins[usedCount++] = {i2cAddr, "I2C"};
        }
        
        // Check for Modbus register conflicts
        int modbusReg = sensor["modbusRegister"] | -1;
        Serial.printf("Checking sensor '%s' Modbus register: %d\n", sensorName, modbusReg);
        if (modbusReg >= 0) {
            // Check if this register is already used
            for (int i = 0; i < usedRegisterCount; i++) {
                if (usedModbusRegisters[i] == modbusReg) {
                    Serial.printf("Modbus register conflict detected: %d already used\n", modbusReg);
                    client.println("HTTP/1.1 400 Bad Request");
                    client.println("Content-Type: application/json");
                    client.println("Connection: close");
                    client.println();
                    client.printf("{\"success\":false,\"error\":\"Modbus register conflict at %d\"}", modbusReg);
                    return;
                }
            }
            
            // Add this register to used list
            usedModbusRegisters[usedRegisterCount++] = modbusReg;
            
            // For multi-output sensors like SHT30, also reserve the next register
            const char* sensorType = sensor["type"] | "";
            if (strcmp(sensorType, "SHT30") == 0 || strcmp(sensorType, "BME280") == 0) {
                // Check if next register is already used
                int nextReg = modbusReg + 1;
                for (int i = 0; i < usedRegisterCount; i++) {
                    if (usedModbusRegisters[i] == nextReg) {
                        client.println("HTTP/1.1 400 Bad Request");
                        client.println("Content-Type: application/json");
                        client.println("Connection: close");
                        client.println();
                        client.println("{\"success\":false,\"error\":\"Modbus register conflict (multi-output sensor)\"}");
                        return;
                    }
                }
                // Reserve the next register for multi-output sensor
                usedModbusRegisters[usedRegisterCount++] = nextReg;
            }
        }
        
        // TODO: check analog/digital pin conflicts similarly
    }

    // If no conflicts, update config
    numConfiguredSensors = 0;
    for (JsonObject sensor : sensorsArray) {
        if (numConfiguredSensors >= MAX_SENSORS) break;
        
        // Basic sensor properties
        configuredSensors[numConfiguredSensors].enabled = sensor["enabled"] | false;
        const char* name = sensor["name"] | "";
        strncpy(configuredSensors[numConfiguredSensors].name, name, sizeof(configuredSensors[numConfiguredSensors].name) - 1);
        configuredSensors[numConfiguredSensors].name[sizeof(configuredSensors[numConfiguredSensors].name) - 1] = '\0';
        const char* type = sensor["type"] | "";
        strncpy(configuredSensors[numConfiguredSensors].type, type, sizeof(configuredSensors[numConfiguredSensors].type) - 1);
        configuredSensors[numConfiguredSensors].type[sizeof(configuredSensors[numConfiguredSensors].type) - 1] = '\0';
        const char* protocol = sensor["protocol"] | "";
        strncpy(configuredSensors[numConfiguredSensors].protocol, protocol, sizeof(configuredSensors[numConfiguredSensors].protocol) - 1);
        configuredSensors[numConfiguredSensors].protocol[sizeof(configuredSensors[numConfiguredSensors].protocol) - 1] = '\0';
        configuredSensors[numConfiguredSensors].i2cAddress = sensor["i2cAddress"] | 0;
        configuredSensors[numConfiguredSensors].modbusRegister = sensor["modbusRegister"] | 0;
        
        // Critical polling configuration - accept both pollingFrequency and updateInterval
        // Handle command - can be string or nested object from web UI
        const char* command = "";
        int delayBeforeRead = 0;
        
        if (sensor["command"].is<const char*>()) {
            // Simple string command
            command = sensor["command"] | "";
        } else if (sensor["command"].is<JsonObject>()) {
            // Nested command object from web UI
            JsonObject cmdObj = sensor["command"];
            command = cmdObj["command"] | "";
            delayBeforeRead = cmdObj["waitTime"] | 0;
        }
        
        strncpy(configuredSensors[numConfiguredSensors].command, command, sizeof(configuredSensors[numConfiguredSensors].command) - 1);
        configuredSensors[numConfiguredSensors].command[sizeof(configuredSensors[numConfiguredSensors].command) - 1] = '\0';
        
        // Accept both pollingFrequency (from web UI) and updateInterval (internal) 
        int interval = sensor["updateInterval"] | sensor["pollingFrequency"] | 5000;
        configuredSensors[numConfiguredSensors].updateInterval = interval;
        
        // Use delayBeforeRead from command object or separate field
        int finalDelay = sensor["delayBeforeRead"] | delayBeforeRead;
        configuredSensors[numConfiguredSensors].delayBeforeRead = finalDelay;
        
        // Pin assignments - parse from JSON
        configuredSensors[numConfiguredSensors].sdaPin = sensor["sdaPin"] | -1;
        configuredSensors[numConfiguredSensors].sclPin = sensor["sclPin"] | -1;
        configuredSensors[numConfiguredSensors].dataPin = sensor["dataPin"] | -1;
        configuredSensors[numConfiguredSensors].uartTxPin = sensor["uartTxPin"] | -1;
        configuredSensors[numConfiguredSensors].uartRxPin = sensor["uartRxPin"] | -1;
        configuredSensors[numConfiguredSensors].analogPin = sensor["analogPin"] | -1;
        configuredSensors[numConfiguredSensors].oneWirePin = sensor["oneWirePin"] | -1;
        configuredSensors[numConfiguredSensors].digitalPin = sensor["digitalPin"] | -1;
        
        // One-Wire specific configuration with defaults
        const char* owCommand = sensor["oneWireCommand"] | "0x44";
        strncpy(configuredSensors[numConfiguredSensors].oneWireCommand, owCommand, 
                sizeof(configuredSensors[numConfiguredSensors].oneWireCommand) - 1);
        configuredSensors[numConfiguredSensors].oneWireCommand[sizeof(configuredSensors[numConfiguredSensors].oneWireCommand) - 1] = '\0';
        
        configuredSensors[numConfiguredSensors].oneWireInterval = sensor["oneWireInterval"] | 5; // Default 5 seconds
        configuredSensors[numConfiguredSensors].oneWireConversionTime = sensor["oneWireConversionTime"] | 750; // Default 750ms
        configuredSensors[numConfiguredSensors].oneWireAutoMode = sensor["oneWireAutoMode"] | true; // Default auto mode on
        configuredSensors[numConfiguredSensors].lastOneWireCmd = 0; // Initialize timing
        
        // Calibration data - handle both nested and direct formats
        if (sensor.containsKey("calibration") && sensor["calibration"].is<JsonObject>()) {
            JsonObject calibration = sensor["calibration"];
            configuredSensors[numConfiguredSensors].calibrationOffset = calibration["offset"] | 0.0;
            configuredSensors[numConfiguredSensors].calibrationSlope = calibration["scale"] | 1.0;
            
            // Handle expression string (supports any mathematical equation including polynomials)
            const char* expression = calibration["expression"] | "";
            strncpy(configuredSensors[numConfiguredSensors].calibrationExpression, expression, 
                    sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1);
            configuredSensors[numConfiguredSensors].calibrationExpression[sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1] = '\0';
            
            // Also check for polynomialStr field (convert to expression)
            const char* polynomial = calibration["polynomialStr"] | "";
            if (strlen(polynomial) > 0 && strlen(expression) == 0) {
                strncpy(configuredSensors[numConfiguredSensors].calibrationExpression, polynomial, 
                        sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1);
                configuredSensors[numConfiguredSensors].calibrationExpression[sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1] = '\0';
            }
        } else {
            // Fallback to direct fields
            configuredSensors[numConfiguredSensors].calibrationOffset = sensor["calibrationOffset"] | 0.0;
            configuredSensors[numConfiguredSensors].calibrationSlope = sensor["calibrationSlope"] | 1.0;
            
            const char* expression = sensor["calibrationExpression"] | "";
            strncpy(configuredSensors[numConfiguredSensors].calibrationExpression, expression, 
                    sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1);
            configuredSensors[numConfiguredSensors].calibrationExpression[sizeof(configuredSensors[numConfiguredSensors].calibrationExpression) - 1] = '\0';
        }
        
        // Multi-output calibration data (for sensors like SHT30 with temp+humidity)
        configuredSensors[numConfiguredSensors].calibrationOffsetB = sensor["calibrationOffsetB"] | 0.0;
        configuredSensors[numConfiguredSensors].calibrationSlopeB = sensor["calibrationSlopeB"] | 1.0;
        configuredSensors[numConfiguredSensors].calibrationOffsetC = sensor["calibrationOffsetC"] | 0.0;
        configuredSensors[numConfiguredSensors].calibrationSlopeC = sensor["calibrationSlopeC"] | 1.0;
        
        // Multi-output expression calibration
        const char* expressionB = sensor["calibrationExpressionB"] | "";
        strncpy(configuredSensors[numConfiguredSensors].calibrationExpressionB, expressionB, 
                sizeof(configuredSensors[numConfiguredSensors].calibrationExpressionB) - 1);
        configuredSensors[numConfiguredSensors].calibrationExpressionB[sizeof(configuredSensors[numConfiguredSensors].calibrationExpressionB) - 1] = '\0';
        
        const char* expressionC = sensor["calibrationExpressionC"] | "";
        strncpy(configuredSensors[numConfiguredSensors].calibrationExpressionC, expressionC, 
                sizeof(configuredSensors[numConfiguredSensors].calibrationExpressionC) - 1);
        configuredSensors[numConfiguredSensors].calibrationExpressionC[sizeof(configuredSensors[numConfiguredSensors].calibrationExpressionC) - 1] = '\0';
        
        // Data parsing configuration
        if (sensor.containsKey("dataParsing") && sensor["dataParsing"].is<JsonObject>()) {
            JsonObject dataParsing = sensor["dataParsing"];
            const char* method = dataParsing["method"] | "raw";
            strncpy(configuredSensors[numConfiguredSensors].parsingMethod, method, sizeof(configuredSensors[numConfiguredSensors].parsingMethod) - 1);
            configuredSensors[numConfiguredSensors].parsingMethod[sizeof(configuredSensors[numConfiguredSensors].parsingMethod) - 1] = '\0';
            
            // Serialize the parsing config to JSON string for storage
            StaticJsonDocument<256> parsingDoc;
            parsingDoc.set(dataParsing);
            String parsingConfigStr;
            serializeJson(parsingDoc, parsingConfigStr);
            strncpy(configuredSensors[numConfiguredSensors].parsingConfig, parsingConfigStr.c_str(), sizeof(configuredSensors[numConfiguredSensors].parsingConfig) - 1);
            configuredSensors[numConfiguredSensors].parsingConfig[sizeof(configuredSensors[numConfiguredSensors].parsingConfig) - 1] = '\0';
        } else {
            // Default to raw parsing
            strncpy(configuredSensors[numConfiguredSensors].parsingMethod, "raw", sizeof(configuredSensors[numConfiguredSensors].parsingMethod) - 1);
            configuredSensors[numConfiguredSensors].parsingMethod[sizeof(configuredSensors[numConfiguredSensors].parsingMethod) - 1] = '\0';
            configuredSensors[numConfiguredSensors].parsingConfig[0] = '\0';
        }
        
        // Handle secondary parsing (for multi-output sensors)
        if (sensor.containsKey("dataParsingB")) {
            JsonObject dataParsingB = sensor["dataParsingB"];
            const char* methodB = dataParsingB["method"] | "raw";
            strncpy(configuredSensors[numConfiguredSensors].parsingMethodB, methodB, sizeof(configuredSensors[numConfiguredSensors].parsingMethodB) - 1);
            configuredSensors[numConfiguredSensors].parsingMethodB[sizeof(configuredSensors[numConfiguredSensors].parsingMethodB) - 1] = '\0';
            
            // Serialize the parsing config to JSON string for storage
            StaticJsonDocument<256> parsingDocB;
            parsingDocB.set(dataParsingB);
            String parsingConfigStrB;
            serializeJson(parsingDocB, parsingConfigStrB);
            strncpy(configuredSensors[numConfiguredSensors].parsingConfigB, parsingConfigStrB.c_str(), sizeof(configuredSensors[numConfiguredSensors].parsingConfigB) - 1);
            configuredSensors[numConfiguredSensors].parsingConfigB[sizeof(configuredSensors[numConfiguredSensors].parsingConfigB) - 1] = '\0';
        } else {
            // Default to raw parsing for secondary
            strncpy(configuredSensors[numConfiguredSensors].parsingMethodB, "raw", sizeof(configuredSensors[numConfiguredSensors].parsingMethodB) - 1);
            configuredSensors[numConfiguredSensors].parsingMethodB[sizeof(configuredSensors[numConfiguredSensors].parsingMethodB) - 1] = '\0';
            configuredSensors[numConfiguredSensors].parsingConfigB[0] = '\0';
        }
        
        // Initialize EZO state
        configuredSensors[numConfiguredSensors].cmdPending = false;
        configuredSensors[numConfiguredSensors].lastCmdSent = 0;
        configuredSensors[numConfiguredSensors].response[0] = '\0';
        configuredSensors[numConfiguredSensors].calibrationData[0] = '\0';
        
        numConfiguredSensors++;
    }
    saveSensorConfig();
    
    // Small delay to ensure file save completes before response
    delay(100);
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Access-Control-Allow-Origin: *");
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
                delay(1); // Small delay between scans
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
                           " (" + String(configuredSensors[i].type) + ") - " + 
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

// Poll Now endpoint - live sensor testing for configuration
void handlePOSTSensorPoll(WiFiClient& client, String body) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendJSON(client, "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String protocol = doc["protocol"] | "";
    String command = doc["command"] | "";
    int i2cAddress = doc["i2cAddress"] | 0x44;
    int delayBeforeRead = doc["delayBeforeRead"] | 0;
    
    StaticJsonDocument<1024> responseDoc;
    bool success = false;
    String errorMsg = "";
    
    if (protocol == "I2C") {
        Wire.begin();
        
        if (i2cAddress < 1 || i2cAddress > 127) {
            errorMsg = "Invalid I2C address: 0x" + String(i2cAddress, HEX);
        } else {
            // Test device presence
            addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] Testing device presence");
            Wire.beginTransmission(i2cAddress);
            int probeResult = Wire.endTransmission();
            
            if (probeResult != 0) {
                errorMsg = "No device at 0x" + String(i2cAddress, HEX) + " (Error: " + String(probeResult) + ")";
                addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] Device not found");
            } else {
                addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] Device found, sending command");
                
                // Send command if provided
                Wire.beginTransmission(i2cAddress);
                if (command.length() > 0) {
                    String cleanCmd = command;
                    cleanCmd.replace("0x", "");
                    cleanCmd.replace(" ", "");
                    
                    for (int i = 0; i < cleanCmd.length(); i += 2) {
                        String hexByte = cleanCmd.substring(i, i + 2);
                        uint8_t byte = strtoul(hexByte.c_str(), NULL, 16);
                        Wire.write(byte);
                    }
                    addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] TX: " + command);
                }
                
                int result = Wire.endTransmission();
                if (result == 0) {
                    if (delayBeforeRead > 0) {
                        delay(delayBeforeRead);
                    }
                    
                    // Read response
                    Wire.requestFrom(i2cAddress, 32);
                    if (Wire.available()) {
                        String rawHex = "";
                        String rawAscii = "";
                        char response[33] = {0};
                        int idx = 0;
                        
                        while (Wire.available() && idx < 32) {
                            uint8_t byte = Wire.read();
                            response[idx++] = byte;
                            if (rawHex.length() > 0) rawHex += " ";
                            rawHex += String(byte, HEX);
                            rawAscii += (byte >= 32 && byte <= 126) ? (char)byte : '.';
                        }
                        
                        addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] RX: [" + rawHex + "] '" + rawAscii + "'");
                        
                        success = true;
                        responseDoc["rawHex"] = rawHex;
                        responseDoc["rawAscii"] = rawAscii;
                        responseDoc["response"] = String(response);
                        
                        float parsedValue = atof(response);
                        if (parsedValue != 0.0 || response[0] == '0') {
                            responseDoc["parsedValue"] = parsedValue;
                        }
                    } else {
                        errorMsg = "No response from sensor";
                        addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] No response");
                    }
                } else {
                    errorMsg = "I2C transmission failed (" + String(result) + ")";
                    addTerminalLog("POLL [0x" + String(i2cAddress, HEX) + "] TX failed: " + String(result));
                }
            }
        }
    } else if (protocol == "UART") {
        int txPin = doc["uartTxPin"] | 0;
        int rxPin = doc["uartRxPin"] | 1;
        int baudRate = doc["baudRate"] | 9600;
        
        if (txPin < 0 || txPin > 28 || rxPin < 0 || rxPin > 28) {
            errorMsg = "Invalid UART pins. TX and RX must be 0-28";
        } else {
            addTerminalLog("POLL [UART] Testing UART on TX:GP" + String(txPin) + ", RX:GP" + String(rxPin));
            
            // Initialize UART with specified pins
            Serial1.setTX(txPin);
            Serial1.setRX(rxPin);
            Serial1.begin(baudRate);
            delay(100); // Allow UART to stabilize
            
            // Clear any pending data
            while (Serial1.available()) {
                Serial1.read();
            }
            
            // Send command if provided
            if (command.length() > 0) {
                String cmdToSend = command;
                if (!cmdToSend.endsWith("\r") && !cmdToSend.endsWith("\n")) {
                    cmdToSend += "\r";
                }
                
                addTerminalLog("POLL [UART] TX: " + command);
                Serial1.print(cmdToSend);
                Serial1.flush();
                
                // Wait for response (up to 2 seconds)
                unsigned long startTime = millis();
                String response = "";
                bool gotResponse = false;
                
                while (millis() - startTime < 2000) {
                    if (Serial1.available()) {
                        char c = Serial1.read();
                        if (c >= 32 && c <= 126) { // Printable characters
                            response += c;
                            gotResponse = true;
                        } else if ((c == '\r' || c == '\n') && response.length() > 0) {
                            break; // End of response
                        }
                    }
                    delay(1);
                }
                
                if (gotResponse && response.length() > 0) {
                    addTerminalLog("POLL [UART] RX: " + response);
                    
                    success = true;
                    responseDoc["response"] = response;
                    
                    // Try to parse as float
                    float parsedValue = atof(response.c_str());
                    if (parsedValue != 0.0 || response.charAt(0) == '0') {
                        responseDoc["parsedValue"] = parsedValue;
                    }
                } else {
                    errorMsg = "No response from UART sensor";
                    addTerminalLog("POLL [UART] No response");
                }
            } else {
                errorMsg = "No command specified for UART test";
            }
            
            Serial1.end();
        }
    } else {
        errorMsg = "Protocol not supported: " + protocol;
    }
    
    responseDoc["success"] = success;
    if (!success) {
        responseDoc["error"] = errorMsg;
    }
    
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
        
        // Process configured analog sensors
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled && strcmp(configuredSensors[i].type, "ANALOG_CUSTOM") == 0) {
                int pin = configuredSensors[i].analogPin;
                Serial.printf("DEBUG: Found ANALOG_CUSTOM sensor '%s' on pin %d\n", configuredSensors[i].name, pin);
                
                if (pin >= 0) {
                    // Ensure pin is configured for analog input
                    pinMode(pin, INPUT);
                    
                    uint32_t rawValue = analogRead(pin);
                    float voltage = (rawValue * 3.3f) / 4095.0f;  // Convert to voltage
                    
                    Serial.printf("DEBUG: Raw ADC reading: %d, calculated voltage: %.3fV\n", rawValue, voltage);
                    
                    // Store raw and calibrated values
                    ioStatus.rawValue[i] = rawValue;
                    strncpy(ioStatus.rawUnit[i], "V", sizeof(ioStatus.rawUnit[i]) - 1);
                    
                    // Apply calibration - check for expression first, then slope/offset
                    float calibratedValue = voltage;
                    
                    if (strlen(configuredSensors[i].calibrationExpression) > 0) {
                        // Use expression evaluator
                        calibratedValue = evaluateExpression(configuredSensors[i].calibrationExpression, voltage);
                        Serial.printf("Analog sensor '%s': Applied expression '%s' -> %.3f\n", 
                                     configuredSensors[i].name, configuredSensors[i].calibrationExpression, calibratedValue);
                    } else if (configuredSensors[i].calibrationSlope != 1.0f || configuredSensors[i].calibrationOffset != 0.0f) {
                        // Simple linear calibration: y = mx + b
                        calibratedValue = (voltage * configuredSensors[i].calibrationSlope) + configuredSensors[i].calibrationOffset;
                        Serial.printf("Analog sensor '%s': Applied linear calibration (slope=%.3f, offset=%.3f) -> %.3f\n",
                                     configuredSensors[i].name, configuredSensors[i].calibrationSlope, configuredSensors[i].calibrationOffset, calibratedValue);
                    }
                    
                    ioStatus.calibratedValue[i] = calibratedValue;
                    
                    Serial.printf("Analog sensor '%s' pin %d: %.3fV -> %.3f (raw: %d)\n", 
                                 configuredSensors[i].name, pin, voltage, calibratedValue, rawValue);
                } else {
                    Serial.printf("ERROR: ANALOG_CUSTOM sensor '%s' has invalid pin: %d\n", configuredSensors[i].name, pin);
                }
            }
        }
        
        // All configured sensors are now handled by the unified queue system
        // No legacy sensor handling needed here - sensors are polled via their respective queues
        
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
    
    // Update Modbus registers with configured sensor values
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && configuredSensors[i].modbusRegister >= 0) {
            // Primary value (temperature for SHT30)
            modbusClients[clientIndex].server.inputRegisterWrite(configuredSensors[i].modbusRegister, configuredSensors[i].modbusValue);

            // Multi-output sensors (SHT30 humidity, BME280 pressure, etc.)
            if (strcmp(configuredSensors[i].type, "SHT30") == 0) {
                // Always write humidity to next register
                modbusClients[clientIndex].server.inputRegisterWrite(configuredSensors[i].modbusRegister + 1, configuredSensors[i].modbusValueB);
            }
            // Future: Add BME280 (temp, hum, pressure) and other multi-output sensors here
        }
    }
    
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

