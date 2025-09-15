/**
 * Modbus TCP IO Module - Clean Web Interface
 * Supports the new modular architecture with protocol-based sensor configuration
 */

// Global state management
const AppState = {
    currentTab: 'dashboard',
    sensors: [],
    ioStatus: {
        digitalInputs: Array(8).fill(false),
        digitalOutputs: Array(8).fill(false),
        analogInputs: Array(3).fill(0)
    },
    systemInfo: {
        ip: '',
        connectedClients: 0,
        freeMemory: 0,
        uptime: 0
    },
    dataFlow: {
        paused: false,
        history: []
    }
};

// Configuration constants
const Config = {
    DATA_FORMATS: {
        0: 'uint8',
        1: 'uint16 BE',
        2: 'uint16 LE', 
        3: 'uint32 BE',
        4: 'uint32 LE',
        5: 'float32',
        6: 'int16 BE'
    },
    PROTOCOLS: ['I2C', 'SPI', 'UART', 'Analog', 'Digital'],
    UPDATE_INTERVAL: 1000, // 1 second
    CHART_MAX_POINTS: 50
};

// Main application initialization
document.addEventListener('DOMContentLoaded', function() {
    initializeApp();
});

function initializeApp() {
    console.log('Modbus IO Module - Initializing...');
    
    setupEventListeners();
    setupTabNavigation();
    setupModals();
    setupTerminalInterface();
    loadInitialData();
    startDataPolling();
    
    showToast('System initialized successfully', 'success');
}

// Event listeners setup
function setupEventListeners() {
    // Navigation tabs
    document.querySelectorAll('.nav-tab').forEach(tab => {
        tab.addEventListener('click', handleTabClick);
    });
    
    // Add sensor button
    document.getElementById('add-sensor-btn').addEventListener('click', openAddSensorModal);
    
    // Network form
    document.getElementById('network-form').addEventListener('submit', handleNetworkConfig);
    
    // System actions
    document.getElementById('restart-system').addEventListener('click', restartSystem);
    document.getElementById('factory-reset').addEventListener('click', factoryReset);
    
    // Data flow controls
    document.getElementById('pause-data-btn').addEventListener('click', toggleDataFlow);
    document.getElementById('clear-data-btn').addEventListener('click', clearDataFlow);
    
    // DHCP toggle
    document.getElementById('dhcp-enabled').addEventListener('change', toggleStaticIPFields);
}

// Tab navigation
function setupTabNavigation() {
    const tabs = document.querySelectorAll('.nav-tab');
    const contents = document.querySelectorAll('.tab-content');
    
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetTab = tab.dataset.tab;
            
            // Update active tab
            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            
            // Update active content
            contents.forEach(content => content.classList.remove('active'));
            document.getElementById(targetTab + '-tab').classList.add('active');
            
            AppState.currentTab = targetTab;
            
            // Load tab-specific data
            loadTabData(targetTab);
        });
    });
}

// Modal setup
function setupModals() {
    // Add sensor modal
    const addSensorModal = document.getElementById('add-sensor-modal');
    const closeBtn = addSensorModal.querySelector('.modal-close');
    const cancelBtn = document.getElementById('cancel-add-sensor');
    const form = document.getElementById('add-sensor-form');
    const protocolSelect = document.getElementById('sensor-protocol');
    
    closeBtn.addEventListener('click', closeAddSensorModal);
    cancelBtn.addEventListener('click', closeAddSensorModal);
    form.addEventListener('submit', handleAddSensor);
    protocolSelect.addEventListener('change', updateProtocolConfig);
    
    // Close modal on outside click
    addSensorModal.addEventListener('click', (e) => {
        if (e.target === addSensorModal) {
            closeAddSensorModal();
        }
    });
}

// Data loading and polling
function loadInitialData() {
    Promise.all([
        loadSystemInfo(),
        loadNetworkConfig(),
        loadSensorConfig(),
        loadIOStatus()
    ]).then(() => {
        updateUI();
    }).catch(error => {
        console.error('Failed to load initial data:', error);
        showToast('Failed to load system data', 'error');
    });
}

function startDataPolling() {
    setInterval(() => {
        if (AppState.currentTab === 'dashboard' || AppState.currentTab === 'io-control') {
            loadIOStatus();
        }
        if (AppState.currentTab === 'data-flow' && !AppState.dataFlow.paused) {
            loadSensorData();
        }
        loadSystemInfo();
    }, Config.UPDATE_INTERVAL);
}

// API calls
async function apiCall(endpoint, method = 'GET', data = null) {
    try {
        const options = {
            method,
            headers: {
                'Content-Type': 'application/json'
            }
        };
        
        if (data) {
            options.body = JSON.stringify(data);
        }
        
        const response = await fetch(endpoint, options);
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        return await response.json();
    } catch (error) {
        console.error(`API call failed: ${endpoint}`, error);
        throw error;
    }
}

// Data loading functions
async function loadSystemInfo() {
    try {
        const config = await apiCall('/config');
        AppState.systemInfo.ip = config.staticIP || 'DHCP';
        AppState.systemInfo.connectedClients = config.connectedClients || 0;
        
        updateSystemStatus();
    } catch (error) {
        console.error('Failed to load system info:', error);
    }
}

