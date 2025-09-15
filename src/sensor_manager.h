#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Ezo_i2c.h>
#include "sys_init.h"

// Forward declarations
struct SensorConfig;
struct IOStatus;

// Sensor Manager - Handles all sensor operations
class SensorManager {
public:
    // Initialization
    static void init();
    static void initializeI2C();
    static void initializeSPI();
    static void initializeUART();
    
    // Sensor Management
    static void updateAllSensors();
    static void readI2CSensors();
    static void readSPISensors();
    static void readUARTSensors();
    static void readAnalogSensors();
    static void updateSimulatedSensors();
    
    // EZO Sensor Management
    static void initializeEzoSensors();
    static void handleEzoSensors();
    static void sendEzoCommand(int sensorIndex, const char* command);
    
    // Calibration and Data Processing
    static void applySensorCalibration(int sensorIndex);
    static float applyMathematicalFormula(float rawValue, const char* formula);
    static void mapSensorToModbusRegister(int sensorIndex);
    
    // Configuration
    static void configureSensor(int index, const SensorConfig& config);
    static void enableSensor(int index, bool enable);
    static bool validateSensorConfig(const SensorConfig& config);
    
    // I2C Operations
    static bool scanI2CAddress(uint8_t address);
    static bool readI2CRegister(uint8_t address, uint8_t reg, uint8_t* data, uint8_t length);
    static bool writeI2CRegister(uint8_t address, uint8_t reg, uint8_t data);
    static bool readI2CData(uint8_t address, uint8_t* data, uint8_t length);
    
    // SPI Operations
    static void selectSPIDevice(uint8_t csPin);
    static void deselectSPIDevice(uint8_t csPin);
    static uint8_t transferSPI(uint8_t data);
    static void readSPIData(uint8_t csPin, uint8_t* data, uint8_t length);
    
    // UART Operations
    static void initUARTPort(int port, int baudRate);
    static bool readUARTData(int port, char* buffer, int maxLength);
    static void writeUARTData(int port, const char* data);
    
    // Data Access
    static float getSensorValue(int index);
    static float getSensorRawValue(int index);
    static float getSensorCalibratedValue(int index);
    static const char* getSensorResponse(int index);
    static bool isSensorEnabled(int index);
    static const char* getSensorStatus(int index);
    
    // Utility
    static void printSensorStatus();
    static void resetSensorData(int index);
    static void resetAllSensorData();
    
private:
    // Internal state
    static bool i2cInitialized;
    static bool spiInitialized;
    static bool uartInitialized;
    static bool ezoSensorsInitialized;
    static unsigned long lastSensorUpdate;
    static unsigned long lastEzoUpdate;
    static SPISettings spiSettings;
    
    // EZO sensor instances
    static Ezo_board* ezoSensors[MAX_SENSORS];
    
    // Helper functions
    static void updateSensorTimestamps(int index);
    static bool isTimeForSensorUpdate(int index);
    static void logSensorError(int index, const char* error);
    static void processSensorData(int index, float rawValue);
    
    // Protocol-specific helpers
    static void initI2CPins(uint8_t sda, uint8_t scl);
    static void initSPIPins();
    static bool validateI2CAddress(uint8_t address);
    static bool validateSPIPin(uint8_t pin);
    static bool validateUARTPort(int port);
    
    // Data processing
    static float parseNumericValue(const char* response);
    static void updateModbusRegisters(int sensorIndex);
};

// Legacy compatibility functions
void updateSimulatedSensors();
void handleEzoSensors();
void initializeEzoSensors();