// Global variables for configuration state
let ioConfigState = {
    di_pullup: [false, false, false, false, false, false, false, false],
    di_invert: [false, false, false, false, false, false, false, false],
    di_latch: [false, false, false, false, false, false, false, false],
    do_invert: [false, false, false, false, false, false, false, false],
    do_init: [false, false, false, false, false, false, false, false]
};

// Global variables for sensor management
let currentSensorConfig = [];
let currentCalibratedSensors = [];
let currentSensorCommand = null;
let currentSensorI2CData = null;

// Toast notification functions
function showToast(message, type, countdown = false, duration = 3000) {
    const toastContainer = document.getElementById('toast-container');
    
    // Create toast element
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    if (countdown) {
        toast.classList.add('countdown');
    }
    
    // Create message content
    const toastContent = document.createElement('span');
    toastContent.innerHTML = message;
    
    // Create close button
    const closeButton = document.createElement('button');
    closeButton.className = 'toast-close';
    closeButton.innerHTML = '&times;';
    closeButton.addEventListener('click', () => {
        toastContainer.removeChild(toast);
    });
    
    // Assemble toast
    toast.appendChild(toastContent);
    toast.appendChild(closeButton);
    
    // Add to container
    toastContainer.appendChild(toast);
    
    // Auto-remove after duration (if not countdown)
    if (!countdown) {
        setTimeout(() => {
            if (toastContainer.contains(toast)) {
                toastContainer.removeChild(toast);
            }
        }, duration);
    }
    
    return toast;
}

function showCountdownToast(message, type, countdownDuration, onComplete) {
    const toast = showToast(message, type, true);
    
    let countdown = countdownDuration;
    const updateMessage = (seconds) => {
        toast.firstChild.innerHTML = `${message} Page will reload in ${seconds} seconds`;
    };
    
    updateMessage(countdown);
    
    const countdownInterval = setInterval(() => {
        countdown--;
        updateMessage(countdown);
        
        if (countdown <= 0) {
            clearInterval(countdownInterval);
            if (onComplete) {
                onComplete();
            }
        }
    }, 1000);
    
    return toast;
}

let configLoadAttempts = 0;
const MAX_CONFIG_LOAD_ATTEMPTS = 5;

function loadConfig() {
    console.log("Attempting to load configuration...");
    fetch('/config')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            // Check if we got valid data (current_ip should exist)
            if (!data.current_ip) {
                throw new Error('Incomplete data received');
            }
            
            console.log("Configuration loaded successfully:", data);
            
            // Detect device type and store device info
            isSimulator = data.is_simulator || false;
            deviceInfo = {
                isSimulator: isSimulator,
                firmwareVersion: data.firmware_version || "Unknown",
                deviceType: data.device_type || "Modbus IO Module"
            };
            
            // Update device info display
            updateDeviceInfoDisplay();
            
            document.getElementById('dhcp').checked = data.dhcp;
            document.getElementById('ip').value = data.ip;
            document.getElementById('subnet').value = data.subnet;
            document.getElementById('gateway').value = data.gateway;
            document.getElementById('hostname').value = data.hostname;
            document.getElementById('modbus_port').value = data.modbus_port;
            document.getElementById('current-ip').textContent = data.current_ip || data.ip;
            document.getElementById('current-hostname').textContent = data.hostname;
            document.getElementById('current-modbus-port').textContent = data.modbus_port;
            document.getElementById('client-count').textContent = data.modbus_client_count;
            
            // Update connected clients information
            const clientList = document.getElementById('client-list');
            if (data.modbus_client_count > 0 && data.modbus_clients) {
                document.getElementById('client-section').style.display = 'block';
                clientList.innerHTML = data.modbus_clients.map(client => 
                    `<div class="client-item">
                        <h4>Client ${client.slot + 1}</h4>
                        <p>IP Address: ${client.ip}</p>
                        <p>Connected for: ${formatDuration(client.connected_for)}</p>
                    </div>`
                ).join('');
            } else {
                document.getElementById('client-section').style.display = 'none';
                clientList.innerHTML = '<div class="no-clients">No clients connected</div>';
            }
            
            // Disable IP fields if DHCP is enabled
            const dhcpEnabled = data.dhcp;
            ['ip', 'subnet', 'gateway'].forEach(id => {
                document.getElementById(id).disabled = dhcpEnabled;
            });
            
            // Reset attempts counter on success
            configLoadAttempts = 0;
            
            // Once config is loaded successfully, wait a moment then start loading IO status
            setTimeout(() => {
                console.log("Starting IO status updates...");
                updateIOStatus();
            }, 500);
        })
        .catch(error => {
            console.error('Error loading config:', error);
            configLoadAttempts++;
            
            if (configLoadAttempts < MAX_CONFIG_LOAD_ATTEMPTS) {
                // Retry after a delay with exponential backoff
                const retryDelay = Math.min(1000 * Math.pow(1.5, configLoadAttempts), 5000);
                console.log(`Retrying config load (attempt ${configLoadAttempts+1}/${MAX_CONFIG_LOAD_ATTEMPTS}) in ${retryDelay}ms...`);
                setTimeout(loadConfig, retryDelay);
            } else {
                // Even after max attempts, try one more time after a longer delay
                console.log("Maximum config load attempts reached. Trying one final attempt in 5 seconds...");
                setTimeout(loadConfig, 5000);
            }
        });
}

let ioStatusLoadAttempts = 0;
const MAX_IO_STATUS_LOAD_ATTEMPTS = 5;

