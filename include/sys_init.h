#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <W5500lwIP.h>
#include <LwipEthernet.h>
#include <WebServer.h>
#include <ArduinoModbus.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

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

// Add I2C Pin Definitions
#define I2C_SDA_PIN 24
#define I2C_SCL_PIN 25

// Constants
#define CONFIG_FILE "/config.json"
#define SENSORS_FILE "/sensors.json"
#define CONFIG_VERSION 6  // Increment this when config structure changes
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
};

// Universal sensor configuration structure - supports EZO + generic protocols
struct SensorConfig {
    bool enabled;
    char name[32];
    char sensor_type[16];     // "EZO", "Analog", "I2C", "UART", "OneWire", "DigitalCounter", "GPIO"
    char protocol[16];        // "EZO_PH", "EZO_RTD", "ADC", "4-20mA", "0-10V", "Thermistor", "BME280", "SHT30", "SHT31", "VL53L1X", "ModbusRTU", "DS18B20", "PulseCount", "Digital"
    char formula[64];         // Mathematical conversion formula (e.g., "x*3.3/4096")
    char units[16];           // Engineering units (°C, %, psi, L/min, etc.)
    uint8_t pin_assignment;   // Primary GPIO pin (for non-I2C sensors)
    uint8_t pin_secondary;    // Secondary pin (for differential, clock/data pairs)
    uint32_t baud_rate;       // For UART sensors (9600, 115200, etc.)
    uint8_t i2cAddress;       // For I2C/EZO sensors (0x63, 0x76, etc.)
    uint8_t device_id;        // Modbus device ID or OneWire ROM index
    int modbusRegister;       // Target output Modbus register
    float calibrationOffset;
    float calibrationSlope;
    uint32_t sample_interval; // Sampling interval in milliseconds
    // Runtime state - EZO specific
    bool cmdPending;
    unsigned long lastCmdSent;
    char response[64];
    char calibrationData[256];
    // Runtime state - Generic sensors
    unsigned long lastSample;
    float lastReading;
    uint32_t pulse_count;     // For digital counters
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

extern Config config;
extern IOStatus ioStatus;
extern SensorConfig configuredSensors[MAX_SENSORS];
extern ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
extern int numConfiguredSensors;
extern int connectedClients;
extern bool systemInitialized;
extern unsigned long lastSensorRead;
extern unsigned long lastEzoCommand;