#include "sensor_manager.h"
#include "sys_init.h"
#include <math.h>

// Static member definitions
bool SensorManager::i2cInitialized = false;
bool SensorManager::spiInitialized = false;
bool SensorManager::uartInitialized = false;
bool SensorManager::ezoSensorsInitialized = false;
unsigned long SensorManager::lastSensorUpdate = 0;
unsigned long SensorManager::lastEzoUpdate = 0;
SPISettings SensorManager::spiSettings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
Ezo_board* SensorManager::ezoSensors[MAX_SENSORS] = {nullptr};

// External references
extern SensorConfig configuredSensors[MAX_SENSORS];
extern int numConfiguredSensors;
extern IOStatus ioStatus;
extern Config config;

void SensorManager::init() {
    Serial.println("SensorManager: Initializing Sensor Manager");
    
    initializeI2C();
    initializeSPI();
    initializeUART();
    
    lastSensorUpdate = millis();
    lastEzoUpdate = millis();
    
    Serial.println("SensorManager: Initialization complete");
}

void SensorManager::initializeI2C() {
    if (i2cInitialized) return;
    
    // Initialize I2C with pin configuration
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin();
    Wire.setClock(100000); // 100kHz standard mode
    
    i2cInitialized = true;
    Serial.printf("SensorManager: I2C initialized on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
}

void SensorManager::initializeSPI() {
    if (spiInitialized) return;
    
    // Initialize SPI with modern RP2040 API
    SPI.begin();
    // Use SPISettings instead of deprecated methods
    spiSettings = SPISettings(1000000, MSBFIRST, SPI_MODE0); // 1MHz, MSB first, Mode 0
    
    spiInitialized = true;
    Serial.println("SensorManager: SPI initialized");
}

void SensorManager::initializeUART() {
    if (uartInitialized) return;
    
    // Initialize UART ports as needed
    // Serial1 and Serial2 can be initialized on demand
    
    uartInitialized = true;
    Serial.println("SensorManager: UART ready for initialization");
}

void SensorManager::updateAllSensors() {
    unsigned long currentTime = millis();
    
    // Update different sensor types at different intervals
    if (currentTime - lastSensorUpdate >= 100) { // 10Hz update rate
        readI2CSensors();
        readSPISensors();
        readUARTSensors();
        readAnalogSensors();
        updateSimulatedSensors();
        
        lastSensorUpdate = currentTime;
    }
    
    // EZO sensors have their own timing
    handleEzoSensors();
}

void SensorManager::readI2CSensors() {
    if (!i2cInitialized) initializeI2C();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strcmp(configuredSensors[i].protocol, "I2C") != 0) continue;
        
        // Skip EZO sensors (handled separately)
        if (strncmp(configuredSensors[i].type, "EZO_", 4) == 0) continue;
        
        uint8_t address = configuredSensors[i].i2cAddress;
        uint8_t data[16] = {0};
        
        // Read I2C data based on configuration
        bool success = false;
        if (configuredSensors[i].i2cRegister != 0xFF) {
            // Register-based read
            success = readI2CRegister(address, configuredSensors[i].i2cRegister, 
                                    data, configuredSensors[i].dataLength);
        } else {
            // Direct read
            success = readI2CData(address, data, configuredSensors[i].dataLength);
        }
        
        if (success) {
            // Store raw data in hex format
            char hexStr[33] = {0};
            for (int j = 0; j < configuredSensors[i].dataLength && j < 16; j++) {
                sprintf(hexStr + (j * 2), "%02X", data[j]);
            }
            strncpy(configuredSensors[i].rawDataHex, hexStr, sizeof(configuredSensors[i].rawDataHex) - 1);
            
            // Parse numeric value based on data format
            float rawValue = 0.0;
            int offset = configuredSensors[i].dataOffset;
            
            if (configuredSensors[i].dataFormat == DATA_FORMAT_UINT16_BE && offset + 1 < configuredSensors[i].dataLength) {
                rawValue = (data[offset] << 8) | data[offset + 1];
            } else if (configuredSensors[i].dataFormat == DATA_FORMAT_UINT16_LE && offset + 1 < configuredSensors[i].dataLength) {
                rawValue = (data[offset + 1] << 8) | data[offset];
            } else if (configuredSensors[i].dataFormat == DATA_FORMAT_INT16_BE && offset + 1 < configuredSensors[i].dataLength) {
                int16_t temp = (data[offset] << 8) | data[offset + 1];
                rawValue = (float)temp;
            } else if (configuredSensors[i].dataFormat == DATA_FORMAT_UINT8 && offset < configuredSensors[i].dataLength) {
                rawValue = data[offset];
            }
            
            processSensorData(i, rawValue);
            updateSensorTimestamps(i);
        } else {
            logSensorError(i, "I2C read failed");
        }
    }
}