function updateIOStatus() {
    console.log("=== UPDATE IO STATUS CALLED ===");
    console.log("Fetching IO status...");
    fetch('/iostatus')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            console.log("=== IO STATUS RESPONSE ===");
            console.log("Full data object:", data);
            console.log("configured_sensors exists:", !!data.configured_sensors);
            console.log("configured_sensors length:", data.configured_sensors ? data.configured_sensors.length : 'N/A');
            if (data.configured_sensors) {
                console.log("configured_sensors content:", data.configured_sensors);
            }
            
            // Store the current IO status for calibration modal
            window.currentIOStatus = data;
            
            // Update Modbus status
            const statusIndicator = document.getElementById('modbus-status');
            const statusText = document.getElementById('modbus-status-text');
            
            if (data.modbus_connected) {
                statusIndicator.className = 'status-indicator status-connected';
                statusText.textContent = 'Connected';
            } else {
                statusIndicator.className = 'status-indicator status-disconnected';
                statusText.textContent = 'Disconnected';
            }

            // Update client count and modbus status
            document.getElementById('client-count').textContent = data.modbus_client_count;
            
            // Update modbus status indicator
            const modbusStatus = document.getElementById('modbus-status');
            if (modbusStatus) {
                if (data.modbus_client_count > 0) {
                    modbusStatus.textContent = 'Active';
                    modbusStatus.className = 'status-indicator active';
                } else {
                    modbusStatus.textContent = 'No Clients';
                    modbusStatus.className = 'status-indicator inactive';
                }
            }
            
            // Update connected clients
            const clientList = document.getElementById('client-list');
            if (data.modbus_client_count > 0) {
                document.getElementById('client-section').style.display = 'block';
                clientList.innerHTML = data.modbus_clients.map(client => 
                    `<div class="client-item">
                        <h4>Client ${client.slot + 1}</h4>
                        <p>IP Address: ${client.ip}</p>
                        <p>Connected for: ${formatDuration(client.connected_for)}</p>
                    </div>`
                ).join('');
            } else {
                document.getElementById('client-section').style.display = 'none';
                clientList.innerHTML = '<div class="no-clients">No clients connected</div>';
            }
            
            // Update I2C sensors with dynamic display - remove this legacy section as it conflicts with new sensor data flow
            // This section is now handled by the configured_sensors logic below
            
            // Update Protocol-based Configured Sensors section
            console.log("Debug: Checking configured_sensors data:", data.configured_sensors);
            if (data.configured_sensors && data.configured_sensors.length > 0) {
                console.log("Debug: Found", data.configured_sensors.length, "configured sensors");
                // Organize sensors by protocol
                const sensorsByProtocol = {
                    'I2C': [],
                    'UART': [],
                    'Analog Voltage': [],
                    'One-Wire': [],
                    'Digital Counter': []
                };
                
                data.configured_sensors.forEach(sensor => {
                    // Determine protocol from sensor configuration or type
                    let protocol = 'I2C'; // default
                    
                    if (sensor.protocol) {
                        protocol = sensor.protocol;
                    } else {
                        // Infer protocol from sensor type if not specified
                        if (sensor.type && sensor.type.includes('ANALOG')) {
                            protocol = 'Analog Voltage';
                        } else if (sensor.type && sensor.type.includes('UART')) {
                            protocol = 'UART';
                        } else if (sensor.type && sensor.type.includes('DS18B20')) {
                            protocol = 'One-Wire';
                        } else if (sensor.type && sensor.type.includes('DIGITAL')) {
                            protocol = 'Digital Counter';
                        }
                    }
                    
                    if (sensorsByProtocol[protocol]) {
                        sensorsByProtocol[protocol].push(sensor);
                    }
                });
                
                console.log("Debug: Sensors organized by protocol:", sensorsByProtocol);
                
                // Update each protocol category with visual flow
                Object.keys(sensorsByProtocol).forEach(protocol => {
                    let containerId, categoryId;
                    
                    switch(protocol) {
                        case 'I2C':
                            containerId = 'i2c-sensors-flow';
                            categoryId = 'i2c-category';
                            break;
                        case 'UART':
                            containerId = 'uart-sensors-flow';
                            categoryId = 'uart-category';
                            break;
                        case 'Analog Voltage':
                            containerId = 'analog-sensors-flow';
                            categoryId = 'analog-category';
                            break;
                        case 'One-Wire':
                            containerId = 'onewire-sensors-flow';
                            categoryId = 'onewire-category';
                            break;
                        case 'Digital Counter':
                            containerId = 'digital-sensors-flow';
                            categoryId = 'digital-category';
                            break;
                        default:
                            return; // Skip unknown protocols
                    }
                    
                    const container = document.getElementById(containerId);
                    const category = document.getElementById(categoryId);
                    
                    if (container && category) {
                        if (sensorsByProtocol[protocol].length > 0) {
                            let sensorsHtml = '';
                            
                            sensorsByProtocol[protocol].forEach(sensor => {
                                const statusClass = sensor.enabled ? 'sensor-status-enabled' : 'sensor-status-disabled';
                                const statusText = sensor.enabled ? 'Enabled' : 'Disabled';
                                
                                // Raw data stage
                                let rawValue = 'No data';
                                let rawDetails = '';
                                
                                // Use raw I2C data if available
                                if (sensor.raw_i2c_data && sensor.raw_i2c_data.length > 0) {
                                    rawValue = sensor.raw_i2c_data;
                                    rawDetails = `<div class="raw-data-details"><strong>I2C Register:</strong> ${sensor.i2c_parsing?.register || 'N/A'}<br><strong>Data Length:</strong> ${sensor.i2c_parsing?.data_length || 'N/A'} bytes</div>`;
                                } 
                                // Fallback to raw_value if I2C data not available
                                else if (sensor.raw_value !== undefined) {
                                    rawValue = sensor.raw_value;
                                }
                                
                                // Calibration stage
                                let calibratedValue = 'Uncalibrated';
                                let calibratedUnit = '';
                                if (sensor.calibrated_value !== undefined) {
                                    calibratedValue = sensor.calibrated_value;
                                    calibratedUnit = sensor.unit || '';
                                }
                                
                                // Modbus stage - use current_value as final output
                                let modbusValue = 'Not mapped';
                                let modbusRegister = '';
                                if (sensor.modbus_register !== undefined && sensor.modbus_register > 0) {
                                    modbusRegister = `Reg ${sensor.modbus_register}`;
                                    if (sensor.current_value !== undefined) {
                                        modbusValue = sensor.current_value.toString();
                                    }
                                }
                                
                                // Handle secondary values for multi-value sensors
                                let secondaryValueHtml = '';
                                if (sensor.secondary_value) {
                                    secondaryValueHtml = `
                                        <div class="secondary-value-section">
                                            <h5>Secondary Value: ${sensor.secondary_value.name}</h5>
                                            <div class="sensor-flow-pipeline">
                                                <div class="flow-stage">
                                                    <div class="flow-stage-box flow-stage-raw">
                                                        <div class="flow-stage-title">Raw</div>
                                                        <div class="flow-stage-value">${sensor.secondary_value.raw}</div>
                                                    </div>
                                                </div>
                                                <div class="flow-arrow">→</div>
                                                <div class="flow-stage">
                                                    <div class="flow-stage-box flow-stage-calibration">
                                                        <div class="flow-stage-title">Calibrated</div>
                                                        <div class="flow-stage-value">${sensor.secondary_value.calibrated}</div>
                                                    </div>
                                                </div>
                                                <div class="flow-arrow">→</div>
                                                <div class="flow-stage">
                                                    <div class="flow-stage-box flow-stage-modbus">
                                                        <div class="flow-stage-title">Modbus</div>
                                                        <div class="flow-stage-value">${sensor.secondary_value.calibrated}</div>
                                                        <div class="flow-stage-unit">${sensor.secondary_value.modbus_register ? `Reg ${sensor.secondary_value.modbus_register}` : 'Not mapped'}</div>
                                                    </div>
                                                </div>
                                            </div>
                                        </div>
                                    `;
                                }
                                
                                sensorsHtml += `
                                    <div class="sensor-flow-item">
                                        <div class="sensor-flow-header">
                                            <div class="sensor-flow-title">${sensor.name}</div>
                                            <div class="sensor-status-badge ${statusClass}">${statusText}</div>
                                        </div>
                                        
                                        <div class="sensor-flow-pipeline">
                                            <div class="flow-stage">
                                                <div class="flow-stage-box flow-stage-raw">
                                                    <div class="flow-stage-title">Raw Data</div>
                                                    <div class="flow-stage-value">${rawValue}</div>
                                                    ${rawDetails}
                                                </div>
                                            </div>
                                            
                                            <div class="flow-arrow">→</div>
                                            
                                            <div class="flow-stage">
                                                <div class="flow-stage-box flow-stage-calibration">
                                                    <div class="flow-stage-title">Calibrated</div>
                                                    <div class="flow-stage-value">${calibratedValue}</div>
                                                    <div class="flow-stage-unit">${calibratedUnit}</div>
                                                    <button class="calibration-btn" onclick="showCalibrationModal('${sensor.name}')" title="Configure calibration">⚙</button>
                                                </div>
                                            </div>
                                            
                                            <div class="flow-arrow">→</div>
                                            
                                            <div class="flow-stage">
                                                <div class="flow-stage-box flow-stage-modbus">
                                                    <div class="flow-stage-title">Modbus Register</div>
                                                    <div class="flow-stage-value">${modbusValue}</div>
                                                    <div class="flow-stage-unit">${modbusRegister}</div>
                                                </div>
                                            </div>
                                        </div>
                                        
                                        ${secondaryValueHtml}
                                        
                                        <div class="sensor-flow-actions">
                                            <button class="flow-action-btn flow-action-refresh" onclick="refreshSensorData('${sensor.name}')" title="Refresh sensor data">🔄 Refresh</button>
                                            <button class="flow-action-btn flow-action-test" onclick="testSensorRead('${sensor.name}')" title="Test sensor communication">🔍 Test</button>
                                        </div>
                                    </div>
                                `;
                            });
                            
                            container.innerHTML = sensorsHtml;
                            category.classList.remove('empty');
                            category.style.display = 'block';
                        } else {
                            category.classList.add('empty');
                            category.style.display = 'none';
                        }
                    }
                });
            } else {
                console.log("=== NO CONFIGURED SENSORS FOUND ===");
                console.log("Data object keys:", Object.keys(data));
                
                // Hide all protocol categories when no sensors are configured
                ['i2c-category', 'uart-category', 'analog-category', 'onewire-category', 'digital-category'].forEach(categoryId => {
                    const category = document.getElementById(categoryId);
                    if (category) {
                        category.classList.add('empty');
                        category.style.display = 'none';
                    }
                });
            }
            
            // Update Modbus Register Map
            const modbusContainer = document.getElementById('modbus-registers-container');
            if (data.modbus_registers && data.modbus_registers.length > 0) {
                let registerHtml = '';
                
                data.modbus_registers.forEach(reg => {
                    const isSimulated = reg.simulated || false;
                    const simulatedClass = isSimulated ? 'simulated' : '';
                    
                    let nameDisplay = '';
                    if (reg.sensor_name) {
                        nameDisplay = `<div class="register-name">${reg.sensor_name}</div>`;
                    } else if (reg.type === 'analog_input') {
                        nameDisplay = `<div class="register-name">Analog Input ${reg.register + 1}</div>`;
                    } else {
                        nameDisplay = `<div class="register-name">Register ${reg.register}</div>`;
                    }
                    
                    let typeDisplay = '';
                    if (reg.sensor_type) {
                        typeDisplay = `<div class="register-type">${reg.sensor_type}</div>`;
                    } else {
                        typeDisplay = `<div class="register-type">${reg.type}</div>`;
                    }
                    
                    let valueDisplay = '';
                    if (reg.value !== undefined) {
                        const unit = reg.unit || '';
                        const value = typeof reg.value === 'number' ? 
                                     (reg.value % 1 === 0 ? reg.value : reg.value.toFixed(2)) : 
                                     reg.value;
                        valueDisplay = `<span>${value} <span class="register-unit">${unit}</span></span>`;
                    } else if (reg.response) {
                        valueDisplay = `<span>${reg.response}</span>`;
                    }
                    
                    const simulationBadge = isSimulated ? 
                        '<span class="simulation-badge">SIM</span>' : '';
                    
                    registerHtml += `
                        <div class="register-item ${simulatedClass}">
                            <div class="register-info">
                                <span class="register-number">${reg.register}</span>
                                <div class="register-details">
                                    ${nameDisplay}
                                    ${typeDisplay}
                                </div>
                                ${simulationBadge}
                            </div>
                            <div class="register-value">
                                ${valueDisplay}
                            </div>
                        </div>
                    `;
                });
                
                modbusContainer.innerHTML = registerHtml;
            } else {
                modbusContainer.innerHTML = `
                    <div class="register-item" style="color: #666; font-style: italic;">
                        <span>No register mappings available</span>
                    </div>
                `;
            }
            
            // Update EZO calibration modal response if it's open
            const ezoModal = document.getElementById('ezo-calibration-modal-overlay');
            if (ezoModal && ezoModal.classList.contains('show') && currentEzoSensorIndex >= 0) {
                if (data.ezo_sensors && data.ezo_sensors[currentEzoSensorIndex]) {
                    const sensorData = data.ezo_sensors[currentEzoSensorIndex];
                    const responseElement = document.getElementById('ezo-sensor-response');
                    if (sensorData.response && sensorData.response.trim() !== '') {
                        responseElement.textContent = sensorData.response;
                    }
                }
            }
            
            // Reset attempts counter on success
            ioStatusLoadAttempts = 0;
        })
        .catch(error => {
            console.error('Error updating IO status:', error);
            ioStatusLoadAttempts++;
            
            if (ioStatusLoadAttempts < MAX_IO_STATUS_LOAD_ATTEMPTS) {
                // Retry after a delay with exponential backoff
                const retryDelay = Math.min(1000 * Math.pow(1.5, ioStatusLoadAttempts), 5000);
                console.log(`Retrying IO status update (attempt ${ioStatusLoadAttempts+1}/${MAX_IO_STATUS_LOAD_ATTEMPTS}) in ${retryDelay}ms...`);
                setTimeout(updateIOStatus, retryDelay);
            } else {
                // After max attempts, still try again but less frequently
                console.log("Maximum IO status load attempts reached. Continuing with reduced frequency...");
                setTimeout(updateIOStatus, 5000);
            }
        });
}

// Function to reset a single latched input
function loadIOConfig() {
    console.log("Loading IO configuration...");
    fetch('/ioconfig')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            console.log("IO configuration loaded successfully:", data);
            
            // Store the configuration state globally
            ioConfigState = {
                di_pullup: [...data.di_pullup],
                di_invert: [...data.di_invert],
                di_latch: [...data.di_latch],
                do_invert: [...data.do_invert],
                do_initial_state: [...data.do_initial_state]
            };
            
            // Digital Inputs configuration
            const diConfigDiv = document.getElementById('di-config');
            let diConfigHtml = '<table class="io-config-table">';
            diConfigHtml += `
                <thead>
                    <tr>
                        <th>Input</th>
                        <th>Pullup</th>
                        <th>Invert</th>
                        <th>Latch</th>
                    </tr>
                </thead>
                <tbody>
            `;
            
            for (let i = 0; i < data.di_pullup.length; i++) {
                diConfigHtml += `
                    <tr>
                        <td>DI${i + 1}</td>
                        <td>
                            <label class="switch-label">
                                <input type="checkbox" id="di-pullup-${i}" ${data.di_pullup[i] ? 'checked' : ''}>
                                <span class="switch-text">Pullup</span>
                            </label>
                        </td>
                        <td>
                            <label class="switch-label">
                                <input type="checkbox" id="di-invert-${i}" ${data.di_invert[i] ? 'checked' : ''}>
                                <span class="switch-text">Invert</span>
                            </label>
                        </td>
                        <td>
                            <label class="switch-label" title="When enabled, the input will latch in the ON state until it is read via Modbus or manually reset">
                                <input type="checkbox" id="di-latch-${i}" ${data.di_latch[i] ? 'checked' : ''}>
                                <span class="switch-text">Latch</span>
                            </label>
                        </td>
                    </tr>
                `;
            }
            diConfigHtml += '</tbody></table>';
            diConfigDiv.innerHTML = diConfigHtml;
            
            // Digital Outputs configuration
            const doConfigDiv = document.getElementById('do-config');
            let doConfigHtml = '<table class="io-config-table">';
            doConfigHtml += `
                <thead>
                    <tr>
                        <th>Output</th>
                        <th>Initial</th>
                        <th>Invert</th>
                        <th></th>
                    </tr>
                </thead>
                <tbody>
            `;
            
            for (let i = 0; i < data.do_invert.length; i++) {
                doConfigHtml += `
                    <tr>
                        <td>DO${i + 1}</td>
                        <td>
                            <label class="switch-label">
                                <input type="checkbox" id="do-initial-${i}" ${data.do_initial_state[i] ? 'checked' : ''}>
                                <span class="switch-text">Initial ON</span>
                            </label>
                        </td>
                        <td>
                            <label class="switch-label">
                                <input type="checkbox" id="do-invert-${i}" ${data.do_invert[i] ? 'checked' : ''}>
                                <span class="switch-text">Invert</span>
                            </label>
                        </td>
                        <td></td>
                    </tr>
                `;
            }
            doConfigHtml += '</tbody></table>';
            doConfigDiv.innerHTML = doConfigHtml;
            
            // Analog Input Configuration section removed as analog values are always in mV
            
            // Reset attempts counter on success
            ioConfigLoadAttempts = 0;
        })
        .catch(error => {
            console.error('Error loading IO configuration:', error);
            ioConfigLoadAttempts++;
            
            if (ioConfigLoadAttempts < MAX_IO_CONFIG_LOAD_ATTEMPTS) {
                // Retry after a delay with exponential backoff
                const retryDelay = Math.min(1000 * Math.pow(1.5, ioConfigLoadAttempts), 5000);
                console.log(`Retrying IO config load (attempt ${ioConfigLoadAttempts+1}/${MAX_IO_CONFIG_LOAD_ATTEMPTS}) in ${retryDelay}ms...`);
                setTimeout(loadIOConfig, retryDelay);
            } else {
                // Even after max attempts, try one more time after a longer delay
                console.log("Maximum IO config load attempts reached. Trying one final attempt in 5 seconds...");
                setTimeout(loadIOConfig, 5000);
            }
        });
}

let ioConfigLoadAttempts = 0;
const MAX_IO_CONFIG_LOAD_ATTEMPTS = 5;

