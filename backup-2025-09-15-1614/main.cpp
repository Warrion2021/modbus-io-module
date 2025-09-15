#include "sys_init.h"
#include <Wire.h>
#include <Ezo_i2c.h>

SensorConfig configuredSensors[MAX_SENSORS];
int numConfiguredSensors = 0;
PinAllocation pinAllocations[40]; // Track allocation for all possible pins
int numAllocatedPins = 0;

// Pin configuration constants
const uint8_t I2C_PIN_PAIRS[][2] = {
    {4, 5},    // Primary I2C pair (Physical pins 6, 7)
    {2, 3},    // Alternative I2C pair (Physical pins 4, 5)
    {6, 7}     // Another alternative I2C pair (Physical pins 9, 10)
};

const uint8_t AVAILABLE_FLEXIBLE_PINS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 23};

const uint8_t ADC_PINS[] = {26, 27, 28};

// EZO Sensor instances - created dynamically based on configuration
static Ezo_board* ezoSensors[MAX_SENSORS] = {nullptr};
static bool ezoSensorsInitialized = false;

// I2C Sensor Library Template - Uncomment when adding I2C sensors
// Example using BME280 sensor:
// #include <Adafruit_Sensor.h>
// #include <Adafruit_BME280.h>

// I2C Sensor Instance Template - Uncomment and modify for your sensor
// Example for BME280:
// Adafruit_BME280 bme; // Create sensor object

/*
 * Modbus IO Module
 * 
 * Features:
 * - 8 Digital Inputs (with optional pullup, inversion, and latching)
 * - 8 Digital Outputs (with optional inversion and initial state)
 * - 3 Analog Inputs (12-bit resolution)
 * - Modbus TCP Server
 * - Web configuration interface
 * 
 * Modbus Register Map:
 * - Discrete Inputs (FC2): 0-7 - Digital input states
 * - Coils (FC1/FC5): 0-7 - Digital output states
 * - Coils (FC5): 100-107 - Reset latches for digital inputs 0-7
 *   (Write 1 to reset the latch for the corresponding input)
 * - Input Registers (FC4): 0-2 - Analog input values (in mV)
 * - Input Registers (FC4): 3+ - Available for I2C sensor data
 * 
 * Web Interface:
 * - Network Configuration (IP, Gateway, Subnet, DHCP)
 * - IO Status and Control
 * - IO Configuration (Input pullup, inversion, latching, etc.)
 */

// Function declarations for terminal functionality
void handleTerminalCommand();
void handleTerminalWatch();
String executeTerminalCommand(String protocol, String pin, String command, String i2cAddress);
String executeDigitalCommand(String pin, String command);
String executeAnalogCommand(String pin, String command);
String executeI2CCommand(String pin, String command, String i2cAddress);
String executeUARTCommand(String pin, String command);
String executeNetworkCommand(String pin, String command);
String executeSystemCommand(String pin, String command);

// Helper function to get sensor unit from type
String getSensorUnitFromType(const char* type) {
    // Simulated sensors
    if (strncmp(type, "SIM_I2C_TEMPERATURE", 19) == 0) return "°C";
    if (strncmp(type, "SIM_I2C_HUMIDITY", 16) == 0) return "%";
    if (strncmp(type, "SIM_I2C_PRESSURE", 16) == 0) return "hPa";
    if (strncmp(type, "SIM_ANALOG_VOLTAGE", 18) == 0) return "V";
    if (strncmp(type, "SIM_ANALOG_CURRENT", 18) == 0) return "mA";
    if (strncmp(type, "SIM_UART_TEMPERATURE", 20) == 0) return "°C";
    if (strncmp(type, "SIM_UART_FLOW", 13) == 0) return "L/min";
    if (strncmp(type, "SIM_ONEWIRE_TEMP", 16) == 0) return "°C";
    if (strncmp(type, "SIM_DIGITAL_COUNTER", 19) == 0) return "counts";
    if (strncmp(type, "SIM_DIGITAL_SWITCH", 18) == 0) return "";
    
    // Real I2C sensors
    if (strncmp(type, "BME280", 6) == 0) return "°C/%%/hPa";
    if (strncmp(type, "SHT30", 5) == 0) return "°C/%%";
    
    // EZO sensors
    if (strncmp(type, "EZO_PH", 6) == 0) return "pH";
    if (strncmp(type, "EZO_EC", 6) == 0) return "μS/cm";
    if (strncmp(type, "EZO_DO", 6) == 0) return "mg/L";
    if (strncmp(type, "EZO_RTD", 7) == 0) return "°C";
    if (strncmp(type, "EZO_ORP", 7) == 0) return "mV";
    
    // Real UART sensors
    if (strncmp(type, "MODBUS_RTU", 10) == 0) return "varies";
    if (strncmp(type, "NMEA_GPS", 8) == 0) return "lat/lon";
    if (strncmp(type, "ASCII_SENSOR", 12) == 0) return "text";
    if (strncmp(type, "BINARY_SENSOR", 13) == 0) return "binary";
    if (strncmp(type, "GENERIC_UART", 12) == 0) return "custom";
    
    // Real Analog sensors
    if (strncmp(type, "ANALOG_4_20MA", 13) == 0) return "mA";
    if (strncmp(type, "ANALOG_0_10V", 12) == 0) return "V";
    if (strncmp(type, "ANALOG_THERMISTOR", 17) == 0) return "°C";
    if (strncmp(type, "ANALOG_PRESSURE", 15) == 0) return "PSI";
    if (strncmp(type, "ANALOG_CUSTOM", 13) == 0) return "custom";
    
    // Real One-Wire sensors
    if (strncmp(type, "DS18B20", 7) == 0) return "°C";
    if (strncmp(type, "DS18S20", 7) == 0) return "°C";
    if (strncmp(type, "DS1822", 6) == 0) return "°C";
    if (strncmp(type, "GENERIC_ONEWIRE", 15) == 0) return "custom";
    
    // Real Digital sensors
    if (strncmp(type, "DIGITAL_PULSE", 13) == 0) return "pulses";
    if (strncmp(type, "DIGITAL_SWITCH", 14) == 0) return "";
    if (strncmp(type, "DIGITAL_ENCODER", 15) == 0) return "steps";
    if (strncmp(type, "DIGITAL_FREQUENCY", 17) == 0) return "Hz";
    if (strncmp(type, "GENERIC_DIGITAL", 15) == 0) return "custom";
    
    // Generic sensors
    if (strncmp(type, "GENERIC_I2C", 11) == 0) return "custom";
    
    return "";
}

// Pin Management Functions
void initializePinAllocations() {
    numAllocatedPins = 0;
    memset(pinAllocations, 0, sizeof(pinAllocations));
    
    // Mark reserved pins as allocated
    uint8_t reservedPins[] = {PIN_ETH_MISO, PIN_ETH_CS, PIN_ETH_SCK, PIN_ETH_MOSI, PIN_ETH_RST, PIN_ETH_IRQ, PIN_EXT_LED};
    for (int i = 0; i < 7; i++) {
        pinAllocations[numAllocatedPins].pin = reservedPins[i];
        strcpy(pinAllocations[numAllocatedPins].protocol, "RESERVED");
        strcpy(pinAllocations[numAllocatedPins].sensorName, "System");
        pinAllocations[numAllocatedPins].allocated = true;
        numAllocatedPins++;
    }
}

bool isPinAvailable(uint8_t pin, const char* protocol) {
    // I2C pins can be shared among I2C sensors
    if (strcmp(protocol, "I2C") == 0) {
        // Check if pin is already allocated to non-I2C protocol
        for (int i = 0; i < numAllocatedPins; i++) {
            if (pinAllocations[i].pin == pin && pinAllocations[i].allocated) {
                if (strcmp(pinAllocations[i].protocol, "I2C") != 0) {
                    return false; // Pin is allocated to non-I2C protocol
                }
            }
        }
        return true; // Pin is available for I2C (can be shared)
    }
    
    // For non-I2C protocols, pin must be completely free
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].pin == pin && pinAllocations[i].allocated) {
            return false;
        }
    }
    return true;
}

void allocatePin(uint8_t pin, const char* protocol, const char* sensorName) {
    // Check if pin is already allocated to the same sensor (update case)
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].pin == pin && strcmp(pinAllocations[i].sensorName, sensorName) == 0) {
            strcpy(pinAllocations[i].protocol, protocol);
            pinAllocations[i].allocated = true;
            return;
        }
    }
    
    // Add new allocation
    if (numAllocatedPins < 40) {
        pinAllocations[numAllocatedPins].pin = pin;
        strcpy(pinAllocations[numAllocatedPins].protocol, protocol);
        strcpy(pinAllocations[numAllocatedPins].sensorName, sensorName);
        pinAllocations[numAllocatedPins].allocated = true;
        numAllocatedPins++;
    }
}

void deallocatePin(uint8_t pin) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].pin == pin && pinAllocations[i].allocated) {
            pinAllocations[i].allocated = false;
        }
    }
}

void deallocateSensorPins(const char* sensorName) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (strcmp(pinAllocations[i].sensorName, sensorName) == 0) {
            pinAllocations[i].allocated = false;
        }
    }
}

void handleGetAvailablePins() {
    String protocol = webServer.arg("protocol");
    
    StaticJsonDocument<1024> doc;
    JsonArray availablePins = doc.createNestedArray("pins");
    
    if (protocol == "I2C") {
        // Return available I2C pin pairs
        JsonArray pinPairs = doc.createNestedArray("pinPairs");
        for (int i = 0; i < NUM_I2C_PAIRS; i++) {
            JsonObject pair = pinPairs.createNestedObject();
            pair["sda"] = I2C_PIN_PAIRS[i][0];
            pair["scl"] = I2C_PIN_PAIRS[i][1];
            pair["label"] = "SDA:" + String(I2C_PIN_PAIRS[i][0]) + ", SCL:" + String(I2C_PIN_PAIRS[i][1]);
        }
    } else if (protocol == "UART") {
        // Return available pin pairs for UART
        JsonArray pinPairs = doc.createNestedArray("pinPairs");
        for (int i = 0; i < NUM_FLEXIBLE_PINS - 1; i++) {
            uint8_t txPin = AVAILABLE_FLEXIBLE_PINS[i];
            uint8_t rxPin = AVAILABLE_FLEXIBLE_PINS[i + 1];
            if (isPinAvailable(txPin, "UART") && isPinAvailable(rxPin, "UART")) {
                JsonObject pair = pinPairs.createNestedObject();
                pair["tx"] = txPin;
                pair["rx"] = rxPin;
                pair["label"] = "TX:" + String(txPin) + ", RX:" + String(rxPin);
            }
        }
    } else if (protocol == "Analog Voltage") {
        // Return available analog pins
        for (int i = 0; i < NUM_ADC_PINS; i++) {
            if (isPinAvailable(ADC_PINS[i], "Analog")) {
                JsonObject pin = availablePins.createNestedObject();
                pin["pin"] = ADC_PINS[i];
                pin["label"] = "AI" + String(i) + " (Pin " + String(ADC_PINS[i]) + ")";
            }
        }
    } else if (protocol == "One-Wire" || protocol == "Digital Counter") {
        // Return available flexible pins
        for (int i = 0; i < NUM_FLEXIBLE_PINS; i++) {
            if (isPinAvailable(AVAILABLE_FLEXIBLE_PINS[i], protocol.c_str())) {
                JsonObject pin = availablePins.createNestedObject();
                pin["pin"] = AVAILABLE_FLEXIBLE_PINS[i];
                pin["label"] = "Pin " + String(AVAILABLE_FLEXIBLE_PINS[i]);
            }
        }
    }
    
    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

void initializeI2C() {
    // Find first configured I2C sensor to determine pins to use
    int sdaPin = I2C_SDA_PIN;  // Default fallback
    int sclPin = I2C_SCL_PIN;  // Default fallback
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && 
            (strncmp(configuredSensors[i].type, "EZO_", 4) == 0 || 
             strncmp(configuredSensors[i].type, "SIM_I2C", 7) == 0 ||
             strncmp(configuredSensors[i].type, "BME", 3) == 0)) {
            sdaPin = configuredSensors[i].sdaPin;
            sclPin = configuredSensors[i].sclPin;
            Serial.printf("Using I2C pins from sensor %s: SDA=%d, SCL=%d\n", 
                configuredSensors[i].name, sdaPin, sclPin);
            break;
        }
    }
    
    // Initialize I2C with determined pins
    Wire.setSDA(sdaPin);
    Wire.setSCL(sclPin);
    Wire.begin();
    Serial.printf("I2C Bus Initialized on pins SDA=%d, SCL=%d\n", sdaPin, sclPin);
}

// Generic I2C sensor reading function - supports common sensor types
float readGenericI2CSensor(uint8_t i2cAddress, const char* sensorType) {
    Wire.beginTransmission(i2cAddress);
    int error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("I2C communication failed with sensor at 0x%02X, error: %d\n", i2cAddress, error);
        return -1.0; // Error indicator
    }
    
    // Handle specific sensor types with known protocols
    if (strncmp(sensorType, "SHT30", 5) == 0 || strncmp(sensorType, "SHT3", 4) == 0) {
        return readSHT30(i2cAddress);
    }
    else if (strncmp(sensorType, "BME280", 6) == 0) {
        return readBME280(i2cAddress);
    }
    else if (strncmp(sensorType, "AHT", 3) == 0) {
        return readAHT(i2cAddress);
    }
    else if (strncmp(sensorType, "GENERIC", 7) == 0) {
        return readGenericBytes(i2cAddress);
    }
    else {
        // For unknown sensor types, try basic generic read
        Serial.printf("Unknown sensor type '%s', attempting generic read\n", sensorType);
        return readGenericBytes(i2cAddress);
    }
}

// Enhanced multi-value sensor reading function
bool readMultiValueI2CSensor(SensorConfig* sensor) {
    Wire.beginTransmission(sensor->i2cAddress);
    int error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("I2C communication failed with sensor at 0x%02X, error: %d\n", sensor->i2cAddress, error);
        return false;
    }
    
    // Handle multi-value sensors
    if (strncmp(sensor->type, "SHT30", 5) == 0 || strncmp(sensor->type, "SHT3", 4) == 0) {
        return readSHT30MultiValue(sensor);
    }
    else if (strncmp(sensor->type, "BME280", 6) == 0) {
        return readBME280MultiValue(sensor);
    }
    else if (strncmp(sensor->type, "CONFIGURABLE", 12) == 0 || 
             strncmp(sensor->type, "GENERIC", 7) == 0 ||
             sensor->dataLength > 0) { // If data parsing is configured
        // Use configurable I2C reading for user-defined sensors
        return readConfigurableI2CSensor(sensor);
    }
    else {
        // Fall back to single-value reading for unknown sensors
        float value = readGenericI2CSensor(sensor->i2cAddress, sensor->type);
        if (value >= 0) {
            sensor->rawValue = value;
            strcpy(sensor->valueName, "value");
            sensor->rawValue2 = 0.0;
            sensor->valueName2[0] = '\0';
            return true;
        }
        return false;
    }
}