async function loadNetworkConfig() {
    try {
        const config = await apiCall('/config');
        
        document.getElementById('dhcp-enabled').checked = config.dhcpEnabled;
        
        if (!config.dhcpEnabled) {
            document.getElementById('static-ip').value = 
                config.ip ? config.ip.join('.') : '';
            document.getElementById('gateway').value = 
                config.gateway ? config.gateway.join('.') : '';
            document.getElementById('subnet').value = 
                config.subnet ? config.subnet.join('.') : '';
        }
        
        toggleStaticIPFields();
    } catch (error) {
        console.error('Failed to load network config:', error);
    }
}

async function loadSensorConfig() {
    try {
        const response = await apiCall('/sensors/config');
        AppState.sensors = response.sensors || [];
        updateSensorList();
    } catch (error) {
        console.error('Failed to load sensor config:', error);
        AppState.sensors = [];
        updateSensorList();
    }
}

async function loadIOStatus() {
    try {
        const status = await apiCall('/iostatus');
        
        AppState.ioStatus.digitalInputs = status.digital_inputs || Array(8).fill(false);
        AppState.ioStatus.digitalOutputs = status.digital_outputs || Array(8).fill(false);
        AppState.ioStatus.analogInputs = status.analog_inputs || Array(3).fill(0);
        
        updateIOStatus();
    } catch (error) {
        console.error('Failed to load IO status:', error);
    }
}

async function loadSensorData() {
    try {
        const status = await apiCall('/iostatus');
        
        // Add to data flow history if not paused
        if (!AppState.dataFlow.paused) {
            const timestamp = new Date();
            const dataPoint = {
                timestamp,
                temperature: status.temperature,
                humidity: status.humidity,
                pressure: status.pressure,
                sensors: {}
            };
            
            // Add sensor data
            AppState.sensors.forEach(sensor => {
                if (sensor.enabled && sensor.calibratedValue !== undefined) {
                    dataPoint.sensors[sensor.name] = sensor.calibratedValue;
                }
            });
            
            AppState.dataFlow.history.push(dataPoint);
            
            // Limit history size
            if (AppState.dataFlow.history.length > Config.CHART_MAX_POINTS) {
                AppState.dataFlow.history.shift();
            }
            
            updateDataFlow();
        }
    } catch (error) {
        console.error('Failed to load sensor data:', error);
    }
}

// UI update functions
function updateUI() {
    updateSystemStatus();
    updateDashboard();
    updateIOStatus();
    updateSensorList();
}

function updateSystemStatus() {
    document.getElementById('device-ip').textContent = AppState.systemInfo.ip;
    document.getElementById('client-count').textContent = AppState.systemInfo.connectedClients;
    
    const statusIndicator = document.getElementById('connection-status');
    const statusText = document.getElementById('connection-text');
    
    if (AppState.systemInfo.connectedClients > 0) {
        statusIndicator.className = 'status-indicator online';
        statusText.textContent = 'Connected';
    } else {
        statusIndicator.className = 'status-indicator offline';
        statusText.textContent = 'No Clients';
    }
}

function updateDashboard() {
    // Update stats
    const activeSensors = AppState.sensors.filter(s => s.enabled).length;
    const digitalInputsHigh = AppState.ioStatus.digitalInputs.filter(Boolean).length;
    const digitalOutputsOn = AppState.ioStatus.digitalOutputs.filter(Boolean).length;
    
    document.getElementById('active-sensors').textContent = activeSensors;
    document.getElementById('digital-inputs-high').textContent = digitalInputsHigh;
    document.getElementById('digital-outputs-on').textContent = digitalOutputsOn;
    
    // Update quick I/O indicators
    updateQuickIOIndicators();
    
    // Update recent sensor data
    updateRecentSensorData();
    
    // Update sensor data flow visualization
    updateSensorDataFlow();
}

// Update sensor data flow visualization
function updateSensorDataFlow() {
    if (!AppState.sensors || AppState.sensors.length === 0) {
        console.log("No sensor data available for flow visualization");
        return;
    }
    
    // Organize sensors by protocol
    const sensorsByProtocol = {};
    AppState.sensors.forEach(sensor => {
        const protocol = sensor.protocol || 'Unknown';
        if (!sensorsByProtocol[protocol]) {
            sensorsByProtocol[protocol] = [];
        }
        sensorsByProtocol[protocol].push(sensor);
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
    
    // Hide all protocol categories when no sensors are configured
    if (Object.keys(sensorsByProtocol).length === 0) {
        ['i2c-category', 'uart-category', 'analog-category', 'onewire-category', 'digital-category'].forEach(categoryId => {
            const category = document.getElementById(categoryId);
            if (category) {
                category.classList.add('empty');
                category.style.display = 'none';
            }
        });
    }
}

// Helper functions for sensor data flow
function refreshSensorData(sensorName) {
    console.log('Refreshing sensor data for:', sensorName);
    
    // Trigger a data update
    updateStatus();
    
    // Show feedback
    showToast('Refreshing sensor data...', 'info');
}

function testSensorRead(sensorName) {
    console.log('Testing sensor communication for:', sensorName);
    
    // Find the sensor
    const sensor = AppState.sensors.find(s => s.name === sensorName);
    if (!sensor) {
        showToast('Sensor not found', 'error');
        return;
    }
    
    // Send test command to backend (this would need backend support)
    fetch('/api/sensor/test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: sensorName })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showToast('Sensor communication test successful', 'success');
        } else {
            showToast('Sensor communication test failed', 'error');
        }
    })
    .catch(error => {
        console.error('Error testing sensor:', error);
        showToast('Error testing sensor communication', 'error');
    });
}

