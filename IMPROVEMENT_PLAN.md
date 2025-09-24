# Modbus IO Module – Improvement Plan

_Last updated: 2025-09-24_
existing features of the web ui to keep:
- header showing modbus client status.
-network configuration section
-IO status section containing the sensor dataflow(sensor dataflow to be improved as per plan)
-IO configuration that allows basic modbus latch functions
-sensor configuration section with a table showing sensors configured and settings(including the pins the sensors are added on). This section has an add sensor button that allows adding sensors by first specifying the protocol type then adding a sensor type from a sensor type dropdown with a list of hardcoded sensors (aranged by protocol) and an option to add a generic sensor of any protocol type.
-terminal interface that allows watching traffic on any ports and pins of the wiznet w5500 and also allows sending comands as per the terminal_guide.md to the various sensor types

Improvements needed

## 1. Sensor configuration/Add Sensor Modal
- The UI must dynamically fetch the list of supported sensor types and available pins for each protocol from the firmware (via REST endpoints) to populate the drop downs of the add sensor modal. This ensures the UI always matches the firmware's current capabilities and pin availability.
- **Protocol dropdown**: select from protocols; analog voltage, digital count, uart, I2C and onewire
- **Sensor type dropdown**: order by protocol, always includes a “Generic” option for all protocol types.
  - The dropdown is dynamically populated with sensor types relevant to the selected protocol.This includes any sensors that are hard coded. hard coded sensors that are activated by this process can still have the pin assignments eddited in the pin assignment drop down.
- **Pin assignment dropdown**: Populated by protocol/type, only shows pins available for a sensor of the type selected.Only shows pins currently available. Needs to take into account specifics of the protocols ie. some protocols can have multiple sensors on a single bus(check protocol and hardware restrictions)
  - When a protocol is selected, the pin selection dropdown appears with only the currently available pins for that protocol at that time. ie it looks in the firmaware to see the sensors condigured and gives a list of pins available to which sensors of that protocol can be assigned. Need to take into account the pinout of the wiznet w5500.
  -
- **I2C address field**: Only shown when I2C sensor is selected.
  - When I2C is selected, a field for entering/selecting the I2C address appears.
  - The field is inactive for all other protocols.
- **Modbus register selector**: User selects the register that the sensor being added will eventually have it values published to out the end of the sensor dataflow in the IO status section
- **Add button**: Adds the sensor to a pending config array (not yet committed).
- **Save & Reboot button**: this is on the main chart of all sensors configured in the sensor configuration section and when pressed Commits all sensor updates(additions via the add sensor button and pop up), triggers firmware update. Similarly there should be some way to delete unwanted sensors.

### 2. Dataflow Display
- **Raw Data**: Shows live values from the assigned pins for each sensor.
- **Calibration Pane**: Shows/edits calibration math and, for digital sensors, data extraction logic. This allows any output from any sensor type to be converted to engineering units that reflect the real world state.
- **Calibrated Value**: Shows the calibrated value that results from the calibration pane displayed in the Modbus register it is sent to.

### 3. Terminal/Firmware Link
functionality to watch data transmission on selected pins and ports is available for fault finding sensor connections and address faults etc. protocol and pins are selected for querry and either the watch button can be pressed or commands specific to the sensor type; as per the TERMINAL_GUIDE.md can be issued to the various sensors.

### 4. Code Linkage Checklist
-The different sections of the web ui need to dynamically update according to the sensors configured as do the pin assignments in the firmware. 

---

This document is the authoritative improvement plan for linking the sensor config UI, dataflow display, and firmware/terminal logic. All future work should reference and update this plan as features are implemented or changed.
