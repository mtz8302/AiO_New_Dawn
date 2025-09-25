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
    <title>CAN Configuration</title>
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
            padding: 12px;
            margin: 5px 0;
            font-size: 16px;
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

        .brand-row {
            display: grid;
            grid-template-columns: 150px 1fr;
            gap: 10px;
            align-items: center;
            margin-bottom: 25px;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
        }

        .brand-row label {
            font-weight: bold;
            font-size: 18px;
            color: white;
        }

        .can-config-grid {
            display: grid;
            gap: 15px;
            margin-bottom: 20px;
        }

        .can-row {
            display: grid;
            grid-template-columns: 80px 1fr 1fr;
            gap: 10px;
            align-items: center;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
        }

        .can-label {
            font-weight: bold;
            font-size: 18px;
            color: white;
        }

        .info {
            margin-top: 20px;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
            color: white;
        }

        .info h3 {
            margin-top: 0;
            color: white;
        }

        .info p {
            margin: 5px 0;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>CAN Configuration</h1>

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
            <div class="brand-row">
                <label for="brand">Tractor Brand</label>
                <select id="brand" name="brand">
                    <option value="9">Generic</option>
                    <option value="0">Disabled</option>
                    <option value="1">Fendt SCR/S4/Gen6</option>
                    <option value="2">Valtra/Massey Ferguson</option>
                    <option value="3">Case IH/New Holland</option>
                    <option value="4">Fendt One</option>
                    <option value="5">Claas</option>
                    <option value="6">JCB</option>
                    <option value="7">Lindner</option>
                    <option value="8">CAT MT Series</option>
                </select>
            </div>

            <div class="can-config-grid">
                <div class="can-row">
                    <div class="can-label">CAN1</div>
                    <select id="can1Speed" name="can1Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can1Function" name="can1Function">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">ISO_Bus</option>
                        <option value="4">K_Bus</option>
                    </select>
                </div>

                <div class="can-row">
                    <div class="can-label">CAN2</div>
                    <select id="can2Speed" name="can2Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can2Function" name="can2Function">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">ISO_Bus</option>
                        <option value="4">K_Bus</option>
                    </select>
                </div>

                <div class="can-row">
                    <div class="can-label">CAN3</div>
                    <select id="can3Speed" name="can3Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can3Function" name="can3Function">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">ISO_Bus</option>
                        <option value="4">K_Bus</option>
                    </select>
                </div>
            </div>
        </form>

        <div class="info">
            <h3>Function Descriptions</h3>
            <p><strong>Keya:</strong> Keya motor control • <strong>V_Bus:</strong> Valve/steering commands • <strong>ISO_Bus:</strong> ISOBUS implement control</p>
            <p><strong>K_Bus:</strong> Tractor control bus • <strong>Generic Brand:</strong> Use when mixing functions from different brands</p>
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
                    document.getElementById('can1Speed').value = config.can1Speed || 0;
                    document.getElementById('can1Function').value = config.can1Function || 0;
                    document.getElementById('can2Speed').value = config.can2Speed || 0;
                    document.getElementById('can2Function').value = config.can2Function || 0;
                    document.getElementById('can3Speed').value = config.can3Speed || 0;
                    document.getElementById('can3Function').value = config.can3Function || 0;
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
                can1Speed: parseInt(formData.get('can1Speed')),
                can1Function: parseInt(formData.get('can1Function')),
                can2Speed: parseInt(formData.get('can2Speed')),
                can2Function: parseInt(formData.get('can2Function')),
                can3Speed: parseInt(formData.get('can3Speed')),
                can3Function: parseInt(formData.get('can3Function'))
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

        // Brand capabilities definition
        const brandCapabilities = {
            0: { // Disabled
                name: 'Disabled',
                functions: ['NONE']
            },
            1: { // Fendt SCR/S4/Gen6
                name: 'Fendt SCR/S4/Gen6',
                functions: ['NONE', 'V_BUS', 'K_BUS', 'ISO_BUS']
            },
            2: { // Valtra/Massey
                name: 'Valtra/Massey Ferguson',
                functions: ['NONE', 'V_BUS']
            },
            3: { // Case IH/NH
                name: 'Case IH/New Holland',
                functions: ['NONE', 'V_BUS', 'K_BUS']
            },
            4: { // Fendt One
                name: 'Fendt One',
                functions: ['NONE', 'V_BUS', 'K_BUS', 'ISO_BUS']
            },
            5: { // Claas
                name: 'Claas',
                functions: ['NONE', 'V_BUS']
            },
            6: { // JCB
                name: 'JCB',
                functions: ['NONE', 'V_BUS']
            },
            7: { // Lindner
                name: 'Lindner',
                functions: ['NONE', 'V_BUS']
            },
            8: { // CAT MT
                name: 'CAT MT Series',
                functions: ['NONE', 'V_BUS']
            },
            9: { // Generic
                name: 'Generic',
                functions: ['NONE', 'KEYA', 'V_BUS', 'ISO_BUS', 'K_BUS']
            }
        };

        // Function value mapping
        const functionValues = {
            'NONE': 0,
            'KEYA': 1,
            'V_BUS': 2,
            'ISO_BUS': 3,
            'K_BUS': 4
        };

        // Update function dropdowns based on brand
        function updateFunctionOptions() {
            const brandSelect = document.getElementById('brand');
            const brand = parseInt(brandSelect.value);
            const capabilities = brandCapabilities[brand] || brandCapabilities[9]; // Default to Generic

            // Update all three CAN function dropdowns
            ['can1Function', 'can2Function', 'can3Function'].forEach(id => {
                const select = document.getElementById(id);
                const currentValue = select.value;

                // Clear existing options
                select.innerHTML = '';

                // Add allowed functions for this brand
                capabilities.functions.forEach(func => {
                    const option = document.createElement('option');
                    option.value = functionValues[func];
                    option.text = func.replace('_', ' ');
                    select.appendChild(option);
                });

                // Restore previous value if it's still valid
                const validValues = capabilities.functions.map(f => functionValues[f].toString());
                if (validValues.includes(currentValue)) {
                    select.value = currentValue;
                } else {
                    select.value = 0; // Default to None
                }
            });

            // Update info text based on brand
            updateInfoText(brand);
        }

        // Update info text to show relevant functions only
        function updateInfoText(brand) {
            const infoDiv = document.querySelector('.info');
            const capabilities = brandCapabilities[brand] || brandCapabilities[9];

            let infoHtml = '<h3>Function Descriptions</h3>';

            if (brand === 0) { // Disabled
                infoHtml += '<p>CAN bus functions are disabled.</p>';
            } else {
                const descriptions = {
                    'KEYA': '<strong>Keya:</strong> Keya motor control',
                    'V_BUS': '<strong>V_Bus:</strong> Valve/steering commands',
                    'ISO_BUS': '<strong>ISO_Bus:</strong> ISOBUS implement control',
                    'K_BUS': '<strong>K_Bus:</strong> Tractor control bus'
                };

                const availableFuncs = capabilities.functions.filter(f => f !== 'NONE');
                const descParts = availableFuncs.map(f => descriptions[f]).filter(d => d);

                if (descParts.length > 0) {
                    // Group into lines of 2-3 descriptions
                    for (let i = 0; i < descParts.length; i += 3) {
                        const line = descParts.slice(i, i + 3).join(' • ');
                        infoHtml += `<p>${line}</p>`;
                    }
                }

                if (brand === 9) { // Generic
                    infoHtml += '<p><strong>Generic Brand:</strong> Use when mixing functions from different brands</p>';
                }
            }

            infoDiv.innerHTML = infoHtml;
        }

        // Add brand change listener
        document.addEventListener('DOMContentLoaded', function() {
            const brandSelect = document.getElementById('brand');
            brandSelect.addEventListener('change', updateFunctionOptions);

            // Load configuration and update options
            loadConfig().then(() => {
                updateFunctionOptions();
            });
        });

        // Load configuration on page load
        window.addEventListener('DOMContentLoaded', loadConfig);
    </script>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H