function updateQuickIOIndicators() {
    const diContainer = document.getElementById('quick-digital-inputs');
    const doContainer = document.getElementById('quick-digital-outputs');
    
    diContainer.innerHTML = '';
    doContainer.innerHTML = '';
    
    // Digital inputs
    AppState.ioStatus.digitalInputs.forEach((state, index) => {
        const indicator = document.createElement('div');
        indicator.className = `io-indicator ${state ? 'active' : 'inactive'}`;
        indicator.textContent = index;
        indicator.title = `Digital Input ${index}: ${state ? 'HIGH' : 'LOW'}`;
        diContainer.appendChild(indicator);
    });
    
    // Digital outputs
    AppState.ioStatus.digitalOutputs.forEach((state, index) => {
        const indicator = document.createElement('div');
        indicator.className = `io-indicator ${state ? 'active' : 'inactive'}`;
        indicator.textContent = index;
        indicator.title = `Digital Output ${index}: ${state ? 'ON' : 'OFF'}`;
        indicator.addEventListener('click', () => toggleDigitalOutput(index));
        doContainer.appendChild(indicator);
    });
}

function updateRecentSensorData() {
    const container = document.getElementById('recent-sensor-data');
    container.innerHTML = '';
    
    if (AppState.sensors.length === 0) {
        container.innerHTML = '<p class="no-data">No sensors configured</p>';
        return;
    }
    
    AppState.sensors.forEach(sensor => {
        if (sensor.enabled) {
            const sensorCard = document.createElement('div');
            sensorCard.className = 'sensor-reading-card';
            
            const value = sensor.calibratedValue || 0;
            const unit = sensor.unit || '';
            
            sensorCard.innerHTML = `
                <div class="sensor-name">${sensor.name}</div>
                <div class="sensor-value">${value.toFixed(2)} ${unit}</div>
                <div class="sensor-protocol">${sensor.protocol}</div>
            `;
            
            container.appendChild(sensorCard);
        }
    });
}

function updateIOStatus() {
    updateDigitalInputs();
    updateDigitalOutputs();
    updateAnalogInputs();
}

function updateDigitalInputs() {
    const container = document.getElementById('digital-inputs');
    container.innerHTML = '';
    
    AppState.ioStatus.digitalInputs.forEach((state, index) => {
        const inputControl = document.createElement('div');
        inputControl.className = 'io-control-item';
        
        inputControl.innerHTML = `
            <div class="io-label">DI ${index}</div>
            <div class="io-indicator ${state ? 'active' : 'inactive'}">
                ${state ? 'HIGH' : 'LOW'}
            </div>
            <div class="io-details">
                <small>GPIO ${index}</small>
            </div>
        `;
        
        container.appendChild(inputControl);
    });
}

function updateDigitalOutputs() {
    const container = document.getElementById('digital-outputs');
    container.innerHTML = '';
    
    AppState.ioStatus.digitalOutputs.forEach((state, index) => {
        const outputControl = document.createElement('div');
        outputControl.className = 'io-control-item';
        
        outputControl.innerHTML = `
            <div class="io-label">DO ${index}</div>
            <button class="io-toggle ${state ? 'active' : 'inactive'}" 
                    onclick="toggleDigitalOutput(${index})">
                ${state ? 'ON' : 'OFF'}
            </button>
            <div class="io-details">
                <small>GPIO ${index + 8}</small>
            </div>
        `;
        
        container.appendChild(outputControl);
    });
}

function updateAnalogInputs() {
    const container = document.getElementById('analog-inputs');
    container.innerHTML = '';
    
    AppState.ioStatus.analogInputs.forEach((value, index) => {
        const voltage = (value / 4095) * 3.3; // Convert to voltage
        
        const analogControl = document.createElement('div');
        analogControl.className = 'analog-control-item';
        
        analogControl.innerHTML = `
            <div class="analog-label">AI ${index}</div>
            <div class="analog-value">${voltage.toFixed(3)} V</div>
            <div class="analog-raw">${value} (raw)</div>
            <div class="analog-details">
                <small>GPIO ${26 + index}</small>
            </div>
        `;
        
        container.appendChild(analogControl);
    });
}