function saveIOConfig() {
    // Gather all configuration data
    const ioConfig = {
        di_pullup: [],
        di_invert: [],
        di_latch: [],
        do_invert: [],
        do_initial_state: []
    };
    
    // Get digital input configuration
    for (let i = 0; i < 8; i++) {
        ioConfig.di_pullup.push(document.getElementById(`di-pullup-${i}`).checked);
        ioConfig.di_invert.push(document.getElementById(`di-invert-${i}`).checked);
        ioConfig.di_latch.push(document.getElementById(`di-latch-${i}`).checked);
    }
    
    // Get digital output configuration
    for (let i = 0; i < 8; i++) {
        ioConfig.do_invert.push(document.getElementById(`do-invert-${i}`).checked);
        ioConfig.do_initial_state.push(document.getElementById(`do-initial-${i}`).checked);
    }
    
    // Update the global config state immediately
    ioConfigState = {
        di_pullup: [...ioConfig.di_pullup],
        di_invert: [...ioConfig.di_invert],
        di_latch: [...ioConfig.di_latch],
        do_invert: [...ioConfig.do_invert],
        do_initial_state: [...ioConfig.do_initial_state]
    };
    
    console.log("Saving IO configuration:", ioConfig);
    
    // Show loading toast
    const loadingToast = showToast('Saving IO configuration...', 'info');
    
    // Send configuration to server
    fetch('/ioconfig', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(ioConfig)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        // Remove loading toast
        document.getElementById('toast-container').removeChild(loadingToast);
        
        if (data.success) {
            if (data.changed) {
                showToast('IO Configuration saved successfully!', 'success');
            } else {
                showToast('No changes detected in IO Configuration.', 'success');
            }
        } else {
            showToast('Failed to save IO Configuration. Please try again.', 'error', false, 5000);
        }
        
        // Update IO status immediately to show the new inversion indicators
        updateIOStatus();
    })
    .catch(error => {
        // Remove loading toast if it exists
        if (document.getElementById('toast-container').contains(loadingToast)) {
            document.getElementById('toast-container').removeChild(loadingToast);
        }
        showToast('Error saving IO configuration: ' + error.message, 'error', false, 5000);
    });
}

function saveConfig() {
    // Ensure modbus_port is parsed as a number
    const modbus_port_raw = document.getElementById('modbus_port').value;
    const modbus_port_value = parseInt(modbus_port_raw, 10);
    
    console.log("Modbus port value from form (raw):", modbus_port_raw);
    console.log("Modbus port value from form (parsed):", modbus_port_value);
    
    const config = {
        dhcp: document.getElementById('dhcp').checked,
        ip: document.getElementById('ip').value,
        subnet: document.getElementById('subnet').value,
        gateway: document.getElementById('gateway').value,
        hostname: document.getElementById('hostname').value,
        modbus_port: modbus_port_value
    };
    
    console.log("Config object being sent:", JSON.stringify(config));
    
    // Show loading toast
    const loadingToast = showToast('Saving configuration...', 'info');
    
    fetch('/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(config)
    })
    .then(response => response.json())
    .then(data => {
        // Remove loading toast
        document.getElementById('toast-container').removeChild(loadingToast);
        
        if (data.success) {
            // Update current IP display if it changed
            document.getElementById('current-ip').textContent = data.ip;
            
            // Show countdown toast with reload
            showCountdownToast(
                'Configuration saved successfully! Rebooting now...', 
                'success', 
                5, 
                () => window.location.reload()
            );
        } else {
            showToast('Error: ' + data.error, 'error', false, 5000);
        }
    })
    .catch(error => {
        // Remove loading toast
        if (document.getElementById('toast-container').contains(loadingToast)) {
            document.getElementById('toast-container').removeChild(loadingToast);
        }
        showToast('Failed to save configuration: ' + error.message, 'error', false, 5000);
    });
}

function updateUI() {
    // Update IO status only - removed updateClientStatus() which doesn't exist
    updateIOStatus();
}
setInterval(updateUI, 500);

// Toggle IP fields when DHCP checkbox changes
document.getElementById('dhcp').addEventListener('change', function(e) {
    const disabled = e.target.checked;
    ['ip', 'subnet', 'gateway'].forEach(id => {
        document.getElementById(id).disabled = disabled;
    });
});

// Helper function to format duration in seconds to a readable format
function formatDuration(seconds) {
    if (seconds < 60) {
        return seconds + " seconds";
    } else if (seconds < 3600) {
        const minutes = Math.floor(seconds / 60);
        const remainingSeconds = seconds % 60;
        return minutes + " min " + remainingSeconds + " sec";
    } else {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        return hours + " hr " + minutes + " min";
    }
}

// Device mode detection
let isSimulator = false;
let deviceInfo = {};


function updateDeviceInfoDisplay() {
    console.log("Updating device info display:", deviceInfo);
    
    // Update device info in the UI if elements exist
    const deviceTypeElement = document.getElementById('device-type');
    const firmwareVersionElement = document.getElementById('firmware-version');
    
    if (deviceTypeElement) {
        deviceTypeElement.textContent = deviceInfo.deviceType;
    }
    
    if (firmwareVersionElement) {
        firmwareVersionElement.textContent = deviceInfo.firmwareVersion;
    }
    
    
    // Update sensor data display logic
    updateSensorDataDisplayMode();
}

function updateSensorDataDisplayMode() {
    // This function can be used to adjust UI behavior based on hardware vs simulator
    console.log("Sensor data display mode updated for", deviceInfo.isSimulator ? "simulator" : "hardware");
}

function getSensorDisplayName(sensorKey) {
    const displayNames = {
        'temperature': 'Temperature',
        'humidity': 'Humidity',
        'pressure': 'Pressure',
        'light': 'Light Level',
        'ph': 'pH Level',
        'ec': 'Electrical Conductivity',
        'co2': 'CO2 Level'
    };
    
    return displayNames[sensorKey] || sensorKey.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
}

function getSensorUnit(sensorKey) {
    const units = {
        'temperature': '°C',
        'humidity': '%',
        'pressure': 'hPa',
        'light': 'lux',
        'ph': 'pH',
        'ec': 'µS/cm',
        'co2': 'ppm'
    };
    
    return units[sensorKey] || '';
}


// Wait a short time before loading initial configuration to ensure server is ready
setTimeout(() => {
    console.log("Starting initial data load sequence...");
    
    // First attempt at loading config
    loadConfig();
    
    // Load IO configuration after a short delay
    setTimeout(() => {
        loadIOConfig();
    }, 500);
    
    // Load sensor configuration after another short delay
    setTimeout(() => {
        loadSensorConfig();
    }, 1000);
}, 500);

// Sensor Management Functions
let sensorConfigData = [];
let editingSensorIndex = -1;

// EZO Calibration Management
let currentEzoSensorIndex = -1;

// EZO Sensor calibration configurations
const EZO_CALIBRATION_CONFIG = {
    'EZO_PH': {
        name: 'pH Sensor',
        buttons: [
            { label: 'Mid (7.0)', command: 'Cal,mid,7.00', type: 'primary', description: 'Calibrate mid point (pH 7.0) - Always calibrate this first!' },
            { label: 'Low (4.0)', command: 'Cal,low,4.00', type: 'secondary', description: 'Calibrate low point (pH 4.0)' },
            { label: 'High (10.0)', command: 'Cal,high,10.00', type: 'secondary', description: 'Calibrate high point (pH 10.0)' },
            { label: 'Clear Cal', command: 'Cal,clear', type: 'warning', description: 'Clear all calibration data' },
            { label: 'Check Status', command: 'Cal,?', type: 'info', description: 'Check calibration status' }
        ],
        notes: [
            '• Always calibrate Mid point (pH 7.0) FIRST - this clears previous calibrations',
            '• Rinse probe thoroughly between different calibration solutions',
            '• Allow readings to stabilize before issuing calibration commands',
            '• Use pH buffer solutions at current temperature',
            '• Expected responses: *OK (success), *ER (error), or calibration status'
        ]
    },
    'EZO_EC': {
        name: 'Conductivity Sensor', 
        buttons: [
            { label: 'Dry Cal', command: 'Cal,dry', type: 'primary', description: 'Calibrate dry (probe must be completely dry) - Always do this first!' },
            { label: 'Single Point', command: 'Cal,one,{value}', type: 'secondary', description: 'Single point calibration (enter EC value)', needsValue: true },
            { label: 'Low Point', command: 'Cal,low,{value}', type: 'secondary', description: 'Low point calibration (enter EC value)', needsValue: true },
            { label: 'High Point', command: 'Cal,high,{value}', type: 'secondary', description: 'High point calibration (enter EC value)', needsValue: true },
            { label: 'Clear Cal', command: 'Cal,clear', type: 'warning', description: 'Clear all calibration data' },
            { label: 'Check Status', command: 'Cal,?', type: 'info', description: 'Check calibration status' }
        ],
        notes: [
            '• Always calibrate Dry point FIRST with completely dry probe',
            '• For dual-point calibration: do Dry, then Low, then High',
            '• For single-point: do Dry, then Single Point',
            '• Use certified conductivity standards',
            '• Typical values: Low ~1000 µS/cm, High ~80000 µS/cm',
            '• Expected responses: *OK (success), *ER (error), or calibration status'
        ]
    },
    'EZO_DO': {
        name: 'Dissolved Oxygen Sensor',
        buttons: [
            { label: 'Atmospheric', command: 'Cal,atmospheric', type: 'primary', description: 'Calibrate at atmospheric oxygen levels (probe in air)' },
            { label: 'Zero Point', command: 'Cal,zero', type: 'secondary', description: 'Calibrate zero dissolved oxygen (probe in zero DO solution)' },
            { label: 'Clear Cal', command: 'Cal,clear', type: 'warning', description: 'Clear all calibration data' },
            { label: 'Check Status', command: 'Cal,?', type: 'info', description: 'Check calibration status' }
        ],
        notes: [
            '• For Atmospheric calibration: remove probe cap and expose to air until stable',
            '• Expected reading after atmospheric cal: 9.09-9.1X mg/L',
            '• Zero calibration is optional (only needed for readings under 1 mg/L)',
            '• For zero cal: use zero dissolved oxygen solution, stir to remove air bubbles',
            '• Wait for readings to stabilize before calibrating',
            '• Expected responses: *OK (success), *ER (error), or calibration status'
        ]
    }
};

// Load sensor configuration from the server
function loadSensorConfig() {
    console.log("Loading sensor configuration...");
    fetch('/sensors/config')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            console.log("Sensor configuration loaded:", data);
            sensorConfigData = data.sensors || [];
            renderSensorTable();
            updateRegisterSummary();
        })
        .catch(error => {
            console.error('Error loading sensor configuration:', error);
            showToast('Failed to load sensor configuration: ' + error.message, 'error', false, 5000);
        });
}


// Render the sensor table
function renderSensorTable() {
    const tableBody = document.getElementById('sensor-table-body');
    
    if (sensorConfigData.length === 0) {
        tableBody.innerHTML = '<tr class="no-sensors"><td colspan="7">No sensors configured</td></tr>';
        return;
    }
    
    tableBody.innerHTML = sensorConfigData.map((sensor, index) => {
        const i2cAddress = sensor.type.startsWith('SIM_') ? 
            (sensor.i2cAddress === 0 ? 'Simulated' : `0x${sensor.i2cAddress.toString(16).toUpperCase().padStart(2, '0')}`) :
            (sensor.i2cAddress ? `0x${sensor.i2cAddress.toString(16).toUpperCase().padStart(2, '0')}` : 'N/A');
        const enabledClass = sensor.enabled ? 'sensor-enabled' : 'sensor-disabled';
        const enabledText = sensor.enabled ? 'Yes' : 'No';
        const hasCalibration = sensor.calibration && (sensor.calibration.offset !== undefined || sensor.calibration.scale !== undefined);
        const calibrationText = hasCalibration ? 'Yes' : 'No';
        const calibrationClass = hasCalibration ? 'calibration-enabled' : 'calibration-disabled';
        
        return `
            <tr>
                <td>${sensor.name}</td>
                <td>${sensor.type}</td>
                <td>${i2cAddress}</td>
                <td>${sensor.modbusRegister || 'N/A'}</td>
                <td class="${enabledClass}">${enabledText}</td>
                <td class="${calibrationClass}">${calibrationText}</td>
                <td>
                    <div class="sensor-actions">
                        <button class="edit-btn" onclick="editSensor(${index})" title="Edit sensor">Edit</button>
                        <button class="delete-btn" onclick="deleteSensor(${index})" title="Delete sensor">Delete</button>
                    </div>
                </td>
            </tr>
        `;
    }).join('');
    
    // Update register summary
    updateRegisterSummary();
}

// Update the compact register summary
function updateRegisterSummary() {
    const usedRegisters = sensorConfigData
        .filter(sensor => sensor.modbusRegister !== undefined)
        .map(sensor => sensor.modbusRegister)
        .sort((a, b) => a - b);
    
    const usedRegisterElement = document.getElementById('used-registers');
    if (usedRegisterElement) {
        if (usedRegisters.length === 0) {
            usedRegisterElement.textContent = 'None';
        } else {
            usedRegisterElement.textContent = usedRegisters.join(', ');
        }
    }
    
    // Find next available register (first unused register >= 3)
    const availableRegisterElement = document.getElementById('available-registers');
    if (availableRegisterElement) {
        let nextAvailable = 3;
        while (usedRegisters.includes(nextAvailable)) {
            nextAvailable++;
        }
        availableRegisterElement.textContent = `${nextAvailable}+`;
    }
}

