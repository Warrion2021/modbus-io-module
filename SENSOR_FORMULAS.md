# Multi-Protocol Sensor Framework - Formula Examples

## Successfully Integrated Features ✅

### 🧮 Mathematical Formula Parser
- **Basic Operations**: `+`, `-`, `*`, `/`, `()`
- **Advanced Functions**: `sqrt(x)`, `log(x)`, `pow(x,y)`
- **Variable Support**: Uses `x` as the raw sensor value
- **Complex Expressions**: Full recursive parsing with precedence

### 🔌 Supported Sensor Protocols
1. **EZO** - Atlas Scientific I2C sensors (pH, DO, EC, RTD)
2. **Analog** - 4-20mA, 0-10V, Thermistor, generic ADC
3. **I2C** - BME280, SHT30/31, VL53L1X, generic I2C devices
4. **UART** - RS232/RS485 sensors with configurable baud rates
5. **OneWire** - DS18B20 temperature sensors
6. **GPIO** - Digital sensors with pullup/inversion
7. **DigitalCounter** - Pulse/frequency counting sensors

## Formula Examples

### Industrial Calibration Examples
```json
{
  "name": "Pressure Transmitter",
  "protocol": "Analog",
  "formula": "(x-4000)/16000*100",
  "unit": "PSI",
  "description": "4-20mA to 0-100 PSI"
}
```

```json
{
  "name": "Temperature Linearization", 
  "protocol": "Analog",
  "formula": "sqrt(x*1000)+32",
  "unit": "°F",
  "description": "Thermistor with square root compensation"
}
```

```json
{
  "name": "Flow Rate Calculation",
  "protocol": "DigitalCounter", 
  "formula": "pow(x/1000,0.5)*15.5",
  "unit": "GPM",
  "description": "Pulse count to flow rate with square root extraction"
}
```

```json
{
  "name": "pH Sensor Calibration",
  "protocol": "EZO",
  "formula": "log(x+1)*2.5+7.0",
  "unit": "pH",
  "description": "Logarithmic pH calibration"
}
```

### Complex Mathematical Examples

**Pressure Compensation**:
```
"formula": "x*sqrt(298.15/(273.15+25))"
```

**Multi-Point Calibration**:
```
"formula": "x*1.847-0.032+pow(x,2)*0.001"
```

**Temperature Correction**:
```
"formula": "(x-273.15)*1.8+32"
```

## Configuration Structure

```json
{
  "enabled": true,
  "name": "Custom Sensor",
  "protocol": "Analog|I2C|UART|OneWire|GPIO|DigitalCounter|EZO",
  "pin_assignment": "A0|D2|etc",
  "i2cAddress": "0x48",
  "baud_rate": 9600,
  "device_id": "1",
  "formula": "mathematical_expression",
  "unit": "unit_string",
  "sample_interval_ms": 1000,
  "modbusRegister": 10
}
```

## Implementation Status

✅ **Multi-protocol sensor framework** - Complete  
✅ **Formula parser with sqrt(), log(), pow()** - Complete  
✅ **JSON configuration loading/saving** - Complete  
✅ **Pin-based sensor assignment** - Complete  
✅ **Modbus register mapping** - Complete  
✅ **REST API endpoints** - Complete  
✅ **I2C device auto-detection** - Complete  
✅ **EZO sensor integration** - Complete  
✅ **Compilation successful** - Complete  

## Next Steps

1. **Hardware Testing** - Test with real sensors
2. **Formula Validation** - Verify complex math operations
3. **Performance Testing** - Ensure timing requirements met
4. **Documentation** - Complete API documentation
5. **Web UI Enhancement** - Add formula editing interface

## Notes

- Formula parser supports nested expressions: `sqrt(pow(x,2)+1)`
- Error handling for division by zero and invalid formulas
- Debug output for complex formula evaluation
- Memory efficient implementation suitable for RP2040
- Compatible with all existing EZO sensor functionality