function updateSensorList() {
    const container = document.getElementById('sensor-list');
    container.innerHTML = '';
    
    if (AppState.sensors.length === 0) {
        container.innerHTML = '<p class="no-sensors">No sensors configured. Click "Add Sensor" to get started.</p>';
        return;
    }
    
    AppState.sensors.forEach((sensor, index) => {
        const sensorItem = document.createElement('div');
        sensorItem.className = `sensor-item ${sensor.enabled ? 'enabled' : 'disabled'}`;
        
        sensorItem.innerHTML = `
            <div class="sensor-info">
                <div class="sensor-name">${sensor.name}</div>
                <div class="sensor-protocol">${sensor.protocol}</div>
                <div class="sensor-details">
                    ${getSensorDetails(sensor)}
                </div>
            </div>
            <div class="sensor-status">
                <div class="sensor-value">
                    ${sensor.calibratedValue ? sensor.calibratedValue.toFixed(2) : '--'}
                    ${sensor.unit || ''}
                </div>
                <div class="sensor-controls">
                    <button class="btn-small" onclick="editSensor(${index})">Edit</button>
                    <button class="btn-small btn-danger" onclick="deleteSensor(${index})">Delete</button>
                </div>
            </div>
        `;
        
        container.appendChild(sensorItem);
    });
}

function getSensorDetails(sensor) {
    switch (sensor.protocol) {
        case 'I2C':
            return `Address: 0x${sensor.i2cAddress ? sensor.i2cAddress.toString(16).padStart(2, '0') : '??'}`;
        case 'SPI':
            return `CS Pin: ${sensor.csPin || '??'}`;
        case 'UART':
            return `Baud: ${sensor.baudRate || '??'}`;
        case 'Analog':
            return `Pin: GPIO ${sensor.analogPin || '??'}`;
        default:
            return '';
    }
}

function updateDataFlow() {
    const container = document.getElementById('live-data-stream');
    
    // Show latest data points
    const latest = AppState.dataFlow.history.slice(-10).reverse();
    
    container.innerHTML = '';
    
    latest.forEach(dataPoint => {
        const dataItem = document.createElement('div');
        dataItem.className = 'data-stream-item';
        
        const timestamp = dataPoint.timestamp.toLocaleTimeString();
        let content = `<div class="data-timestamp">${timestamp}</div>`;
        
        // System sensors
        if (dataPoint.temperature !== undefined) {
            content += `<div class="data-value">Temp: ${dataPoint.temperature.toFixed(1)}°C</div>`;
        }
        if (dataPoint.humidity !== undefined) {
            content += `<div class="data-value">Humidity: ${dataPoint.humidity.toFixed(1)}%</div>`;
        }
        
        // Custom sensors
        Object.entries(dataPoint.sensors).forEach(([name, value]) => {
            content += `<div class="data-value">${name}: ${value.toFixed(2)}</div>`;
        });
        
        dataItem.innerHTML = content;
        container.appendChild(dataItem);
    });
}

// Tab-specific data loading
function loadTabData(tab) {
    switch (tab) {
        case 'dashboard':
            updateDashboard();
            break;
        case 'io-control':
            updateIOStatus();
            break;
        case 'sensors':
            updateSensorList();
            break;
        case 'data-flow':
            updateDataFlow();
            break;
    }
}

// Event handlers
function handleTabClick(event) {
    // Handled by setupTabNavigation
}

async function toggleDigitalOutput(index) {
    try {
        const newState = !AppState.ioStatus.digitalOutputs[index];
        
        await apiCall('/setoutput', 'POST', {
            output: index,
            state: newState
        });
        
        // Update local state immediately for responsiveness
        AppState.ioStatus.digitalOutputs[index] = newState;
        updateIOStatus();
        updateQuickIOIndicators();
        
        showToast(`Digital Output ${index} ${newState ? 'ON' : 'OFF'}`, 'success');
    } catch (error) {
        showToast(`Failed to toggle output ${index}`, 'error');
    }
}

// Network configuration
async function handleNetworkConfig(event) {
    event.preventDefault();
    
    try {
        const dhcpEnabled = document.getElementById('dhcp-enabled').checked;
        const staticIP = document.getElementById('static-ip').value;
        const gateway = document.getElementById('gateway').value;
        const subnet = document.getElementById('subnet').value;
        
        const config = {
            dhcpEnabled
        };
        
        if (!dhcpEnabled) {
            config.ip = staticIP.split('.').map(Number);
            config.gateway = gateway.split('.').map(Number);
            config.subnet = subnet.split('.').map(Number);
        }
        
        await apiCall('/config', 'POST', config);
        
        showToast('Network configuration saved. System will restart.', 'success');
        
        // System will restart, so show countdown
        setTimeout(() => {
            window.location.reload();
        }, 3000);
        
    } catch (error) {
        showToast('Failed to save network configuration', 'error');
    }
}

function toggleStaticIPFields() {
    const dhcpEnabled = document.getElementById('dhcp-enabled').checked;
    const staticGroups = ['static-ip-group', 'gateway-group', 'subnet-group'];
    
    staticGroups.forEach(groupId => {
        const group = document.getElementById(groupId);
        group.style.display = dhcpEnabled ? 'none' : 'block';
    });
}

// Sensor management
function openAddSensorModal() {
    document.getElementById('add-sensor-modal').classList.add('active');
    resetAddSensorForm();
}

