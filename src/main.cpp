#include "sys_init.h"
#include <Wire.h>
#include "io_manager.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include "webserver_manager.h"
#include "modbus_manager.h"
#include "pin_manager.h"

// Global variable definitions (moved from header)
Config config = DEFAULT_CONFIG;
IOStatus ioStatus = {};
WebServer webServer(80);
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_RST);
WiFiClient client;
int connectedClients = 0;
bool core0setupComplete = false;

SensorConfig configuredSensors[MAX_SENSORS];
int numConfiguredSensors = 0;

// Pin configuration constants
const uint8_t I2C_PIN_PAIRS[][2] = {
    {4, 5},    // Primary I2C pair (Physical pins 6, 7)
    {2, 3},    // Alternative I2C pair (Physical pins 4, 5)
    {6, 7}     // Another alternative I2C pair (Physical pins 9, 10)
};

const uint8_t AVAILABLE_FLEXIBLE_PINS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 23};
const uint8_t ADC_PINS[] = {26, 27, 28};

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
    
    // Load configuration using ConfigManager
    ConfigManager::loadConfig();
    ConfigManager::loadSensorConfig();
    
    // Initialize pin allocations using PinManager
    PinManager::initialize();
    
    // Initialize core modules
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
    
    // Initialize Modbus server using ModbusManager
    ModbusManager::initialize();
    
    // Setup web server using WebServerManager
    WebServerManager::initialize(&webServer);
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
    
    // Handle Modbus clients using ModbusManager
    ModbusManager::handleClients();
    
    // Update IO and sensors through modules
    IOManager::updateIOState();
    SensorManager::updateAllSensors();
    
    // Handle web server using WebServerManager
    WebServerManager::handleClient();
    
    // Memory monitoring (every 30 seconds)
    if (millis() - lastMemoryReport > 30000) {
        Serial.printf("Free heap: %d bytes\n", rp2040.getFreeHeap());
    }
}