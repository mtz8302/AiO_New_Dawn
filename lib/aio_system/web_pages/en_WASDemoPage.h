// en_WASDemoPage.h
// English version of WAS demo page - Simple text display

#ifndef EN_WASDEMO_PAGE_H
#define EN_WASDEMO_PAGE_H

#include <Arduino.h>

const char EN_WASDEMO_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>WAS Live Data Demo</title>
    <style>
        %CSS_STYLES%
        .was-container {
            text-align: center;
            margin: 20px 0;
        }
        .angle-display {
            font-size: 48px;
            font-weight: bold;
            margin: 30px 0;
            padding: 20px;
            border: 3px solid #333;
            border-radius: 10px;
            background-color: #f0f0f0;
        }
        .connection-status {
            padding: 5px 10px;
            border-radius: 5px;
            display: inline-block;
            margin: 10px 0;
        }
        .connected {
            background-color: #4CAF50;
            color: white;
        }
        .disconnected {
            background-color: #f44336;
            color: white;
        }
        .status-text {
            margin: 20px 0;
            font-size: 18px;
        }
    </style>
</head>
<body>
    <div class='container'>
        <h1>WAS Live Data Demo</h1>
        
        <div class='was-container'>
            <div id='connectionStatus' class='connection-status disconnected'>
                Connecting...
            </div>
            
            <div class='angle-display'>
                WAS Angle: <span id='angleValue'>--.-</span> degrees
            </div>
            
            <div class='status-text'>
                <div>Update Rate: <span id='updateRate'>-- Hz</span></div>
                <div>Last Update: <span id='lastUpdate'>--</span></div>
            </div>
        </div>
        
        <button type='button' onclick='window.location="/"'>Back to Home</button>
    </div>
    
    <script>
        let eventSource = null;
        let updateCount = 0;
        let lastCountTime = Date.now();
        
        function connectSSE() {
            if (eventSource) return;
            
            eventSource = new EventSource('/events/was');
            
            eventSource.addEventListener('was-data', function(e) {
                const data = JSON.parse(e.data);
                updateAngle(data.angle);
                updateStats();
            });
            
            eventSource.onopen = function() {
                document.getElementById('connectionStatus').textContent = 'Connected';
                document.getElementById('connectionStatus').className = 'connection-status connected';
            };
            
            eventSource.onerror = function() {
                document.getElementById('connectionStatus').textContent = 'Connection Error';
                document.getElementById('connectionStatus').className = 'connection-status disconnected';
            };
        }
        
        function disconnectSSE() {
            if (eventSource) {
                eventSource.close();
                eventSource = null;
            }
            document.getElementById('connectionStatus').textContent = 'Disconnected';
            document.getElementById('connectionStatus').className = 'connection-status disconnected';
            document.getElementById('angleValue').textContent = '--.-';
            document.getElementById('updateRate').textContent = '-- Hz';
        }
        
        function updateAngle(angle) {
            document.getElementById('angleValue').textContent = angle.toFixed(1);
            document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
        }
        
        function updateStats() {
            updateCount++;
            const now = Date.now();
            if (now - lastCountTime >= 1000) {
                const rate = updateCount;
                document.getElementById('updateRate').textContent = rate + ' Hz';
                updateCount = 0;
                lastCountTime = now;
            }
        }
        
        // Initialize on page load
        window.onload = function() {
            connectSSE();
        };
        
        // Cleanup on page unload
        window.onbeforeunload = function() {
            disconnectSSE();
        };
    </script>
</body>
</html>
)rawliteral";

#endif // EN_WASDEMO_PAGE_H