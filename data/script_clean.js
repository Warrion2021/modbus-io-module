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