// SHT30 multi-value reading function
bool readSHT30MultiValue(SensorConfig* sensor) {
    // SHT30 measurement command: 0x2C06 (high repeatability, clock stretching disabled)
    Wire.beginTransmission(sensor->i2cAddress);
    Wire.write(0x2C);
    Wire.write(0x06);
    int error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("SHT30 command send failed: %d\n", error);
        return false;
    }
    
    delay(15); // Wait for measurement (SHT30 needs ~15ms)
    
    // Request 6 bytes (temp + humidity with CRC)
    Wire.requestFrom(sensor->i2cAddress, (uint8_t)6);
    
    if (Wire.available() != 6) {
        Serial.printf("SHT30 insufficient data received: %d bytes\n", Wire.available());
        return false;
    }
    
    // Read temperature data (first 3 bytes: MSB, LSB, CRC)
    uint8_t tempMSB = Wire.read();
    uint8_t tempLSB = Wire.read();
    uint8_t tempCRC = Wire.read();
    
    // Read humidity data (next 3 bytes: MSB, LSB, CRC)
    uint8_t humMSB = Wire.read();
    uint8_t humLSB = Wire.read();
    uint8_t humCRC = Wire.read();
    
    // Convert temperature (16-bit value)
    uint16_t tempRaw = (tempMSB << 8) | tempLSB;
    float temperature = -45.0 + 175.0 * ((float)tempRaw / 65535.0);
    
    // Convert humidity (16-bit value) 
    uint16_t humRaw = (humMSB << 8) | humLSB;
    float humidity = 100.0 * ((float)humRaw / 65535.0);
    
    // Store both values in sensor config
    sensor->rawValue = temperature;
    sensor->rawValue2 = humidity;
    strcpy(sensor->valueName, "temperature");
    strcpy(sensor->valueName2, "humidity");
    
    Serial.printf("SHT30 multi-value read - Temp: %.2f°C, Hum: %.2f%%\n", temperature, humidity);
    
    return true;
}

// BME280 multi-value placeholder
bool readBME280MultiValue(SensorConfig* sensor) {
    Serial.printf("BME280 multi-value reading not implemented yet for address 0x%02X\n", sensor->i2cAddress);
    return false;
}

// SHT30 temperature/humidity sensor reading
float readSHT30(uint8_t i2cAddress) {
    // SHT30 measurement command: 0x2C06 (high repeatability, clock stretching disabled)
    Wire.beginTransmission(i2cAddress);
    Wire.write(0x2C);
    Wire.write(0x06);
    int error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("SHT30 command send failed: %d\n", error);
        return -1.0;
    }
    
    delay(15); // Wait for measurement (SHT30 needs ~15ms)
    
    // Request 6 bytes (temp + humidity with CRC)
    Wire.requestFrom(i2cAddress, (uint8_t)6);
    
    if (Wire.available() != 6) {
        Serial.printf("SHT30 insufficient data received: %d bytes\n", Wire.available());
        return -1.0;
    }
    
    // Read temperature data (first 3 bytes: MSB, LSB, CRC)
    uint8_t tempMSB = Wire.read();
    uint8_t tempLSB = Wire.read();
    uint8_t tempCRC = Wire.read();
    
    // Read humidity data (next 3 bytes: MSB, LSB, CRC)
    uint8_t humMSB = Wire.read();
    uint8_t humLSB = Wire.read();
    uint8_t humCRC = Wire.read();
    
    // Convert temperature (16-bit value)
    uint16_t tempRaw = (tempMSB << 8) | tempLSB;
    float temperature = -45.0 + 175.0 * ((float)tempRaw / 65535.0);
    
    // Convert humidity (16-bit value) 
    uint16_t humRaw = (humMSB << 8) | humLSB;
    float humidity = 100.0 * ((float)humRaw / 65535.0);
    
    Serial.printf("SHT30 raw data - Temp: %d (%.2f°C), Hum: %d (%.2f%%)\n", 
                  tempRaw, temperature, humRaw, humidity);
    
    // Store humidity in global variable for multi-value sensor support
    // This is a temporary solution - we'll improve this in the sensor reading loop
    static float sht30_humidity = 0.0;
    sht30_humidity = humidity;
    
    // Return temperature as primary value
    return temperature;
}

// BME280 sensor reading (placeholder - requires BME280 library)
float readBME280(uint8_t i2cAddress) {
    Serial.printf("BME280 reading not implemented yet for address 0x%02X\n", i2cAddress);
    return -1.0;
}

// AHT sensor reading (placeholder)
float readAHT(uint8_t i2cAddress) {
    Serial.printf("AHT reading not implemented yet for address 0x%02X\n", i2cAddress);
    return -1.0;
}

// Generic byte reading for unknown sensors
float readGenericBytes(uint8_t i2cAddress) {
    // Try to read 2 bytes as a 16-bit value (common for many sensors)
    Wire.requestFrom(i2cAddress, (uint8_t)2);
    
    if (Wire.available() != 2) {
        Serial.printf("Generic read failed for 0x%02X: %d bytes available\n", i2cAddress, Wire.available());
        return -1.0;
    }
    
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    uint16_t rawValue = (msb << 8) | lsb;
    
    Serial.printf("Generic I2C read from 0x%02X: 0x%04X (%d)\n", i2cAddress, rawValue, rawValue);
    
    // Return as float for calibration processing
    return (float)rawValue;
}

// Configurable I2C sensor reading with flexible data parsing
bool readConfigurableI2CSensor(SensorConfig* sensor) {
    uint8_t buffer[32]; // Maximum 32 bytes
    uint8_t bytesToRead = sensor->dataLength;
    
    // Limit to reasonable size
    if (bytesToRead == 0 || bytesToRead > 32) {
        bytesToRead = 2; // Default to 2 bytes
    }
    
    // If register address is specified, write it first
    if (sensor->i2cRegister != 0xFF) {
        Wire.beginTransmission(sensor->i2cAddress);
        Wire.write(sensor->i2cRegister);
        int error = Wire.endTransmission(false); // Send restart, not stop
        
        if (error != 0) {
            Serial.printf("Configurable I2C: Register write failed to 0x%02X, error: %d\n", 
                         sensor->i2cAddress, error);
            return false;
        }
    }
    
    // Read the specified number of bytes
    int bytesReceived = Wire.requestFrom(sensor->i2cAddress, bytesToRead);
    
    if (bytesReceived != bytesToRead) {
        Serial.printf("Configurable I2C: Expected %d bytes, got %d from 0x%02X\n", 
                     bytesToRead, bytesReceived, sensor->i2cAddress);
        return false;
    }
    
    // Read all bytes into buffer
    for (int i = 0; i < bytesToRead; i++) {
        buffer[i] = Wire.read();
    }
    
    // Store raw data as hex string for debugging
    char hexStr[65] = {0}; // 32 bytes * 2 chars + null terminator
    for (int i = 0; i < bytesToRead; i++) {
        sprintf(hexStr + (i * 2), "%02X", buffer[i]);
    }
    strncpy(sensor->rawDataHex, hexStr, sizeof(sensor->rawDataHex) - 1);
    sensor->rawDataHex[sizeof(sensor->rawDataHex) - 1] = '\0';
    
    // Parse primary value
    float value1 = parseI2CData(buffer, sensor->dataOffset, sensor->dataFormat, bytesToRead);
    sensor->rawValue = value1;
    
    // Parse secondary value if configured
    if (sensor->dataOffset2 != 0xFF && sensor->dataOffset2 < bytesToRead) {
        float value2 = parseI2CData(buffer, sensor->dataOffset2, sensor->dataFormat2, bytesToRead);
        sensor->rawValue2 = value2;
    } else {
        sensor->rawValue2 = 0.0;
    }
    
    Serial.printf("Configurable I2C 0x%02X: Raw data [%s], Value1=%.2f, Value2=%.2f\n", 
                 sensor->i2cAddress, sensor->rawDataHex, sensor->rawValue, sensor->rawValue2);
    
    return true;
}

// Parse I2C data based on format specification
float parseI2CData(uint8_t* buffer, uint8_t offset, uint8_t format, uint8_t bufferLen) {
    if (offset >= bufferLen) {
        return 0.0;
    }
    
    switch (format) {
        case 0: // uint8
            return (float)buffer[offset];
            
        case 1: // uint16 big-endian
            if (offset + 1 < bufferLen) {
                uint16_t val = (buffer[offset] << 8) | buffer[offset + 1];
                return (float)val;
            }
            break;
            
        case 2: // uint16 little-endian
            if (offset + 1 < bufferLen) {
                uint16_t val = (buffer[offset + 1] << 8) | buffer[offset];
                return (float)val;
            }
            break;
            
        case 3: // uint32 big-endian
            if (offset + 3 < bufferLen) {
                uint32_t val = ((uint32_t)buffer[offset] << 24) | 
                              ((uint32_t)buffer[offset + 1] << 16) |
                              ((uint32_t)buffer[offset + 2] << 8) | 
                              buffer[offset + 3];
                return (float)val;
            }
            break;
            
        case 4: // uint32 little-endian
            if (offset + 3 < bufferLen) {
                uint32_t val = ((uint32_t)buffer[offset + 3] << 24) | 
                              ((uint32_t)buffer[offset + 2] << 16) |
                              ((uint32_t)buffer[offset + 1] << 8) | 
                              buffer[offset];
                return (float)val;
            }
            break;
            
        case 5: // float32 (IEEE 754)
            if (offset + 3 < bufferLen) {
                union { uint32_t i; float f; } converter;
                converter.i = ((uint32_t)buffer[offset] << 24) | 
                             ((uint32_t)buffer[offset + 1] << 16) |
                             ((uint32_t)buffer[offset + 2] << 8) | 
                             buffer[offset + 3];
                return converter.f;
            }
            break;
    }
    
    return 0.0;
}

float applySensorCalibration(float rawValue, SensorConfig* sensor) {
    // Apply mathematical expression calibration first if available
    if (strlen(sensor->expression) > 0) {
        // Simple variable substitution - replace 'x' with rawValue
        String expr = String(sensor->expression);
        expr.replace("x", String(rawValue, 6));
        expr.replace("X", String(rawValue, 6));
        
        // Basic expression evaluation for simple cases
        // For now, just handle linear expressions like "2*x + 5"
        if (expr.indexOf("*") > 0 && expr.indexOf("+") > 0) {
            int multPos = expr.indexOf("*");
            int plusPos = expr.indexOf("+");
            if (multPos < plusPos) {
                float multiplier = expr.substring(0, multPos).toFloat();
                float offset = expr.substring(plusPos + 1).toFloat();
                return multiplier * rawValue + offset;
            }
        }
        // Handle simple scale: "2*x" or "x*2"
        else if (expr.indexOf("*") > 0) {
            int multPos = expr.indexOf("*");
            if (expr.startsWith(String(rawValue, 6))) {
                float multiplier = expr.substring(multPos + 1).toFloat();
                return rawValue * multiplier;
            } else {
                float multiplier = expr.substring(0, multPos).toFloat();
                return multiplier * rawValue;
            }
        }
        // Handle simple offset: "x + 5"
        else if (expr.indexOf("+") > 0) {
            int plusPos = expr.indexOf("+");
            float offset = expr.substring(plusPos + 1).toFloat();
            return rawValue + offset;
        }
    }
    
    // Handle polynomial string calibration if available
    if (strlen(sensor->polynomialStr) > 0) {
        // Parse polynomial string like "2x^2 + 3x + 1"
        // For now, implement basic linear case: "ax + b"
        String poly = String(sensor->polynomialStr);
        poly.replace(" ", ""); // Remove spaces
        
        // Simple linear polynomial parser
        if (poly.indexOf("x") >= 0) {
            float result = 0.0;
            int xPos = poly.indexOf("x");
            
            // Get coefficient before x
            String coeffStr = poly.substring(0, xPos);
            float coeff = (coeffStr.length() == 0) ? 1.0 : coeffStr.toFloat();
            result += coeff * rawValue;
            
            // Get constant term after +
            int plusPos = poly.indexOf("+", xPos);
            if (plusPos > 0) {
                String constStr = poly.substring(plusPos + 1);
                result += constStr.toFloat();
            }
            
            return result;
        }
    }
    
    // Fall back to simple linear calibration
    return rawValue * sensor->scale + sensor->offset;
}

void setup() {
    Serial.begin(115200);
    uint32_t timeStamp = millis();
    while (!Serial) {
        if (millis() - timeStamp > 5000) {
            break;
        }
    }
    Serial.println("Booting...");
    
    pinMode(LED_BUILTIN, OUTPUT);

    analogReadResolution(12);
    
    // Initialize pin allocation tracking
    initializePinAllocations();
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS filesystem");
        // Format if mount failed
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
    // Load configuration
    loadConfig();
    
    Serial.println("Loading sensor configuration...");
    delay(500);
    // Load sensor configuration
    loadSensorConfig();

    Serial.println("Setting pin modes...");
    delay(500);
    // Set pin modes
    setPinModes();
    
    Serial.println("Setup network and services...");
    // Setup network and services
    setupEthernet();
    
    // Print IP address for easy connection
    Serial.println("========================================");
    Serial.print("IP Address: ");
    Serial.println(eth.localIP());
    Serial.println("========================================");
    
    setupModbus();
    setupWebServer();
    
    // Initialize I2C bus with dynamic pin allocation
    initializeI2C();
    
    // I2C Sensor Initialization Template - Uncomment when adding I2C sensors
    // Example for BME280 sensor:
    // if (!bme.begin(0x76)) { // Common I2C address for BME280
    //     Serial.println("Could not find a valid BME280 sensor, check wiring!");
    //     // Handle sensor initialization failure as needed
    // } else {
    //     Serial.println("BME280 sensor initialized successfully");
    // }
    
    core0setupComplete = true;

    // Start watchdog
    rp2040.wdt_begin(WDT_TIMEOUT);
}