function closeAddSensorModal() {
    document.getElementById('add-sensor-modal').classList.remove('active');
}

function resetAddSensorForm() {
    document.getElementById('add-sensor-form').reset();
    document.querySelectorAll('.protocol-config').forEach(config => {
        config.style.display = 'none';
    });
}

function updateProtocolConfig() {
    const protocol = document.getElementById('sensor-protocol').value;
    
    // Hide all protocol configs
    document.querySelectorAll('.protocol-config').forEach(config => {
        config.style.display = 'none';
    });
    
    // Show relevant config
    if (protocol) {
        const configId = protocol.toLowerCase() + '-config';
        const configElement = document.getElementById(configId);
        if (configElement) {
            configElement.style.display = 'block';
        }
    }
}

async function handleAddSensor(event) {
    event.preventDefault();
    
    try {
        const formData = new FormData(event.target);
        const protocol = document.getElementById('sensor-protocol').value;
        
        const sensor = {
            name: document.getElementById('sensor-name').value,
            protocol: protocol,
            enabled: document.getElementById('sensor-enabled').checked,
            modbusRegister: parseInt(document.getElementById('modbus-register').value)
        };
        
        // Add protocol-specific configuration
        switch (protocol) {
            case 'I2C':
                sensor.i2cAddress = parseInt(document.getElementById('i2c-address').value, 16);
                sensor.i2cRegister = document.getElementById('i2c-register').value || 0;
                sensor.dataFormat = parseInt(document.getElementById('data-format').value);
                break;
            case 'SPI':
                sensor.csPin = parseInt(document.getElementById('spi-cs-pin').value);
                break;
            case 'UART':
                sensor.baudRate = parseInt(document.getElementById('uart-baud').value);
                break;
            case 'Analog':
                sensor.analogPin = parseInt(document.getElementById('analog-pin').value);
                break;
        }
        
        // Add to local state
        AppState.sensors.push(sensor);
        
        // Save to device
        await apiCall('/sensors/config', 'POST', {
            sensors: AppState.sensors
        });
        
        closeAddSensorModal();
        updateSensorList();
        
        showToast(`Sensor "${sensor.name}" added successfully. System will restart.`, 'success');
        
        // System will restart to apply new sensor config
        setTimeout(() => {
            window.location.reload();
        }, 3000);
        
    } catch (error) {
        showToast('Failed to add sensor', 'error');
    }
}

function editSensor(index) {
    // TODO: Implement sensor editing
    showToast('Sensor editing coming soon', 'info');
}

async function deleteSensor(index) {
    if (!confirm('Are you sure you want to delete this sensor?')) {
        return;
    }
    
    try {
        AppState.sensors.splice(index, 1);
        
        await apiCall('/sensors/config', 'POST', {
            sensors: AppState.sensors
        });
        
        updateSensorList();
        
        showToast('Sensor deleted successfully. System will restart.', 'success');
        
        setTimeout(() => {
            window.location.reload();
        }, 3000);
        
    } catch (error) {
        showToast('Failed to delete sensor', 'error');
    }
}

// Data flow controls
function toggleDataFlow() {
    AppState.dataFlow.paused = !AppState.dataFlow.paused;
    const btn = document.getElementById('pause-data-btn');
    btn.textContent = AppState.dataFlow.paused ? 'Resume' : 'Pause';
    
    showToast(`Data flow ${AppState.dataFlow.paused ? 'paused' : 'resumed'}`, 'info');
}

function clearDataFlow() {
    AppState.dataFlow.history = [];
    updateDataFlow();
    showToast('Data flow history cleared', 'info');
}

// System actions
async function restartSystem() {
    if (!confirm('Are you sure you want to restart the system?')) {
        return;
    }
    
    try {
        showToast('Restarting system...', 'info');
        await apiCall('/system/restart', 'POST');
        
        setTimeout(() => {
            window.location.reload();
        }, 5000);
        
    } catch (error) {
        showToast('Failed to restart system', 'error');
    }
}

async function factoryReset() {
    const confirmation = prompt('Type "RESET" to confirm factory reset:');
    if (confirmation !== 'RESET') {
        return;
    }
    
    try {
        showToast('Performing factory reset...', 'warning');
        await apiCall('/system/factory-reset', 'POST');
        
        setTimeout(() => {
            window.location.reload();
        }, 10000);
        
    } catch (error) {
        showToast('Failed to perform factory reset', 'error');
    }
}

// Utility functions
function showToast(message, type = 'info', duration = 3000) {
    const container = document.getElementById('toast-container');
    
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    toast.innerHTML = `
        <span>${message}</span>
        <button class="toast-close" onclick="this.parentElement.remove()">&times;</button>
    `;
    
    container.appendChild(toast);
    
    // Auto-remove after duration
    setTimeout(() => {
        if (container.contains(toast)) {
            container.removeChild(toast);
        }
    }, duration);
}

// Make functions available globally for onclick handlers
window.toggleDigitalOutput = toggleDigitalOutput;
window.editSensor = editSensor;
window.deleteSensor = deleteSensor;

// ==================== CALIBRATION FUNCTIONALITY ====================

