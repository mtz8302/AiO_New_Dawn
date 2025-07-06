// en_DeviceSettingsPage.h
// English version of Device Settings page

#ifndef EN_DEVICE_SETTINGS_PAGE_H
#define EN_DEVICE_SETTINGS_PAGE_H

#include <Arduino.h>

const char EN_DEVICE_SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Device Settings - AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
    <script>
        function saveSettings() {
            const settings = {
                udpPassthrough: document.getElementById('udpPassthrough').checked,
                sensorFusion: document.getElementById('sensorFusion').checked
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
                if (data.success) {
                    document.getElementById('status').innerHTML = 
                        '<span style="color: green;">Settings saved successfully!</span>';
                } else {
                    document.getElementById('status').innerHTML = 
                        '<span style="color: red;">Error saving settings: ' + data.error + '</span>';
                }
            })
            .catch((error) => {
                document.getElementById('status').innerHTML = 
                    '<span style="color: red;">Error: ' + error + '</span>';
            });
        }
        
        function loadSettings() {
            fetch('/api/device/settings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('udpPassthrough').checked = data.udpPassthrough || false;
                    document.getElementById('sensorFusion').checked = data.sensorFusion || false;
                })
                .catch((error) => {
                    console.error('Error loading settings:', error);
                });
        }
        
        // Load settings when page loads
        window.onload = function() {
            loadSettings();
        };
    </script>
</head>
<body>
    <div class='container'>
        <h1>Device Settings</h1>
        
        <form onsubmit='saveSettings(); return false;'>
            <h2>GPS Configuration</h2>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='udpPassthrough' name='udpPassthrough' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>GPS-UDP Passthrough</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    Enable direct UDP passthrough of NMEA sentences from GPS1 to AgIO.
                </div>
            </div>
            
            <h2>Steering Configuration</h2>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='sensorFusion' name='sensorFusion' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>Enable Virtual WAS (VWAS)</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    Use Keya motor encoder and GPS/IMU to create a virtual wheel angle sensor. Requires Keya CAN motor and vehicle movement.
                </div>
            </div>
            
            <div id='status' style='margin: 10px 0;'></div>
            
            <button type='submit' class='btn btn-primary'>Save Settings</button>
            <button type='button' class='btn' onclick='window.location="/"'>Back to Home</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // EN_DEVICE_SETTINGS_PAGE_H