void updateSimulatedSensors() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled || strncmp(configuredSensors[i].type, "SIM_", 4) != 0) {
            continue;
        }
        
        // Update simulation every 500ms
        if (currentTime - configuredSensors[i].lastSimulationUpdate > 500) {
            float baseValue = 0;
            float variation = 0;
            
            // Set base values and variations based on sensor type
            if (strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0) {
                baseValue = 22.5; // °C
                variation = sin(currentTime / 5000.0) * 5.0;
                configuredSensors[i].simulatedValue = baseValue + variation;
                ioStatus.temperature = configuredSensors[i].simulatedValue; // Update global value
            }
            else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
                baseValue = 45.0; // %
                variation = cos(currentTime / 7000.0) * 10.0;
                configuredSensors[i].simulatedValue = baseValue + variation;
                ioStatus.humidity = configuredSensors[i].simulatedValue; // Update global value
            }
            else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
                baseValue = 1013.25; // hPa
                variation = sin(currentTime / 9000.0) * 50.0;
                configuredSensors[i].simulatedValue = baseValue + variation;
                ioStatus.pressure = configuredSensors[i].simulatedValue; // Update global value
            }
            else if (strcmp(configuredSensors[i].type, "SIM_ANALOG_VOLTAGE") == 0) {
                baseValue = 1650; // mV (middle of 0-3300 range)
                variation = sin(currentTime / 5000.0) * 800.0;
                configuredSensors[i].simulatedValue = baseValue + variation;
            }
            else if (strcmp(configuredSensors[i].type, "SIM_ANALOG_CURRENT") == 0) {
                baseValue = 12; // mA (4-20mA loop)
                variation = cos(currentTime / 7000.0) * 4.0;
                configuredSensors[i].simulatedValue = baseValue + variation;
            }
            else if (strcmp(configuredSensors[i].type, "SIM_DIGITAL_SWITCH") == 0) {
                // Random state changes every ~10 seconds
                if (random(1000) < 10) { // 1% chance per update
                    configuredSensors[i].simulatedValue = configuredSensors[i].simulatedValue > 0.5 ? 0.0 : 1.0;
                }
                configuredSensors[i].lastSimulationUpdate = currentTime;
                continue; // Skip the normal update below
            }
            else if (strcmp(configuredSensors[i].type, "SIM_DIGITAL_COUNTER") == 0) {
                // Increment occasionally
                if (random(1000) < 20) { // 2% chance per update
                    configuredSensors[i].simulatedValue += 1.0;
                }
                configuredSensors[i].lastSimulationUpdate = currentTime;
                continue; // Skip the normal update below
            }
            
            configuredSensors[i].lastSimulationUpdate = currentTime;
        }
    }
}

void loop() {
    // Memory check at start of loop
    uint32_t free_heap = rp2040.getFreeHeap();
    static uint32_t min_heap = UINT32_MAX;
    if (free_heap < min_heap) {
        min_heap = free_heap;
        if (free_heap < 50000) { // Less than 50KB free
            Serial.printf("Warning: Low memory: %u bytes free (min seen: %u)\n", free_heap, min_heap);
        }
    }
    
    // Check for new client connections
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
    
    // Add yield point after Modbus client handling
    yield();
    
    updateIOpins();
    
    // Add yield point after IO updates
    yield();
    
    updateSimulatedSensors();
    handleEzoSensors();
    
    // Add yield point after sensor handling
    yield();
    
    webServer.handleClient();
    
    // Check network connectivity periodically
    static unsigned long lastNetworkCheck = 0;
    if (millis() - lastNetworkCheck > 30000) { // Check every 30 seconds
        lastNetworkCheck = millis();
        
        // Simple connectivity check - if we can't get our own IP, there's a problem
        if (!WiFi.localIP()) {
            Serial.println("Network connection lost, attempting reconnection...");
            // Don't restart network here as it's disruptive, just log the issue
            // The network stack should handle DHCP renewal automatically
        }
    }

    // Watchdog timer reset
    rp2040.wdt_reset();
}

void initializeEzoSensors() {
    if (ezoSensorsInitialized) return;
    
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
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
        if (!configuredSensors[i].enabled || strncmp(configuredSensors[i].type, "EZO_", 4) != 0) {
            continue;
        }
        
        if (ezoSensors[i] == nullptr) {
            continue;
        }
        
        unsigned long currentTime = millis();
        
        if (configuredSensors[i].cmdPending && (currentTime - configuredSensors[i].lastCmdSent > 1000)) {
            ezoSensors[i]->receive_read_cmd();
            
            if (ezoSensors[i]->get_error() == Ezo_board::SUCCESS) {
                float reading = ezoSensors[i]->get_last_received_reading();
                snprintf(configuredSensors[i].response, sizeof(configuredSensors[i].response), "%.2f", reading);
                Serial.printf("EZO sensor %s reading: %s\n", configuredSensors[i].name, configuredSensors[i].response);
            } else {
                snprintf(configuredSensors[i].response, sizeof(configuredSensors[i].response), "ERROR");
                Serial.printf("EZO sensor %s error: %d\n", configuredSensors[i].name, ezoSensors[i]->get_error());
            }
            
            configuredSensors[i].cmdPending = false;
        } 
        else if (!configuredSensors[i].cmdPending && (currentTime - configuredSensors[i].lastCmdSent > 5000)) {
            ezoSensors[i]->send_read_cmd();
            configuredSensors[i].lastCmdSent = currentTime;
            configuredSensors[i].cmdPending = true;
            Serial.printf("Sent read command to EZO sensor %s\n", configuredSensors[i].name);
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
    
    // Create JSON document to parse the file
    StaticJsonDocument<1024> doc;
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
            
            // Parse sensor configuration
            configuredSensors[index].enabled = sensor["enabled"] | false;
            
            const char* name = sensor["name"] | "";
            strncpy(configuredSensors[index].name, name, sizeof(configuredSensors[index].name) - 1);
            configuredSensors[index].name[sizeof(configuredSensors[index].name) - 1] = '\0';
            
            const char* type = sensor["type"] | "";
            strncpy(configuredSensors[index].type, type, sizeof(configuredSensors[index].type) - 1);
            configuredSensors[index].type[sizeof(configuredSensors[index].type) - 1] = '\0';
            
            const char* protocol = sensor["protocol"] | "";
            strncpy(configuredSensors[index].protocol, protocol, sizeof(configuredSensors[index].protocol) - 1);
            configuredSensors[index].protocol[sizeof(configuredSensors[index].protocol) - 1] = '\0';
            
            configuredSensors[index].i2cAddress = sensor["i2cAddress"] | 0;
            configuredSensors[index].modbusRegister = sensor["modbusRegister"] | 0;
            
            // Load pin assignments
            configuredSensors[index].sdaPin = sensor["sdaPin"] | I2C_PIN_PAIRS[0][0];
            configuredSensors[index].sclPin = sensor["sclPin"] | I2C_PIN_PAIRS[0][1];
            configuredSensors[index].txPin = sensor["txPin"] | 0;
            configuredSensors[index].rxPin = sensor["rxPin"] | 1;
            configuredSensors[index].analogPin = sensor["analogPin"] | 26;
            configuredSensors[index].digitalPin = sensor["digitalPin"] | 0;
            
            // Initialize simulation values for SIM_ sensors
            if (strncmp(configuredSensors[index].type, "SIM_", 4) == 0) {
                configuredSensors[index].lastSimulationUpdate = 0;
                
                // Set initial simulation values based on sensor type
                if (strcmp(configuredSensors[index].type, "SIM_I2C_TEMPERATURE") == 0) {
                    configuredSensors[index].simulatedValue = 22.5;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_I2C_HUMIDITY") == 0) {
                    configuredSensors[index].simulatedValue = 45.0;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_I2C_PRESSURE") == 0) {
                    configuredSensors[index].simulatedValue = 1013.25;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_ANALOG_VOLTAGE") == 0) {
                    configuredSensors[index].simulatedValue = 1650.0;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_ANALOG_CURRENT") == 0) {
                    configuredSensors[index].simulatedValue = 12.0;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_DIGITAL_SWITCH") == 0) {
                    configuredSensors[index].simulatedValue = 0.0;
                }
                else if (strcmp(configuredSensors[index].type, "SIM_DIGITAL_COUNTER") == 0) {
                    configuredSensors[index].simulatedValue = 0.0;
                }
            }
            
            // Initialize multi-value sensor fields
            configuredSensors[index].rawValue = 0.0;
            configuredSensors[index].calibratedValue = 0.0;
            configuredSensors[index].rawValue2 = 0.0;
            configuredSensors[index].calibratedValue2 = 0.0;
            configuredSensors[index].modbusRegister2 = 0; // 0 means not used
            strcpy(configuredSensors[index].valueName, "value");
            configuredSensors[index].valueName2[0] = '\0'; // Empty string
            
            // Initialize configurable I2C parsing fields
            configuredSensors[index].i2cRegister = 0xFF; // 0xFF means no register address (direct read)
            configuredSensors[index].dataLength = 2; // Default to 2 bytes
            configuredSensors[index].dataOffset = 0; // Start at byte 0
            configuredSensors[index].dataOffset2 = 0xFF; // No secondary value by default
            configuredSensors[index].dataFormat = 1; // Default to uint16 big-endian
            configuredSensors[index].dataFormat2 = 1; // Default to uint16 big-endian
            configuredSensors[index].rawDataHex[0] = '\0'; // Empty string
            
            // Debug output
            Serial.printf("Loaded sensor %d: %s (%s) at I2C 0x%02X, Modbus register %d, enabled: %s\n",
                index,
                configuredSensors[index].name,
                configuredSensors[index].type,
                configuredSensors[index].i2cAddress,
                configuredSensors[index].modbusRegister,
                configuredSensors[index].enabled ? "true" : "false"
            );
            
            index++;
        }
        
        numConfiguredSensors = index;
        Serial.printf("Loaded %d sensor configurations\n", numConfiguredSensors);
        
        // Rebuild pin allocations based on loaded sensors
        initializePinAllocations();
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled) {
                const char* protocol = configuredSensors[i].protocol;
                const char* name = configuredSensors[i].name;
                
                if (strcmp(protocol, "I2C") == 0) {
                    allocatePin(configuredSensors[i].sdaPin, "I2C", name);
                    allocatePin(configuredSensors[i].sclPin, "I2C", name);
                } else if (strcmp(protocol, "UART") == 0) {
                    allocatePin(configuredSensors[i].txPin, "UART", name);
                    allocatePin(configuredSensors[i].rxPin, "UART", name);
                } else if (strcmp(protocol, "Analog Voltage") == 0) {
                    allocatePin(configuredSensors[i].analogPin, "Analog", name);
                } else if (strcmp(protocol, "One-Wire") == 0 || strcmp(protocol, "Digital Counter") == 0) {
                    allocatePin(configuredSensors[i].digitalPin, protocol, name);
                }
            }
        }
    } else {
        Serial.println("No sensors array found in configuration file");
    }
}