// Global calibration state
window.currentCalibrationSensor = null;
let currentEzoSensorIndex = -1;
let editingSensorIndex = -1;

// Show calibration modal for sensor
function showCalibrationModal(sensorName) {
    console.log('Opening calibration modal for sensor:', sensorName);
    
    // Find the sensor in the current AppState sensors
    const sensor = AppState.sensors.find(s => s.name === sensorName);
    if (!sensor) {
        console.error('Sensor not found:', sensorName);
        showToast('Sensor not found', 'error');
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

// Hide calibration modal
function hideCalibrationModal() {
    document.getElementById('calibration-modal-overlay').classList.remove('show');
    document.getElementById('calibration-modal-overlay').style.display = 'none';
    window.currentCalibrationSensor = null;
}

// Show/hide calibration method sections
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

// Add event listeners for calibration method radio buttons
document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('input[name="calibration-method"]').forEach(radio => {
        radio.addEventListener('change', function() {
            if (this.checked) {
                showCalibrationMethod(this.value);
            }
        });
    });
});

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

// Save calibration data
function saveCalibration() {
    if (!window.currentCalibrationSensor) {
        console.error('No sensor selected for calibration');
        showToast('No sensor selected for calibration', 'error');
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
            showToast('Calibration saved successfully', 'success');
            hideCalibrationModal();
            // Refresh the data to show updated calibration
            updateStatus();
        } else {
            console.error('Failed to save calibration:', data.error);
            showToast('Failed to save calibration: ' + (data.error || 'Unknown error'), 'error');
        }
    })
    .catch(error => {
        console.error('Error saving calibration:', error);
        showToast('Error saving calibration: ' + error.message, 'error');
    });
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

// EZO Sensor calibration modal functionality
function showEzoCalibrationModal(sensorName) {
    const sensor = AppState.sensors.find(s => s.name === sensorName);
    if (!sensor || !sensor.type.startsWith('EZO_')) {
        showToast('This sensor is not an EZO sensor', 'error');
        return;
    }
    
    // Update modal title and sensor info
    document.getElementById('ezo-calibration-modal-title').textContent = `EZO Calibration - ${sensor.name}`;
    document.getElementById('ezo-calibration-sensor-name').textContent = sensor.name;
    document.getElementById('ezo-calibration-sensor-type').textContent = sensor.type;
    document.getElementById('ezo-calibration-sensor-address').textContent = sensor.i2c_address ? `0x${sensor.i2c_address.toString(16).toUpperCase()}` : 'N/A';
    
    // Clear previous response
    document.getElementById('ezo-sensor-response').textContent = 'No response yet';
    
    // Show the modal
    document.getElementById('ezo-calibration-modal-overlay').style.display = 'flex';
    document.getElementById('ezo-calibration-modal-overlay').classList.add('show');
}

function hideEzoCalibrationModal() {
    document.getElementById('ezo-calibration-modal-overlay').classList.remove('show');
    document.getElementById('ezo-calibration-modal-overlay').style.display = 'none';
}

// Make calibration functions globally available
window.showCalibrationModal = showCalibrationModal;
window.hideCalibrationModal = hideCalibrationModal;
window.showCalibrationMethod = showCalibrationMethod;
window.testByteExtraction = testByteExtraction;
window.saveCalibration = saveCalibration;
window.showPresetTab = showPresetTab;
window.setLinearPreset = setLinearPreset;
window.setPolynomialPreset = setPolynomialPreset;
window.setExpressionPreset = setExpressionPreset;
window.showEzoCalibrationModal = showEzoCalibrationModal;
window.hideEzoCalibrationModal = hideEzoCalibrationModal;
window.refreshSensorData = refreshSensorData;
window.testSensorRead = testSensorRead;

// Terminal functionality
let terminalWatchInterval = null;
let isWatching = false;

function updateTerminalInterface() {
    const protocol = document.getElementById('terminal-protocol').value;
    const pinSelect = document.getElementById('terminal-pin');
    const i2cGroup = document.querySelector('.terminal-i2c-group');
    
    // Clear existing options
    pinSelect.innerHTML = '<option value="">Select pin...</option>';
    
    // Show/hide I2C address field
    i2cGroup.style.display = protocol === 'i2c' ? 'block' : 'none';
    
    // Populate pin options based on protocol
    switch(protocol) {
        case 'digital':
            for(let i = 0; i <= 7; i++) {
                pinSelect.innerHTML += `<option value="${i}">DI${i} (GPIO ${i})</option>`;
            }
            for(let i = 8; i <= 15; i++) {
                pinSelect.innerHTML += `<option value="${i}">DO${i-8} (GPIO ${i})</option>`;
            }
            break;
        case 'analog':
            for(let i = 0; i < 3; i++) {
                pinSelect.innerHTML += `<option value="${i}">AI${i} (GPIO ${26+i})</option>`;
            }
            break;
        case 'i2c':
            pinSelect.innerHTML += '<option value="4,5">I2C Bus (SDA=4, SCL=5)</option>';
            break;
        case 'spi':
            pinSelect.innerHTML += '<option value="16,17,18,19">SPI Bus (MISO=16, CS=17, SCK=18, MOSI=19)</option>';
            break;
        case 'uart':
            pinSelect.innerHTML += '<option value="0,1">UART0 (TX=0, RX=1)</option>';
            pinSelect.innerHTML += '<option value="8,9">UART1 (TX=8, RX=9)</option>';
            break;
        case 'sensor':
            for(let i = 0; i < 8; i++) {
                pinSelect.innerHTML += `<option value="${i}">Sensor Slot ${i}</option>`;
            }
            break;
        case 'network':
            pinSelect.innerHTML += '<option value="ethernet">Ethernet Interface</option>';
            pinSelect.innerHTML += '<option value="pins">Ethernet Pins</option>';
            pinSelect.innerHTML += '<option value="modbus">Modbus TCP Server</option>';
            pinSelect.innerHTML += '<option value="clients">Connected Clients</option>';
            break;
        case 'system':
            pinSelect.innerHTML += '<option value="status">System Status</option>';
            pinSelect.innerHTML += '<option value="sensors">Sensor List</option>';
            pinSelect.innerHTML += '<option value="info">Hardware Info</option>';
            break;
    }
}