// Show the add sensor modal
function showAddSensorModal() {
    editingSensorIndex = -1;
    document.getElementById('sensor-modal-title').textContent = 'Add Sensor';
    document.getElementById('sensor-modal-overlay').querySelector('button[onclick="saveSensor()"]').textContent = 'Add';
    document.getElementById('sensor-form').reset();
    document.getElementById('sensor-enabled').checked = true;

    // Setup sensor calibration method listeners
    setupSensorCalibrationMethodListeners();

    // Reset calibration to defaults
    document.getElementById('sensor-method-linear').checked = true;
    document.getElementById('sensor-calibration-offset').value = 0;
    document.getElementById('sensor-calibration-scale').value = 1;
    document.getElementById('sensor-calibration-polynomial').value = '';
    document.getElementById('sensor-calibration-expression').value = '';
    showSensorCalibrationMethod('linear');

    // Setup sensor type change listener
    document.getElementById('sensor-type').addEventListener('change', updateSensorFormFields);
    
    // Initialize form fields visibility
    updateSensorFormFields();

    document.getElementById('sensor-modal-overlay').classList.add('show');
}

// Update protocol-specific configuration fields
function updateSensorProtocolFields() {
    const protocolType = document.getElementById('sensor-protocol').value;
    const protocolConfig = document.getElementById('protocol-config');
    
    // Hide all protocol configs first
    const allConfigs = document.querySelectorAll('.protocol-config');
    allConfigs.forEach(config => config.style.display = 'none');
    
    // Show appropriate protocol config
    if (protocolType === 'I2C') {
        protocolConfig.style.display = 'block';
        document.getElementById('i2c-config').style.display = 'block';
        loadAvailablePins('I2C');
    } else if (protocolType === 'UART') {
        protocolConfig.style.display = 'block';
        document.getElementById('uart-config').style.display = 'block';
        loadAvailablePins('UART');
    } else if (protocolType === 'Analog Voltage') {
        protocolConfig.style.display = 'block';
        document.getElementById('analog-config').style.display = 'block';
        loadAvailablePins('Analog Voltage');
    } else if (protocolType === 'One-Wire') {
        protocolConfig.style.display = 'block';
        document.getElementById('onewire-config').style.display = 'block';
        loadAvailablePins('One-Wire');
    } else if (protocolType === 'Digital Counter') {
        protocolConfig.style.display = 'block';
        document.getElementById('digital-config').style.display = 'block';
        loadAvailablePins('Digital Counter');
    } else {
        // No protocol selected
        protocolConfig.style.display = 'none';
    }
    
    // Also update sensor type options based on protocol
    updateSensorTypeOptions();
}

// Load available pins for the selected protocol
function loadAvailablePins(protocol) {
    fetch(`/available-pins?protocol=${encodeURIComponent(protocol)}`)
        .then(response => response.json())
        .then(data => {
            if (protocol === 'I2C') {
                const pinsSelect = document.getElementById('sensor-i2c-pins');
                pinsSelect.innerHTML = '<option value="">Select I2C pins...</option>';
                
                if (data.pinPairs) {
                    data.pinPairs.forEach(pair => {
                        const option = document.createElement('option');
                        option.value = `${pair.sda},${pair.scl}`;
                        option.textContent = pair.label;
                        pinsSelect.appendChild(option);
                    });
                }
            } else if (protocol === 'UART') {
                const pinsSelect = document.getElementById('sensor-uart-pins');
                pinsSelect.innerHTML = '<option value="">Select UART pins...</option>';
                
                if (data.pinPairs) {
                    data.pinPairs.forEach(pair => {
                        const option = document.createElement('option');
                        option.value = `${pair.tx},${pair.rx}`;
                        option.textContent = pair.label;
                        pinsSelect.appendChild(option);
                    });
                }
            } else if (protocol === 'Analog Voltage') {
                const pinsSelect = document.getElementById('sensor-analog-pin');
                pinsSelect.innerHTML = '<option value="">Select analog pin...</option>';
                
                if (data.pins) {
                    data.pins.forEach(pin => {
                        const option = document.createElement('option');
                        option.value = pin.pin;
                        option.textContent = pin.label;
                        pinsSelect.appendChild(option);
                    });
                }
            } else if (protocol === 'One-Wire') {
                const pinsSelect = document.getElementById('sensor-onewire-pin');
                pinsSelect.innerHTML = '<option value="">Select One-Wire pin...</option>';
                
                if (data.pins) {
                    data.pins.forEach(pin => {
                        const option = document.createElement('option');
                        option.value = pin.pin;
                        option.textContent = pin.label;
                        pinsSelect.appendChild(option);
                    });
                }
            } else if (protocol === 'Digital Counter') {
                const pinsSelect = document.getElementById('sensor-digital-pin');
                pinsSelect.innerHTML = '<option value="">Select digital pin...</option>';
                
                if (data.pins) {
                    data.pins.forEach(pin => {
                        const option = document.createElement('option');
                        option.value = pin.pin;
                        option.textContent = pin.label;
                        pinsSelect.appendChild(option);
                    });
                }
            }
        })
        .catch(error => {
            console.error('Error loading available pins:', error);
            showToast('Error loading available pins', 'error');
        });
}

// Update sensor type options based on selected protocol
function updateSensorTypeOptions() {
    const protocolType = document.getElementById('sensor-protocol').value;
    const sensorTypeSelect = document.getElementById('sensor-type');
    
    // Clear current options
    sensorTypeSelect.innerHTML = '<option value="">Select sensor type</option>';
    
    let optgroups = [];
    
    if (protocolType === 'I2C') {
        optgroups.push({
            label: 'Real I2C Sensors',
            options: [
                { value: 'BME280', text: 'BME280 (Temperature, Humidity, Pressure)' },
                { value: 'SHT30', text: 'SHT30 (Temperature, Humidity)' },
                { value: 'EZO_PH', text: 'EZO-pH (pH Sensor)' },
                { value: 'EZO_EC', text: 'EZO-EC (Conductivity)' },
                { value: 'EZO_DO', text: 'EZO-DO (Dissolved Oxygen)' },
                { value: 'EZO_RTD', text: 'EZO-RTD (Temperature)' },
                { value: 'EZO_ORP', text: 'EZO-ORP (Oxidation-Reduction Potential)' },
                { value: 'GENERIC_I2C', text: 'Generic I2C Sensor' }
            ]
        });
        optgroups.push({
            label: 'Simulated I2C Sensors',
            options: [
                { value: 'SIM_I2C_TEMPERATURE', text: 'Simulated I2C Temperature' },
                { value: 'SIM_I2C_HUMIDITY', text: 'Simulated I2C Humidity' },
                { value: 'SIM_I2C_PRESSURE', text: 'Simulated I2C Pressure' }
            ]
        });
    } else if (protocolType === 'UART') {
        optgroups.push({
            label: 'Real UART Sensors',
            options: [
                { value: 'MODBUS_RTU', text: 'Modbus RTU Sensor' },
                { value: 'NMEA_GPS', text: 'NMEA GPS Receiver' },
                { value: 'ASCII_SENSOR', text: 'ASCII Text Sensor' },
                { value: 'BINARY_SENSOR', text: 'Binary Protocol Sensor' },
                { value: 'GENERIC_UART', text: 'Generic UART Device' }
            ]
        });
        optgroups.push({
            label: 'Simulated UART Sensors',
            options: [
                { value: 'SIM_UART_TEMPERATURE', text: 'Simulated UART Temperature' },
                { value: 'SIM_UART_FLOW', text: 'Simulated UART Flow Meter' }
            ]
        });
    } else if (protocolType === 'Analog Voltage') {
        optgroups.push({
            label: 'Real Analog Sensors',
            options: [
                { value: 'ANALOG_4_20MA', text: '4-20mA Current Loop' },
                { value: 'ANALOG_0_10V', text: '0-10V Voltage Input' },
                { value: 'ANALOG_THERMISTOR', text: 'Thermistor Temperature' },
                { value: 'ANALOG_PRESSURE', text: 'Analog Pressure Transducer' },
                { value: 'ANALOG_CUSTOM', text: 'Custom Analog Input' }
            ]
        });
        optgroups.push({
            label: 'Simulated Analog Sensors',
            options: [
                { value: 'SIM_ANALOG_VOLTAGE', text: 'Simulated Analog Voltage' },
                { value: 'SIM_ANALOG_CURRENT', text: 'Simulated Analog Current' }
            ]
        });
    } else if (protocolType === 'One-Wire') {
        optgroups.push({
            label: 'Real One-Wire Sensors',
            options: [
                { value: 'DS18B20', text: 'DS18B20 Temperature Sensor' },
                { value: 'DS18S20', text: 'DS18S20 Temperature Sensor' },
                { value: 'DS1822', text: 'DS1822 Temperature Sensor' },
                { value: 'GENERIC_ONEWIRE', text: 'Generic One-Wire Device' }
            ]
        });
        optgroups.push({
            label: 'Simulated One-Wire Sensors',
            options: [
                { value: 'SIM_ONEWIRE_TEMP', text: 'Simulated One-Wire Temperature' }
            ]
        });
    } else if (protocolType === 'Digital Counter') {
        optgroups.push({
            label: 'Real Digital Sensors',
            options: [
                { value: 'DIGITAL_PULSE', text: 'Digital Pulse Counter' },
                { value: 'DIGITAL_SWITCH', text: 'Digital Switch/Contact' },
                { value: 'DIGITAL_ENCODER', text: 'Digital Encoder' },
                { value: 'DIGITAL_FREQUENCY', text: 'Frequency Counter' },
                { value: 'GENERIC_DIGITAL', text: 'Generic Digital Input' }
            ]
        });
        optgroups.push({
            label: 'Simulated Digital Sensors',
            options: [
                { value: 'SIM_DIGITAL_SWITCH', text: 'Simulated Digital Switch' },
                { value: 'SIM_DIGITAL_COUNTER', text: 'Simulated Digital Counter' }
            ]
        });
    }
    
    // Add optgroups to select
    optgroups.forEach(group => {
        const optgroup = document.createElement('optgroup');
        optgroup.label = group.label;
        
        group.options.forEach(option => {
            const optionElement = document.createElement('option');
            optionElement.value = option.value;
            optionElement.textContent = option.text;
            optgroup.appendChild(optionElement);
        });
        
        sensorTypeSelect.appendChild(optgroup);
    });
}

// Update sensor form fields based on selected sensor type
function updateSensorFormFields() {
    const protocol = document.getElementById('sensor-protocol').value;
    const sensorType = document.getElementById('sensor-type').value;
    const i2cAddressContainer = document.getElementById('sensor-i2c-address').parentElement;
    const i2cAddressField = document.getElementById('sensor-i2c-address');
    
    // Hide I2C address field for non-I2C protocols
    if (protocol === 'I2C') {
        i2cAddressContainer.style.display = 'block';
        i2cAddressField.required = true;
        
        if (sensorType.startsWith('SIM_')) {
            // For simulated I2C sensors, use 0x00 as default
            i2cAddressField.value = '0x00';
            i2cAddressField.required = false;
        } else if (i2cAddressField.value === '0x00' || i2cAddressField.value === '') {
            // Clear default for real I2C sensors
            i2cAddressField.value = '';
        }
    } else {
        // For non-I2C protocols, hide I2C address field
        i2cAddressContainer.style.display = 'none';
        i2cAddressField.value = '0x00'; // Use placeholder for non-I2C
        i2cAddressField.required = false;
    }
    
    // Set default modbus register if not already set
    const modbusRegField = document.getElementById('sensor-modbus-register');
    if (!modbusRegField.value) {
        const nextRegister = getNextAvailableRegister();
        modbusRegField.value = nextRegister;
    }
}