void saveSensorConfig() {
    Serial.println("Saving sensor configuration...");
    
    // Create JSON document
    StaticJsonDocument<1024> doc;
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
        
        // Save pin assignments
        sensor["sdaPin"] = configuredSensors[i].sdaPin;
        sensor["sclPin"] = configuredSensors[i].sclPin;
        sensor["txPin"] = configuredSensors[i].txPin;
        sensor["rxPin"] = configuredSensors[i].rxPin;
        sensor["analogPin"] = configuredSensors[i].analogPin;
        sensor["digitalPin"] = configuredSensors[i].digitalPin;
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
        Serial.println("Sensor configuration saved successfully");
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
    // Initialize SPI for Ethernet
    SPI.setRX(PIN_ETH_MISO);
    SPI.setCS(PIN_ETH_CS);
    SPI.setSCK(PIN_ETH_SCK);
    SPI.setTX(PIN_ETH_MOSI);
    SPI.begin();
    
    // Set hostname first
    eth.hostname(config.hostname);
    eth.setSPISpeed(30000000);
    lwipPollingPeriod(3);
    
    bool connected = false;
    
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
    
    Serial.print("Hostname: ");
    Serial.println(config.hostname);
    Serial.print("IP Address: ");
    Serial.println(eth.localIP());
    
    if (!connected || eth.status() != WL_CONNECTED) {
        Serial.println("WARNING: Network connection not established");
    }
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

// Web handler function declarations
void handleSetSensorCalibration();

void setupWebServer() {
    // Add a small delay before starting the web server
    // This helps ensure the network stack is fully initialized
    delay(50);
    
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/index.html", HTTP_GET, handleRoot);
    webServer.on("/styles.css", HTTP_GET, handleCSS);
    webServer.on("/script.js", HTTP_GET, handleJS);
    webServer.on("/favicon.ico", HTTP_GET, handleFavicon);
    webServer.on("/logo.png", HTTP_GET, handleLogo);
    webServer.on("/config", HTTP_GET, handleGetConfig);
    webServer.on("/config", HTTP_POST, handleSetConfig);
    webServer.on("/iostatus", HTTP_GET, handleGetIOStatus);
    webServer.on("/setoutput", HTTP_POST, handleSetOutput);
    webServer.on("/ioconfig", HTTP_GET, handleGetIOConfig);
    webServer.on("/ioconfig", HTTP_POST, handleSetIOConfig);
    webServer.on("/reset-latches", HTTP_POST, handleResetLatches);
    webServer.on("/reset-latch", HTTP_POST, handleResetSingleLatch);
    webServer.on("/sensors/config", HTTP_GET, handleGetSensorConfig);
    webServer.on("/sensors/config", HTTP_POST, handleSetSensorConfig);
    webServer.on("/api/sensor/command", HTTP_POST, handleSensorCommand);
    webServer.on("/api/sensor/calibration", HTTP_POST, handleSetSensorCalibration);
    webServer.on("/terminal/command", HTTP_POST, handleTerminalCommand);
    webServer.on("/terminal/watch", HTTP_POST, handleTerminalWatch);
    webServer.on("/available-pins", HTTP_GET, handleGetAvailablePins);
    webServer.begin();
    
    Serial.println("Web server started");
}

void updateIOpins() {
    // Update Modbus registers with current IO state
    
    // Update digital inputs - account for invert configuration and latching behavior
    for (int i = 0; i < 8; i++) {
        uint16_t rawValue;
        
        // Check if there's a simulated digital sensor for this channel
        bool simulationActive = false;
        for (int j = 0; j < numConfiguredSensors; j++) {
            if (configuredSensors[j].enabled && 
                configuredSensors[j].modbusRegister == i &&
                (strncmp(configuredSensors[j].type, "SIM_DIGITAL", 11) == 0)) {
                // Use simulated value instead of real digital reading
                rawValue = configuredSensors[j].simulatedValue > 0.5 ? 1 : 0;
                simulationActive = true;
                break;
            }
        }
        
        // If no simulation active, read from real hardware
        if (!simulationActive) {
            rawValue = digitalRead(DIGITAL_INPUTS[i]);
        }
        
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
        // If latching is not enabled, just use the raw value
        else {
            ioStatus.dIn[i] = rawValue;
            ioStatus.dInLatched[i] = false; // Ensure latch flag is cleared
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
        // Check if there's a simulated analog sensor for this channel
        bool simulationActive = false;
        for (int j = 0; j < numConfiguredSensors; j++) {
            if (configuredSensors[j].enabled && 
                configuredSensors[j].modbusRegister == i &&
                (strncmp(configuredSensors[j].type, "SIM_ANALOG", 10) == 0)) {
                // Use simulated value instead of real ADC reading
                ioStatus.aIn[i] = (uint16_t)configuredSensors[j].simulatedValue;
                simulationActive = true;
                break;
            }
        }
        
        // If no simulation active, read from real hardware
        if (!simulationActive) {
            uint32_t rawValue = analogRead(ANALOG_INPUTS[i]);
            uint16_t valueToWrite = (rawValue * 3300UL) / 4095UL;
            ioStatus.aIn[i] = valueToWrite;
        }
    }
    
    // I2C Sensor Reading - Dynamic sensor configuration
    static uint32_t sensorReadTime = 0;
    if (millis() - sensorReadTime > 1000) { // Update every 1 second
        // Initialize sensor values - only temperature is used by EZO_RTD if configured
        ioStatus.temperature = 0.0;  // Only used by EZO_RTD sensors for pH compensation
        
        // Read from configured sensors
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled) {
                Serial.printf("Reading sensor %s (%s) at I2C address 0x%02X on pins SDA=%d, SCL=%d\n", 
                    configuredSensors[i].name,
                    configuredSensors[i].type,
                    configuredSensors[i].i2cAddress,
                    configuredSensors[i].sdaPin,
                    configuredSensors[i].sclPin
                );
                
                // Configure I2C pins for this sensor
                Wire.setSDA(configuredSensors[i].sdaPin);
                Wire.setSCL(configuredSensors[i].sclPin);
                Wire.begin();
                
                float rawValue = 0.0;
                bool readSuccess = false;
                
                // Add actual sensor reading logic here based on sensor type
                if (strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
                    // Read from EZO sensor via I2C
                    Wire.beginTransmission(configuredSensors[i].i2cAddress);
                    Wire.write('R');  // Send read command to EZO sensor
                    int error = Wire.endTransmission();
                    
                    if (error == 0) {
                        delay(900);  // EZO sensors need time to process
                        
                        Wire.requestFrom(configuredSensors[i].i2cAddress, 32);
                        String response = "";
                        while (Wire.available()) {
                            char c = Wire.read();
                            if (c != 1 && c != 0) {  // Filter out status bytes
                                response += c;
                            }
                        }
                        
                        if (response.length() > 0) {
                            rawValue = response.toFloat();
                            readSuccess = true;
                            
                            // Store response for debugging
                            strncpy(configuredSensors[i].response, response.c_str(), 
                                   sizeof(configuredSensors[i].response) - 1);
                            configuredSensors[i].response[sizeof(configuredSensors[i].response) - 1] = '\0';
                            
                            Serial.printf("EZO sensor %s raw reading: %.2f\n", configuredSensors[i].type, rawValue);
                        } else {
                            strcpy(configuredSensors[i].response, "ERROR");
                            Serial.printf("EZO sensor %s: No response\n", configuredSensors[i].type);
                        }
                    } else {
                        strcpy(configuredSensors[i].response, "ERROR");
                        Serial.printf("EZO sensor %s: I2C communication error %d\n", configuredSensors[i].type, error);
                    }
                }
                // Handle simulated I2C sensors
                else if (strncmp(configuredSensors[i].type, "SIM_I2C", 7) == 0) {
                    rawValue = configuredSensors[i].simulatedValue;
                    readSuccess = true;
                    Serial.printf("Simulated sensor %s: %.2f\n", configuredSensors[i].type, rawValue);
                }
                // Handle generic I2C sensors (any real I2C sensor that's not EZO or simulated)
                else if (strcmp(configuredSensors[i].protocol, "I2C") == 0) {
                    // Use multi-value sensor reading for advanced sensors
                    if (readMultiValueI2CSensor(&configuredSensors[i])) {
                        readSuccess = true;
                        Serial.printf("Multi-value I2C sensor %s (0x%02X): Primary=%.2f, Secondary=%.2f\n", 
                            configuredSensors[i].type, configuredSensors[i].i2cAddress, 
                            configuredSensors[i].rawValue, configuredSensors[i].rawValue2);
                    } else {
                        Serial.printf("Multi-value I2C sensor %s (0x%02X): Read failed\n", 
                            configuredSensors[i].type, configuredSensors[i].i2cAddress);
                    }
                    
                    // Use primary value for main processing
                    rawValue = configuredSensors[i].rawValue;
                }
                
                // Store raw value for later calibration
                if (readSuccess) {
                    configuredSensors[i].rawValue = rawValue;
                    
                    // Apply calibration
                    configuredSensors[i].calibratedValue = applySensorCalibration(rawValue, &configuredSensors[i]);
                    
                    Serial.printf("Sensor %s: Raw=%.2f, Calibrated=%.2f\n", 
                        configuredSensors[i].name, rawValue, configuredSensors[i].calibratedValue);
                    
                    // Handle secondary value for multi-value sensors
                    if (configuredSensors[i].rawValue2 != 0.0 && strlen(configuredSensors[i].valueName2) > 0) {
                        // Apply calibration to secondary value (use same calibration parameters for now)
                        configuredSensors[i].calibratedValue2 = applySensorCalibration(configuredSensors[i].rawValue2, &configuredSensors[i]);
                        
                        Serial.printf("Sensor %s secondary (%s): Raw=%.2f, Calibrated=%.2f\n", 
                            configuredSensors[i].name, configuredSensors[i].valueName2, 
                            configuredSensors[i].rawValue2, configuredSensors[i].calibratedValue2);
                    }
                    
                    // Map sensor types to appropriate ioStatus fields for backwards compatibility
                    if (strcmp(configuredSensors[i].type, "EZO_RTD") == 0 || 
                        strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0 ||
                        strncmp(configuredSensors[i].type, "SHT30", 5) == 0) {
                        ioStatus.temperature = configuredSensors[i].calibratedValue;
                        // For SHT30, also map humidity if secondary value exists
                        if (configuredSensors[i].rawValue2 != 0.0 && strcmp(configuredSensors[i].valueName2, "humidity") == 0) {
                            ioStatus.humidity = configuredSensors[i].calibratedValue2;
                        }
                    } else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
                        ioStatus.humidity = configuredSensors[i].calibratedValue;
                    } else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
                        ioStatus.pressure = configuredSensors[i].calibratedValue;
                    }
                } else {
                    configuredSensors[i].rawValue = 0.0;
                    configuredSensors[i].calibratedValue = 0.0;
                }
            }
        }
        
        // No built-in sensors - all sensor data comes from configured I2C sensors
        if (numConfiguredSensors == 0) {
            Serial.println("No I2C sensors configured");
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
    
    // Dynamic Sensor Data Modbus Mapping - Map calibrated sensor values to configured registers
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && configuredSensors[i].modbusRegister >= 3) {
            // Convert calibrated float value to 16-bit integer for Modbus
            // Using scaling of x100 to preserve 2 decimal places
            uint16_t scaledValue = (uint16_t)(configuredSensors[i].calibratedValue * 100);
            
            modbusClients[clientIndex].server.inputRegisterWrite(
                configuredSensors[i].modbusRegister, 
                scaledValue
            );
            
            Serial.printf("Modbus Register %d: Sensor %s = %.2f (scaled: %d)\n",
                configuredSensors[i].modbusRegister,
                configuredSensors[i].name,
                configuredSensors[i].calibratedValue,
                scaledValue
            );
            
            // Handle secondary value if configured
            if (configuredSensors[i].modbusRegister2 > 0 && configuredSensors[i].rawValue2 != 0.0) {
                uint16_t scaledValue2 = (uint16_t)(configuredSensors[i].calibratedValue2 * 100);
                
                modbusClients[clientIndex].server.inputRegisterWrite(
                    configuredSensors[i].modbusRegister2, 
                    scaledValue2
                );
                
                Serial.printf("Modbus Register %d: Sensor %s (%s) = %.2f (scaled: %d)\n",
                    configuredSensors[i].modbusRegister2,
                    configuredSensors[i].name,
                    configuredSensors[i].valueName2,
                    configuredSensors[i].calibratedValue2,
                    scaledValue2
                );
            }
        }
    }
    
    // Legacy I2C Sensor Data Modbus Mapping - Convert float values to 16-bit integers
    // Only enabled when sensors are actually configured and have data
    if (ioStatus.temperature != 0.0) {
        uint16_t temp_x_100 = (uint16_t)(ioStatus.temperature * 100);
        // Map to register 3 if not already used by configured sensor
        bool register3Used = false;
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled && configuredSensors[i].modbusRegister == 3) {
                register3Used = true;
                break;
            }
        }
        if (!register3Used) {
            modbusClients[clientIndex].server.inputRegisterWrite(3, temp_x_100); // Temperature
        }
    }
    
    if (ioStatus.humidity != 0.0) {
        uint16_t hum_x_100 = (uint16_t)(ioStatus.humidity * 100);
        // Map to register 4 if not already used by configured sensor
        bool register4Used = false;
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled && configuredSensors[i].modbusRegister == 4) {
                register4Used = true;
                break;
            }
        }
        if (!register4Used) {
            modbusClients[clientIndex].server.inputRegisterWrite(4, hum_x_100); // Humidity
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

void handleRoot() {
    if (LittleFS.exists("/index.html")) {
        File file = LittleFS.open("/index.html", "r");
        webServer.streamFile(file, "text/html");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void handleCSS() {
    if (LittleFS.exists("/styles.css")) {
        File file = LittleFS.open("/styles.css", "r");
        webServer.streamFile(file, "text/css");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void handleJS() {
    if (LittleFS.exists("/script.js")) {
        File file = LittleFS.open("/script.js", "r");
        webServer.streamFile(file, "application/javascript");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void handleFavicon() {
    if (LittleFS.exists("/favicon.ico")) {
        File file = LittleFS.open("/favicon.ico", "r");
        webServer.streamFile(file, "image/x-icon");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void handleLogo() {
    if (LittleFS.exists("/logo.png")) {
        File file = LittleFS.open("/logo.png", "r");
        webServer.streamFile(file, "image/png");
        file.close();
    } else {
        webServer.send(404, "text/plain", "File not found");
    }
}

void handleGetConfig() {
    // Add a brief delay to ensure all internal states are updated
    // This helps with the first request after server initialization
    delay(5);
    
    char jsonBuffer[1024];  
    StaticJsonDocument<1024> doc;  

    // Add network configuration
    doc["dhcp"] = config.dhcpEnabled;
    
    // Format IP addresses as strings
    char ipStr[16], gwStr[16], subStr[16], currentIpStr[16];
    sprintf(ipStr, "%d.%d.%d.%d", config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
    sprintf(gwStr, "%d.%d.%d.%d", config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]);
    sprintf(subStr, "%d.%d.%d.%d", config.subnet[0], config.subnet[1], config.subnet[2], config.subnet[3]);
    
    doc["ip"] = ipStr;
    doc["gateway"] = gwStr;
    doc["subnet"] = subStr;
    doc["modbus_port"] = config.modbusPort;
    doc["hostname"] = config.hostname;
    
    // Always include the current IP address
    IPAddress localIP = eth.localIP();
    // Check if IP is valid (not 0.0.0.0)
    if (localIP[0] == 0 && localIP[1] == 0 && localIP[2] == 0 && localIP[3] == 0) {
        // IP is not set, use configured IP instead
        doc["current_ip"] = ipStr;
    } else {
        sprintf(currentIpStr, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
        doc["current_ip"] = currentIpStr;
    }

    // Add Modbus client status
    doc["modbus_connected"] = (connectedClients > 0);
    doc["modbus_client_count"] = connectedClients;
    
    // Add detailed client information
    if (connectedClients > 0) {
        JsonArray clients = doc.createNestedArray("modbus_clients");
        
        for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
            if (modbusClients[i].connected) {
                JsonObject client = clients.createNestedObject();
                
                // Add client IP address
                IPAddress clientIP = modbusClients[i].clientIP;
                char clientIPStr[16];
                sprintf(clientIPStr, "%d.%d.%d.%d", clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
                client["ip"] = clientIPStr;
                
                // Add client slot number
                client["slot"] = i;
                
                // Add connection duration in seconds
                unsigned long duration = (millis() - modbusClients[i].connectionTime) / 1000;
                client["connected_for"] = duration;
            }
        }
    }

    // Digital inputs - account for inversion
    JsonArray di = doc.createNestedArray("di");
    for (int i = 0; i < 8; i++) {
        di.add(ioStatus.dIn[i]);
    }

    // Digital outputs - account for inversion
    JsonArray do_ = doc.createNestedArray("do");
    for (int i = 0; i < 8; i++) {
        do_.add(ioStatus.dOut[i]);
    }

    // Add analog input values - always in millivolts
    JsonArray ai = doc.createNestedArray("ai");
    for (int i = 0; i < sizeof(ANALOG_INPUTS)/sizeof(ANALOG_INPUTS[0]); i++) {
        ai.add(ioStatus.aIn[i]);
    }

    // I2C Sensor Data Template - Uncomment when adding I2C sensors
    // Add sensor readings to config response:
    // doc["temperature"] = ioStatus.temperature;    // Temperature in Celsius
    // doc["humidity"] = ioStatus.humidity;          // Humidity in %
    // doc["pressure"] = ioStatus.pressure;          // Pressure in hPa

    serializeJson(doc, jsonBuffer);
    webServer.send(200, "application/json", jsonBuffer);
}

void handleSetConfig() {
    Serial.println("Received config update request");
    
    if (!webServer.hasArg("plain")) {
        Serial.println("No data received");
        webServer.send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }
    
    String data = webServer.arg("plain");
    Serial.print("Received data length: ");
    Serial.println(data.length());

    // Debug: Print the raw JSON data
    Serial.println("Raw JSON data received:");
    Serial.println(data);
    
    // Prevent buffer overflow
    if (data.length() > 256) {
        Serial.println("Data too long");
        webServer.send(400, "application/json", "{\"error\":\"Request too large\"}");
        return;
    }

    // Allocate JSON document based on input size
    const size_t capacity = JSON_OBJECT_SIZE(6) + 150;  
    StaticJsonDocument<384> doc;  
    
    Serial.print("JSON capacity: ");
    Serial.println(capacity);

    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        webServer.send(400, "application/json", "{\"error\":\"JSON parse error\"}");
        return;
    }

    Serial.println("JSON parsed successfully");

    // Process fields directly from JSON to save memory
    bool dhcpEnabled = doc["dhcp"] | config.dhcpEnabled;
    
    // Detailed debug for modbusPort field
    Serial.println("Checking for modbusPort in JSON data...");
    if (doc.containsKey("modbus_port")) {
        Serial.println("modbus_port field found!");
        Serial.print("Raw value: ");
        serializeJson(doc["modbus_port"], Serial);
        Serial.println();
        
        // Try to get as different types to debug type issues
        String portStr = doc["modbus_port"].as<String>();
        int portInt = doc["modbus_port"].as<int>();
        Serial.print("As String: ");
        Serial.println(portStr);
        Serial.print("As int: ");
        Serial.println(portInt);
        
        Serial.print("Final parsed modbus_port value (as uint16_t): ");
        Serial.println(doc["modbus_port"].as<uint16_t>());
    } else {
        Serial.println("modbus_port field NOT found in JSON!");
        Serial.print("Using existing value: ");
        Serial.println(config.modbusPort);
    }
    
    uint16_t modbusPort = doc["modbus_port"] | config.modbusPort;
    
    Serial.print("Received modbus_port value from web: ");
    Serial.println(modbusPort);
    
    // Validate port early
    if (modbusPort <= 0 || modbusPort > 65535) {
        webServer.send(400, "application/json", "{\"error\":\"Invalid port\"}");
        return;
    }

    // Process IP addresses
    uint8_t newIp[4], newGw[4], newSub[4];
    memcpy(newIp, config.ip, 4);
    memcpy(newGw, config.gateway, 4);
    memcpy(newSub, config.subnet, 4);

    const char* ipStr = doc["ip"] | "";
    if (*ipStr) {  
        if (sscanf(ipStr, "%hhu.%hhu.%hhu.%hhu", 
            &newIp[0], &newIp[1], &newIp[2], &newIp[3]) != 4) {
            webServer.send(400, "application/json", "{\"error\":\"Invalid IP\"}");
            return;
        }
    }

    const char* gwStr = doc["gateway"] | "";
    if (*gwStr) {
        if (sscanf(gwStr, "%hhu.%hhu.%hhu.%hhu",
            &newGw[0], &newGw[1], &newGw[2], &newGw[3]) != 4) {
            webServer.send(400, "application/json", "{\"error\":\"Invalid gateway\"}");
            return;
        }
    }

    const char* subStr = doc["subnet"] | "";
    if (*subStr) {
        if (sscanf(subStr, "%hhu.%hhu.%hhu.%hhu",
            &newSub[0], &newSub[1], &newSub[2], &newSub[3]) != 4) {
            webServer.send(400, "application/json", "{\"error\":\"Invalid subnet\"}");
            return;
        }
    }

    // Process hostname
    char newHostname[HOSTNAME_MAX_LENGTH];
    strncpy(newHostname, config.hostname, HOSTNAME_MAX_LENGTH - 1);
    newHostname[HOSTNAME_MAX_LENGTH - 1] = '\0';

    const char* hostname = doc["hostname"] | "";
    if (*hostname) {
        if (strlen(hostname) >= HOSTNAME_MAX_LENGTH) {
            webServer.send(400, "application/json", "{\"error\":\"Hostname too long\"}");
            return;
        }
        strncpy(newHostname, hostname, HOSTNAME_MAX_LENGTH - 1);
        newHostname[HOSTNAME_MAX_LENGTH - 1] = '\0';
    }

    Serial.println("All validation passed, updating config");

    // Apply the new configuration
    config.dhcpEnabled = dhcpEnabled;
    config.modbusPort = modbusPort;
    memcpy(config.ip, newIp, 4);
    memcpy(config.gateway, newGw, 4);
    memcpy(config.subnet, newSub, 4);
    strncpy(config.hostname, newHostname, HOSTNAME_MAX_LENGTH - 1);
    config.hostname[HOSTNAME_MAX_LENGTH - 1] = '\0';

    Serial.println("Saving to LittleFS");
    saveConfig();

    // Send minimal response
    char response[64];
    snprintf(response, sizeof(response), 
        "{\"success\":true,\"ip\":\"%d.%d.%d.%d\"}", 
        config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
    
    webServer.send(200, "application/json", response);
    Serial.println("Config update complete, rebooting...");
    rp2040.reboot();
}

void handleSetOutput() {
    if (!webServer.hasArg("output") || !webServer.hasArg("state")) {
        webServer.send(400, "text/plain", "Missing output or state parameter");
        Serial.println("Missing output or state parameter");
        return;
    }

    int output = webServer.arg("output").toInt();
    int state = webServer.arg("state").toInt();

    Serial.printf("Received output %d, state %d from web interface\n", output, state);

    // Validate output number
    if (output < 0 || output >= sizeof(DIGITAL_OUTPUTS)/sizeof(DIGITAL_OUTPUTS[0])) {
        webServer.send(400, "text/plain", "Invalid output number");
        Serial.println("Invalid output number");
        return;
    }

    // Store the *logical* state (not inverted)
    ioStatus.dOut[output] = state;
    
    // Apply inversion if configured for the physical pin state
    bool physicalState = state;
    if (config.doInvert[output]) {
        physicalState = !state;
    }
    
    // Set the physical pin state
    digitalWrite(DIGITAL_OUTPUTS[output], physicalState);
    
    // Update coil state for ALL connected clients with the *logical* state
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (modbusClients[i].connected) {
            modbusClients[i].server.coilWrite(output, state);
            Serial.printf("Updated client %d with output %d state %d\n", i, output, state);
        }
    }

    webServer.send(200, "text/plain", "OK");
}

void handleGetIOStatus() {
    // Memory check before processing
    uint32_t free_heap_before = rp2040.getFreeHeap();
    
    // Add a brief delay to ensure all internal states are updated
    // This helps with the first request after server initialization
    delay(5);
    
    char jsonBuffer[2048];  // Increased buffer size for configured_sensors
    StaticJsonDocument<2048> doc;  // Increased document size  

    // Add Modbus client status - now shows number of connected clients
    doc["modbus_connected"] = (connectedClients > 0);
    doc["modbus_client_count"] = connectedClients;
    
    // Add detailed client information
    if (connectedClients > 0) {
        JsonArray clients = doc.createNestedArray("modbus_clients");
        
        for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
            if (modbusClients[i].connected) {
                JsonObject client = clients.createNestedObject();
                
                // Add client IP address
                IPAddress clientIP = modbusClients[i].clientIP;
                char clientIPStr[16];
                sprintf(clientIPStr, "%d.%d.%d.%d", clientIP[0], clientIP[1], clientIP[2], clientIP[3]);
                client["ip"] = clientIPStr;
                
                // Add client slot number
                client["slot"] = i;
                
                // Add connection duration in seconds
                unsigned long duration = (millis() - modbusClients[i].connectionTime) / 1000;
                client["connected_for"] = duration;
            }
        }
    }

    // Digital inputs - account for inversion
    JsonArray di = doc.createNestedArray("di");
    for (int i = 0; i < 8; i++) {
        di.add(ioStatus.dIn[i]);
    }

    // Add raw digital input states (without latching effect)
    JsonArray diRaw = doc.createNestedArray("di_raw");
    for (int i = 0; i < 8; i++) {
        diRaw.add(ioStatus.dInRaw[i]);
    }

    // Add latched state information
    JsonArray diLatched = doc.createNestedArray("di_latched");
    for (int i = 0; i < 8; i++) {
        diLatched.add(ioStatus.dInLatched[i]);
    }

    // Digital outputs - account for inversion
    JsonArray do_ = doc.createNestedArray("do");
    for (int i = 0; i < 8; i++) {
        do_.add(ioStatus.dOut[i]);
    }

    // Add analog input values - always in millivolts
    JsonArray ai = doc.createNestedArray("ai");
    for (int i = 0; i < sizeof(ANALOG_INPUTS)/sizeof(ANALOG_INPUTS[0]); i++) {
        ai.add(ioStatus.aIn[i]);
    }

    // I2C Sensor Data - Only include if sensors are actually configured and providing data
    JsonObject i2c_sensors = doc.createNestedObject("i2c_sensors");
    
    // Only add sensor data if we have configured sensors providing real data
    bool hasSensorData = false;
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled) continue;
        
        // EZO sensors
        if (strncmp(configuredSensors[i].type, "EZO_RTD", 7) == 0) {
            // EZO_RTD provides temperature
            if (strlen(configuredSensors[i].response) > 0 && strcmp(configuredSensors[i].response, "ERROR") != 0) {
                i2c_sensors["temperature"] = String(ioStatus.temperature, 2);
                hasSensorData = true;
            }
        }
        // Simulated I2C sensors
        else if (strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0) {
            i2c_sensors["temperature"] = String(ioStatus.temperature, 2);
            hasSensorData = true;
        }
        else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
            i2c_sensors["humidity"] = String(ioStatus.humidity, 2);
            hasSensorData = true;
        }
        else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
            i2c_sensors["pressure"] = String(ioStatus.pressure, 2);
            hasSensorData = true;
        }
        // Add other sensor types (analog and digital sensors appear in other sections)
        // TODO: Add other sensor types that provide environmental data here
    }
    
    // If no sensor data is available, don't send any sensor fields
    if (!hasSensorData) {
        doc.remove("i2c_sensors");
    }

    // All Configured Sensors - Include all sensors with their current values
    JsonArray configured_sensors = doc.createNestedArray("configured_sensors");
    for (int i = 0; i < numConfiguredSensors; i++) {
        JsonObject sensor = configured_sensors.createNestedObject();
        sensor["name"] = configuredSensors[i].name;
        sensor["type"] = configuredSensors[i].type;
        sensor["protocol"] = configuredSensors[i].protocol;
        sensor["enabled"] = configuredSensors[i].enabled;
        sensor["i2c_address"] = configuredSensors[i].i2cAddress;
        sensor["modbus_register"] = configuredSensors[i].modbusRegister;
        
        // Add pin assignments for debugging
        if (strcmp(configuredSensors[i].protocol, "I2C") == 0) {
            sensor["sda_pin"] = configuredSensors[i].sdaPin;
            sensor["scl_pin"] = configuredSensors[i].sclPin;
            
            // Add configurable I2C parsing information
            if (configuredSensors[i].dataLength > 0) {
                JsonObject i2c_config = sensor.createNestedObject("i2c_parsing");
                i2c_config["register"] = configuredSensors[i].i2cRegister == 0xFF ? "none" : String(configuredSensors[i].i2cRegister, HEX);
                i2c_config["data_length"] = configuredSensors[i].dataLength;
                i2c_config["data_offset"] = configuredSensors[i].dataOffset;
                i2c_config["data_format"] = configuredSensors[i].dataFormat;
                if (configuredSensors[i].dataOffset2 != 0xFF) {
                    i2c_config["data_offset2"] = configuredSensors[i].dataOffset2;
                    i2c_config["data_format2"] = configuredSensors[i].dataFormat2;
                }
            }
            
            // Show raw I2C data in hex format (for debugging and configuration)
            if (strlen(configuredSensors[i].rawDataHex) > 0) {
                sensor["raw_i2c_data"] = configuredSensors[i].rawDataHex;
            }
        }
        
        // Add current values - both raw and calibrated
        if (configuredSensors[i].enabled) {
            // Show both raw and calibrated values for real sensors
            sensor["raw_value"] = String(configuredSensors[i].rawValue, 3);
            sensor["calibrated_value"] = String(configuredSensors[i].calibratedValue, 3);
            
            // For simulated sensors, also show the simulation source value
            if (strncmp(configuredSensors[i].type, "SIM_", 4) == 0) {
                sensor["simulated_value"] = String(configuredSensors[i].simulatedValue, 3);
                sensor["current_value"] = String(configuredSensors[i].calibratedValue, 2);
            }
            else if (strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
                if (strlen(configuredSensors[i].response) > 0 && strcmp(configuredSensors[i].response, "ERROR") != 0) {
                    sensor["current_value"] = String(configuredSensors[i].calibratedValue, 2);
                    sensor["ezo_response"] = configuredSensors[i].response;
                } else {
                    sensor["current_value"] = "No data";
                    sensor["ezo_response"] = configuredSensors[i].response;
                }
            }
            else {
                // Real hardware sensors
                sensor["current_value"] = String(configuredSensors[i].calibratedValue, 2);
                
                // Add secondary value information for multi-value sensors
                if (configuredSensors[i].rawValue2 != 0.0 && strlen(configuredSensors[i].valueName2) > 0) {
                    JsonObject secondary = sensor.createNestedObject("secondary_value");
                    secondary["name"] = configuredSensors[i].valueName2;
                    secondary["raw"] = String(configuredSensors[i].rawValue2, 3);
                    secondary["calibrated"] = String(configuredSensors[i].calibratedValue2, 2);
                    if (configuredSensors[i].modbusRegister2 > 0) {
                        secondary["modbus_register"] = configuredSensors[i].modbusRegister2;
                    }
                }
            }
            
            // Set unit based on sensor type
            if (strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0 || 
                strcmp(configuredSensors[i].type, "EZO_RTD") == 0 ||
                strncmp(configuredSensors[i].type, "SHT30", 5) == 0 ||
                strncmp(configuredSensors[i].type, "BME280", 6) == 0) {
                sensor["unit"] = "°C";
            }
            else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
                sensor["unit"] = "%";
            }
            else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
                sensor["unit"] = "hPa";
            }
            else if (strcmp(configuredSensors[i].type, "SIM_ANALOG_VOLTAGE") == 0) {
                sensor["unit"] = "V";
            }
            else if (strcmp(configuredSensors[i].type, "SIM_ANALOG_CURRENT") == 0) {
                sensor["unit"] = "mA";
            }
            else if (strcmp(configuredSensors[i].type, "EZO_PH") == 0) {
                sensor["unit"] = "pH";
            }
            else if (strcmp(configuredSensors[i].type, "EZO_DO") == 0) {
                sensor["unit"] = "mg/L";
            }
            else if (strcmp(configuredSensors[i].type, "EZO_EC") == 0) {
                sensor["unit"] = "μS/cm";
            }
            else if (strncmp(configuredSensors[i].type, "GENERIC", 7) == 0) {
                sensor["unit"] = "raw";
            }
            else if (strncmp(configuredSensors[i].type, "SIM_DIGITAL", 11) == 0) {
                sensor["current_value"] = configuredSensors[i].calibratedValue > 0.5 ? "ON" : "OFF";
                sensor["unit"] = "";
            }
            else {
                // Default unit for unknown sensor types
                sensor["unit"] = getSensorUnitFromType(configuredSensors[i].type);
            }
            
            // Add calibration info
            JsonObject calibration = sensor.createNestedObject("calibration");
            calibration["offset"] = configuredSensors[i].offset;
            calibration["scale"] = configuredSensors[i].scale;
            if (strlen(configuredSensors[i].expression) > 0) {
                calibration["expression"] = configuredSensors[i].expression;
            }
            if (strlen(configuredSensors[i].polynomialStr) > 0) {
                calibration["polynomial"] = configuredSensors[i].polynomialStr;
            }
        } else {
            sensor["current_value"] = "Disabled";
            sensor["raw_value"] = "0";
            sensor["calibrated_value"] = "0";
            sensor["unit"] = "";
        }
    }

    // EZO Sensor Responses - Add response strings for real-time feedback
    JsonArray ezo_sensors = doc.createNestedArray("ezo_sensors");
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (configuredSensors[i].enabled && strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
            JsonObject ezo_sensor = ezo_sensors.createNestedObject();
            ezo_sensor["index"] = i;
            ezo_sensor["name"] = configuredSensors[i].name;
            ezo_sensor["type"] = configuredSensors[i].type;
            ezo_sensor["i2cAddress"] = configuredSensors[i].i2cAddress;
            ezo_sensor["response"] = configuredSensors[i].response;
            ezo_sensor["cmdPending"] = configuredSensors[i].cmdPending;
            ezo_sensor["lastCmdSent"] = configuredSensors[i].lastCmdSent;
        }
    }

    // Modbus Register Mappings - Show which registers are being used
    JsonArray modbus_registers = doc.createNestedArray("modbus_registers");
    
    // Add analog input register mappings (registers 0-2)
    for (int i = 0; i < 3; i++) {
        JsonObject reg = modbus_registers.createNestedObject();
        reg["register"] = i;
        reg["type"] = "analog_input";
        reg["value"] = ioStatus.aIn[i];
        reg["unit"] = "mV";
        
        // Check if there's a simulated sensor mapped to this register
        for (int j = 0; j < numConfiguredSensors; j++) {
            if (configuredSensors[j].enabled && 
                configuredSensors[j].modbusRegister == i &&
                (strncmp(configuredSensors[j].type, "SIM_ANALOG", 10) == 0)) {
                reg["sensor_name"] = configuredSensors[j].name;
                reg["sensor_type"] = configuredSensors[j].type;
                reg["simulated"] = true;
                break;
            }
        }
    }
    
    // Add analog input register mappings (registers 0-2) - extend existing modbus_registers array
    for (int i = 0; i < 3; i++) {
        JsonObject reg = modbus_registers.createNestedObject();
        reg["register"] = i;
        reg["type"] = "analog_input";
        reg["value"] = ioStatus.aIn[i];
        reg["unit"] = "mV";
        
        // Check if there's a simulated sensor mapped to this register
        for (int j = 0; j < numConfiguredSensors; j++) {
            if (configuredSensors[j].enabled && 
                configuredSensors[j].modbusRegister == i &&
                (strncmp(configuredSensors[j].type, "SIM_ANALOG", 10) == 0)) {
                reg["sensor_name"] = configuredSensors[j].name;
                reg["sensor_type"] = configuredSensors[j].type;
                reg["simulated"] = true;
                reg["raw_value"] = configuredSensors[j].rawValue;
                reg["calibrated_value"] = configuredSensors[j].calibratedValue;
                break;
            }
        }
    }
    
    // Add configured sensor register mappings (register 3+)
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (!configuredSensors[i].enabled || configuredSensors[i].modbusRegister < 3) continue;
        
        JsonObject reg = modbus_registers.createNestedObject();
        reg["register"] = configuredSensors[i].modbusRegister;
        reg["sensor_name"] = configuredSensors[i].name;
        reg["sensor_type"] = configuredSensors[i].type;
        reg["raw_value"] = configuredSensors[i].rawValue;
        reg["calibrated_value"] = configuredSensors[i].calibratedValue;
        
        // Scaled value that appears in Modbus (x100 for 2 decimal places)
        uint16_t scaledValue = (uint16_t)(configuredSensors[i].calibratedValue * 100);
        reg["modbus_value"] = scaledValue;
        reg["scaling_note"] = "Modbus value = calibrated_value * 100";
        
        // Set unit and simulation status
        if (strncmp(configuredSensors[i].type, "SIM_", 4) == 0) {
            reg["simulated"] = true;
            reg["simulated_source"] = configuredSensors[i].simulatedValue;
        } else {
            reg["simulated"] = false;
            if (strncmp(configuredSensors[i].type, "EZO_", 4) == 0) {
                reg["ezo_response"] = configuredSensors[i].response;
                reg["i2c_address"] = configuredSensors[i].i2cAddress;
                reg["sda_pin"] = configuredSensors[i].sdaPin;
                reg["scl_pin"] = configuredSensors[i].sclPin;
            }
            else if (strcmp(configuredSensors[i].protocol, "I2C") == 0) {
                // Generic I2C sensor information
                reg["i2c_address"] = configuredSensors[i].i2cAddress;
                reg["sda_pin"] = configuredSensors[i].sdaPin;
                reg["scl_pin"] = configuredSensors[i].sclPin;
                reg["protocol"] = "I2C";
            }
        }
        
        // Add appropriate units based on sensor type
        if (strcmp(configuredSensors[i].type, "SIM_I2C_TEMPERATURE") == 0 || 
            strcmp(configuredSensors[i].type, "EZO_RTD") == 0 ||
            strncmp(configuredSensors[i].type, "SHT30", 5) == 0 ||
            strncmp(configuredSensors[i].type, "BME280", 6) == 0) {
            reg["unit"] = "°C";
        }
        else if (strcmp(configuredSensors[i].type, "SIM_I2C_HUMIDITY") == 0) {
            reg["unit"] = "%";
        }
        else if (strcmp(configuredSensors[i].type, "SIM_I2C_PRESSURE") == 0) {
            reg["unit"] = "hPa";
        }
        else if (strcmp(configuredSensors[i].type, "EZO_PH") == 0) {
            reg["unit"] = "pH";
        }
        else if (strcmp(configuredSensors[i].type, "EZO_DO") == 0) {
            reg["unit"] = "mg/L";
        }
        else if (strcmp(configuredSensors[i].type, "EZO_EC") == 0) {
            reg["unit"] = "μS/cm";
        }
        else if (strncmp(configuredSensors[i].type, "GENERIC", 7) == 0) {
            reg["unit"] = "raw";
        }
        else {
            // Use helper function for unknown types
            reg["unit"] = getSensorUnitFromType(configuredSensors[i].type);
        }
        
        // Add calibration information
        JsonObject calibration = reg.createNestedObject("calibration");
        calibration["offset"] = configuredSensors[i].offset;
        calibration["scale"] = configuredSensors[i].scale;
        if (strlen(configuredSensors[i].expression) > 0) {
            calibration["expression"] = configuredSensors[i].expression;
        }
        if (strlen(configuredSensors[i].polynomialStr) > 0) {
            calibration["polynomial"] = configuredSensors[i].polynomialStr;
        }
    }

    serializeJson(doc, jsonBuffer);
    
    // Memory check after processing
    uint32_t free_heap_after = rp2040.getFreeHeap();
    if (free_heap_before - free_heap_after > 10000) { // More than 10KB used
        Serial.printf("handleGetIOStatus used %u bytes of heap (before: %u, after: %u)\n", 
                     free_heap_before - free_heap_after, free_heap_before, free_heap_after);
    }
    
    webServer.send(200, "application/json", jsonBuffer);
}