function handleTerminalKeypress(event) {
    if (event.key === 'Enter') {
        sendTerminalCommand();
    }
}

async function sendTerminalCommand() {
    const protocol = document.getElementById('terminal-protocol').value;
    const pin = document.getElementById('terminal-pin').value;
    const command = document.getElementById('terminal-command').value.trim();
    const i2cAddress = document.getElementById('terminal-i2c-address').value;
    
    if (!command) {
        appendTerminalOutput('Error: Command cannot be empty', 'error');
        return;
    }
    
    let fullCommand = command;
    
    // Build command based on protocol
    switch(protocol) {
        case 'i2c':
            if (i2cAddress) {
                fullCommand = `${i2cAddress} ${command}`;
            }
            break;
        case 'digital':
        case 'analog':
        case 'sensor':
            if (pin) {
                fullCommand = `${pin} ${command}`;
            }
            break;
    }
    
    appendTerminalOutput(`> ${protocol}:${pin ? ' pin ' + pin : ''}${i2cAddress ? ' @' + i2cAddress : ''} ${command}`, 'command');
    
    try {
        const response = await fetch('/api/terminal/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                protocol: protocol,
                pin: pin,
                command: fullCommand,
                i2c_address: i2cAddress
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            appendTerminalOutput(result.response || 'Command executed successfully', 'response');
        } else {
            appendTerminalOutput(`Error: ${result.error || 'Command failed'}`, 'error');
        }
    } catch (error) {
        appendTerminalOutput(`Connection error: ${error.message}`, 'error');
    }
    
    // Clear command input
    document.getElementById('terminal-command').value = '';
}

function toggleTerminalWatch() {
    const watchBtn = document.getElementById('watch-btn');
    
    if (isWatching) {
        // Stop watching
        if (terminalWatchInterval) {
            clearInterval(terminalWatchInterval);
            terminalWatchInterval = null;
        }
        stopTerminalWatch();
        watchBtn.textContent = 'Start Watch';
        watchBtn.classList.remove('watching');
        isWatching = false;
        appendTerminalOutput('Watch mode stopped', 'system');
    } else {
        // Start watching
        const protocol = document.getElementById('terminal-protocol').value;
        const pin = document.getElementById('terminal-pin').value;
        
        if (!pin && protocol !== 'system') {
            appendTerminalOutput('Error: Select a pin/port to watch', 'error');
            return;
        }
        
        startTerminalWatch();
        watchBtn.textContent = 'Stop Watch';
        watchBtn.classList.add('watching');
        isWatching = true;
        appendTerminalOutput(`Watch mode started for ${protocol}:${pin}`, 'system');
        
        // Poll for updates every 1000ms
        terminalWatchInterval = setInterval(updateTerminalWatch, 1000);
    }
}

async function startTerminalWatch() {
    const protocol = document.getElementById('terminal-protocol').value;
    const pin = document.getElementById('terminal-pin').value;
    
    try {
        await fetch('/api/terminal/watch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                protocol: protocol,
                pin: pin,
                enable: true
            })
        });
    } catch (error) {
        console.error('Failed to start watch:', error);
    }
}

