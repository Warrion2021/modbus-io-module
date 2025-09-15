#include "pin_manager.h"

// Static member definitions
PinAllocation PinManager::pinAllocations[40];
int PinManager::numAllocatedPins = 0;

void PinManager::initialize() {
    numAllocatedPins = 0;
    for (int i = 0; i < 40; i++) {
        pinAllocations[i].allocated = false;
        pinAllocations[i].pin = 0;
        strcpy(pinAllocations[i].protocol, "");
        strcpy(pinAllocations[i].sensorName, "");
    }
    Serial.println("Pin allocation manager initialized");
}

void PinManager::clearAllAllocations() {
    for (int i = 0; i < numAllocatedPins; i++) {
        pinAllocations[i].allocated = false;
        pinAllocations[i].pin = 0;
        strcpy(pinAllocations[i].protocol, "");
        strcpy(pinAllocations[i].sensorName, "");
    }
    numAllocatedPins = 0;
    Serial.println("All pin allocations cleared");
}

bool PinManager::isPinAvailable(uint8_t pin, const char* protocol) {
    // Check if pin is already allocated
    if (isPinAllocated(pin)) {
        return false;
    }
    
    // Check if pin is valid for this protocol
    if (!isValidPin(pin)) {
        return false;
    }
    
    // Check if pin is in available list for this protocol
    if (isFlexiblePin(pin)) {
        return true;
    }
    
    return false;
}

bool PinManager::isPinAllocated(uint8_t pin) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && pinAllocations[i].pin == pin) {
            return true;
        }
    }
    return false;
}

bool PinManager::isValidPin(uint8_t pin) {
    // Check against reserved pins
    if (isReservedPin(pin)) {
        return false;
    }
    
    // Pin must be in valid range for RP2040
    if (pin > 29) {
        return false;
    }
    
    return true;
}

bool PinManager::allocatePin(uint8_t pin, const char* protocol, const char* sensorName) {
    if (!isPinAvailable(pin, protocol)) {
        Serial.printf("Pin %d not available for protocol %s\n", pin, protocol);
        return false;
    }
    
    if (numAllocatedPins >= 40) {
        Serial.println("Maximum pin allocations reached");
        return false;
    }
    
    pinAllocations[numAllocatedPins].pin = pin;
    pinAllocations[numAllocatedPins].allocated = true;
    strlcpy(pinAllocations[numAllocatedPins].protocol, protocol, sizeof(pinAllocations[numAllocatedPins].protocol));
    strlcpy(pinAllocations[numAllocatedPins].sensorName, sensorName, sizeof(pinAllocations[numAllocatedPins].sensorName));
    numAllocatedPins++;
    
    Serial.printf("Allocated pin %d for %s (%s)\n", pin, sensorName, protocol);
    return true;
}

void PinManager::deallocatePin(uint8_t pin) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && pinAllocations[i].pin == pin) {
            Serial.printf("Deallocated pin %d from %s (%s)\n", 
                         pin, pinAllocations[i].sensorName, pinAllocations[i].protocol);
            
            // Shift remaining allocations down
            shiftAllocationsDown(i);
            numAllocatedPins--;
            return;
        }
    }
}

void PinManager::deallocatePinsForSensor(const char* sensorName) {
    int i = 0;
    while (i < numAllocatedPins) {
        if (pinAllocations[i].allocated && 
            strcmp(pinAllocations[i].sensorName, sensorName) == 0) {
            
            Serial.printf("Deallocated pin %d from sensor %s\n", 
                         pinAllocations[i].pin, sensorName);
            
            shiftAllocationsDown(i);
            numAllocatedPins--;
            // Don't increment i since we shifted down
        } else {
            i++;
        }
    }
}

void PinManager::deallocatePinsForProtocol(const char* protocol) {
    int i = 0;
    while (i < numAllocatedPins) {
        if (pinAllocations[i].allocated && 
            strcmp(pinAllocations[i].protocol, protocol) == 0) {
            
            Serial.printf("Deallocated pin %d from protocol %s\n", 
                         pinAllocations[i].pin, protocol);
            
            shiftAllocationsDown(i);
            numAllocatedPins--;
            // Don't increment i since we shifted down
        } else {
            i++;
        }
    }
}