// Helper function to get the next available modbus register
function getNextAvailableRegister() {
    let maxRegister = 2; // System reserves 0-2
    sensorConfigData.forEach(sensor => {
        if (sensor.modbusRegister > maxRegister) {
            maxRegister = sensor.modbusRegister;
        }
    });
    return maxRegister + 1;
}// Show the edit sensor modal
function editSensor(index) {
    if (index < 0 || index >= sensorConfigData.length) {
        showToast('Invalid sensor index', 'error');
        return;
    }

    const sensor = sensorConfigData[index];
    editingSensorIndex = index;

    document.getElementById('sensor-modal-title').textContent = 'Edit Sensor';
    document.getElementById('sensor-modal-overlay').querySelector('button[onclick="saveSensor()"]').textContent = 'Update';
    document.getElementById('sensor-name').value = sensor.name;
    document.getElementById('sensor-protocol').value = sensor.protocol || 'I2C';
    document.getElementById('sensor-type').value = sensor.type;
    
    // Trigger protocol change to update form fields
    updateSensorProtocolFields();
    
    // Load pin assignments if they exist
    if (sensor.protocol === 'I2C' && sensor.sdaPin !== undefined && sensor.sclPin !== undefined) {
        document.getElementById('sensor-i2c-pins').value = `${sensor.sdaPin},${sensor.sclPin}`;
    } else if (sensor.protocol === 'UART' && sensor.txPin !== undefined && sensor.rxPin !== undefined) {
        document.getElementById('sensor-uart-pins').value = `${sensor.txPin},${sensor.rxPin}`;
    } else if (sensor.protocol === 'Analog Voltage' && sensor.analogPin !== undefined) {
        document.getElementById('sensor-analog-pin').value = sensor.analogPin.toString();
    } else if (sensor.protocol === 'One-Wire' && sensor.digitalPin !== undefined) {
        document.getElementById('sensor-onewire-pin').value = sensor.digitalPin.toString();
    } else if (sensor.protocol === 'Digital Counter' && sensor.digitalPin !== undefined) {
        document.getElementById('sensor-digital-pin').value = sensor.digitalPin.toString();
    }
    document.getElementById('sensor-i2c-address').value = sensor.i2cAddress ? `0x${sensor.i2cAddress.toString(16).toUpperCase().padStart(2, '0')}` : '';
    document.getElementById('sensor-modbus-register').value = sensor.modbusRegister || '';
    document.getElementById('sensor-enabled').checked = sensor.enabled;

    // Setup sensor calibration method listeners
    setupSensorCalibrationMethodListeners();
    
    // Setup sensor type change listener
    document.getElementById('sensor-type').addEventListener('change', updateSensorFormFields);
    
    // Update form fields based on sensor type
    updateSensorFormFields();

    // Load calibration data and determine method
    if (sensor.calibration) {
        if (sensor.calibration.expression && sensor.calibration.expression.trim() !== '') {
            // Expression calibration
            document.getElementById('sensor-method-expression').checked = true;
            document.getElementById('sensor-calibration-expression').value = sensor.calibration.expression;
            showSensorCalibrationMethod('expression');
        } else if (sensor.calibration.polynomialStr && sensor.calibration.polynomialStr.trim() !== '') {
            // Polynomial calibration
            document.getElementById('sensor-method-polynomial').checked = true;
            document.getElementById('sensor-calibration-polynomial').value = sensor.calibration.polynomialStr;
            showSensorCalibrationMethod('polynomial');
        } else {
            // Linear calibration
            document.getElementById('sensor-method-linear').checked = true;
            document.getElementById('sensor-calibration-offset').value = sensor.calibration.offset || 0;
            document.getElementById('sensor-calibration-scale').value = sensor.calibration.scale || 1;
            showSensorCalibrationMethod('linear');
        }
    } else {
        // Default to linear calibration
        document.getElementById('sensor-method-linear').checked = true;
        document.getElementById('sensor-calibration-offset').value = 0;
        document.getElementById('sensor-calibration-scale').value = 1;
        document.getElementById('sensor-calibration-polynomial').value = '';
        document.getElementById('sensor-calibration-expression').value = '';
        showSensorCalibrationMethod('linear');
    }

    document.getElementById('sensor-modal-overlay').classList.add('show');
}

// Setup sensor calibration method radio button listeners
function setupSensorCalibrationMethodListeners() {
    const methodRadios = document.querySelectorAll('input[name="sensor-calibration-method"]');
    methodRadios.forEach(radio => {
        radio.addEventListener('change', function() {
            if (this.checked) {
                showSensorCalibrationMethod(this.value);
            }
        });
    });
}

// Show the appropriate sensor calibration method section
function showSensorCalibrationMethod(method) {
    // Hide all sensor calibration sections
    document.getElementById('sensor-linear-calibration').style.display = 'none';
    document.getElementById('sensor-polynomial-calibration').style.display = 'none';
    document.getElementById('sensor-expression-calibration').style.display = 'none';
    
    // Show the selected method
    switch (method) {
        case 'linear':
            document.getElementById('sensor-linear-calibration').style.display = 'block';
            break;
        case 'polynomial':
            document.getElementById('sensor-polynomial-calibration').style.display = 'block';
            break;
        case 'expression':
            document.getElementById('sensor-expression-calibration').style.display = 'block';
            break;
    }
}

// Hide the sensor modal
function hideSensorModal() {
    document.getElementById('sensor-modal-overlay').classList.remove('show');
    editingSensorIndex = -1;
}

// Parse I2C address from input (supports 0x48, 48, etc.)
function parseI2CAddress(addressStr) {
    if (!addressStr) return 0;
    
    const cleanStr = addressStr.trim();
    if (cleanStr.toLowerCase().startsWith('0x')) {
        return parseInt(cleanStr, 16);
    } else {
        return parseInt(cleanStr, 16);
    }
}

// Save sensor (add or update)
function saveSensor() {    
    const form = document.getElementById('sensor-form');
    if (!form.checkValidity()) {
        form.reportValidity();
        return;
    }
    
    const name = document.getElementById('sensor-name').value.trim();
    const protocol = document.getElementById('sensor-protocol').value;
    const type = document.getElementById('sensor-type').value;
    const i2cAddressStr = document.getElementById('sensor-i2c-address').value.trim();
    const modbusRegister = parseInt(document.getElementById('sensor-modbus-register').value);
    const enabled = document.getElementById('sensor-enabled').checked;
    
    // Validate inputs
    if (!name || !protocol || !type || isNaN(modbusRegister)) {
        showToast('Please fill in all required fields', 'error');
        return;
    }
    
    // For I2C sensors, validate I2C address
    let i2cAddress = 0;
    if (protocol === 'I2C') {
        const i2cAddressStr = document.getElementById('sensor-i2c-address').value.trim();
        if (!i2cAddressStr) {
            showToast('I2C address is required for I2C sensors', 'error');
            return;
        }
        
        i2cAddress = parseI2CAddress(i2cAddressStr);
        
        // For simulated sensors, allow address 0x00, for real sensors validate range
        if (type.startsWith('SIM_')) {
            // Simulated sensors can use 0x00 as placeholder
            if (i2cAddress < 0 || i2cAddress > 127) {
                showToast('I2C address must be between 0x00 and 0x7F (0-127) for simulated sensors', 'error');
                return;
            }
        } else {
            // Real sensors need valid I2C addresses (1-127)
            if (i2cAddress < 1 || i2cAddress > 127) {
                showToast('I2C address must be between 0x01 and 0x7F (1-127) for real sensors', 'error');
                return;
            }
        }
    }
    
    // Validate Modbus register range - system reserves 0-2 for built-in I/O
    if (modbusRegister < 3) {
        showToast('Modbus register must be 3 or higher (0-2 are reserved for system I/O)', 'error');
        return;
    }
    
    if (modbusRegister > 65535) {
        showToast('Modbus register must be 65535 or lower', 'error');
        return;
    }
    
    // Validate pin assignments based on protocol
    switch (protocol) {
        case 'I2C':
            const i2cPinPair = document.getElementById('sensor-i2c-pins').value;
            if (!i2cPinPair) {
                showToast('Please select I2C pin pair', 'error');
                return;
            }
            break;
            
        case 'UART':
            const uartPinPair = document.getElementById('sensor-uart-pins').value;
            if (!uartPinPair) {
                showToast('Please select UART pin pair', 'error');
                return;
            }
            break;
            
        case 'Analog Voltage':
            const analogPin = document.getElementById('sensor-analog-pin').value;
            if (!analogPin) {
                showToast('Please select analog pin', 'error');
                return;
            }
            break;
            
        case 'One-Wire':
            const oneWirePin = document.getElementById('sensor-onewire-pin').value;
            if (!oneWirePin) {
                showToast('Please select one-wire pin', 'error');
                return;
            }
            break;
            
        case 'Digital Counter':
            const digitalPin = document.getElementById('sensor-digital-pin').value;
            if (!digitalPin) {
                showToast('Please select digital pin', 'error');
                return;
            }
            break;
    }
    
    // Check for duplicate I2C addresses (excluding the sensor being edited)
    // Only check for I2C protocol sensors and skip simulated sensors using 0x00
    if (protocol === 'I2C' && !(type.startsWith('SIM_') && i2cAddress === 0)) {
        const duplicateI2C = sensorConfigData.some((sensor, index) => 
            index !== editingSensorIndex && 
            sensor.protocol === 'I2C' && 
            sensor.i2cAddress === i2cAddress
        );
        if (duplicateI2C) {
            showToast('I2C address already in use by another I2C sensor', 'error');
            return;
        }
    }
    
    // Check for duplicate Modbus registers (excluding the sensor being edited)
    const duplicateModbus = sensorConfigData.some((sensor, index) => 
        index !== editingSensorIndex && sensor.modbusRegister === modbusRegister
    );
    if (duplicateModbus) {
        showToast('Modbus register already in use by another sensor', 'error');
        return;
    }
    
    // Get calibration data based on selected method
    const selectedCalibrationMethod = document.querySelector('input[name="sensor-calibration-method"]:checked').value;
    let calibration = {};
    
    try {
        switch (selectedCalibrationMethod) {
            case 'linear':
                const offset = parseFloat(document.getElementById('sensor-calibration-offset').value) || 0;
                const scale = parseFloat(document.getElementById('sensor-calibration-scale').value) || 1;
                
                if (scale === 0) {
                    showToast('Scale factor cannot be zero', 'error');
                    return;
                }
                
                calibration = {
                    offset: offset,
                    scale: scale,
                    expression: '',
                    polynomialStr: ''
                };
                break;
                
            case 'polynomial':
                const polynomial = document.getElementById('sensor-calibration-polynomial').value.trim();
                const polyValidation = validatePolynomial(polynomial);
                
                if (!polyValidation.valid) {
                    showToast(polyValidation.error, 'error');
                    return;
                }
                
                calibration = {
                    polynomialStr: polynomial,
                    offset: 0,
                    scale: 1,
                    expression: ''
                };
                break;
                
            case 'expression':
                const expression = document.getElementById('sensor-calibration-expression').value.trim();
                const exprValidation = validateExpression(expression);
                
                if (!exprValidation.valid) {
                    showToast(exprValidation.error, 'error');
                    return;
                }
                
                calibration = {
                    expression: expression,
                    offset: 0,
                    scale: 1,
                    polynomialStr: ''
                };
                break;
                
            default:
                showToast('Invalid calibration method selected', 'error');
                return;
        }
        
    } catch (error) {
        console.error('Error processing calibration:', error);
        showToast('Error processing calibration: ' + error.message, 'error');
        return;
    }
    
    // Capture pin assignments based on protocol
    let pinAssignments = {};
    
    switch (protocol) {
        case 'I2C':
            const i2cPinPair = document.getElementById('sensor-i2c-pins').value;
            if (i2cPinPair) {
                const [sdaPin, sclPin] = i2cPinPair.split(',').map(p => parseInt(p));
                pinAssignments.sdaPin = sdaPin;
                pinAssignments.sclPin = sclPin;
            }
            break;
            
        case 'UART':
            const uartPinPair = document.getElementById('sensor-uart-pins').value;
            if (uartPinPair) {
                const [txPin, rxPin] = uartPinPair.split(',').map(p => parseInt(p));
                pinAssignments.txPin = txPin;
                pinAssignments.rxPin = rxPin;
            }
            break;
            
        case 'Analog Voltage':
            const analogPin = document.getElementById('sensor-analog-pin').value;
            if (analogPin) {
                pinAssignments.analogPin = parseInt(analogPin);
            }
            break;
            
        case 'One-Wire':
            const oneWirePin = document.getElementById('sensor-onewire-pin').value;
            if (oneWirePin) {
                pinAssignments.digitalPin = parseInt(oneWirePin);
            }
            break;
            
        case 'Digital Counter':
            const digitalPin = document.getElementById('sensor-digital-pin').value;
            if (digitalPin) {
                pinAssignments.digitalPin = parseInt(digitalPin);
            }
            break;
    }

    const sensor = {
        enabled: enabled,
        name: name,
        protocol: protocol,
        type: type,
        i2cAddress: i2cAddress,
        modbusRegister: modbusRegister,
        calibration: calibration,
        ...pinAssignments
    };
    
    if (editingSensorIndex === -1) {
        // Adding new sensor
        if (sensorConfigData.length >= 10) { // MAX_SENSORS from backend
            showToast('Maximum number of sensors (10) reached', 'error');
            return;
        }
        sensorConfigData.push(sensor);
        console.log('Sensor added to array:', sensor);
        console.log('Current sensor array:', sensorConfigData);
        showToast('Sensor added successfully', 'success');
    } else {
        // Updating existing sensor
        sensorConfigData[editingSensorIndex] = sensor;
        console.log('Sensor updated:', sensor);
        showToast('Sensor updated successfully', 'success');
    }
    
    console.log('About to render table...');
    renderSensorTable();
    console.log('Table rendered, hiding modal...');
    hideSensorModal();
}

