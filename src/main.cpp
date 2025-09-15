#include "sys_init.h"
#include <Wire.h>
#include "io_manager.h"
#include "sensor_manager.h"

// Global variable definitions (moved from header)
Config config = DEFAULT_CONFIG;
IOStatus ioStatus = {};
WebServer webServer(80);
WiFiServer modbusServer(502);
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_RST);
WiFiClient client;
ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS] = {};
int connectedClients = 0;
bool core0setupComplete = false;

SensorConfig configuredSensors[MAX_SENSORS];
int numConfiguredSensors = 0;
PinAllocation pinAllocations[40];
int numAllocatedPins = 0;

// Pin configuration constants
const uint8_t I2C_PIN_PAIRS[][2] = {
    {4, 5},    // Primary I2C pair (Physical pins 6, 7)
    {2, 3},    // Alternative I2C pair (Physical pins 4, 5)
    {6, 7}     // Another alternative I2C pair (Physical pins 9, 10)
};

const uint8_t AVAILABLE_FLEXIBLE_PINS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 23};
const uint8_t ADC_PINS[] = {26, 27, 28};

// Forward declarations
void setupWebServer();
void handleModbusClients();
void loadConfig();
void saveConfig();
void loadSensorConfig();
void saveSensorConfig();

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Modbus IO Module Starting...");
    
    // Initialize file system
    if (!LittleFS.begin()) {
        Serial.println("LittleFS initialization failed");
        delay(5000);
        rp2040.restart();
    }
    Serial.println("LittleFS initialized");
    
    // Load configuration
    loadConfig();
    loadSensorConfig();
    
    // Initialize pin allocations
    initializePinAllocations();
    
    // Initialize modules
    IOManager::init();
    SensorManager::init();
    
    // Initialize Ethernet
    eth.setSPISpeed(1000000);
    if (!eth.begin()) {
        Serial.println("Ethernet initialization failed!");
        delay(5000);
        rp2040.restart();
    }
    
    // Configure network
    if (config.dhcpEnabled) {
        Serial.println("Starting DHCP...");
        if (!eth.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE)) {
            Serial.println("DHCP failed!");
        }
    } else {
        IPAddress staticIP(config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
        IPAddress gateway(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]);
        IPAddress subnet(config.subnet[0], config.subnet[1], config.subnet[2], config.subnet[3]);
        eth.config(staticIP, gateway, subnet);
    }
    
    // Start Modbus server
    modbusServer.begin();
    Serial.printf("Modbus TCP server started on port %d\n", config.modbusPort);
    
    // Setup web server
    setupWebServer();
    webServer.begin();
    Serial.printf("Web server started on port 80\n");
    
    // Initialize watchdog
    rp2040.wdt_begin(WDT_TIMEOUT);
    
    // Setup complete
    core0setupComplete = true;
    Serial.println("System initialization complete");
    Serial.printf("IP Address: %s\n", eth.localIP().toString().c_str());
}

void loop() {
    static unsigned long lastMemoryReport = 0;
    static unsigned long lastYield = 0;
    
    // Reset watchdog
    rp2040.wdt_reset();
    
    // Yield periodically for system stability
    if (millis() - lastYield > 10) {
        yield();
        lastYield = millis();
    }
    
    // Handle Modbus clients
    handleModbusClients();
    
    // Update IO and sensors through modules
    IOManager::updateIOState();
    SensorManager::updateAllSensors();
    
    // Handle web server
    webServer.handleClient();
    
    // Memory monitoring (every 30 seconds)
    if (millis() - lastMemoryReport > 30000) {
        Serial.printf("Free heap: %d bytes\n", rp2040.getFreeHeap());
        lastMemoryReport = millis();
    }
}

void handleModbusClients() {
    // Check for new client connections
    WiFiClient newClient = modbusServer.accept();
    
    if (newClient) {
        // Find empty slot for new client
        for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
            if (!modbusClients[i].connected) {
                Serial.printf("New client connected to slot %d\n", i);
                
                // Store the client and mark as connected
                modbusClients[i].client = newClient;
                modbusClients[i].connected = true;
                modbusClients[i].server.begin();
                modbusClients[i].connectionTime = millis();
                
                Serial.println("Modbus server accepted client connection");
                
                // Initialize coil states for this client to match current output states
                for (int j = 0; j < 8; j++) {
                    modbusClients[i].server.coilWrite(j, ioStatus.dOut[j]);
                }
                
                connectedClients++;
                digitalWrite(LED_BUILTIN, HIGH);
                break;
            }
        }
    }
    
    // Poll existing clients and update their registers
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (modbusClients[i].connected) {
            if (!modbusClients[i].client.connected()) {
                // Client disconnected
                Serial.printf("Client disconnected from slot %d\n", i);
                modbusClients[i].connected = false;
                modbusClients[i].client.stop();
                connectedClients--;
                
                if (connectedClients == 0) {
                    digitalWrite(LED_BUILTIN, LOW);
                }
            } else {
                // Update IO for this client through IOManager
                // IOManager::updateIOForClient(i);  // TODO: Implement this function
                
                // Poll the client
                modbusClients[i].server.poll();
                modbusClients[i].connectionTime = millis();
            }
        }
    }
}

