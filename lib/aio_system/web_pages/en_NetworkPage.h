// en_NetworkPage.h
// English version of network settings page

#ifndef EN_NETWORK_PAGE_H
#define EN_NETWORK_PAGE_H

#include <Arduino.h>

const char EN_NETWORK_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Network Settings - AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
    <script>
        function saveIPSettings() {
            const octet1 = parseInt(document.getElementById('octet1').value);
            const octet2 = parseInt(document.getElementById('octet2').value);
            const octet3 = parseInt(document.getElementById('octet3').value);
            
            // Validate octets
            if (octet1 < 0 || octet1 > 255 || octet2 < 0 || octet2 > 255 || octet3 < 0 || octet3 > 255) {
                alert('Invalid IP address. Each octet must be between 0 and 255.');
                return false;
            }
            
            const formData = {
                ip: [octet1, octet2, octet3, 126]
            };
            
            fetch('/api/network/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(formData)
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok') {
                    alert('Settings saved successfully! Click Reboot to apply new IP address.');
                } else {
                    alert('Error saving settings: ' + data.error);
                }
            })
            .catch((error) => {
                alert('Error: ' + error);
            });
            
            return false;
        }
        
        function rebootSystem() {
            if (confirm('Are you sure you want to reboot? The system will be unavailable for a moment.')) {
                fetch('/api/restart', {
                    method: 'POST'
                })
                .then(() => {
                    alert('System is rebooting...');
                })
                .catch((error) => {
                    alert('Error: ' + error);
                });
            }
        }
        
        function loadSettings() {
            fetch('/api/network/config')
            .then(response => response.json())
            .then(data => {
                if (data.ip && data.ip.length >= 3) {
                    document.getElementById('octet1').value = data.ip[0];
                    document.getElementById('octet2').value = data.ip[1];
                    document.getElementById('octet3').value = data.ip[2];
                }
            })
            .catch((error) => {
                console.error('Error loading settings:', error);
            });
        }
        
        window.onload = loadSettings;
    </script>
</head>
<body>
    <div class='container'>
        <h1>Network Settings</h1>
        
        <div class='status'>
            <p>Current IP: %IP_ADDRESS%</p>
            <p>Link Speed: %LINK_SPEED% Mbps</p>
        </div>
        
        <form onsubmit='return false;'>
            <h2>IP Address Configuration</h2>
            
            <div class='form-group'>
                <label>IP Address:</label>
                <div style='display: flex; align-items: center; gap: 5px;'>
                    <input type='number' id='octet1' min='0' max='255' style='width: 60px' required>
                    <span>.</span>
                    <input type='number' id='octet2' min='0' max='255' style='width: 60px' required>
                    <span>.</span>
                    <input type='number' id='octet3' min='0' max='255' style='width: 60px' required>
                    <span>.</span>
                    <span style='font-weight: bold;'>126</span>
                </div>
                <div class='help-text'>
                    Configure the first three octets of the IP address. The last octet is fixed at 126.
                </div>
            </div>
            
            <div style='margin-top: 20px;'>
                <button type='button' class='btn btn-primary' onclick='saveIPSettings()'>Save</button>
                <button type='button' class='btn btn-warning' onclick='rebootSystem()'>Reboot</button>
                <button type='button' class='btn' onclick='window.location.href="/"'>Cancel</button>
            </div>
            
            <p style='margin-top: 20px; color: #666;'>
                <strong>Note:</strong> After saving, you must click Reboot for the new IP address to take effect.
            </p>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // EN_NETWORK_PAGE_H