void SensorManager::readSPISensors() {
    if (!spiInitialized) initializeSPI();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strcmp(configuredSensors[i].protocol, "SPI") != 0) continue;
        
        uint8_t csPin = configuredSensors[i].csPin;
        uint8_t data[16] = {0};
        
        // Read SPI data
        selectSPIDevice(csPin);
        delay(1); // Small delay for device selection
        
        for (int j = 0; j < configuredSensors[i].dataLength && j < 16; j++) {
            data[j] = transferSPI(0x00); // Send dummy byte to read
        }
        
        deselectSPIDevice(csPin);
        
        // Parse the data similar to I2C
        float rawValue = 0.0;
        int offset = configuredSensors[i].dataOffset;
        
        if (configuredSensors[i].dataFormat == DATA_FORMAT_UINT16_BE && offset + 1 < configuredSensors[i].dataLength) {
            rawValue = (data[offset] << 8) | data[offset + 1];
        } else if (configuredSensors[i].dataFormat == DATA_FORMAT_UINT8 && offset < configuredSensors[i].dataLength) {
            rawValue = data[offset];
        }
        
        processSensorData(i, rawValue);
        updateSensorTimestamps(i);
    }
}

void SensorManager::readUARTSensors() {
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strcmp(configuredSensors[i].protocol, "UART") != 0) continue;
        
        char buffer[64] = {0};
        int port = configuredSensors[i].uartPort;
        
        if (readUARTData(port, buffer, sizeof(buffer) - 1)) {
            // Parse numeric value from UART response
            float rawValue = parseNumericValue(buffer);
            processSensorData(i, rawValue);
            updateSensorTimestamps(i);
        }
    }
}

void SensorManager::readAnalogSensors() {
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strcmp(configuredSensors[i].protocol, "Analog") != 0) continue;
        
        int pin = configuredSensors[i].analogPin;
        if (pin >= 26 && pin <= 28) { // Valid ADC pins
            int rawValue = analogRead(pin);
            float voltage = (rawValue * 3300.0) / 4095.0; // Convert to mV
            
            processSensorData(i, voltage);
            updateSensorTimestamps(i);
        }
    }
}

void SensorManager::updateSimulatedSensors() {
    static unsigned long lastSimUpdate = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastSimUpdate < 1000) return; // Update every second
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strncmp(configuredSensors[i].type, "SIM_", 4) != 0) continue;
        
        float simulatedValue = 0.0;
        
        // Generate simulated values based on sensor type
        if (strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0) {
            simulatedValue = 20.0 + 5.0 * sin(currentTime / 10000.0); // 20-25°C range
            ioStatus.temperature = simulatedValue;
        } else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
            simulatedValue = 50.0 + 10.0 * cos(currentTime / 15000.0); // 40-60% range
            ioStatus.humidity = simulatedValue;
        } else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
            simulatedValue = 1013.25 + 5.0 * sin(currentTime / 20000.0); // ~1atm ±5hPa
            ioStatus.pressure = simulatedValue;
        }
        
        configuredSensors[i].simulatedValue = simulatedValue;
        processSensorData(i, simulatedValue);
    }
    
    lastSimUpdate = currentTime;
}

void SensorManager::initializeEzoSensors() {
    if (ezoSensorsInitialized) return;
    
    if (!i2cInitialized) initializeI2C();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
            ezoSensors[i] = new Ezo_board(configuredSensors[i].i2cAddress, configuredSensors[i].name);
            configuredSensors[i].cmdPending = false;
            configuredSensors[i].lastCmdSent = 0;
            memset(configuredSensors[i].response, 0, sizeof(configuredSensors[i].response));
            Serial.printf("SensorManager: Initialized EZO sensor %s at I2C address 0x%02X\n", 
                configuredSensors[i].name, configuredSensors[i].i2cAddress);
        }
    }
    ezoSensorsInitialized = true;
}

void SensorManager::handleEzoSensors() {
    static bool initialized = false;
    
    if (!initialized) {
        initializeEzoSensors();
        initialized = true;
    }
    
    unsigned long currentTime = millis();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        if (strncmp(configuredSensors[i].type, "EZO_", 4) != 0) continue;
        if (ezoSensors[i] == nullptr) continue;
        
        // Handle pending commands
        if (configuredSensors[i].cmdPending) {
            if (currentTime - configuredSensors[i].lastCmdSent >= 1000) { // 1 second timeout
                // Read response
                char response[32] = {0};
                ezoSensors[i]->receive_cmd(response, sizeof(response) - 1);
                
                strncpy(configuredSensors[i].response, response, sizeof(configuredSensors[i].response) - 1);
                configuredSensors[i].cmdPending = false;
                
                // Parse numeric value if it's a reading
                if (strlen(response) > 0 && response[0] != 'E') { // Not an error
                    float value = parseNumericValue(response);
                    processSensorData(i, value);
                }
                
                updateSensorTimestamps(i);
            }
        } else {
            // Send periodic reading command (every 5 seconds)
            if (currentTime - configuredSensors[i].lastCmdSent >= 5000) {
                ezoSensors[i]->send_cmd("R");
                configuredSensors[i].cmdPending = true;
                configuredSensors[i].lastCmdSent = currentTime;
            }
        }
    }
}