// Show calibration modal for a specific sensor
function calibrateSensor(index) {
    if (index < 0 || index >= sensorConfigData.length) {
        showToast('Invalid sensor index', 'error');
        return;
    }
    
    const sensor = sensorConfigData[index];
    editingSensorIndex = index;
    
    document.getElementById('calibration-modal-title').textContent = `Calibrate ${sensor.name}`;
    document.getElementById('calibration-sensor-name').textContent = sensor.name;
    document.getElementById('calibration-sensor-type').textContent = sensor.type;
    
    // Setup calibration method listeners
    setupCalibrationMethodListeners();
    
    // Load current calibration values and determine method
    if (sensor.calibration) {
        if (sensor.calibration.expression && sensor.calibration.expression.trim() !== '') {
            // Expression calibration
            document.getElementById('method-expression').checked = true;
            document.getElementById('calibration-expression').value = sensor.calibration.expression;
            showCalibrationMethod('expression');
        } else if (sensor.calibration.polynomialStr && sensor.calibration.polynomialStr.trim() !== '') {
            // Polynomial calibration
            document.getElementById('method-polynomial').checked = true;
            document.getElementById('calibration-polynomial').value = sensor.calibration.polynomialStr;
            showCalibrationMethod('polynomial');
        } else {
            // Linear calibration
            document.getElementById('method-linear').checked = true;
            document.getElementById('calibration-offset').value = sensor.calibration.offset || 0;
            document.getElementById('calibration-scale').value = sensor.calibration.scale || 1;
            showCalibrationMethod('linear');
        }
    } else {
        // Default to linear calibration
        document.getElementById('method-linear').checked = true;
        document.getElementById('calibration-offset').value = 0;
        document.getElementById('calibration-scale').value = 1;
        document.getElementById('calibration-polynomial').value = '';
        document.getElementById('calibration-expression').value = '';
        showCalibrationMethod('linear');
    }
    
    document.getElementById('calibration-modal-overlay').classList.add('show');
}

// Setup calibration method radio button listeners
function setupCalibrationMethodListeners() {
    const methodRadios = document.querySelectorAll('input[name="calibration-method"]');
    methodRadios.forEach(radio => {
        radio.addEventListener('change', function() {
            if (this.checked) {
                showCalibrationMethod(this.value);
            }
        });
    });
}

// Show the appropriate calibration method section
function showCalibrationMethod(method) {
    // Hide all calibration sections
    document.getElementById('linear-calibration').style.display = 'none';
    document.getElementById('polynomial-calibration').style.display = 'none';
    document.getElementById('expression-calibration').style.display = 'none';
    
    // Show the selected method
    switch (method) {
        case 'linear':
            document.getElementById('linear-calibration').style.display = 'block';
            break;
        case 'polynomial':
            document.getElementById('polynomial-calibration').style.display = 'block';
            break;
        case 'expression':
            document.getElementById('expression-calibration').style.display = 'block';
            break;
    }
}

// Preset tab management
function showPresetTab(tabName) {
    // Update tab buttons
    document.querySelectorAll('.preset-tab').forEach(tab => {
        tab.classList.remove('active');
    });
    event.target.classList.add('active');
    
    // Show corresponding preset content
    document.querySelectorAll('.preset-content').forEach(content => {
        content.style.display = 'none';
    });
    document.getElementById(tabName + '-presets').style.display = 'block';
}

// Preset functions
function setLinearPreset(offset, scale) {
    document.getElementById('method-linear').checked = true;
    document.getElementById('calibration-offset').value = offset;
    document.getElementById('calibration-scale').value = scale;
    showCalibrationMethod('linear');
}

function setPolynomialPreset(polynomial) {
    document.getElementById('method-polynomial').checked = true;
    document.getElementById('calibration-polynomial').value = polynomial;
    showCalibrationMethod('polynomial');
}

function setExpressionPreset(expression) {
    document.getElementById('method-expression').checked = true;
    document.getElementById('calibration-expression').value = expression;
    showCalibrationMethod('expression');
}

// Client-side validation for expressions and polynomials
function validateExpression(expression) {
    if (!expression || expression.trim() === '') {
        return { valid: false, error: 'Expression cannot be empty' };
    }
    
    // Check for allowed characters only
    const allowedChars = /^[0-9+\-*/.()x\s]+$/;
    if (!allowedChars.test(expression)) {
        return { valid: false, error: 'Expression contains invalid characters. Only +, -, *, /, (), x, and numbers are allowed.' };
    }
    
    // Check for balanced parentheses
    let openCount = 0;
    let closeCount = 0;
    for (let char of expression) {
        if (char === '(') openCount++;
        if (char === ')') closeCount++;
    }
    if (openCount !== closeCount) {
        return { valid: false, error: 'Unbalanced parentheses in expression' };
    }
    
    // Check for dangerous patterns
    if (expression.includes('..') || expression.includes('//')) {
        return { valid: false, error: 'Invalid pattern in expression' };
    }
    
    return { valid: true };
}

function validatePolynomial(polynomial) {
    if (!polynomial || polynomial.trim() === '') {
        return { valid: false, error: 'Polynomial cannot be empty' };
    }
    
    // Basic validation for polynomial format
    const polynomialPattern = /^[+\-]?\s*\d*\.?\d*\s*\*?\s*x?(\^\d+)?\s*([+\-]\s*\d*\.?\d*\s*\*?\s*x?(\^\d+)?\s*)*$/;
    if (!polynomialPattern.test(polynomial.replace(/\s/g, ''))) {
        return { valid: false, error: 'Invalid polynomial format. Use format like: 2x^2 + 3x + 1' };
    }
    
    return { valid: true };
}

// Hide calibration modal
function hideCalibrationModal() {
    document.getElementById('calibration-modal-overlay').classList.remove('show');
    document.getElementById('calibration-modal-overlay').style.display = 'none';
    editingSensorIndex = -1;
    window.currentCalibrationSensor = null;
}

// Save calibration data
function saveCalibration() {
    // Check if we're using the new data flow interface
    if (window.currentCalibraSensor) {
        return saveDataFlowCalibration();
    }
    
    // Legacy sensor config mode
    if (editingSensorIndex < 0 || editingSensorIndex >= sensorConfigData.length) {
        showToast('Invalid sensor selected', 'error');
        return;
    }
    
    // Get selected calibration method
    const selectedMethod = document.querySelector('input[name="calibration-method"]:checked').value;
    
    // Initialize calibration object
    const calibration = {};
    
    try {
        switch (selectedMethod) {
            case 'linear':
                const offset = parseFloat(document.getElementById('calibration-offset').value) || 0;
                const scale = parseFloat(document.getElementById('calibration-scale').value) || 1;
                
                if (scale === 0) {
                    showToast('Scale factor cannot be zero', 'error');
                    return;
                }
                
                calibration.offset = offset;
                calibration.scale = scale;
                // Clear other calibration methods
                calibration.expression = '';
                calibration.polynomialStr = '';
                break;
                
            case 'polynomial':
                const polynomial = document.getElementById('calibration-polynomial').value.trim();
                const polyValidation = validatePolynomial(polynomial);
                
                if (!polyValidation.valid) {
                    showToast(polyValidation.error, 'error');
                    return;
                }
                
                calibration.polynomialStr = polynomial;
                // Clear other calibration methods
                calibration.offset = 0;
                calibration.scale = 1;
                calibration.expression = '';
                break;
                
            case 'expression':
                const expression = document.getElementById('calibration-expression').value.trim();
                const exprValidation = validateExpression(expression);
                
                if (!exprValidation.valid) {
                    showToast(exprValidation.error, 'error');
                    return;
                }
                
                calibration.expression = expression;
                // Clear other calibration methods
                calibration.offset = 0;
                calibration.scale = 1;
                calibration.polynomialStr = '';
                break;
                
            default:
                showToast('Invalid calibration method selected', 'error');
                return;
        }
        
        // Update sensor calibration
        sensorConfigData[editingSensorIndex].calibration = calibration;
        
        // Update UI
        renderSensorTable();
        hideCalibrationModal();
        
        showToast(`Calibration updated successfully (${selectedMethod})`, 'success');
        
    } catch (error) {
        console.error('Error saving calibration:', error);
        showToast('Error saving calibration: ' + error.message, 'error');
    }
}

// Reset calibration to defaults
function resetCalibration() {
    // Reset linear calibration
    document.getElementById('calibration-offset').value = 0;
    document.getElementById('calibration-scale').value = 1;
    
    // Clear polynomial and expression
    document.getElementById('calibration-polynomial').value = '';
    document.getElementById('calibration-expression').value = '';
    
    // Set to linear method
    document.getElementById('method-linear').checked = true;
    showCalibrationMethod('linear');
}

// Delete sensor
function deleteSensor(index) {
    if (index < 0 || index >= sensorConfigData.length) {
        showToast('Invalid sensor index', 'error');
        return;
    }
    
    const sensor = sensorConfigData[index];
    if (confirm(`Are you sure you want to delete sensor "${sensor.name}"?`)) {
        sensorConfigData.splice(index, 1);
        renderSensorTable();
        showToast('Sensor deleted successfully', 'success');
    }
}

// Save sensor configuration to the device
function saveSensorConfig() {
    console.log("Saving sensor configuration:", sensorConfigData);
    
    // Show loading toast
    const loadingToast = showToast('Saving sensor configuration...', 'info');
    
    const configData = {
        sensors: sensorConfigData
    };
    
    fetch('/sensors/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(configData)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        // Remove loading toast
        document.getElementById('toast-container').removeChild(loadingToast);
        
        if (data.success) {
            // Show success message and immediately refresh sensor data
            showToast('Sensor configuration saved successfully!', 'success');
            
            // Close the sensor config modal
            const modal = document.getElementById('sensor-config-modal');
            if (modal) {
                modal.style.display = 'none';
            }
            
            // Immediately refresh the Status IO to show new sensors
            setTimeout(() => {
                updateIOStatus();
                loadSensorConfig(); // Refresh the sensor config list too
            }, 500); // Quick delay to ensure backend has processed
        } else {
            showToast('Failed to save sensor configuration: ' + (data.message || 'Unknown error'), 'error', false, 5000);
        }
    })
    .catch(error => {
        // Remove loading toast if it exists
        if (document.getElementById('toast-container').contains(loadingToast)) {
            document.getElementById('toast-container').removeChild(loadingToast);
        }
        
        // Show actual error since we're not rebooting anymore
        showToast('Error saving sensor configuration: ' + error.message, 'error', false, 5000);
    });
}

// EZO Calibration Functions