void handleGetIOConfig() {
    delay(5);
    
    StaticJsonDocument<1024> doc;
    
    // Add version information
    doc["version"] = config.version;
    
    // Add digital input configuration
    JsonArray diPullup = doc.createNestedArray("di_pullup");
    JsonArray diInvert = doc.createNestedArray("di_invert");
    JsonArray diLatch = doc.createNestedArray("di_latch");
    
    for (int i = 0; i < sizeof(DIGITAL_INPUTS)/sizeof(DIGITAL_INPUTS[0]); i++) {
        diPullup.add(config.diPullup[i]);
        diInvert.add(config.diInvert[i]);
        diLatch.add(config.diLatch[i]);
    }
    
    // Add digital output configuration
    JsonArray doInvert = doc.createNestedArray("do_invert");
    JsonArray doInitialState = doc.createNestedArray("do_initial_state");
    
    for (int i = 0; i < sizeof(DIGITAL_OUTPUTS)/sizeof(DIGITAL_OUTPUTS[0]); i++) {
        doInvert.add(config.doInvert[i]);
        doInitialState.add(config.doInitialState[i]);
    }
    
    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

void handleSetIOConfig() {
    Serial.println("Received IO config update request");
    delay(100);

    Serial.println("Validating receieved request");
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "text/plain", "No data provided");
        return;
    }
    
    Serial.println("Parsing JSON");
    String jsonData = webServer.arg("plain");
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
        String errorMsg = "JSON parsing failed: ";
        errorMsg += error.c_str();
        Serial.println(errorMsg);
        webServer.send(400, "text/plain", errorMsg);
        return;
    }
    
    bool changed = false;
    
    Serial.println("Updating digital input configuration");
    // Update digital input configuration
    if (doc.containsKey("di_pullup") && doc["di_pullup"].is<JsonArray>()) {
        JsonArray diPullup = doc["di_pullup"].as<JsonArray>();
        int index = 0;
        for (JsonVariant value : diPullup) {
            if (index < sizeof(DIGITAL_INPUTS)/sizeof(DIGITAL_INPUTS[0])) {
                bool newValue = value.as<bool>();
                if (config.diPullup[index] != newValue) {
                    config.diPullup[index] = newValue;
                    changed = true;
                    // Apply pullup setting immediately
                    if (newValue) {
                        pinMode(DIGITAL_INPUTS[index], INPUT_PULLUP);
                    } else {
                        pinMode(DIGITAL_INPUTS[index], INPUT);
                    }
                }
                index++;
            }
        }
    }
    
    Serial.println("Updating digital input invert configuration");
    // Update digital input invert configuration
    if (doc.containsKey("di_invert") && doc["di_invert"].is<JsonArray>()) {
        JsonArray diInvert = doc["di_invert"].as<JsonArray>();
        int index = 0;
        for (JsonVariant value : diInvert) {
            if (index < sizeof(DIGITAL_INPUTS)/sizeof(DIGITAL_INPUTS[0])) {
                bool newValue = value.as<bool>();
                if (config.diInvert[index] != newValue) {
                    config.diInvert[index] = newValue;
                    changed = true;
                }
                index++;
            }
        }
    }
    
    Serial.println("Updating digital input latch configuration");
    // Update digital input latch configuration
    if (doc.containsKey("di_latch") && doc["di_latch"].is<JsonArray>()) {
        JsonArray diLatch = doc["di_latch"].as<JsonArray>();
        int index = 0;
        for (JsonVariant value : diLatch) {
            if (index < sizeof(DIGITAL_INPUTS)/sizeof(DIGITAL_INPUTS[0])) {
                bool newValue = value.as<bool>();
                if (config.diLatch[index] != newValue) {
                    config.diLatch[index] = newValue;
                    
                    // If disabling latching, clear any latched states
                    if (!newValue && ioStatus.dInLatched[index]) {
                        ioStatus.dInLatched[index] = false;
                    }
                    
                    changed = true;
                }
                index++;
            }
        }
    }
    
    Serial.println("Updating digital output invert configuration");
    // Update digital output invert configuration
    if (doc.containsKey("do_invert") && doc["do_invert"].is<JsonArray>()) {
        JsonArray doInvert = doc["do_invert"].as<JsonArray>();
        int index = 0;
        for (JsonVariant value : doInvert) {
            if (index < sizeof(DIGITAL_OUTPUTS)/sizeof(DIGITAL_OUTPUTS[0])) {
                bool newValue = value.as<bool>();
                if (config.doInvert[index] != newValue) {
                    config.doInvert[index] = newValue;
                    changed = true;
                }
                index++;
            }
        }
    }
    
    Serial.println("Updating digital output initial state configuration");
    // Update digital output initial state configuration
    if (doc.containsKey("do_initial_state") && doc["do_initial_state"].is<JsonArray>()) {
        JsonArray doInitialState = doc["do_initial_state"].as<JsonArray>();
        int index = 0;
        for (JsonVariant value : doInitialState) {
            if (index < sizeof(DIGITAL_OUTPUTS)/sizeof(DIGITAL_OUTPUTS[0])) {
                bool newValue = value.as<bool>();
                if (config.doInitialState[index] != newValue) {
                    config.doInitialState[index] = newValue;
                    changed = true;
                }
                index++;
            }
        }
    }
    
    Serial.printf("Checking for changes... %s\n", changed ? "changes found" : "no changes");
    
    // Save the configuration if changes were made
    if (changed) {
        Serial.print("Saving IO configuration...");
        saveConfig();
        Serial.println(" done");
    }
    
    // Send success response
    StaticJsonDocument<64> responseDoc;
    responseDoc["success"] = true;
    responseDoc["changed"] = changed;
    
    String response;
    serializeJson(responseDoc, response);
    webServer.send(200, "application/json", response);
}

