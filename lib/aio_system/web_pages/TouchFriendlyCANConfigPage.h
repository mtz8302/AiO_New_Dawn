// TouchFriendlyCANConfigPage.h
// CAN configuration page for touch-friendly interface

#ifndef TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H
#define TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_CAN_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>CAN Steering Configuration</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        .status-box {
            margin: 20px 0;
            padding: 15px;
            background-color: #333;
            border-radius: 10px;
            text-align: center;
        }

        .status-connected {
            color: #4CAF50;
            font-weight: bold;
        }

        .status-disconnected {
            color: #f44336;
            font-weight: bold;
        }

        select {
            width: 100%;
            padding: 15px;
            margin: 10px 0;
            font-size: 18px;
            border: 2px solid #666;
            border-radius: 10px;
            background-color: #333;
            color: white;
            box-sizing: border-box;
        }


        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }

        .form-row {
            display: flex;
            align-items: center;
            margin: 15px 0;
            gap: 15px;
        }

        .form-row label {
            flex: 0 0 180px;
            font-weight: bold;
            font-size: 18px;
        }

        .form-row select {
            flex: 1;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>CAN Steering Configuration</h1>

        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;"
                    onclick="window.location.href='/'">
                Home
            </button>
            <button type="button" class="touch-button" style="background: #e74c3c;"
                    onclick="confirmRestart()">
                Restart
            </button>
            <button type="submit" class="touch-button" form="canConfigForm">
                Save
            </button>
        </div>

        <div id="statusMessage" class="status-box" style="display:none;"></div>

        <form id="canConfigForm">
            <div class="form-row">
                <label for="brand">Tractor Brand</label>
                <select id="brand" name="brand">
                    <option value="0">Disabled</option>
                    <option value="1">Keya</option>
                    <option value="2">Fendt SCR/S4/Gen6</option>
                    <option value="3">Valtra/Massey Ferguson</option>
                    <option value="4">Case IH/New Holland</option>
                    <option value="5">Fendt One</option>
                    <option value="6">Claas</option>
                    <option value="7">JCB</option>
                    <option value="8">Lindner</option>
                    <option value="9">CAT MT Series</option>
                </select>
            </div>

            <div class="form-row">
                <label for="steerBus">Steering Bus</label>
                <select id="steerBus" name="steerBus">
                    <option value="0">None</option>
                    <option value="1">K_Bus (CAN1)</option>
                    <option value="2">ISO_Bus (CAN2)</option>
                    <option value="3">V_Bus (CAN3)</option>
                </select>
            </div>

            <div class="form-row">
                <label for="buttonBus">Work Switch</label>
                <select id="buttonBus" name="buttonBus">
                    <option value="0">None</option>
                    <option value="1">K_Bus (CAN1)</option>
                    <option value="2">ISO_Bus (CAN2)</option>
                    <option value="3">V_Bus (CAN3)</option>
                </select>
            </div>

            <div class="form-row">
                <label for="hitchBus">Hitch Control</label>
                <select id="hitchBus" name="hitchBus">
                    <option value="0">None</option>
                    <option value="1">K_Bus (CAN1)</option>
                    <option value="2">ISO_Bus (CAN2)</option>
                    <option value="3">V_Bus (CAN3)</option>
                </select>
            </div>

        </form>

        <div class="info">
            <h3>Bus Descriptions</h3>
            <p><strong>K_Bus (CAN1):</strong> Tractor control bus (250 kbps)</p>
            <p><strong>ISO_Bus (CAN2):</strong> ISOBUS for implements (250 kbps)</p>
            <p><strong>V_Bus (CAN3):</strong> Valve/steering bus (250 kbps)</p>
        </div>
    </div>

    <script>
        // Load current configuration
        async function loadConfig() {
            try {
                const response = await fetch('/api/can/config');
                if (response.ok) {
                    const config = await response.json();
                    document.getElementById('brand').value = config.brand || 0;
                    document.getElementById('steerBus').value = config.steerBus || 0;
                    document.getElementById('buttonBus').value = config.buttonBus || 0;
                    document.getElementById('hitchBus').value = config.hitchBus || 0;
                }
            } catch (error) {
                console.error('Error loading config:', error);
                showStatus('Error loading configuration', 'error');
            }
        }

        // Save configuration
        document.getElementById('canConfigForm').addEventListener('submit', async (e) => {
            e.preventDefault();

            const formData = new FormData(e.target);
            const config = {
                brand: parseInt(formData.get('brand')),
                steerBus: parseInt(formData.get('steerBus')),
                buttonBus: parseInt(formData.get('buttonBus')),
                hitchBus: parseInt(formData.get('hitchBus'))
            };

            try {
                const response = await fetch('/api/can/config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(config)
                });

                if (response.ok) {
                    const result = await response.json();
                    showStatus(result.message || 'Configuration saved', 'success');
                } else {
                    showStatus('Error saving configuration', 'error');
                }
            } catch (error) {
                console.error('Error saving config:', error);
                showStatus('Network error', 'error');
            }
        });

        // Show status message
        function showStatus(message, type) {
            const statusDiv = document.getElementById('statusMessage');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.className = 'status-box ' + (type === 'success' ? 'status-connected' : 'status-disconnected');

            // Auto-hide after 5 seconds
            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 5000);
        }

        // Touch-friendly confirmation dialog
        function confirmRestart() {
            if (confirm('Are you sure you want to restart the system?')) {
                restartSystem();
            }
        }

        function restartSystem() {
            fetch('/api/restart', { method: 'POST' })
                .then(response => {
                    if (response.ok) {
                        showStatus('System is restarting...', 'success');
                    }
                })
                .catch(error => {
                    showStatus('Error restarting system', 'error');
                });
        }

        // Load configuration on page load
        window.addEventListener('DOMContentLoaded', loadConfig);
    </script>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H