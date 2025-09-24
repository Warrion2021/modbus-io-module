# Changelog - Modbus TCP IO Module

## [v2.0.0] - 2025-09-09 - Major Sensor Enhancement Update

### 🎯 Major Features Added

#### Multi-Sensor Type Support
- **Added**: Sensor type selection dropdown in web UI (I2C, UART, Digital, Analog)
- **Enhanced**: SensorConfig struct with `sensor_type[16]` field
- **Impact**: Flexible configuration for different sensor interfaces

#### Mathematical Formula Conversion System
-- **Added**: formula field for mathematical expression parsing (using built-in math/string parsing, no tinyexpr)
- **Enhanced**: Replaced simple polynomial calibration with full mathematical formula support
- **Examples**: `(x * 1.8) + 32`, `sqrt(x * 9.8)`, `log(x + 1) * 100`
- **Impact**: Works across all sensor types with complex conversion equations

#### Terminal Interface for Diagnostics
- **Added**: Bash-like command interface for sensor fault-finding
- **IP Commands**: `ipconfig`, `arp`, `ping <target>`, `tracert <target>`
- **Sensor Commands**: `sensor list`, `sensor read <id>`, `sensor info <id>`, `sensor test <id>`
- **UI**: Dark terminal theme with command history
- **Impact**: Real-time diagnostics and network troubleshooting capabilities

#### Engineering Units Enhancement
- **Fixed**: Bug where calibrated engineering units weren't displaying in sensor table
- **Added**: `units[16]` field to SensorConfig struct
- **Enhanced**: Proper units labeling throughout interface (°C, %, psi, etc.)
- **Impact**: Clear display of both raw and converted values

### 🔧 Technical Changes

#### Backend (main.cpp)
-- **Added**: formula conversion logic using built-in math/string parsing (no tinyexpr)
- **Added**: `applyFormulaConversion(double raw_value, const char* formula)` function
- **Enhanced**: `handleTerminalCommand()` with IP and sensor diagnostic routing
- **Updated**: `saveSensorConfig()` and `loadSensorConfig()` for new fields
- **Fixed**: MAC address retrieval using `eth.macAddress(mac)` with buffer parameter

#### Frontend (index.html)
- **Added**: Sensor type dropdown with options: I2C, UART, Digital, Analog
- **Added**: Formula input field for mathematical expressions
- **Added**: Engineering units input field
- **Enhanced**: Terminal interface section with command input and output display
- **Updated**: Sensor table to show engineering units column

#### Frontend (script.js)
- **Enhanced**: `renderSensorTable()` to display engineering units
- **Added**: `executeTerminalCommand()` function for command processing
- **Added**: `updateTerminalTargets()` for sensor command completion
- **Updated**: Sensor form handling for new fields

#### Styling (styles.css)
- **Added**: Terminal styling with dark theme
- **Enhanced**: Form layouts for new input fields
- **Improved**: Responsive design for terminal interface

#### Dependencies (platformio.ini)
-- **Added**: formula conversion using built-in math/string parsing (no tinyexpr)

### 🏗️ Data Structure Changes

#### SensorConfig Struct Enhancement
```cpp
struct SensorConfig {
    char name[32];           // Existing
    char sensor_type[16];    // NEW: I2C, UART, Digital, Analog
    char formula[64];        // NEW: Mathematical conversion formula
    char units[16];          // NEW: Engineering units
    // ... existing fields maintained
};
```

### 🚀 Build Information
- **Status**: ✅ Successfully compiles
- **Memory Usage**: 21.1% RAM, 9.5% Flash
- **Target**: Raspberry Pi Pico (RP2040)
- **Framework**: Arduino with PlatformIO

### 📋 Compatibility
- **Maintains**: All existing Modbus TCP functionality
- **Backwards Compatible**: Existing sensor configurations will work
- **New Features**: Optional - existing setups continue to function

### 🐛 Bug Fixes
- **Fixed**: Engineering units not displaying in sensor table for analog sensors
- **Fixed**: MAC address function compilation error (`eth.macAddress()` → `eth.macAddress(mac)`)

### 📚 Documentation
- **Added**: This changelog for tracking modifications
- **Location**: Project root directory
- **Purpose**: Historical record of enhancements and technical decisions

---

## Previous Versions

### [v1.0.0] - Initial Release
- Basic Modbus TCP IO Module functionality
- Simple analog sensor support
- Web-based configuration interface
- Polynomial calibration system
