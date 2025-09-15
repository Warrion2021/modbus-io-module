#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

#include <Arduino.h>
#include "sys_init.h"

class PinManager {
public:
    // Pin allocation lifecycle
    static void initialize();
    static void clearAllAllocations();
    
    // Pin availability checking
    static bool isPinAvailable(uint8_t pin, const char* protocol);
    static bool isPinAllocated(uint8_t pin);
    static bool isValidPin(uint8_t pin);
    
    // Pin allocation management
    static bool allocatePin(uint8_t pin, const char* protocol, const char* sensorName);
    static void deallocatePin(uint8_t pin);
    static void deallocatePinsForSensor(const char* sensorName);
    static void deallocatePinsForProtocol(const char* protocol);
    
    // Pin allocation queries
    static int getAllocatedPinCount();
    static int getAvailablePinCount();
    static PinAllocation* getAllocations();
    static PinAllocation* findAllocation(uint8_t pin);
    
    // Pin protocol support
    static bool isProtocolSupported(const char* protocol);
    static uint8_t* getAvailablePinsForProtocol(const char* protocol, int& count);
    
    // Debugging and status
    static void printAllocations();
    static bool validateAllocations();
    
private:
    static PinAllocation pinAllocations[40];
    static int numAllocatedPins;
    
    // Helper methods
    static bool isFlexiblePin(uint8_t pin);
    static bool isReservedPin(uint8_t pin);
    static void shiftAllocationsDown(int startIndex);
};

#endif // PIN_MANAGER_H