function showEzoCalibrationModal(index) {
    if (index < 0 || index >= sensorConfigData.length) {
        showToast('Invalid sensor index', 'error');
        return;
    }
    
    const sensor = sensorConfigData[index];
    if (!sensor.type.startsWith('EZO_')) {
        showToast('This sensor is not an EZO sensor', 'error');
        return;
    }
    
    currentEzoSensorIndex = index;
    
    // Update modal title and sensor info
    document.getElementById('ezo-calibration-modal-title').textContent = `EZO Calibration - ${sensor.name}`;
    document.getElementById('ezo-calibration-sensor-name').textContent = sensor.name;
    document.getElementById('ezo-calibration-sensor-type').textContent = sensor.type;
    document.getElementById('ezo-calibration-sensor-address').textContent = `0x${sensor.i2cAddress.toString(16).toUpperCase().padStart(2, '0')}`;
    
    // Clear previous response
    document.getElementById('ezo-sensor-response').textContent = 'No response yet';
    
    // Get calibration config for this sensor type
    const calibrationConfig = EZO_CALIBRATION_CONFIG[sensor.type];
    if (!calibrationConfig) {
        showToast(`Calibration not supported for sensor type: ${sensor.type}`, 'error');
        return;
    }
    
    // Populate calibration buttons
    const buttonsContainer = document.getElementById('ezo-calibration-buttons');
    buttonsContainer.innerHTML = calibrationConfig.buttons.map(button => {
        const buttonClass = `calibration-btn ${button.type}`;
        const onclick = button.needsValue ? 
            `sendEzoCalibrationCommandWithValue('${button.command}', '${button.label}')` :
            `sendEzoCalibrationCommand('${button.command}')`;
        
        return `<button class="${buttonClass}" onclick="${onclick}" title="${button.description}">
            ${button.label}
        </button>`;
    }).join('');
    
    // Populate calibration notes
    const notesContainer = document.getElementById('ezo-calibration-notes');
    notesContainer.innerHTML = '<ul>' + calibrationConfig.notes.map(note => `<li>${note}</li>`).join('') + '</ul>';
    
    // Show the modal
    document.getElementById('ezo-calibration-modal-overlay').classList.add('show');
}

function hideEzoCalibrationModal() {
    document.getElementById('ezo-calibration-modal-overlay').classList.remove('show');
    currentEzoSensorIndex = -1;
}