async function stopTerminalWatch() {
    try {
        await fetch('/api/terminal/stop', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
    } catch (error) {
        console.error('Failed to stop watch:', error);
    }
}

async function updateTerminalWatch() {
    if (!isWatching) return;
    
    try {
        const response = await fetch('/iostatus');
        const data = await response.json();
        
        const protocol = document.getElementById('terminal-protocol').value;
        const pin = document.getElementById('terminal-pin').value;
        
        let watchData = '';
        
        switch(protocol) {
            case 'digital':
                if (pin !== '') {
                    const state = data.digital_inputs && data.digital_inputs[pin] ? 'HIGH' : 'LOW';
                    watchData = `DI${pin}: ${state}`;
                }
                break;
            case 'analog':
                if (pin !== '' && data.analog_inputs && data.analog_inputs[pin] !== undefined) {
                    watchData = `AI${pin}: ${data.analog_inputs[pin]}mV`;
                }
                break;
            case 'sensor':
                if (pin !== '' && data.sensors && data.sensors[pin]) {
                    const sensor = data.sensors[pin];
                    watchData = `S${pin}: ${sensor.type || 'Unknown'} = ${JSON.stringify(sensor.data || {})}`;
                }
                break;
            case 'system':
                watchData = `Uptime: ${data.uptime || 0}ms, Free Heap: ${data.free_heap || 0}`;
                break;
        }
        
        if (watchData) {
            appendTerminalOutput(watchData, 'watch');
        }
    } catch (error) {
        // Silently handle watch errors to avoid spam
    }
}

function clearTerminalOutput() {
    document.getElementById('terminal-output-content').innerHTML = '';
}

function showTerminalHelp() {
    const helpText = `
Interactive Terminal Guide - Complete Command Reference:

DIGITAL I/O Commands:
  read               - Read current pin state
  write <1/0>        - Write to output pin (HIGH/LOW)
  high / 1           - Set output HIGH  
  low / 0            - Set output LOW
  toggle             - Toggle output state
  config <option>    - Configure input (pullup/invert/latch)
  Pin Map: DI0-7=GPIO0-7 (inputs), DO0-7=GPIO8-15 (outputs)

ANALOG Commands:
  read               - Read analog value in millivolts
  config             - Show pin configuration
  Pin Map: AI0-2 = GPIO 26-28

I2C Commands:
  scan               - Scan for I2C devices on bus
  probe              - Check if device exists at address  
  read <register>    - Read from device register
  write <reg> <data> - Write data to device register
  Address format: Decimal (72) or Hex (0x48)

UART Commands:
  help               - Show all available UART commands
  init               - Initialize UART at 9600 baud (default)
  send <data>        - Send data to connected UART device
  read               - Read data from UART receive buffer
  loopback           - Test UART loopback functionality
  baudrate <rate>    - Set baud rate (9600,19200,38400,57600,115200)
  status             - Show UART status and pin configuration
  at                 - Send AT command (useful for modems/GPS)
  echo <on|off>      - Enable/disable echo mode
  clear              - Clear UART receive buffer

NETWORK Commands:
  status             - Show network/ethernet configuration
  clients            - Show connected Modbus clients
  link               - Show ethernet link status
  stats              - Show network statistics
  Pin options: ethernet, pins, modbus, clients

SYSTEM Commands:
  status             - System status and uptime
  sensors            - List configured sensors
  info               - Hardware information
  restart            - Restart system

SENSOR Commands (Protocol-specific):
  scan               - Update all sensors
  read               - Read sensor by index
  help               - Protocol-specific help

Watch Mode:
  Click "Start Watch" to continuously monitor selected pin/protocol
  
Usage Examples:
  Digital: Pin DI0, Command: read → "DI0 = HIGH (Raw: HIGH)"
  Digital: Pin DO0, Command: write 1 → "DO0 set to HIGH"  
  Digital: Pin DI0, Command: config pullup → "DI0 pullup toggled"
  Analog: Pin AI0, Command: read → "AI0 = 1650 mV"
  I2C: Address 0x48, Command: scan → "Found device at 0x48"
  UART: Command: init → "UART initialized on Serial1 at 9600 baud"
  Network: Pin ethernet, Command: status → Shows IP/MAC/DHCP info
  System: Command: info → Shows hardware configuration
  `;
    
    appendTerminalOutput(helpText, 'help');
}

function appendTerminalOutput(text, type = 'response') {
    const output = document.getElementById('terminal-output-content');
    const timestamp = new Date().toLocaleTimeString();
    
    const line = document.createElement('div');
    line.className = `terminal-line terminal-${type}`;
    line.innerHTML = `<span class="terminal-timestamp">[${timestamp}]</span> ${text}`;
    
    output.appendChild(line);
    output.scrollTop = output.scrollHeight;
    
    // Limit output lines to prevent memory issues
    const lines = output.querySelectorAll('.terminal-line');
    if (lines.length > 500) {
        for (let i = 0; i < 100; i++) {
            if (lines[i]) {
                lines[i].remove();
            }
        }
    }
}

// Make terminal functions globally available
window.updateTerminalInterface = updateTerminalInterface;
window.handleTerminalKeypress = handleTerminalKeypress;
window.sendTerminalCommand = sendTerminalCommand;
window.toggleTerminalWatch = toggleTerminalWatch;
window.clearTerminalOutput = clearTerminalOutput;
window.showTerminalHelp = showTerminalHelp;

// Terminal interface setup
function setupTerminalInterface() {
    // Initialize terminal interface when terminal tab is loaded
    const terminalTab = document.querySelector('[data-tab="terminal"]');
    if (terminalTab) {
        terminalTab.addEventListener('click', function() {
            // Initialize terminal interface on first load
            setTimeout(updateTerminalInterface, 100);
        });
    }
    
    // Set up terminal protocol change handler
    const protocolSelect = document.getElementById('terminal-protocol');
    if (protocolSelect) {
        updateTerminalInterface(); // Initialize with default protocol
    }
}