void handleResetLatches() {
    Serial.println("Handling reset latches request");
    
    // Reset all latched inputs
    resetLatches();
    
    // Create a JSON response
    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["message"] = "All latched inputs have been reset";
    
    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

void handleResetSingleLatch() {
    // Check if the request has a body
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No request body\"}");
        return;
    }
    
    String requestBody = webServer.arg("plain");
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, requestBody);
    
    if (error) {
        Serial.print(F("Reset latch JSON deserialization failed: "));
        Serial.println(error.c_str());
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Check if the input field exists and is valid
    if (!doc.containsKey("input")) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing input field\"}");
        return;
    }
    
    int inputIndex = doc["input"];
    
    // Validate input index
    if (inputIndex < 0 || inputIndex >= 8) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid input index\"}");
        return;
    }
    
    // Reset the latch for the specified input
    Serial.printf("Resetting latch for input %d\n", inputIndex);
    ioStatus.dInLatched[inputIndex] = false;
    
    // Prepare response
    StaticJsonDocument<128> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Latch has been reset for input " + String(inputIndex);
    
    String response;
    serializeJson(responseDoc, response);
    webServer.send(200, "application/json", response);
}

void handleGetSensorConfig() {
    Serial.println("Handling get sensor config request");
    
    // Create JSON document for response
    StaticJsonDocument<1024> doc;
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
        
        // Include pin assignments
        sensor["sdaPin"] = configuredSensors[i].sdaPin;
        sensor["sclPin"] = configuredSensors[i].sclPin;
        sensor["txPin"] = configuredSensors[i].txPin;
        sensor["rxPin"] = configuredSensors[i].rxPin;
        sensor["analogPin"] = configuredSensors[i].analogPin;
        sensor["digitalPin"] = configuredSensors[i].digitalPin;
    }
    
    // Add metadata
    doc["count"] = numConfiguredSensors;
    doc["maxSensors"] = MAX_SENSORS;
    
    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

