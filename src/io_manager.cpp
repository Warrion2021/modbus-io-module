#include "io_manager.h"
#include "sys_init.h"

// Static member definitions
bool IOManager::digitalInputStates[8] = {false};
bool IOManager::digitalOutputStates[8] = {false};
float IOManager::analogInputValues[3] = {0.0};
int IOManager::analogInputRaw[3] = {0};
bool IOManager::latchStates[8] = {false};
unsigned long IOManager::lastUpdateTime = 0;

// External references to global config and status
extern Config config;
extern IOStatus ioStatus;

void IOManager::init() {
    Serial.println("IOManager: Initializing IO Manager");
    
    initDigitalPins();
    applyIOConfiguration();
    updateIOState();
    
    lastUpdateTime = millis();
    Serial.println("IOManager: Initialization complete");
}

void IOManager::initDigitalPins() {
    // Initialize digital inputs
    for (int i = 0; i < 8; i++) {
        pinMode(DIGITAL_INPUTS[i], INPUT);
        
        // Apply pullup configuration
        if (config.diPullup[i]) {
            pinMode(DIGITAL_INPUTS[i], INPUT_PULLUP);
        }
        
        digitalInputStates[i] = false;
        latchStates[i] = false;
    }
    
    // Initialize digital outputs
    for (int i = 0; i < 8; i++) {
        pinMode(DIGITAL_OUTPUTS[i], OUTPUT);
        
        // Set initial state (considering inversion)
        bool initialState = config.doInitialState[i];
        if (config.doInvert[i]) {
            initialState = !initialState;
        }
        
        digitalWrite(DIGITAL_OUTPUTS[i], initialState);
        digitalOutputStates[i] = config.doInitialState[i]; // Store logical state
        ioStatus.dOut[i] = digitalOutputStates[i];
    }
    
    Serial.println("IOManager: Digital pins initialized");
}

void IOManager::updateDigitalInputs() {
    for (int i = 0; i < 8; i++) {
        // Read raw digital input
        bool rawState = digitalRead(DIGITAL_INPUTS[i]);
        
        // Apply inversion if configured
        bool logicalState = rawState;
        if (config.diInvert[i]) {
            logicalState = !rawState;
        }
        
        // Store raw and logical states
        ioStatus.dInRaw[i] = rawState;
        digitalInputStates[i] = logicalState;
        
        // Handle latching
        if (config.diLatch[i]) {
            if (logicalState && !ioStatus.dInLatched[i]) {
                // Rising edge detected, set latch
                ioStatus.dInLatched[i] = true;
                latchStates[i] = true;
            }
            // Use latched state for final output
            ioStatus.dIn[i] = ioStatus.dInLatched[i];
        } else {
            // No latching, use direct state
            ioStatus.dIn[i] = logicalState;
            ioStatus.dInLatched[i] = false;
        }
    }
}

void IOManager::updateDigitalOutputs() {
    for (int i = 0; i < 8; i++) {
        // Apply inversion if configured for physical output
        bool physicalState = digitalOutputStates[i];
        if (config.doInvert[i]) {
            physicalState = !digitalOutputStates[i];
        }
        
        // Update physical pin
        digitalWrite(DIGITAL_OUTPUTS[i], physicalState);
        
        // Update status structure
        ioStatus.dOut[i] = digitalOutputStates[i]; // Store logical state
    }
}

void IOManager::updateAnalogInputs() {
    for (int i = 0; i < 3; i++) {
        // Read 12-bit ADC value
        int rawValue = analogRead(ADC_PINS[i]);
        analogInputRaw[i] = rawValue;
        
        // Convert to millivolts (3.3V reference, 12-bit ADC)
        float voltage = (rawValue * 3300.0) / 4095.0;
        analogInputValues[i] = voltage;
        
        // Update status structure
        ioStatus.aIn[i] = (int)voltage; // Store as integer mV
    }
}

