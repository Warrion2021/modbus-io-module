#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoModbus.h>
#include "sys_init.h"
#include "io_manager.h"

class ModbusManager {
public:
    // Modbus server lifecycle
    static void initialize();
    static void handleClients();
    static void stopAllClients();
    
    // Client management
    static void acceptNewClients();
    static void pollExistingClients();
    static void disconnectClient(int clientIndex);
    
    // Register synchronization
    static void updateClientRegisters(int clientIndex);
    static void syncAllClientRegisters();
    
    // Server status
    static int getConnectedClientCount();
    static bool isServerRunning();
    static void restartServer();
    
private:
    static WiFiServer modbusServer;
    static ModbusClientConnection clients[MAX_MODBUS_CLIENTS];
    static int connectedClientCount;
    static bool serverRunning;
    
    // Helper methods
    static int findEmptyClientSlot();
    static void initializeClientCoils(int clientIndex);
    static void updateConnectionStatus();
};

#endif // MODBUS_MANAGER_H