function sendEzoCalibrationCommand(command) {
    if (currentEzoSensorIndex < 0) {
        showToast('No sensor selected for calibration', 'error');
        return;
    }
    
    const sensor = sensorConfigData[currentEzoSensorIndex];
    console.log(`Sending EZO calibration command '${command}' to sensor ${sensor.name}`);
    
    fetch('/api/sensor/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 
            sensorIndex: currentEzoSensorIndex, 
            command: command 
        })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        return response.json();
    })
    .then(data => {
        if (data.success) {
            showToast(`Command '${command}' sent successfully`, 'success');
            // Clear the response display and show "Waiting..."
            document.getElementById('ezo-sensor-response').textContent = 'Waiting for response...';
        } else {
            showToast(`Failed to send command: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        showToast(`Error sending command: ${error.message}`, 'error');
    });
}

function sendEzoCalibrationCommandWithValue(commandTemplate, buttonLabel) {
    const value = prompt(`Enter the value for ${buttonLabel}:`);
    if (value === null) return; // User cancelled
    
    if (!value.trim()) {
        showToast('Please enter a valid value', 'error');
        return;
    }
    
    // Replace {value} placeholder with the entered value
    const command = commandTemplate.replace('{value}', value.trim());
    sendEzoCalibrationCommand(command);
}

// ==================== TERMINAL FUNCTIONALITY ====================

let watchInterval = null;
let isWatching = false;

// Update the terminal interface based on selected protocol
function updateTerminalInterface() {
    const protocol = document.getElementById('terminal-protocol').value;
    const pinSelector = document.getElementById('terminal-pin');
    const i2cAddressGroup = document.getElementById('i2c-address-group');
    const commandInput = document.getElementById('terminal-command');
    
    // Clear current options
    pinSelector.innerHTML = '<option value="">Select pin/port...</option>';
    
    // Hide/show I2C address field
    i2cAddressGroup.style.display = protocol === 'i2c' ? 'block' : 'none';
    
    // Populate pin/port options based on protocol and configured sensors
    switch (protocol) {
        case 'digital':
            for (let i = 0; i < 8; i++) {
                pinSelector.innerHTML += `<option value="DI${i}">DI${i} - Digital Input ${i}</option>`;
                pinSelector.innerHTML += `<option value="DO${i}">DO${i} - Digital Output ${i}</option>`;
            }
            commandInput.placeholder = 'Commands: read, write 1/0, config pullup/invert/latch';
            break;
        
        case 'analog':
            for (let i = 0; i < 3; i++) {
                pinSelector.innerHTML += `<option value="AI${i}">AI${i} - Analog Input ${i}</option>`;
            }
            commandInput.placeholder = 'Commands: read, calibrate, range, simulate';
            break;
            
        case 'i2c':
            // Show only pins with I2C sensors configured
            if (sensorConfigData && sensorConfigData.length > 0) {
                sensorConfigData.forEach((sensor, index) => {
                    if (sensor.protocol === 'I2C' && sensor.enabled) {
                        pinSelector.innerHTML += `<option value="${sensor.sdaPin || 24}">${sensor.name} (SDA:${sensor.sdaPin || 24}, SCL:${sensor.sclPin || 25})</option>`;
                    }
                });
            }
            if (pinSelector.children.length === 1) {
                pinSelector.innerHTML += '<option value="24">No I2C sensors configured</option>';
            }
            commandInput.placeholder = 'Commands: scan, read, write, detect, status';
            break;
            
        case 'uart':
            // Show only pins with UART sensors configured
            if (sensorConfigData && sensorConfigData.length > 0) {
                sensorConfigData.forEach((sensor, index) => {
                    if (sensor.protocol === 'UART' && sensor.enabled) {
                        pinSelector.innerHTML += `<option value="${sensor.txPin || 0}">${sensor.name} (TX:${sensor.txPin || 0}, RX:${sensor.rxPin || 1})</option>`;
                    }
                });
            }
            if (pinSelector.children.length === 1) {
                pinSelector.innerHTML += '<option value="0">No UART sensors configured</option>';
            }
            commandInput.placeholder = 'Commands: init, send, read, baudrate, loopback, status';
            break;
            
        case 'onewire':
            // Show only pins with One-Wire sensors configured
            if (sensorConfigData && sensorConfigData.length > 0) {
                sensorConfigData.forEach((sensor, index) => {
                    if (sensor.protocol === 'One-Wire' && sensor.enabled) {
                        pinSelector.innerHTML += `<option value="${sensor.digitalPin || 0}">${sensor.name} (Pin ${sensor.digitalPin || 0})</option>`;
                    }
                });
            }
            if (pinSelector.children.length === 1) {
                pinSelector.innerHTML += '<option value="0">No One-Wire sensors configured</option>';
            }
            commandInput.placeholder = 'Commands: scan, read, search, power, reset';
            break;
            
        case 'analog':
            for (let i = 0; i < 3; i++) {
                pinSelector.innerHTML += `<option value="AI${i}">AI${i} - Analog Input ${i} (Pin ${26 + i})</option>`;
            }
            commandInput.placeholder = 'Commands: read, config';
            break;
            
        case 'i2c':
            pinSelector.innerHTML += '<option value="I2C0">I2C0 - SDA: Pin 24, SCL: Pin 25</option>';
            commandInput.placeholder = 'Commands: scan, read [reg], write [reg] [data], probe';
            break;
            
        case 'uart':
            pinSelector.innerHTML += '<option value="UART0">UART0 - TX: Pin 0, RX: Pin 1</option>';
            pinSelector.innerHTML += '<option value="UART1">UART1 - TX: Pin 4, RX: Pin 5</option>';
            commandInput.placeholder = 'Commands: send [data], config [baud], read';
            break;
            
        case 'network':
            pinSelector.innerHTML += '<option value="ETH">Ethernet Interface - W5500</option>';
            pinSelector.innerHTML += '<option value="ETH_PHY">Ethernet PHY Status</option>';
            pinSelector.innerHTML += '<option value="ETH_PINS">Ethernet Pins (MISO:16, CS:17, SCK:18, MOSI:19, RST:20, IRQ:21)</option>';
            pinSelector.innerHTML += '<option value="MODBUS">Modbus TCP Server</option>';
            pinSelector.innerHTML += '<option value="MODBUS_CLIENTS">Connected Modbus Clients</option>';
            commandInput.placeholder = 'Commands: status, clients, reset, config, link, stats';
            break;
            
        case 'system':
            pinSelector.innerHTML += '<option value="CPU">CPU Status</option>';
            pinSelector.innerHTML += '<option value="MEMORY">Memory Usage</option>';
            pinSelector.innerHTML += '<option value="SENSORS">All Sensors</option>';
            commandInput.placeholder = 'Commands: status, reset, info, sensors';
            break;
    }
}

// Handle Enter key in terminal command input
function handleTerminalKeypress(event) {
    if (event.key === 'Enter') {
        sendTerminalCommand();
    }
}

// Send a terminal command
function sendTerminalCommand() {
    const protocol = document.getElementById('terminal-protocol').value;
    const pin = document.getElementById('terminal-pin').value;
    const command = document.getElementById('terminal-command').value.trim();
    const i2cAddress = document.getElementById('terminal-i2c-address').value;
    
    if (!command) {
        addTerminalOutput('Error: No command entered', 'error');
        return;
    }
    
    if (!pin && protocol !== 'system') {
        addTerminalOutput('Error: No pin/port selected', 'error');
        return;
    }
    
    // Check for I2C address - but not required for scan command
    if (protocol === 'i2c' && !i2cAddress && command.toLowerCase() !== 'scan') {
        addTerminalOutput('Error: I2C address required (except for scan command)', 'error');
        return;
    }
    
    // Display the command in terminal
    addTerminalOutput(`> ${command}`, 'command');
    
    // Handle special commands locally
    if (command.toLowerCase() === 'help') {
        showTerminalHelp();
        return;
    }
    
    if (command.toLowerCase() === 'clear') {
        clearTerminalOutput();
        return;
    }
    
    // Send command to backend
    const payload = {
        protocol: protocol,
        pin: pin,
        command: command,
        i2cAddress: i2cAddress
    };
    
    fetch('/terminal/command', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(payload)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            addTerminalOutput(data.response || 'Command executed successfully', 'success');
        } else {
            addTerminalOutput(data.error || 'Command failed', 'error');
        }
    })
    .catch(error => {
        addTerminalOutput(`Network error: ${error.message}`, 'error');
    });
    
    // Clear command input
    document.getElementById('terminal-command').value = '';
}

// Toggle watch mode
function toggleWatch() {
    const watchBtn = document.getElementById('watch-btn');
    const watchStatus = document.getElementById('watch-status');
    const protocol = document.getElementById('terminal-protocol').value;
    const pin = document.getElementById('terminal-pin').value;
    
    if (!pin && protocol !== 'system') {
        addTerminalOutput('Error: No pin/port selected for watching', 'error');
        return;
    }
    
    if (isWatching) {
        // Stop watching
        clearInterval(watchInterval);
        isWatching = false;
        watchBtn.textContent = 'Start Watch';
        watchBtn.classList.remove('watching');
        watchStatus.textContent = '';
        watchStatus.classList.remove('watching');
        addTerminalOutput('Watch mode stopped', 'success');
    } else {
        // Start watching
        isWatching = true;
        watchBtn.textContent = 'Stop Watch';
        watchBtn.classList.add('watching');
        watchStatus.textContent = `Watching ${pin}`;
        watchStatus.classList.add('watching');
        addTerminalOutput(`Started watching ${pin}`, 'success');
        
        // Start periodic updates
        watchInterval = setInterval(() => {
            const payload = {
                protocol: protocol,
                pin: pin,
                command: 'read',
                i2cAddress: document.getElementById('terminal-i2c-address').value
            };
            
            fetch('/terminal/watch', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(payload)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    const timestamp = new Date().toLocaleTimeString();
                    addTerminalOutput(`[${timestamp}] ${data.response}`, 'response');
                }
            })
            .catch(error => {
                // Silently handle watch errors to avoid spam
                console.error('Watch error:', error);
            });
        }, 1000); // Update every second
    }
}

// Add output to terminal
function addTerminalOutput(text, type = 'response') {
    const terminal = document.getElementById('terminal-content');
    const div = document.createElement('div');
    div.className = `terminal-${type}`;
    div.textContent = text;
    
    terminal.appendChild(div);
    terminal.scrollTop = terminal.scrollHeight;
}

// Clear terminal output
function clearTerminalOutput() {
    const terminal = document.getElementById('terminal-content');
    terminal.innerHTML = `
        <div class="terminal-welcome">
            Modbus IO Terminal - Ready<br>
            Type 'help' for available commands<br>
            > 
        </div>
    `;
}

// Show terminal help
function showTerminalHelp() {
    const helpText = `
Available Commands:

DIGITAL I/O:
  read           - Read current pin state
  write 1/0      - Set output pin state
  config pullup  - Enable/disable pullup
  config invert  - Enable/disable inversion
  config latch   - Enable/disable latching

ANALOG I/O:
  read           - Read analog value (mV)
  config         - Show pin configuration

I2C:
  scan           - Scan for I2C devices
  read [reg]     - Read from register
  write [reg] [data] - Write to register
  probe          - Test device presence

UART:
  send [data]    - Send data
  read           - Read available data
  config [baud]  - Set baud rate

NETWORK:
  status         - Show connection status
  clients        - List connected clients
  reset          - Reset connections

SYSTEM:
  status         - System information
  sensors        - List all sensors
  reset          - Restart system
  info           - Hardware details

General:
  help           - Show this help
  clear          - Clear terminal
    `;
    
    addTerminalOutput(helpText, 'success');
}

// Initialize terminal on page load
document.addEventListener('DOMContentLoaded', function() {
    updateTerminalInterface();
    
    // Add event listeners for byte extraction controls
    const startByteInput = document.getElementById('data-start-byte');
    const lengthInput = document.getElementById('data-length');
    const formatSelect = document.getElementById('data-format');
    
    if (startByteInput) {
        startByteInput.addEventListener('input', function() {
            if (window.currentCalibrationSensor && window.currentCalibrationSensor.raw_i2c_data) {
                updateByteVisualization(window.currentCalibrationSensor.raw_i2c_data);
            }
        });
    }
    
    if (lengthInput) {
        lengthInput.addEventListener('input', function() {
            if (window.currentCalibrationSensor && window.currentCalibrationSensor.raw_i2c_data) {
                updateByteVisualization(window.currentCalibrationSensor.raw_i2c_data);
            }
        });
    }
    
    if (formatSelect) {
        formatSelect.addEventListener('change', function() {
            if (window.currentCalibrationSensor && window.currentCalibrationSensor.raw_i2c_data) {
                updateByteVisualization(window.currentCalibrationSensor.raw_i2c_data);
            }
        });
    }
});

// Data Flow Interface Functions

// Show calibration modal for a specific sensor
function showCalibrationModal(sensorName) {
    console.log('Opening calibration modal for sensor:', sensorName);
    
    // Find the sensor in the current iostatus data
    if (!currentIOStatus || !currentIOStatus.configured_sensors) {
        console.error('No IO status data available');
        return;
    }
    
    const sensor = currentIOStatus.configured_sensors.find(s => s.name === sensorName);
    if (!sensor) {
        console.error('Sensor not found:', sensorName);
        return;
    }
    
    // Set up the modal with sensor info
    document.getElementById('calibration-sensor-name').textContent = sensor.name;
    document.getElementById('calibration-sensor-type').textContent = sensor.type;
    document.getElementById('calibration-sensor-address').textContent = sensor.i2c_address ? `0x${sensor.i2c_address.toString(16).toUpperCase()}` : 'N/A';
    
    // Display current raw data and value
    document.getElementById('calibration-raw-data').textContent = sensor.raw_i2c_data || sensor.raw_value || 'No data';
    document.getElementById('calibration-current-value').textContent = sensor.current_value || 'No data';
    
    // Set up byte extraction for I2C sensors
    const byteExtractionSection = document.getElementById('byte-extraction-section');
    if (sensor.protocol === 'I2C' && sensor.raw_i2c_data) {
        byteExtractionSection.style.display = 'block';
        
        // Load current I2C parsing settings if available
        if (sensor.i2c_parsing) {
            document.getElementById('data-start-byte').value = sensor.i2c_parsing.data_offset || 0;
            document.getElementById('data-length').value = sensor.i2c_parsing.data_length || 2;
            document.getElementById('data-format').value = sensor.i2c_parsing.data_format || 'uint16_le';
        }
        
        // Update the byte visualization
        updateByteVisualization(sensor.raw_i2c_data);
    } else {
        byteExtractionSection.style.display = 'none';
    }
    
    // Load current calibration settings
    if (sensor.calibration) {
        if (sensor.calibration.method) {
            const methodRadio = document.querySelector(`input[name="calibration-method"][value="${sensor.calibration.method}"]`);
            if (methodRadio) {
                methodRadio.checked = true;
                showCalibrationMethod(sensor.calibration.method);
            }
        }
        
        // Set values based on calibration method
        if (sensor.calibration.offset !== undefined) {
            document.getElementById('calibration-offset').value = sensor.calibration.offset;
        }
        if (sensor.calibration.scale !== undefined) {
            document.getElementById('calibration-scale').value = sensor.calibration.scale;
        }
        if (sensor.calibration.polynomial !== undefined) {
            document.getElementById('calibration-polynomial').value = sensor.calibration.polynomial;
        }
        if (sensor.calibration.expression !== undefined) {
            document.getElementById('calibration-expression').value = sensor.calibration.expression;
        }
    } else {
        // Reset to defaults
        document.getElementById('calibration-offset').value = 0;
        document.getElementById('calibration-scale').value = 1;
        document.getElementById('calibration-polynomial').value = '';
        document.getElementById('calibration-expression').value = '';
        document.querySelector('input[name="calibration-method"][value="linear"]').checked = true;
        showCalibrationMethod('linear');
    }
    
    // Store current sensor for saving
    window.currentCalibrationSensor = sensor;
    
    // Show the modal
    document.getElementById('calibration-modal-overlay').style.display = 'flex';
    document.getElementById('calibration-modal-overlay').classList.add('show');
}

// Refresh sensor data
function refreshSensorData(sensorName) {
    console.log('Refreshing sensor data for:', sensorName);
    
    // Trigger a data update by calling updateIOStatus
    updateIOStatus();
    
    // Show feedback
    const button = event.target;
    const originalText = button.textContent;
    button.textContent = '🔄 Refreshing...';
    button.disabled = true;
    
    setTimeout(() => {
        button.textContent = originalText;
        button.disabled = false;
    }, 1000);
}

// Test sensor communication
function testSensorRead(sensorName) {
    console.log('Testing sensor communication for:', sensorName);
    
    // Find the sensor in the current config
    if (!currentSensorConfig || !currentSensorConfig.sensors) {
        console.error('No sensor config available');
        return;
    }
    
    const sensor = currentSensorConfig.sensors.find(s => s.name === sensorName);
    if (!sensor) {
        console.error('Sensor not found:', sensorName);
        return;
    }
    
    // Show feedback
    const button = event.target;
    const originalText = button.textContent;
    button.textContent = '🔍 Testing...';
    button.disabled = true;
    
    // Send a terminal command to test the sensor
    if (sensor.type && sensor.type.includes('I2C')) {
        const address = sensor.i2c_address || sensor.address;
        if (address) {
            // Use terminal command to test I2C sensor
            executeTerminalCommand(`i2c probe 0x${address.toString(16).padStart(2, '0')}`);
        }
    }
    
    setTimeout(() => {
        button.textContent = originalText;
        button.disabled = false;
    }, 1500);
}

// Update byte visualization display
function updateByteVisualization(rawDataHex) {
    if (!rawDataHex) {
        document.getElementById('raw-bytes-display').textContent = 'No raw data available';
        document.getElementById('extracted-value-display').textContent = 'Extracted: No data';
        return;
    }
    
    // Parse hex string into bytes
    const bytes = [];
    const cleanHex = rawDataHex.replace(/[^0-9A-Fa-f]/g, '');
    for (let i = 0; i < cleanHex.length; i += 2) {
        if (i + 1 < cleanHex.length) {
            bytes.push(parseInt(cleanHex.substr(i, 2), 16));
        }
    }
    
    // Display bytes with highlighting
    const startByte = parseInt(document.getElementById('data-start-byte').value) || 0;
    const length = parseInt(document.getElementById('data-length').value) || 2;
    
    let bytesHtml = '';
    for (let i = 0; i < bytes.length; i++) {
        const isSelected = i >= startByte && i < startByte + length;
        const byteClass = isSelected ? 'byte-highlight' : '';
        bytesHtml += `<span class="${byteClass}">${bytes[i].toString(16).padStart(2, '0').toUpperCase()}</span> `;
    }
    
    document.getElementById('raw-bytes-display').innerHTML = bytesHtml;
    
    // Calculate extracted value
    testByteExtraction();
}

// Test byte extraction with current settings
function testByteExtraction() {
    const sensor = window.currentCalibrationSensor;
    if (!sensor || !sensor.raw_i2c_data) {
        document.getElementById('extracted-value-display').textContent = 'Extracted: No data';
        return;
    }
    
    const rawDataHex = sensor.raw_i2c_data;
    const startByte = parseInt(document.getElementById('data-start-byte').value) || 0;
    const length = parseInt(document.getElementById('data-length').value) || 2;
    const format = document.getElementById('data-format').value;
    
    // Parse hex string into bytes
    const bytes = [];
    const cleanHex = rawDataHex.replace(/[^0-9A-Fa-f]/g, '');
    for (let i = 0; i < cleanHex.length; i += 2) {
        if (i + 1 < cleanHex.length) {
            bytes.push(parseInt(cleanHex.substr(i, 2), 16));
        }
    }
    
    // Extract bytes
    if (startByte >= bytes.length || startByte + length > bytes.length) {
        document.getElementById('extracted-value-display').textContent = 'Extracted: Invalid range';
        return;
    }
    
    const extractedBytes = bytes.slice(startByte, startByte + length);
    let value = 0;
    
    try {
        switch (format) {
            case 'uint8':
                value = extractedBytes[0] || 0;
                break;
            case 'uint16_be':
                value = (extractedBytes[0] << 8) | extractedBytes[1];
                break;
            case 'uint16_le':
                value = (extractedBytes[1] << 8) | extractedBytes[0];
                break;
            case 'uint32_be':
                value = (extractedBytes[0] << 24) | (extractedBytes[1] << 16) | (extractedBytes[2] << 8) | extractedBytes[3];
                break;
            case 'uint32_le':
                value = (extractedBytes[3] << 24) | (extractedBytes[2] << 16) | (extractedBytes[1] << 8) | extractedBytes[0];
                break;
            case 'float32':
                // Convert 4 bytes to IEEE 754 float
                if (extractedBytes.length >= 4) {
                    const buffer = new ArrayBuffer(4);
                    const view = new DataView(buffer);
                    view.setUint8(0, extractedBytes[0]);
                    view.setUint8(1, extractedBytes[1]);
                    view.setUint8(2, extractedBytes[2]);
                    view.setUint8(3, extractedBytes[3]);
                    value = view.getFloat32(0, false); // Big endian
                }
                break;
        }
        
        document.getElementById('extracted-value-display').textContent = `Extracted: ${value}`;
    } catch (error) {
        document.getElementById('extracted-value-display').textContent = 'Extracted: Error';
    }
}

// Enhanced calibration save function for data flow
function saveDataFlowCalibration() {
    if (!window.currentCalibrationSensor) {
        console.error('No sensor selected for calibration');
        return;
    }
    
    const sensor = window.currentCalibrationSensor;
    const method = document.querySelector('input[name="calibration-method"]:checked').value;
    
    // Prepare calibration data
    const calibrationData = {
        name: sensor.name,
        method: method
    };
    
    // Add I2C parsing settings for I2C sensors
    if (sensor.protocol === 'I2C') {
        calibrationData.i2c_parsing = {
            data_offset: parseInt(document.getElementById('data-start-byte').value) || 0,
            data_length: parseInt(document.getElementById('data-length').value) || 2,
            data_format: document.getElementById('data-format').value || 'uint16_le'
        };
    }
    
    switch(method) {
        case 'linear':
            calibrationData.offset = parseFloat(document.getElementById('calibration-offset').value) || 0;
            calibrationData.scale = parseFloat(document.getElementById('calibration-scale').value) || 1;
            break;
        case 'polynomial':
            calibrationData.polynomial = document.getElementById('calibration-polynomial').value || '';
            break;
        case 'expression':
            calibrationData.expression = document.getElementById('calibration-expression').value || '';
            break;
    }
    
    // Send calibration to backend
    fetch('/api/sensor/calibration', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(calibrationData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            console.log('Calibration saved successfully');
            hideCalibrationModal();
            // Refresh the data to show updated calibration
            updateIOStatus();
        } else {
            console.error('Failed to save calibration:', data.error);
            alert('Failed to save calibration: ' + (data.error || 'Unknown error'));
        }
    })
    .catch(error => {
        console.error('Error saving calibration:', error);
        alert('Error saving calibration: ' + error.message);
    });
}