void IOManager::setDigitalOutput(int output, bool state) {
    if (output < 0 || output >= 8) {
        Serial.printf("IOManager: Invalid output number: %d\n", output);
        return;
    }
    
    digitalOutputStates[output] = state;
    
    // Apply inversion for physical pin
    bool physicalState = state;
    if (config.doInvert[output]) {
        physicalState = !state;
    }
    
    digitalWrite(DIGITAL_OUTPUTS[output], physicalState);
    ioStatus.dOut[output] = state; // Store logical state
    
    Serial.printf("IOManager: Set output %d to %s (physical: %s)\n", 
                  output, state ? "HIGH" : "LOW", physicalState ? "HIGH" : "LOW");
}

bool IOManager::getDigitalInput(int input) {
    if (input < 0 || input >= 8) return false;
    return digitalInputStates[input];
}

bool IOManager::getDigitalOutput(int output) {
    if (output < 0 || output >= 8) return false;
    return digitalOutputStates[output];
}

float IOManager::getAnalogInput(int input) {
    if (input < 0 || input >= 3) return 0.0;
    return analogInputValues[input];
}

int IOManager::getAnalogInputRaw(int input) {
    if (input < 0 || input >= 3) return 0;
    return analogInputRaw[input];
}

void IOManager::resetLatch(int input) {
    if (input < 0 || input >= 8) return;
    
    ioStatus.dInLatched[input] = false;
    latchStates[input] = false;
    
    Serial.printf("IOManager: Reset latch for input %d\n", input);
}

void IOManager::resetAllLatches() {
    for (int i = 0; i < 8; i++) {
        resetLatch(i);
    }
    Serial.println("IOManager: All latches reset");
}

bool IOManager::isInputLatched(int input) {
    if (input < 0 || input >= 8) return false;
    return latchStates[input];
}

void IOManager::configurePinMode(int pin, int mode) {
    pinMode(pin, mode);
}

void IOManager::configurePullup(int input, bool enable) {
    if (input < 0 || input >= 8) return;
    
    config.diPullup[input] = enable;
    
    if (enable) {
        pinMode(DIGITAL_INPUTS[input], INPUT_PULLUP);
    } else {
        pinMode(DIGITAL_INPUTS[input], INPUT);
    }
    
    Serial.printf("IOManager: Pullup for input %d: %s\n", input, enable ? "enabled" : "disabled");
}

void IOManager::configureInversion(int input, bool enable) {
    if (input < 0 || input >= 8) return;
    
    config.diInvert[input] = enable;
    Serial.printf("IOManager: Inversion for input %d: %s\n", input, enable ? "enabled" : "disabled");
}

void IOManager::configureLatching(int input, bool enable) {
    if (input < 0 || input >= 8) return;
    
    config.diLatch[input] = enable;
    
    // Reset latch state when disabling
    if (!enable) {
        resetLatch(input);
    }
    
    Serial.printf("IOManager: Latching for input %d: %s\n", input, enable ? "enabled" : "disabled");
}

void IOManager::updateIOState() {
    updateDigitalInputs();
    updateDigitalOutputs();
    updateAnalogInputs();
}

void IOManager::applyIOConfiguration() {
    applyDigitalInputConfig();
    applyDigitalOutputConfig();
}

void IOManager::applyDigitalInputConfig() {
    for (int i = 0; i < 8; i++) {
        // Apply pullup configuration
        if (config.diPullup[i]) {
            pinMode(DIGITAL_INPUTS[i], INPUT_PULLUP);
        } else {
            pinMode(DIGITAL_INPUTS[i], INPUT);
        }
    }
}

void IOManager::applyDigitalOutputConfig() {
    for (int i = 0; i < 8; i++) {
        // Set initial state considering inversion
        bool initialState = config.doInitialState[i];
        bool physicalState = config.doInvert[i] ? !initialState : initialState;
        
        digitalWrite(DIGITAL_OUTPUTS[i], physicalState);
        digitalOutputStates[i] = initialState; // Store logical state
        ioStatus.dOut[i] = initialState;
    }
}

// Legacy wrapper function for compatibility
void updateIOpins() {
    IOManager::updateIOState();
}