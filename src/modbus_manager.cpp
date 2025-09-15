#include "modbus_manager.h"

// Static member definitions
WiFiServer ModbusManager::modbusServer(502);
ModbusClientConnection ModbusManager::clients[MAX_MODBUS_CLIENTS] = {};
int ModbusManager::connectedClientCount = 0;
bool ModbusManager::serverRunning = false;

// External variables from main.cpp
extern Config config;
extern IOStatus ioStatus;

void ModbusManager::initialize() {
    connectedClientCount = 0;
    serverRunning = false;
    
    // Initialize all client slots
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        clients[i].connected = false;
        clients[i].connectionTime = 0;
    }
    
    // Start Modbus server
    modbusServer.begin();
    serverRunning = true;
    
    Serial.printf("Modbus TCP server started on port %d\n", config.modbusPort);
}

void ModbusManager::handleClients() {
    if (!serverRunning) return;
    
    acceptNewClients();
    pollExistingClients();
    updateConnectionStatus();
}

void ModbusManager::acceptNewClients() {
    // Check for new client connections
    WiFiClient newClient = modbusServer.accept();
    
    if (newClient) {
        int emptySlot = findEmptyClientSlot();
        
        if (emptySlot >= 0) {
            Serial.printf("New client connected to slot %d\n", emptySlot);
            
            // Store the client and mark as connected
            clients[emptySlot].client = newClient;
            clients[emptySlot].connected = true;
            clients[emptySlot].server.begin();
            clients[emptySlot].connectionTime = millis();
            
            Serial.println("Modbus server accepted client connection");
            
            // Initialize coil states for this client
            initializeClientCoils(emptySlot);
            
            connectedClientCount++;
            digitalWrite(LED_BUILTIN, HIGH);
        } else {
            // No empty slots, reject the connection
            Serial.println("No empty client slots, rejecting connection");
            newClient.stop();
        }
    }
}

void ModbusManager::pollExistingClients() {
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].connected) {
            if (!clients[i].client.connected()) {
                // Client disconnected
                disconnectClient(i);
            } else {
                // Update IO registers for this client
                updateClientRegisters(i);
                
                // Poll the client
                clients[i].server.poll();
                clients[i].connectionTime = millis();
            }
        }
    }
}

void ModbusManager::disconnectClient(int clientIndex) {
    if (clientIndex < 0 || clientIndex >= MAX_MODBUS_CLIENTS) return;
    
    if (clients[clientIndex].connected) {
        Serial.printf("Client disconnected from slot %d\n", clientIndex);
        clients[clientIndex].connected = false;
        clients[clientIndex].client.stop();
        connectedClientCount--;
        
        if (connectedClientCount == 0) {
            digitalWrite(LED_BUILTIN, LOW);
        }
    }
}

void ModbusManager::updateClientRegisters(int clientIndex) {
    if (clientIndex < 0 || clientIndex >= MAX_MODBUS_CLIENTS) return;
    if (!clients[clientIndex].connected) return;
    
    // Update discrete inputs (FC2) with digital input states
    for (int i = 0; i < 8; i++) {
        clients[clientIndex].server.discreteInputWrite(i, ioStatus.dIn[i]);
    }
    
    // Update coils (FC1) with digital output states
    for (int i = 0; i < 8; i++) {
        clients[clientIndex].server.coilWrite(i, ioStatus.dOut[i]);
    }
    
    // Update input registers (FC4) with analog values
    for (int i = 0; i < 3; i++) {
        clients[clientIndex].server.inputRegisterWrite(i, ioStatus.aIn[i]);
    }
    
    // Update temperature and humidity registers if available
    if (ioStatus.temperature != 0.0) {
        clients[clientIndex].server.inputRegisterWrite(3, (int)(ioStatus.temperature * 100));
    }
    if (ioStatus.humidity != 0.0) {
        clients[clientIndex].server.inputRegisterWrite(4, (int)(ioStatus.humidity * 100));
    }
    
    // Check for coil writes from client and update outputs
    for (int i = 0; i < 8; i++) {
        bool coilState = clients[clientIndex].server.coilRead(i);
        if (coilState != ioStatus.dOut[i]) {
            IOManager::setDigitalOutput(i, coilState);
        }
    }
}

void ModbusManager::syncAllClientRegisters() {
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].connected) {
            updateClientRegisters(i);
        }
    }
}

void ModbusManager::stopAllClients() {
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].connected) {
            disconnectClient(i);
        }
    }
    
    if (serverRunning) {
        modbusServer.stop();
        serverRunning = false;
        Serial.println("Modbus TCP server stopped");
    }
}

int ModbusManager::getConnectedClientCount() {
    return connectedClientCount;
}

bool ModbusManager::isServerRunning() {
    return serverRunning;
}

void ModbusManager::restartServer() {
    stopAllClients();
    delay(100);
    initialize();
}

// Private helper methods
int ModbusManager::findEmptyClientSlot() {
    for (int i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (!clients[i].connected) {
            return i;
        }
    }
    return -1; // No empty slot found
}

void ModbusManager::initializeClientCoils(int clientIndex) {
    if (clientIndex < 0 || clientIndex >= MAX_MODBUS_CLIENTS) return;
    if (!clients[clientIndex].connected) return;
    
    // Initialize coil states for this client to match current output states
    for (int i = 0; i < 8; i++) {
        clients[clientIndex].server.coilWrite(i, ioStatus.dOut[i]);
    }
}

void ModbusManager::updateConnectionStatus() {
    // Update global connected clients count (for external reference)
    extern int connectedClients;
    connectedClients = connectedClientCount;
}