void handleSetSensorConfig() {
    Serial.println("Handling set sensor config request");
    
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No request body\"}");
        return;
    }
    
    String requestBody = webServer.arg("plain");
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, requestBody);
    
    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate that sensors array exists
    if (!doc.containsKey("sensors") || !doc["sensors"].is<JsonArray>()) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing or invalid sensors array\"}");
        return;
    }
    
    JsonArray sensorsArray = doc["sensors"].as<JsonArray>();
    
    // Validate array size
    if (sensorsArray.size() > MAX_SENSORS) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Too many sensors configured\"}");
        return;
    }
    
    // Clear existing configuration
    numConfiguredSensors = 0;
    memset(configuredSensors, 0, sizeof(configuredSensors));
    
    // Clean up existing EZO sensors
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (ezoSensors[i] != nullptr) {
            delete ezoSensors[i];
            ezoSensors[i] = nullptr;
        }
    }
    ezoSensorsInitialized = false;
    
    // Parse and validate each sensor configuration
    int index = 0;
    for (JsonObject sensor : sensorsArray) {
        // Validate required fields
        if (!sensor.containsKey("name") || !sensor.containsKey("type")) {
            webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing required sensor fields\"}");
            return;
        }
        
        // Extract and validate sensor configuration
        configuredSensors[index].enabled = sensor["enabled"] | false;
        
        const char* name = sensor["name"] | "";
        if (strlen(name) >= sizeof(configuredSensors[index].name)) {
            webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Sensor name too long\"}");
            return;
        }
        strncpy(configuredSensors[index].name, name, sizeof(configuredSensors[index].name) - 1);
        configuredSensors[index].name[sizeof(configuredSensors[index].name) - 1] = '\0';
        
        const char* type = sensor["type"] | "";
        if (strlen(type) >= sizeof(configuredSensors[index].type)) {
            webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Sensor type too long\"}");
            return;
        }
        strncpy(configuredSensors[index].type, type, sizeof(configuredSensors[index].type) - 1);
        configuredSensors[index].type[sizeof(configuredSensors[index].type) - 1] = '\0';
        
        const char* protocol = sensor["protocol"] | "";
        if (strlen(protocol) >= sizeof(configuredSensors[index].protocol)) {
            webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Sensor protocol too long\"}");
            return;
        }
        strncpy(configuredSensors[index].protocol, protocol, sizeof(configuredSensors[index].protocol) - 1);
        configuredSensors[index].protocol[sizeof(configuredSensors[index].protocol) - 1] = '\0';
        
        configuredSensors[index].i2cAddress = sensor["i2cAddress"] | 0;
        configuredSensors[index].modbusRegister = sensor["modbusRegister"] | 0;
        
        // Extract and validate pin assignments based on protocol
        if (strcmp(protocol, "I2C") == 0) {
            configuredSensors[index].sdaPin = sensor["sdaPin"] | I2C_PIN_PAIRS[0][0];
            configuredSensors[index].sclPin = sensor["sclPin"] | I2C_PIN_PAIRS[0][1];
            
            // Validate I2C pins are available
            if (!isPinAvailable(configuredSensors[index].sdaPin, "I2C") || 
                !isPinAvailable(configuredSensors[index].sclPin, "I2C")) {
                webServer.send(400, "application/json", "{\"success\":false,\"message\":\"I2C pins not available\"}");
                return;
            }
            
            // Allocate I2C pins
            allocatePin(configuredSensors[index].sdaPin, "I2C", name);
            allocatePin(configuredSensors[index].sclPin, "I2C", name);
            
        } else if (strcmp(protocol, "UART") == 0) {
            configuredSensors[index].txPin = sensor["txPin"] | 0;
            configuredSensors[index].rxPin = sensor["rxPin"] | 1;
            
            // Validate UART pins are available
            if (!isPinAvailable(configuredSensors[index].txPin, "UART") || 
                !isPinAvailable(configuredSensors[index].rxPin, "UART")) {
                webServer.send(400, "application/json", "{\"success\":false,\"message\":\"UART pins not available\"}");
                return;
            }
            
            // Allocate UART pins
            allocatePin(configuredSensors[index].txPin, "UART", name);
            allocatePin(configuredSensors[index].rxPin, "UART", name);
            
        } else if (strcmp(protocol, "Analog Voltage") == 0) {
            configuredSensors[index].analogPin = sensor["analogPin"] | 26;
            
            // Validate analog pin is available
            if (!isPinAvailable(configuredSensors[index].analogPin, "Analog")) {
                webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Analog pin not available\"}");
                return;
            }
            
            // Allocate analog pin
            allocatePin(configuredSensors[index].analogPin, "Analog", name);
            
        } else if (strcmp(protocol, "One-Wire") == 0 || strcmp(protocol, "Digital Counter") == 0) {
            configuredSensors[index].digitalPin = sensor["digitalPin"] | 0;
            
            // Validate digital pin is available
            if (!isPinAvailable(configuredSensors[index].digitalPin, protocol)) {
                webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Digital pin not available\"}");
                return;
            }
            
            // Allocate digital pin
            allocatePin(configuredSensors[index].digitalPin, protocol, name);
        }
        
        // Validate I2C address range
        if (configuredSensors[index].i2cAddress > 127) {
            webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid I2C address\"}");
            return;
        }
        
        Serial.printf("Configured sensor %d: %s (%s, protocol: %s) at I2C 0x%02X, Modbus register %d, enabled: %s\n",
            index,
            configuredSensors[index].name,
            configuredSensors[index].type,
            configuredSensors[index].protocol,
            configuredSensors[index].i2cAddress,
            configuredSensors[index].modbusRegister,
            configuredSensors[index].enabled ? "true" : "false"
        );
        
        index++;
    }
    
    numConfiguredSensors = index;
    
    // Save the configuration
    saveSensorConfig();
    
    // Send success response
    StaticJsonDocument<128> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Sensor configuration updated successfully";
    responseDoc["count"] = numConfiguredSensors;
    
    String response;
    serializeJson(responseDoc, response);
    webServer.send(200, "application/json", response);
    
    Serial.printf("Sensor configuration updated with %d sensors. Configuration active immediately.\n", numConfiguredSensors);
    
    // No reboot needed - sensors are active immediately
}

void handleSensorCommand() {
    Serial.println("Handling sensor command request");
    
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No request body\"}");
        return;
    }
    
    String requestBody = webServer.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, requestBody);
    
    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate required fields
    if (!doc.containsKey("sensorIndex") || !doc.containsKey("command")) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing sensorIndex or command field\"}");
        return;
    }
    
    int sensorIndex = doc["sensorIndex"];
    const char* command = doc["command"];
    
    // Validate sensor index
    if (sensorIndex < 0 || sensorIndex >= numConfiguredSensors) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid sensor index\"}");
        return;
    }
    
    // Check if sensor is enabled and is an EZO sensor
    if (!configuredSensors[sensorIndex].enabled || strncmp(configuredSensors[sensorIndex].type, "EZO_", 4) != 0) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Sensor is not enabled or not an EZO sensor\"}");
        return;
    }
    
    // Check if EZO sensor is initialized
    if (ezoSensors[sensorIndex] == nullptr) {
        webServer.send(400, "application/json", "{\"success\":false,\"message\":\"EZO sensor not initialized\"}");
        return;
    }
    
    // Send custom command to the EZO sensor
    Serial.printf("Sending command '%s' to EZO sensor %s (index %d)\n", command, configuredSensors[sensorIndex].name, sensorIndex);
    
    ezoSensors[sensorIndex]->send_cmd(command);
    configuredSensors[sensorIndex].lastCmdSent = millis();
    configuredSensors[sensorIndex].cmdPending = true;
    
    // Clear previous response
    memset(configuredSensors[sensorIndex].response, 0, sizeof(configuredSensors[sensorIndex].response));
    
    // Send success response
    StaticJsonDocument<128> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Command sent successfully";
    responseDoc["sensorName"] = configuredSensors[sensorIndex].name;
    responseDoc["command"] = command;
    
    String response;
    serializeJson(responseDoc, response);
    webServer.send(200, "application/json", response);
}

void handleSetSensorCalibration() {
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
        return;
    }
    
    String data = webServer.arg("plain");
    Serial.printf("Received calibration data: %s\n", data.c_str());
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.printf("JSON parsing failed: %s\n", error.c_str());
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Extract sensor name and find the sensor
    const char* sensorName = doc["name"];
    if (!sensorName) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Sensor name required\"}");
        return;
    }
    
    // Find sensor by name
    int sensorIndex = -1;
    for (int i = 0; i < numConfiguredSensors; i++) {
        if (strcmp(configuredSensors[i].name, sensorName) == 0) {
            sensorIndex = i;
            break;
        }
    }
    
    if (sensorIndex == -1) {
        webServer.send(404, "application/json", "{\"success\":false,\"error\":\"Sensor not found\"}");
        return;
    }
    
    // Update I2C parsing configuration if provided
    if (doc.containsKey("i2c_parsing")) {
        JsonObject i2cParsing = doc["i2c_parsing"];
        
        if (i2cParsing.containsKey("data_offset")) {
            configuredSensors[sensorIndex].dataOffset = i2cParsing["data_offset"];
        }
        if (i2cParsing.containsKey("data_length")) {
            configuredSensors[sensorIndex].dataLength = i2cParsing["data_length"];
        }
        if (i2cParsing.containsKey("data_format")) {
            const char* format = i2cParsing["data_format"];
            // Convert string format to dataFormat enum
            if (strcmp(format, "uint8") == 0) {
                configuredSensors[sensorIndex].dataFormat = 0;
            } else if (strcmp(format, "uint16_be") == 0) {
                configuredSensors[sensorIndex].dataFormat = 1;
            } else if (strcmp(format, "uint16_le") == 0) {
                configuredSensors[sensorIndex].dataFormat = 2;
            } else if (strcmp(format, "uint32_be") == 0) {
                configuredSensors[sensorIndex].dataFormat = 3;
            } else if (strcmp(format, "uint32_le") == 0) {
                configuredSensors[sensorIndex].dataFormat = 4;
            } else if (strcmp(format, "float32") == 0) {
                configuredSensors[sensorIndex].dataFormat = 5;
            }
        }
        
        Serial.printf("Updated I2C parsing for %s: offset=%d, length=%d, format=%d\n", 
                     sensorName, configuredSensors[sensorIndex].dataOffset, 
                     configuredSensors[sensorIndex].dataLength, configuredSensors[sensorIndex].dataFormat);
    }
    
    // Update calibration settings
    const char* method = doc["method"] | "linear";
    
    if (strcmp(method, "linear") == 0) {
        configuredSensors[sensorIndex].offset = doc["offset"] | 0.0f;
        configuredSensors[sensorIndex].scale = doc["scale"] | 1.0f;
        
        // Clear other calibration fields
        configuredSensors[sensorIndex].polynomialStr[0] = '\0';
        configuredSensors[sensorIndex].expression[0] = '\0';
        
        Serial.printf("Updated linear calibration for %s: offset=%.3f, scale=%.3f\n", 
                     sensorName, configuredSensors[sensorIndex].offset, configuredSensors[sensorIndex].scale);
    }
    else if (strcmp(method, "polynomial") == 0) {
        const char* polynomial = doc["polynomial"] | "";
        strncpy(configuredSensors[sensorIndex].polynomialStr, polynomial, sizeof(configuredSensors[sensorIndex].polynomialStr) - 1);
        configuredSensors[sensorIndex].polynomialStr[sizeof(configuredSensors[sensorIndex].polynomialStr) - 1] = '\0';
        
        // Clear other calibration fields
        configuredSensors[sensorIndex].offset = 0.0f;
        configuredSensors[sensorIndex].scale = 1.0f;
        configuredSensors[sensorIndex].expression[0] = '\0';
        
        Serial.printf("Updated polynomial calibration for %s: %s\n", sensorName, polynomial);
    }
    else if (strcmp(method, "expression") == 0) {
        const char* expression = doc["expression"] | "";
        strncpy(configuredSensors[sensorIndex].expression, expression, sizeof(configuredSensors[sensorIndex].expression) - 1);
        configuredSensors[sensorIndex].expression[sizeof(configuredSensors[sensorIndex].expression) - 1] = '\0';
        
        // Clear other calibration fields
        configuredSensors[sensorIndex].offset = 0.0f;
        configuredSensors[sensorIndex].scale = 1.0f;
        configuredSensors[sensorIndex].polynomialStr[0] = '\0';
        
        Serial.printf("Updated expression calibration for %s: %s\n", sensorName, expression);
    }
    
    // Save the updated sensor configuration
    saveSensorConfig();
    
    // Send success response
    StaticJsonDocument<128> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Calibration updated successfully";
    responseDoc["sensorName"] = sensorName;
    responseDoc["method"] = method;
    
    String response;
    serializeJson(responseDoc, response);
    webServer.send(200, "application/json", response);
}

void handleTerminalCommand() {
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
        return;
    }
    
    String data = webServer.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String protocol = doc["protocol"] | "";
    String pin = doc["pin"] | "";
    String command = doc["command"] | "";
    String i2cAddress = doc["i2cAddress"] | "";
    
    String response = executeTerminalCommand(protocol, pin, command, i2cAddress);
    
    StaticJsonDocument<1024> responseDoc;
    responseDoc["success"] = true;
    responseDoc["response"] = response;
    
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    webServer.send(200, "application/json", jsonResponse);
}

void handleTerminalWatch() {
    if (!webServer.hasArg("plain")) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
        return;
    }
    
    String data = webServer.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String protocol = doc["protocol"] | "";
    String pin = doc["pin"] | "";
    String i2cAddress = doc["i2cAddress"] | "";
    
    // For watch mode, always use "read" command
    String response = executeTerminalCommand(protocol, pin, "read", i2cAddress);
    
    StaticJsonDocument<1024> responseDoc;
    responseDoc["success"] = true;
    responseDoc["response"] = response;
    
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    webServer.send(200, "application/json", jsonResponse);
}

String executeTerminalCommand(String protocol, String pin, String command, String i2cAddress) {
    command.toLowerCase();
    
    if (protocol == "digital") {
        return executeDigitalCommand(pin, command);
    }
    else if (protocol == "analog") {
        return executeAnalogCommand(pin, command);
    }
    else if (protocol == "i2c") {
        return executeI2CCommand(pin, command, i2cAddress);
    }
    else if (protocol == "uart") {
        return executeUARTCommand(pin, command);
    }
    else if (protocol == "network") {
        return executeNetworkCommand(pin, command);
    }
    else if (protocol == "system") {
        return executeSystemCommand(pin, command);
    }
    
    return "Error: Unknown protocol '" + protocol + "'";
}

