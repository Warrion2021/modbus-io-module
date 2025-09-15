#pragma once

#include <Arduino.h>
#include "sys_init.h"

// IO Manager - Handles all digital and analog input/output operations
class IOManager {
public:
    // Initialization
    static void init();
    
    // Digital Input/Output Management
    static void initDigitalPins();
    static void updateDigitalInputs();
    static void updateDigitalOutputs();
    static void setDigitalOutput(int output, bool state);
    static bool getDigitalInput(int input);
    static bool getDigitalOutput(int output);
    
    // Analog Input Management
    static void updateAnalogInputs();
    static float getAnalogInput(int input);
    static int getAnalogInputRaw(int input);
    
    // Latch Management
    static void resetLatch(int input);
    static void resetAllLatches();
    static bool isInputLatched(int input);
    
    // Pin Configuration
    static void configurePinMode(int pin, int mode);
    static void configurePullup(int input, bool enable);
    static void configureInversion(int input, bool enable);
    static void configureLatching(int input, bool enable);
    
    // State Management
    static void updateIOState();
    static void applyIOConfiguration();
    
private:
    // Internal state tracking
    static bool digitalInputStates[8];
    static bool digitalOutputStates[8];
    static float analogInputValues[3];
    static int analogInputRaw[3];
    static bool latchStates[8];
    static unsigned long lastUpdateTime;
    
    // Configuration helpers
    static void applyDigitalInputConfig();
    static void applyDigitalOutputConfig();
};

// Helper functions for legacy compatibility
void updateIOpins();  // Legacy function wrapper