void SensorManager::sendEzoCommand(int sensorIndex, const char* command) {
    if (sensorIndex < 0 || sensorIndex >= numConfiguredSensors) return;
    if (ezoSensors[sensorIndex] == nullptr) return;
    
    ezoSensors[sensorIndex]->send_cmd(command);
    configuredSensors[sensorIndex].cmdPending = true;
    configuredSensors[sensorIndex].lastCmdSent = millis();
    
    Serial.printf("SensorManager: Sent command '%s' to sensor %d\n", command, sensorIndex);
}

void SensorManager::applySensorCalibration(int sensorIndex) {
    if (sensorIndex < 0 || sensorIndex >= numConfiguredSensors) return;
    
    float rawValue = configuredSensors[sensorIndex].rawValue;
    float calibratedValue = rawValue;
    
    // Apply scaling factor
    if (configuredSensors[sensorIndex].scaleFactor != 0.0) {
        calibratedValue *= configuredSensors[sensorIndex].scaleFactor;
    }
    
    // Apply offset
    calibratedValue += configuredSensors[sensorIndex].offset;
    
    // Apply mathematical formula if provided
    if (strlen(configuredSensors[sensorIndex].polynomialStr) > 0) {
        calibratedValue = applyMathematicalFormula(calibratedValue, configuredSensors[sensorIndex].polynomialStr);
    }
    
    configuredSensors[sensorIndex].calibratedValue = calibratedValue;
    
    // Map to Modbus register
    mapSensorToModbusRegister(sensorIndex);
}

float SensorManager::applyMathematicalFormula(float rawValue, const char* formula) {
    // Placeholder for mathematical formula evaluation
    // This would integrate with tinyexpr or similar library
    // For now, return the raw value
    return rawValue;
}

void SensorManager::mapSensorToModbusRegister(int sensorIndex) {
    if (sensorIndex < 0 || sensorIndex >= numConfiguredSensors) return;
    
    int modbusRegister = configuredSensors[sensorIndex].modbusRegister;
    if (modbusRegister > 0) {
        float value = configuredSensors[sensorIndex].calibratedValue;
        
        // Update global status for specific sensor types
        if (strcmp(configuredSensors[sensorIndex].type, "EZO_RTD") == 0) {
            ioStatus.temperature = value;
        }
        
        // Note: Actual Modbus register mapping would be implemented here
        // This depends on the Modbus server architecture
    }
}

// Utility functions
bool SensorManager::scanI2CAddress(uint8_t address) {
    if (!i2cInitialized) initializeI2C();
    
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

bool SensorManager::readI2CRegister(uint8_t address, uint8_t reg, uint8_t* data, uint8_t length) {
    if (!i2cInitialized) return false;
    
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    
    Wire.requestFrom(address, length);
    int i = 0;
    while (Wire.available() && i < length) {
        data[i++] = Wire.read();
    }
    
    return (i == length);
}

bool SensorManager::readI2CData(uint8_t address, uint8_t* data, uint8_t length) {
    if (!i2cInitialized) return false;
    
    Wire.requestFrom(address, length);
    int i = 0;
    while (Wire.available() && i < length) {
        data[i++] = Wire.read();
    }
    
    return (i == length);
}

void SensorManager::selectSPIDevice(uint8_t csPin) {
    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, LOW);
}

void SensorManager::deselectSPIDevice(uint8_t csPin) {
    digitalWrite(csPin, HIGH);
}

uint8_t SensorManager::transferSPI(uint8_t data) {
    return SPI.transfer(data);
}

bool SensorManager::readUARTData(int port, char* buffer, int maxLength) {
    // Placeholder for UART reading implementation
    // Would need to implement based on available Serial ports
    return false;
}

float SensorManager::parseNumericValue(const char* response) {
    if (response == nullptr || strlen(response) == 0) return 0.0;
    
    // Skip non-numeric characters at the beginning
    const char* ptr = response;
    while (*ptr && !isdigit(*ptr) && *ptr != '-' && *ptr != '+' && *ptr != '.') {
        ptr++;
    }
    
    return atof(ptr);
}

void SensorManager::processSensorData(int index, float rawValue) {
    if (index < 0 || index >= numConfiguredSensors) return;
    
    configuredSensors[index].rawValue = rawValue;
    applySensorCalibration(index);
}

void SensorManager::updateSensorTimestamps(int index) {
    if (index < 0 || index >= numConfiguredSensors) return;
    configuredSensors[index].lastUpdate = millis();
}

void SensorManager::logSensorError(int index, const char* error) {
    if (index < 0 || index >= numConfiguredSensors) return;
    Serial.printf("SensorManager: Error on sensor %d (%s): %s\n", 
                  index, configuredSensors[index].name, error);
}

// Legacy wrapper functions
void updateSimulatedSensors() {
    SensorManager::updateSimulatedSensors();
}

void handleEzoSensors() {
    SensorManager::handleEzoSensors();
}

void initializeEzoSensors() {
    SensorManager::initializeEzoSensors();
}