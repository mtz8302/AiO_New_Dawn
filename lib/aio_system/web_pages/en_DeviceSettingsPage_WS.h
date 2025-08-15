// en_DeviceSettingsPage_WS.h
// English Device Settings page with WebSocket telemetry

#ifndef EN_DEVICE_SETTINGS_PAGE_WS_H
#define EN_DEVICE_SETTINGS_PAGE_WS_H

const char EN_DEVICE_SETTINGS_PAGE_WS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Device Settings</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>%CSS_STYLES%</style>
    <script>
        let ws = null;
        let reconnectTimer = null;
        
        function connectWebSocket() {
            // Connect to telemetry WebSocket on port 8082
            const wsUrl = 'ws://' + window.location.hostname + ':8082';
            
            ws = new WebSocket(wsUrl);
            ws.binaryType = 'arraybuffer';
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                document.getElementById('connectionStatus').textContent = 'Connected';
                document.getElementById('connectionStatus').style.color = 'green';
                
                if (reconnectTimer) {
                    clearTimeout(reconnectTimer);
                    reconnectTimer = null;
                }
            };
            
            ws.onmessage = function(event) {
                // Parse binary telemetry packet
                const data = new DataView(event.data);
                if (data.byteLength >= 32) {
                    // Extract encoder count (bytes 12-13, int16)
                    const encoderCount = data.getInt16(12, true);
                    
                    document.getElementById('encoderCount').textContent = encoderCount;
                    document.getElementById('encoderStatus').textContent = 'Live';
                }
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                document.getElementById('connectionStatus').textContent = 'Disconnected';
                document.getElementById('connectionStatus').style.color = 'red';
                document.getElementById('encoderStatus').textContent = 'No Connection';
                
                // Reconnect after 2 seconds
                reconnectTimer = setTimeout(connectWebSocket, 2000);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };
        }
        
        function saveSettings() {
            const settings = {
                udpPassthrough: document.getElementById('udpPassthrough').checked,
                sensorFusion: document.getElementById('sensorFusion').checked,
                pwmBrakeMode: document.getElementById('pwmBrakeMode').checked,
                encoderType: parseInt(document.getElementById('encoderType').value)
            };
            
            fetch('/api/device/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settings)
            })
            .then(response => response.json())
            .then(data => {
                if(data.status === 'saved') {
                    alert('Settings saved successfully!');
                }
            })
            .catch((error) => {
                console.error('Error:', error);
                alert('Failed to save settings');
            });
        }
        
        window.onload = function() {
            connectWebSocket();
            
            // Load current settings
            fetch('/api/device/settings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('udpPassthrough').checked = data.udpPassthrough || false;
                    document.getElementById('sensorFusion').checked = data.sensorFusion || false;
                    document.getElementById('pwmBrakeMode').checked = data.pwmBrakeMode || false;
                    document.getElementById('encoderType').value = data.encoderType || 0;
                })
                .catch((error) => {
                    console.error('Error loading settings:', error);
                });
        };
        
        // Cleanup on page unload
        window.addEventListener('beforeunload', function() {
            if (ws) {
                ws.close();
            }
        });
    </script>
</head>
<body>
    <div class="container">
        <h1>Device Settings</h1>
        
        <div class="status">
            <h2>Connection Status</h2>
            <p>WebSocket: <span id="connectionStatus">Connecting...</span></p>
        </div>
        
        <div class="status">
            <h2>Encoder Status</h2>
            <p>Count: <span id="encoderCount">--</span></p>
            <p>Status: <span id="encoderStatus">--</span></p>
        </div>
        
        <h2>Configuration</h2>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="udpPassthrough"> UDP Passthrough
            </label>
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="sensorFusion"> Sensor Fusion
            </label>
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="pwmBrakeMode"> PWM Brake Mode
            </label>
        </div>
        
        <div class="form-group">
            <label for="encoderType">Encoder Type:</label>
            <select id="encoderType">
                <option value="0">None</option>
                <option value="1">Single Channel</option>
                <option value="2">Quadrature</option>
            </select>
        </div>
        
        <button onclick="saveSettings()">Save Settings</button>
        
        <div style="margin-top: 20px;">
            <a href="/">Back to Home</a>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // EN_DEVICE_SETTINGS_PAGE_WS_H