int PinManager::getAllocatedPinCount() {
    return numAllocatedPins;
}

int PinManager::getAvailablePinCount() {
    int available = 0;
    for (int i = 0; i < NUM_FLEXIBLE_PINS; i++) {
        if (!isPinAllocated(AVAILABLE_FLEXIBLE_PINS[i])) {
            available++;
        }
    }
    return available;
}

PinAllocation* PinManager::getAllocations() {
    return pinAllocations;
}

PinAllocation* PinManager::findAllocation(uint8_t pin) {
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && pinAllocations[i].pin == pin) {
            return &pinAllocations[i];
        }
    }
    return nullptr;
}

bool PinManager::isProtocolSupported(const char* protocol) {
    // List of supported protocols
    const char* supportedProtocols[] = {
        "I2C", "SPI", "UART", "GPIO", "PWM", "ADC", "1-Wire"
    };
    
    int numProtocols = sizeof(supportedProtocols) / sizeof(supportedProtocols[0]);
    
    for (int i = 0; i < numProtocols; i++) {
        if (strcmp(protocol, supportedProtocols[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

uint8_t* PinManager::getAvailablePinsForProtocol(const char* protocol, int& count) {
    static uint8_t availablePins[NUM_FLEXIBLE_PINS];
    count = 0;
    
    for (int i = 0; i < NUM_FLEXIBLE_PINS; i++) {
        uint8_t pin = AVAILABLE_FLEXIBLE_PINS[i];
        if (isPinAvailable(pin, protocol)) {
            availablePins[count++] = pin;
        }
    }
    
    return availablePins;
}

void PinManager::printAllocations() {
    Serial.printf("Pin Allocations (%d/%d):\n", numAllocatedPins, 40);
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated) {
            Serial.printf("  Pin %d: %s (%s)\n", 
                         pinAllocations[i].pin,
                         pinAllocations[i].sensorName,
                         pinAllocations[i].protocol);
        }
    }
    
    Serial.printf("Available pins: %d\n", getAvailablePinCount());
}

bool PinManager::validateAllocations() {
    // Check for duplicate pin allocations
    for (int i = 0; i < numAllocatedPins; i++) {
        if (!pinAllocations[i].allocated) continue;
        
        for (int j = i + 1; j < numAllocatedPins; j++) {
            if (!pinAllocations[j].allocated) continue;
            
            if (pinAllocations[i].pin == pinAllocations[j].pin) {
                Serial.printf("ERROR: Pin %d allocated multiple times!\n", pinAllocations[i].pin);
                return false;
            }
        }
    }
    
    // Check for invalid pin numbers
    for (int i = 0; i < numAllocatedPins; i++) {
        if (pinAllocations[i].allocated && !isValidPin(pinAllocations[i].pin)) {
            Serial.printf("ERROR: Invalid pin %d allocated!\n", pinAllocations[i].pin);
            return false;
        }
    }
    
    return true;
}

// Private helper methods
bool PinManager::isFlexiblePin(uint8_t pin) {
    for (int i = 0; i < NUM_FLEXIBLE_PINS; i++) {
        if (AVAILABLE_FLEXIBLE_PINS[i] == pin) {
            return true;
        }
    }
    return false;
}

bool PinManager::isReservedPin(uint8_t pin) {
    // Reserved pins for RP2040 Pico (SPI flash, etc.)
    const uint8_t reservedPins[] = {
        23, 24, 25,  // SPI flash
        // Add other reserved pins as needed
    };
    
    int numReserved = sizeof(reservedPins) / sizeof(reservedPins[0]);
    
    for (int i = 0; i < numReserved; i++) {
        if (reservedPins[i] == pin) {
            return true;
        }
    }
    
    return false;
}

void PinManager::shiftAllocationsDown(int startIndex) {
    for (int j = startIndex; j < numAllocatedPins - 1; j++) {
        pinAllocations[j] = pinAllocations[j + 1];
    }
    
    // Clear the last element
    pinAllocations[numAllocatedPins - 1].allocated = false;
    pinAllocations[numAllocatedPins - 1].pin = 0;
    strcpy(pinAllocations[numAllocatedPins - 1].protocol, "");
    strcpy(pinAllocations[numAllocatedPins - 1].sensorName, "");
}