String executeDigitalCommand(String pin, String command) {
    // Parse pin number
    int pinNum = -1;
    bool isOutput = false;
    
    if (pin.startsWith("DI")) {
        pinNum = pin.substring(2).toInt();
        isOutput = false;
    } else if (pin.startsWith("DO")) {
        pinNum = pin.substring(2).toInt();
        isOutput = true;
    }
    
    if (pinNum < 0 || pinNum >= 8) {
        return "Error: Invalid pin number";
    }
    
    if (command == "read") {
        if (isOutput) {
            return "DO" + String(pinNum) + " = " + (ioStatus.dOut[pinNum] ? "HIGH" : "LOW");
        } else {
            return "DI" + String(pinNum) + " = " + (ioStatus.dIn[pinNum] ? "HIGH" : "LOW") + 
                   " (Raw: " + (ioStatus.dInRaw[pinNum] ? "HIGH" : "LOW") + ")";
        }
    }
    else if (command.startsWith("write ") && isOutput) {
        String value = command.substring(6);
        value.trim();
        
        if (value == "1" || value.equalsIgnoreCase("high")) {
            ioStatus.dOut[pinNum] = true;
            return "DO" + String(pinNum) + " set to HIGH";
        } else if (value == "0" || value.equalsIgnoreCase("low")) {
            ioStatus.dOut[pinNum] = false;
            return "DO" + String(pinNum) + " set to LOW";
        } else {
            return "Error: Invalid value. Use 1/0 or HIGH/LOW";
        }
    }
    else if (command.startsWith("config ") && !isOutput) {
        String configType = command.substring(7);
        configType.trim();
        
        if (configType == "pullup") {
            config.diPullup[pinNum] = !config.diPullup[pinNum];
            return "DI" + String(pinNum) + " pullup " + (config.diPullup[pinNum] ? "ENABLED" : "DISABLED");
        }
        else if (configType == "invert") {
            config.diInvert[pinNum] = !config.diInvert[pinNum];
            return "DI" + String(pinNum) + " invert " + (config.diInvert[pinNum] ? "ENABLED" : "DISABLED");
        }
        else if (configType == "latch") {
            config.diLatch[pinNum] = !config.diLatch[pinNum];
            return "DI" + String(pinNum) + " latch " + (config.diLatch[pinNum] ? "ENABLED" : "DISABLED");
        }
        else {
            return "Error: Unknown config option. Use pullup/invert/latch";
        }
    }
    
    return "Error: Unknown command or invalid for pin type";
}

String executeAnalogCommand(String pin, String command) {
    int pinNum = -1;
    
    if (pin.startsWith("AI")) {
        pinNum = pin.substring(2).toInt();
    }
    
    if (pinNum < 0 || pinNum >= 3) {
        return "Error: Invalid analog pin number";
    }
    
    if (command == "read") {
        return "AI" + String(pinNum) + " = " + String(ioStatus.aIn[pinNum]) + " mV";
    }
    else if (command == "config") {
        return "AI" + String(pinNum) + " - Pin " + String(26 + pinNum) + ", Range: 0-3300mV, Resolution: 12-bit";
    }
    
    return "Error: Unknown analog command";
}

String executeI2CCommand(String pin, String command, String i2cAddress) {
    // Parse I2C pins from pin specification or use defaults
    int sdaPin = I2C_SDA_PIN;  // Default fallback
    int sclPin = I2C_SCL_PIN;  // Default fallback
    
    // Check if pin format is "SDA_pin,SCL_pin" or similar
    if (pin.indexOf(',') > 0) {
        int commaPos = pin.indexOf(',');
        sdaPin = pin.substring(0, commaPos).toInt();
        sclPin = pin.substring(commaPos + 1).toInt();
    } else {
        // Try to find configured sensor pins
        for (int i = 0; i < numConfiguredSensors; i++) {
            if (configuredSensors[i].enabled && 
                (strncmp(configuredSensors[i].type, "EZO_", 4) == 0 || 
                 strncmp(configuredSensors[i].type, "SIM_I2C", 7) == 0 ||
                 strncmp(configuredSensors[i].type, "BME", 3) == 0)) {
                sdaPin = configuredSensors[i].sdaPin;
                sclPin = configuredSensors[i].sclPin;
                break;
            }
        }
    }
    
    // Handle scan command first - it doesn't need an address
    if (command == "scan") {
        String result = "I2C Device Scan (SDA=" + String(sdaPin) + ", SCL=" + String(sclPin) + "):\n";
        Wire.setSDA(sdaPin);
        Wire.setSCL(sclPin);
        Wire.begin();
        
        int foundDevices = 0;
        for (int addr = 1; addr < 128; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                result += "Found device at 0x" + String(addr, HEX) + "\n";
                foundDevices++;
            }
        }
        
        if (foundDevices == 0) {
            result += "No I2C devices found";
        }
        
        return result;
    }
    
    // For all other commands, require an I2C address
    if (i2cAddress.length() == 0) {
        return "Error: I2C address required";
    }
    
    // Parse I2C address
    int address = 0;
    if (i2cAddress.startsWith("0x") || i2cAddress.startsWith("0X")) {
        address = strtol(i2cAddress.c_str(), NULL, 16);
    } else {
        address = i2cAddress.toInt();
    }
    
    if (address < 1 || address > 127) {
        return "Error: Invalid I2C address (must be 1-127 or 0x01-0x7F)";
    }
    else if (command == "probe") {
        Wire.setSDA(sdaPin);
        Wire.setSCL(sclPin);
        Wire.begin();
        Wire.beginTransmission(address);
        int error = Wire.endTransmission();
        
        if (error == 0) {
            return "Device at 0x" + String(address, HEX) + " is present";
        } else {
            return "No device found at 0x" + String(address, HEX);
        }
    }
    else if (command == "read") {
        // Simple read command for raw data (used by watch mode)
        Wire.setSDA(sdaPin);
        Wire.setSCL(sclPin);
        Wire.begin();
        
        // For SHT30 sensors, send measurement command first
        if (address == 0x44 || address == 0x45) {
            // Send SHT30 measurement command (0x2C06)
            Wire.beginTransmission(address);
            Wire.write(0x2C);
            Wire.write(0x06);
            int cmdError = Wire.endTransmission();
            
            if (cmdError != 0) {
                return "Failed to send measurement command to SHT30 at 0x" + String(address, HEX);
            }
            
            delay(15); // Wait for SHT30 measurement
        }
        
        // Try to read raw data from sensor - start with 6 bytes (common for temp/humidity sensors)
        Wire.requestFrom(address, (uint8_t)6);
        
        if (Wire.available() == 0) {
            return "No data available from 0x" + String(address, HEX);
        }
        
        String result = "Raw data from 0x" + String(address, HEX) + ": ";
        int bytesRead = 0;
        while (Wire.available() && bytesRead < 6) {
            uint8_t data = Wire.read();
            if (bytesRead > 0) result += " ";
            result += "0x" + String(data, HEX);
            bytesRead++;
        }
        result += " (" + String(bytesRead) + " bytes)";
        
        return result;
    }
    else if (command.startsWith("read ")) {
        String regStr = command.substring(5);
        int reg = regStr.toInt();
        
        Wire.setSDA(sdaPin);
        Wire.setSCL(sclPin);
        Wire.begin();
        Wire.beginTransmission(address);
        Wire.write(reg);
        int error = Wire.endTransmission();
        
        if (error != 0) {
            return "Error writing register address";
        }
        
        Wire.requestFrom(address, 1);
        if (Wire.available()) {
            int value = Wire.read();
            return "Register 0x" + String(reg, HEX) + " = 0x" + String(value, HEX) + " (" + String(value) + ")";
        } else {
            return "Error reading from device";
        }
    }
    else if (command.startsWith("write ")) {
        // Parse: write [reg] [data]
        int firstSpace = command.indexOf(' ', 6);
        if (firstSpace == -1) {
            return "Error: Invalid write command format. Use: write [reg] [data]";
        }
        
        String regStr = command.substring(6, firstSpace);
        String dataStr = command.substring(firstSpace + 1);
        
        int reg = regStr.toInt();
        int data = dataStr.toInt();
        
        Wire.setSDA(sdaPin);
        Wire.setSCL(sclPin);
        Wire.begin();
        Wire.beginTransmission(address);
        Wire.write(reg);
        Wire.write(data);
        int error = Wire.endTransmission();
        
        if (error == 0) {
            return "Wrote 0x" + String(data, HEX) + " to register 0x" + String(reg, HEX);
        } else {
            return "Error writing to device";
        }
    }
    
    return "Error: Unknown I2C command";
}

String executeUARTCommand(String pin, String command) {
    // Standard UART diagnostic and testing commands
    if (command == "help") {
        return "UART Commands: help, init, send [data], read, loopback, baudrate [rate], status, at, echo [on/off]";
    }
    else if (command == "init") {
        // Initialize UART on specified pins
        Serial1.begin(9600); // Default baud rate
        return "UART initialized on Serial1 at 9600 baud";
    }
    else if (command.startsWith("send ")) {
        String data = command.substring(5);
        Serial1.print(data);
        return "Sent: " + data;
    }
    else if (command == "read") {
        String received = "";
        unsigned long startTime = millis();
        while (millis() - startTime < 1000 && Serial1.available() > 0) {
            char c = Serial1.read();
            received += c;
        }
        if (received.length() > 0) {
            return "Received: " + received;
        } else {
            return "No data received";
        }
    }
    else if (command == "loopback") {
        // Send test data and check if it comes back
        String testData = "TEST123";
        Serial1.print(testData);
        delay(100);
        String received = "";
        while (Serial1.available() > 0) {
            received += (char)Serial1.read();
        }
        return "Sent: " + testData + ", Received: " + received;
    }
    else if (command.startsWith("baudrate ")) {
        int baudrate = command.substring(9).toInt();
        if (baudrate > 0) {
            Serial1.begin(baudrate);
            return "Baudrate set to " + String(baudrate);
        } else {
            return "Invalid baudrate. Common rates: 9600, 19200, 38400, 57600, 115200";
        }
    }
    else if (command == "status") {
        return "UART Status: Serial1 available, default pins TX=0, RX=1";
    }
    else if (command == "at") {
        // Send AT command (common for modems/GPS)
        Serial1.print("AT\r\n");
        delay(500);
        String response = "";
        while (Serial1.available() > 0) {
            response += (char)Serial1.read();
        }
        return "AT Response: " + (response.length() > 0 ? response : "No response");
    }
    else if (command.startsWith("echo ")) {
        String mode = command.substring(5);
        if (mode == "on") {
            Serial1.print("ATE1\r\n"); // Enable echo
            return "Echo enabled";
        } else if (mode == "off") {
            Serial1.print("ATE0\r\n"); // Disable echo
            return "Echo disabled";
        } else {
            return "Usage: echo on|off";
        }
    }
    else if (command == "clear") {
        // Clear receive buffer
        while (Serial1.available() > 0) {
            Serial1.read();
        }
        return "UART buffer cleared";
    }
    else {
        return "Unknown UART command. Type 'help' for available commands.";
    }
}

String executeNetworkCommand(String pin, String command) {
    if (command == "status") {
        String result = "";
        
        if (pin == "ETH" || pin == "ETH_PHY") {
            result += "Ethernet Interface Status:\n";
            result += "IP: " + eth.localIP().toString() + "\n";
            result += "Gateway: " + eth.gatewayIP().toString() + "\n";
            result += "Subnet: " + eth.subnetMask().toString() + "\n";
            result += "Link Status: " + String(eth.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
            result += "DHCP: " + String(config.dhcpEnabled ? "Enabled" : "Disabled");
        }
        else if (pin == "ETH_PINS") {
            result += "Ethernet Pin Configuration:\n";
            result += "MISO: Pin 16\n";
            result += "CS: Pin 17\n";
            result += "SCK: Pin 18\n";
            result += "MOSI: Pin 19\n";
            result += "RST: Pin 20\n";
            result += "IRQ: Pin 21";
        }
        else if (pin == "MODBUS" || pin == "MODBUS_CLIENTS") {
            result += "Modbus TCP Server:\n";
            result += "Port: " + String(config.modbusPort) + "\n";
            result += "Status: " + String(connectedClients > 0 ? "Active" : "Waiting") + "\n";
            result += "Connected Clients: " + String(connectedClients);
        }
        else {
            result += "Network Status:\n";
            result += "IP: " + eth.localIP().toString() + "\n";
            result += "Gateway: " + eth.gatewayIP().toString() + "\n";
            result += "Subnet: " + eth.subnetMask().toString() + "\n";
            result += "DHCP: " + String(config.dhcpEnabled ? "Enabled" : "Disabled") + "\n";
            result += "Modbus Port: " + String(config.modbusPort);
        }
        
        return result;
    }
    else if (command == "clients") {
        String result = "Modbus Clients:\n";
        result += "Connected: " + String(connectedClients) + "\n";
        
        for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
            if (modbusClients[i].connected) {
                result += "Slot " + String(i) + ": " + modbusClients[i].clientIP.toString() + "\n";
            }
        }
        
        return result;
    }
    else if (command == "link") {
        return "Ethernet Link: " + String(eth.status() == WL_CONNECTED ? "UP" : "DOWN");
    }
    else if (command == "stats") {
        String result = "Network Statistics:\n";
        result += "Status: " + String(eth.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
        result += "Connection Uptime: " + String(millis() / 1000) + " seconds";
        return result;
    }
    
    return "Error: Unknown network command";
}

String executeSystemCommand(String pin, String command) {
    if (command == "status") {
        String result = "System Status:\n";
        result += "CPU: RP2040 @ 133MHz\n";
        result += "RAM: 256KB\n";
        result += "Flash: 2MB\n";
        result += "Uptime: " + String(millis() / 1000) + " seconds\n";
        result += "Free Heap: " + String(rp2040.getFreeHeap()) + " bytes";
        return result;
    }
    else if (command == "sensors") {
        String result = "Configured Sensors:\n";
        
        if (numConfiguredSensors == 0) {
            result += "No sensors configured";
        } else {
            for (int i = 0; i < numConfiguredSensors; i++) {
                result += String(i) + ": " + configuredSensors[i].name + 
                         " (" + configuredSensors[i].type + ") - " + 
                         (configuredSensors[i].enabled ? "Enabled" : "Disabled") + "\n";
            }
        }
        
        return result;
    }
    else if (command == "info") {
        String result = "Hardware Information:\n";
        result += "Board: Raspberry Pi Pico\n";
        result += "Digital Inputs: 8 (Pins 0-7)\n";
        result += "Digital Outputs: 8 (Pins 8-15)\n";
        result += "Analog Inputs: 3 (Pins 26-28)\n";
        result += "I2C: SDA Pin 24, SCL Pin 25\n";
        result += "Ethernet: W5500 (Pins 16-21)";
        return result;
    }
    
    return "Error: Unknown system command";
}