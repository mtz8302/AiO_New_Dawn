// en_WASDemoPage.h
// English version of WAS demo page - Pure CSS horizontal bar

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
        
        /* Horizontal bar display */
        .bar-display {
            width: 600px;
            margin: 20px auto;
            position: relative;
        }
        
        /* Background bar with color zones */
        .bar-background {
            height: 40px;
            border: 2px solid #333;
            border-radius: 5px;
            position: relative;
            overflow: hidden;
            background: linear-gradient(to right, 
                #ff4444 0%, 
                #ff4444 44.4%, 
                #44ff44 44.4%, 
                #44ff44 55.6%, 
                #ff4444 55.6%, 
                #ff4444 100%);
        }
        
        /* Moving indicator */
        .bar-indicator {
            position: absolute;
            width: 4px;
            height: 50px;
            background-color: #000;
            top: -5px;
            left: 50%;
            transform: translateX(-2px);
            transition: left 0.3s ease-out;
        }
        
        /* Scale markings */
        .scale-marks {
            position: relative;
            height: 10px;
            margin-top: 5px;
        }
        
        .scale-mark {
            position: absolute;
            width: 2px;
            height: 10px;
            background-color: #333;
            transform: translateX(-1px);
        }
        
        /* Scale numbers */
        .scale-numbers {
            position: relative;
            height: 20px;
            margin-top: 5px;
            font-size: 12px;
            font-weight: bold;
        }
        
        .scale-number {
            position: absolute;
            transform: translateX(-50%);
        }
        
        /* Value display */
        .value-display {
            font-size: 32px;
            font-weight: bold;
            margin: 20px 0;
            padding: 15px;
            border: 2px solid #333;
            border-radius: 5px;
            background-color: #f8f8f8;
            display: inline-block;
            min-width: 150px;
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
            font-size: 16px;
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
            
            <div class='bar-display'>
                <div class='bar-background'>
                    <div id='indicator' class='bar-indicator'></div>
                </div>
                
                <div class='scale-marks'>
                    <div class='scale-mark' style='left: 0%;'></div>
                    <div class='scale-mark' style='left: 16.67%;'></div>
                    <div class='scale-mark' style='left: 33.33%;'></div>
                    <div class='scale-mark' style='left: 50%;'></div>
                    <div class='scale-mark' style='left: 66.67%;'></div>
                    <div class='scale-mark' style='left: 83.33%;'></div>
                    <div class='scale-mark' style='left: 100%;'></div>
                </div>
                
                <div class='scale-numbers'>
                    <span class='scale-number' style='left: 0%;'>-45</span>
                    <span class='scale-number' style='left: 16.67%;'>-30</span>
                    <span class='scale-number' style='left: 33.33%;'>-15</span>
                    <span class='scale-number' style='left: 50%;'>0</span>
                    <span class='scale-number' style='left: 66.67%;'>15</span>
                    <span class='scale-number' style='left: 83.33%;'>30</span>
                    <span class='scale-number' style='left: 100%;'>45</span>
                </div>
            </div>
            
            <div class='value-display'>
                <span id='angleValue'>----</span>
            </div>
            
            <div class='status-text'>
                <div>Update Rate: <span id='updateRate'>0 Hz</span></div>
            </div>
        </div>
        
        <button type='button' onclick='window.location.href = \'/\''>Back to Home</button>
    </div>
    
    <script>
        var eventSource = null;
        var updateCount = 0;
        var lastCountTime = Date.now();
        
        window.onload = function() {
            eventSource = new EventSource('/events/was');
            
            eventSource.addEventListener('was-data', function(e) {
                var data = JSON.parse(e.data);
                var angle = data.angle;
                
                // Update numeric display
                document.getElementById('angleValue').textContent = angle.toFixed(1);
                
                // Calculate position (map -45 to 45 into 0% to 100%)
                var percent = ((angle + 45) / 90) * 100;
                percent = Math.max(0, Math.min(100, percent));
                
                // Update indicator position
                document.getElementById('indicator').style.left = percent + '%';
                
                // Update stats
                updateCount++;
                var now = Date.now();
                if (now - lastCountTime >= 1000) {
                    document.getElementById('updateRate').textContent = updateCount + ' Hz';
                    updateCount = 0;
                    lastCountTime = now;
                }
            });
            
            eventSource.onopen = function() {
                document.getElementById('connectionStatus').textContent = 'Connected';
                document.getElementById('connectionStatus').className = 'connection-status connected';
            };
            
            eventSource.onerror = function() {
                document.getElementById('connectionStatus').textContent = 'Connection Error';
                document.getElementById('connectionStatus').className = 'connection-status disconnected';
            };
        };
        
        window.onbeforeunload = function() {
            if (eventSource) {
                eventSource.close();
            }
        };
    </script>
</body>
</html>
)rawliteral";

#endif // EN_WASDEMO_PAGE_H