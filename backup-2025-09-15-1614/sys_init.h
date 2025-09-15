#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <W5500lwIP.h>
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

// Default I2C pins (for backward compatibility)
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

// Available I2C Pin Pairs (SDA, SCL)
extern const uint8_t I2C_PIN_PAIRS[][2];
#define NUM_I2C_PAIRS 3

// Available pins for UART, One-Wire, and Digital Counter (excluding reserved pins)
extern const uint8_t AVAILABLE_FLEXIBLE_PINS[];
#define NUM_FLEXIBLE_PINS 17

// ADC pins (fixed)
extern const uint8_t ADC_PINS[];
#define NUM_ADC_PINS 3

// Pin allocation tracking
struct PinAllocation {
    uint8_t pin;
    char protocol[16];
    char sensorName[32];
    bool allocated;
};

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
} config;

// Default configuration
const Config DEFAULT_CONFIG = {
    CONFIG_VERSION,              // version
    true,                       // DHCP enabled
    {192, 168, 1, 10},         // Default IP
    {192, 168, 1, 1},          // Default Gateway
    {255, 255, 255, 0},        // Default Subnet
    502,                       // Default Modbus port
    "modbus-io",                // Default hostname
    {false, false, false, false, false, false, false, false},  // diPullup (all disabled)
    {false, false, false, false, false, false, false, false},  // diInvert (no inversion)
    {false, false, false, false, false, false, false, false},  // diLatch (no latching)
    {false, false, false, false, false, false, false, false},  // doInvert (no inversion)
    {false, false, false, false, false, false, false, false},  // doInitialState (all OFF)
};

struct SensorConfig {
    bool enabled;
    char name[32];
    char type[16]; // "BME280", "EZO_PH", "SIM_ANALOG_VOLTAGE", etc.
    char protocol[16]; // "I2C", "UART", "Analog Voltage", "One-Wire", "Digital Counter"
    uint8_t i2cAddress;
    uint16_t modbusRegister;
    
    // Pin assignments for different protocols
    uint8_t sdaPin;      // I2C SDA pin
    uint8_t sclPin;      // I2C SCL pin
    uint8_t txPin;       // UART TX pin
    uint8_t rxPin;       // UART RX pin
    uint8_t analogPin;   // Analog input pin
    uint8_t digitalPin;  // One-Wire or Digital Counter pin
    
    // EZO sensor state tracking
    bool cmdPending;
    unsigned long lastCmdSent;
    char response[32];
    
    // Simulation state for SIM_ sensors
    float simulatedValue;
    unsigned long lastSimulationUpdate;
    
    // Calibration configuration
    float offset;        // Linear calibration offset
    float scale;         // Linear calibration scale
    char expression[64]; // Mathematical expression for calibration
    char polynomialStr[64]; // Polynomial string for calibration
    
    // Generic I2C Data Parsing Configuration
    uint8_t i2cRegister;    // I2C register address to read from (0xFF = no register, direct read)
    uint8_t dataLength;     // Number of bytes to read (1-32)
    uint8_t dataOffset;     // Byte offset for primary value (0-based)
    uint8_t dataOffset2;    // Byte offset for secondary value (0-based, 0xFF = not used)
    uint8_t dataFormat;     // Data format: 0=uint8, 1=uint16_be, 2=uint16_le, 3=uint32_be, 4=uint32_le, 5=float32
    uint8_t dataFormat2;    // Data format for secondary value
    char rawDataHex[64];    // Last raw data read (for debugging display)
    
    // Runtime sensor values
    float rawValue;      // Raw sensor reading before calibration
    float calibratedValue; // Calibrated sensor value after applying calibration
    
    // Multi-value sensor support (for sensors like SHT30 that provide temp + humidity)
    float rawValue2;     // Secondary raw value (e.g., humidity when primary is temperature)
    float calibratedValue2; // Secondary calibrated value
    uint16_t modbusRegister2; // Secondary Modbus register (0 = not used)
    char valueName[16];  // Primary value name (e.g., "temperature")
    char valueName2[16]; // Secondary value name (e.g., "humidity")
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

IOStatus ioStatus;

extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;
extern PinAllocation pinAllocations[40];
extern int numAllocatedPins;

// Ethernet and Server instances
Wiznet5500lwIP eth(PIN_ETH_CS, SPI, PIN_ETH_IRQ);
WiFiServer modbusServer; 
WebServer webServer(80);
WiFiClient client;

// Client management
struct ModbusClientConnection {
    WiFiClient client;
    ModbusTCPServer server;
    bool connected;
    IPAddress clientIP;
    unsigned long connectionTime;
};

ModbusClientConnection modbusClients[MAX_MODBUS_CLIENTS];
int connectedClients = 0;

// Function declarations
void loadConfig();
void saveConfig();
void setPinModes();
void setupEthernet();
void setupModbus();
void setupWebServer();
void initializeI2C();
float applySensorCalibration(float rawValue, SensorConfig* sensor);

// Generic I2C sensor reading functions
float readGenericI2CSensor(uint8_t i2cAddress, const char* sensorType);
bool readMultiValueI2CSensor(SensorConfig* sensor);
bool readConfigurableI2CSensor(SensorConfig* sensor);
float parseI2CData(uint8_t* buffer, uint8_t offset, uint8_t format, uint8_t bufferLen);
float readSHT30(uint8_t i2cAddress);
bool readSHT30MultiValue(SensorConfig* sensor);
bool readBME280MultiValue(SensorConfig* sensor);
float readBME280(uint8_t i2cAddress);
float readAHT(uint8_t i2cAddress);
float readGenericBytes(uint8_t i2cAddress);

void updateIOpins();
void updateIOForClient(int clientIndex);
void handleRoot();
void handleCSS();
void handleJS();
void handleFavicon();
void handleLogo();
void handleGetConfig();
void handleSetConfig();
void handleSetOutput();
void handleGetIOStatus();
void handleGetIOConfig();
void handleSetIOConfig();
void resetLatches();
void handleResetLatches();
void handleResetSingleLatch();
void handleGetSensorConfig();
void handleSetSensorConfig();
void loadSensorConfig();
void saveSensorConfig();
void handleGetSensorConfig();
void handleSetSensorConfig();
void handleEzoSensors();
void initializeEzoSensors();
void handleSensorCommand();

// Pin management functions
void initializePinAllocations();
bool isPinAvailable(uint8_t pin, const char* protocol);
void allocatePin(uint8_t pin, const char* protocol, const char* sensorName);
void deallocatePin(uint8_t pin);
void handleGetAvailablePins();