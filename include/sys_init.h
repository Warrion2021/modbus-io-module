#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <W5500lwIP.h>
#include <LwipEthernet.h>
#include <WebServer.h>
#include <ArduinoModbus.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define MAX_SENSORS 10

// Forward declarations
struct SensorCommand {
    uint8_t sensorIndex;
    uint32_t nextExecutionMs;
    uint32_t intervalMs;
    const char* command;
    bool isGeneric;
};

// Simple command array handler
struct CommandArray {
    SensorCommand commands[MAX_SENSORS];
    uint8_t count;
    
    void init() {
        count = 0;
    }
    
    bool add(const SensorCommand& cmd) {
        if (count < MAX_SENSORS) {
            commands[count++] = cmd;
            return true;
        }
        return false;
    }
    
    bool isEmpty() const {
        return count == 0;
    }
    
    SensorCommand* getNext() {
        return isEmpty() ? nullptr : &commands[0];
    }
    
    void remove() {
        if (!isEmpty()) {
            for (uint8_t i = 0; i < count - 1; i++) {
                commands[i] = commands[i + 1];
            }
            count--;
        }
    }
    
    void clear() {
        count = 0;
    }
};

// Watchdog timer
#define WDT_TIMEOUT 5000

// Pin Definitions
#define PIN_ETH_MISO 16
#define PIN_ETH_CS 17
#define PIN_ETH_SCK 18
#define PIN_ETH_MOSI 19
#define PIN_ETH_RST 20
#define PIN_ETH_IRQ 21
#define PIN_EXT_LED 22

// Default I2C Pin Definitions (GP4/GP5 are safest for W5500-EVB-PoE-Pico)
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

// Constants
#define CONFIG_FILE "/config.json"
#define SENSORS_FILE "/sensors.json"
#define CONFIG_VERSION 7  // Increment this when config structure changes
#define HOSTNAME_MAX_LENGTH 32
#define MAX_MODBUS_CLIENTS 4  // Maximum number of concurrent Modbus clients
#define MAX_SENSORS 10

// Global flags
bool core0setupComplete = false;

// Digital IO pins
const uint8_t DIGITAL_INPUTS[] = {0, 1, 2, 3, 4, 5, 6, 7};  // Digital input pins
const uint8_t DIGITAL_OUTPUTS[] = {8, 9, 10, 11, 12, 13, 14, 15}; // Digital output pins
const uint8_t ANALOG_INPUTS[] = {26, 27, 28};   // ADC pins

// Network and Modbus Configuration

// Bus operation states
enum class BusOpState {
    IDLE,
    REQUEST_SENT,
    WAITING_CONVERSION,
    READY_TO_READ,
    ERROR
};

// Bus operation structure
struct BusOperation {
    uint8_t sensorIndex;
    uint32_t startTime;
    uint32_t conversionTime;
    BusOpState state;
    uint8_t retryCount;
    bool needsCRC;
};

// SensorCommand is already defined above

struct Config {
    uint8_t version;
    bool dhcpEnabled;
    uint8_t ip[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
    uint16_t modbusPort;
    char hostname[HOSTNAME_MAX_LENGTH];
    bool diPullup[8];         // Enable internal pullup for digital inputs
    bool diInvert[8];         // Invert logic for digital inputs
    bool diLatch[8];          // Enable latching for digital inputs (stay ON until read)
    bool doInvert[8];         // Invert logic for digital outputs
    bool doInitialState[8];   // Initial state for digital outputs (true = ON, false = OFF)
};

struct IOStatus {
    bool dIn[8];          // Current state of digital inputs (including latching behavior if enabled)
    bool dInRaw[8];       // Actual physical state of digital inputs (without latching)
    bool dInLatched[8];   // Tracks if an input has been latched (true = latched)
    bool dOut[8];
    uint16_t aIn[3];
    
    // I2C Sensor Data - Active sensor fields
    float temperature;    // Temperature reading from I2C sensor (e.g., BME280)
    float humidity;       // Humidity reading from I2C sensor (e.g., BME280) 
    float pressure;       // Pressure reading from I2C sensor (e.g., BME280)
    // Add additional sensor fields as needed for your specific I2C sensors
    // Dataflow display improvements
    float rawValue[MAX_SENSORS];   // Raw value per sensor
    char rawUnit[MAX_SENSORS][8];  // Raw unit per sensor ("mV", "ADC", etc.)
    float calibratedValue[MAX_SENSORS]; // Calibrated value per sensor
    float ph;
    float conductivity;
};

// Sensor configuration structure (KEEP - intentional improvements)
struct SensorConfig {
    bool enabled;
    char name[32];
    char type[16];
    char protocol[16]; // Added protocol field for sensor assignment
    uint8_t i2cAddress;
    char i2cAddressStr[8]; // Hex string for I2C address
    int modbusRegister;
    float calibrationOffset;
    float calibrationSlope;
    char calibrationExpression[128];  // Mathematical expression for calibration (supports any equation)
    // Dynamic pin assignment
    int sdaPin;
    int sclPin;
    int dataPin;
    int uartTxPin;
    int uartRxPin;
    int analogPin;
    int oneWirePin;
    int digitalPin;
    // Data parsing configuration
    char parsingMethod[16];   // raw, custom_bits, bit_field, status_register, json_path, csv_column
    char command[32];
    uint32_t updateInterval;
    int delayBeforeRead;      // Delay in ms before reading response (for UART, I2C timing)
    uint8_t i2cMultiplexerChannel;
    char parsingConfig[128];  // JSON string for parsing configuration
    