// Configuration functions (minimal implementations)
void loadConfig() {
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

void saveConfig() {
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

void loadSensorConfig() {
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
                    
                    numConfiguredSensors++;
                }
                
                Serial.printf("Loaded %d sensor configurations\n", numConfiguredSensors);
                return;
            }
        }
    }
    
    Serial.println("No sensor configuration found");
}

void saveSensorConfig() {
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
    }
    
    File file = LittleFS.open(SENSORS_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("Sensor configuration saved");
    }
}

// Web server setup and handlers
void setupWebServer() {
    // Serve static files from LittleFS
    webServer.serveStatic("/", LittleFS, "/");
    
    // API endpoints for configuration
    webServer.on("/config", HTTP_GET, []() {
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
        webServer.send(200, "application/json", response);
    });
    
    webServer.on("/config", HTTP_POST, []() {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            
            if (!error) {
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
                
                saveConfig();
                
                if (needsReboot) {
                    webServer.send(200, "application/json", "{\"success\":true,\"reboot\":true}");
                    delay(1000);
                    rp2040.restart();
                } else {
                    webServer.send(200, "application/json", "{\"success\":true}");
                }
                return;
            }
        }
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
    });
    
    // IO status endpoint
    webServer.on("/iostatus", HTTP_GET, []() {
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
        webServer.send(200, "application/json", response);
    });
    
    // Sensor configuration endpoints
    webServer.on("/sensors/config", HTTP_GET, []() {
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
        webServer.send(200, "application/json", response);
    });
    
    webServer.on("/sensors/config", HTTP_POST, []() {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<2048> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            
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
                
                saveSensorConfig();
                webServer.send(200, "application/json", "{\"success\":true,\"reboot\":true}");
                
                delay(1000);
                rp2040.restart();
                return;
            }
        }
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
    });
    
    // Digital output control
    webServer.on("/setoutput", HTTP_POST, []() {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            
            if (!error && doc.containsKey("output") && doc.containsKey("state")) {
                int output = doc["output"];
                bool state = doc["state"];
                
                if (output >= 0 && output < 8) {
                    IOManager::setDigitalOutput(output, state);
                    webServer.send(200, "application/json", "{\"success\":true}");
                    return;
                }
            }
        }
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
    });
    
    // Latch reset endpoints
    webServer.on("/reset-latches", HTTP_POST, []() {
        IOManager::resetAllLatches();
        webServer.send(200, "application/json", "{\"success\":true}");
    });
    
    webServer.on("/reset-latch", HTTP_POST, []() {
        if (webServer.hasArg("plain")) {
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
            
            if (!error && doc.containsKey("input")) {
                int input = doc["input"];
                if (input >= 0 && input < 8) {
                    IOManager::resetLatch(input);
                    webServer.send(200, "application/json", "{\"success\":true}");
                    return;
                }
            }
        }
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
    });
}

// Pin allocation management
void initializePinAllocations() {
    numAllocatedPins = 0;
    for (int i = 0; i < 40; i++) {
        pinAllocations[i].allocated = false;
        pinAllocations[i].pin = 0;
        strcpy(pinAllocations[i].protocol, "");
        strcpy(pinAllocations[i].sensorName, "");
    }
}

bool isPinAvailable(uint8_t pin, const char* protocol) {
    // Check if pin is already allocated
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && pinAllocations[i].pin == pin) {
            return false;
        }
    }
    
    // Check if pin is in available list for this protocol
    for (int i = 0; i < NUM_FLEXIBLE_PINS; i++) {
        if (AVAILABLE_FLEXIBLE_PINS[i] == pin) {
            return true;
        }
    }
    
    return false;
}

void allocatePin(uint8_t pin, const char* protocol, const char* sensorName) {
    if (numAllocatedPins < 40) {
        pinAllocations[numAllocatedPins].pin = pin;
        pinAllocations[numAllocatedPins].allocated = true;
        strlcpy(pinAllocations[numAllocatedPins].protocol, protocol, sizeof(pinAllocations[numAllocatedPins].protocol));
        strlcpy(pinAllocations[numAllocatedPins].sensorName, sensorName, sizeof(pinAllocations[numAllocatedPins].sensorName));
        numAllocatedPins++;
    }
}

void deallocatePin(uint8_t pin) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && pinAllocations[i].pin == pin) {
            // Shift remaining allocations down
            for (int j = i; j < numAllocatedPins - 1; j++) {
                pinAllocations[j] = pinAllocations[j + 1];
            }
            numAllocatedPins--;
            break;
        }
    }
}