    // Secondary parsing for multi-output sensors
    char parsingMethodB[16];  // Parsing method for secondary value (rawValueB)
    char parsingConfigB[128]; // JSON config for secondary parsing
    char parsingMethodC[16];  // Parsing method for tertiary value (rawValueC)  
    char parsingConfigC[128]; // JSON config for tertiary parsing
    // EZO sensor state tracking
    bool cmdPending;
    unsigned long lastCmdSent;
    // Response data storage
    char response[64];
    char calibrationData[256];
    // Current sensor values for dataflow - support multiple outputs
    float rawValue;           // Primary raw sensor reading (rawValueA)
    float rawValueB;          // Secondary raw reading (humidity for SHT30, pressure for BME280)
    float rawValueC;          // Tertiary raw reading (pressure for BME280, etc.)
    
    float calibratedValue;    // Primary calibrated value (after applying calibration)
    float calibratedValueB;   // Secondary calibrated value
    float calibratedValueC;   // Tertiary calibrated value
    
    int modbusValue;          // Primary value written to Modbus register
    int modbusValueB;         // Secondary value for next Modbus register
    int modbusValueC;         // Tertiary value for next Modbus register
    
    // Calibration for multiple outputs
    float calibrationOffsetB; // Calibration offset for rawValueB
    float calibrationSlopeB;  // Calibration slope for rawValueB
    float calibrationOffsetC; // Calibration offset for rawValueC  
    float calibrationSlopeC;  // Calibration slope for rawValueC
    char calibrationExpressionB[128];  // Mathematical expression for calibrating rawValueB
    char calibrationExpressionC[128];  // Mathematical expression for calibrating rawValueC
    
    char rawDataString[128];  // Raw data string for parsing (I2C/UART responses)
    unsigned long lastReadTime; // When last read was performed
    
    // One-Wire specific configuration
    char oneWireCommand[16];  // Hex command to send (e.g., "0x44" for Convert T)
    int oneWireInterval;      // Interval in seconds between commands (0 = manual only)
    int oneWireConversionTime; // Time in ms to wait after command before reading
    unsigned long lastOneWireCmd; // When last command was sent
    bool oneWireAutoMode;     // Enable automatic periodic commands
};

// Extern declarations for global variables
extern Config config;
extern IOStatus ioStatus;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;

// Ethernet and Server instances - Essential for web server
extern Wiznet5500lwIP eth;
extern WiFiServer modbusServer; 
extern WiFiServer httpServer;    // HTTP server on port 80
extern WiFiClient client;

// Client management
struct ModbusClientConnection {
    WiFiClient client;
    ModbusTCPServer server;
    bool connected;
    IPAddress clientIP;
    unsigned long connectionTime;
};

extern ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
extern int connectedClients;

// Default configuration
const Config DEFAULT_CONFIG = {
    .version = CONFIG_VERSION,
    .dhcpEnabled = true,
    .ip = {192, 168, 1, 10},
    .gateway = {192, 168, 1, 1},
    .subnet = {255, 255, 255, 0},
    .modbusPort = 502,
    .hostname = "modbus-io-module",
    .diPullup = {true, true, true, true, true, true, true, true},
    .diInvert = {false, false, false, false, false, false, false, false},
    .diLatch = {false, false, false, false, false, false, false, false},
    .doInvert = {false, false, false, false, false, false, false, false},
    .doInitialState = {false, false, false, false, false, false, false, false}
};

void initializePins();
void loadConfig();
void saveConfig();
void loadSensorConfig();
void saveSensorConfig();
void updateIOpins();
void resetLatches();
void initializeEzoSensors();
void handleEzoSensors();

// File serving helper for main.cpp
void serveFileFromFS(WiFiClient& client, const String& filename, const String& contentType) {
    Serial.print("[serveFileFromFS] Requested filename: ");
    Serial.println(filename);
    Serial.print("[serveFileFromFS] Content-Type: ");
    Serial.println(contentType);
    if (!LittleFS.begin()) {
        Serial.println("[serveFileFromFS] LittleFS mount failed");
        client.println("HTTP/1.1 500 Internal Server Error");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        client.println("Filesystem mount failed");
        return;
    }
    String fname = filename;
    if (!fname.startsWith("/")) fname = "/" + fname;
    Serial.print("[serveFileFromFS] Opening file: ");
    Serial.println(fname);
    File file = LittleFS.open(fname, "r");
    if (!file) {
        Serial.print("[serveFileFromFS] File not found: ");
        Serial.println(fname);
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        client.println("404 Not Found");
        return;
    } else {
        Serial.print("[serveFileFromFS] File opened successfully: ");
        Serial.println(fname);
    }
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(file.size());
    client.println();
    const size_t bufSize = 1024;
    uint8_t buf[bufSize];
    size_t bytesRead;
    while ((bytesRead = file.read(buf, bufSize)) > 0) {
        client.write(buf, bytesRead);
    }
    file.close();
}

extern Config config;
extern IOStatus ioStatus;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
extern int numConfiguredSensors;
extern int connectedClients;
extern bool systemInitialized;
extern unsigned long lastSensorRead;
extern unsigned long lastEzoCommand;